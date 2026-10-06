#include "wasapi.h"
#include "common.h"
#include "devices.h"
#include "audio_contract.h"
#include "command_queue.h"
#include "status_contract.h"
#include "../audio/format.h"
#include "../audio/mixer.h"
#include <audioclient.h>
#include <avrt.h>
#include <ks.h>
#include <ksmedia.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>
#define MAX_ENDPOINT_FRAMES 384000u

typedef struct Worker Worker;
typedef struct Stream {
    Worker *worker;
    EndpointRole role;
    uint64_t generation,queue_epoch,sequence;
    IMMDevice *device;
    IAudioClient *client;
    IAudioCaptureClient *capture;
    IAudioRenderClient *render;
    WAVEFORMATEX *wave;
    AudioFormat format;
    HANDLE event;
    UINT32 capacity;
    float *scratch;
    AudioStreamLife life;
    HRESULT hr;
} Stream;
typedef struct Notification {
    IMMNotificationClient iface;
    volatile LONG refs,closing,in_flight;
    HANDLE event;
} Notification;
struct Worker {
    HANDLE thread,command_event,quit_event,ready_event,changed_event;
    CRITICAL_SECTION queue_lock;
    AudioCommandQueue queue;
    bool accepting,initialized;
    DWORD owner;
    char startup_error[256];
    AudioStatusCallback callback;
    void *callback_ctx;
    IMMDeviceEnumerator *enumerator;
    Notification *notification;
    bool notification_registered;
    Settings settings;
    ControllerState state;
    Stream mic,send,monitor;
    Clip clips[CLIP_COUNT];
    Mixer mixer;
    HANDLE mmcss;
    bool mmcss_warning;
};
static void owner(Stream *s) { assert(GetCurrentThreadId()==s->worker->owner); }
static bool current_generation(Worker *w,EndpointRole role,uint64_t generation,uint64_t sequence) {
    bool current;
    EnterCriticalSection(&w->queue_lock);
    current=w->accepting && audio_queue_sequence_current(&w->queue,sequence) && audio_queue_generation_current(&w->queue,role,generation,w->queue.epoch);
    LeaveCriticalSection(&w->queue_lock);
    return current && WaitForSingleObject(w->quit_event,0)!=WAIT_OBJECT_0;
}
static void report(Worker *w,const char *error) {
    const char *fallback=error?error:w->mmcss_warning?"Audio scheduling priority unavailable; normal priority fallback":NULL;
    audio_status_error(&w->state,fallback);
    if(w->callback) w->callback(w->callback_ctx,&w->state);
}
static bool wave_format(const WAVEFORMATEX *wave,AudioFormat *out,char error[256]) {
    AudioFormat f={0};
    f.rate=wave->nSamplesPerSec; f.channels=wave->nChannels;
    f.container_bits=wave->wBitsPerSample; f.valid_bits=f.container_bits; f.block_align=wave->nBlockAlign;
    if(wave->wFormatTag==WAVE_FORMAT_PCM) f.kind=SAMPLE_PCM;
    else if(wave->wFormatTag==WAVE_FORMAT_IEEE_FLOAT) f.kind=SAMPLE_FLOAT;
    else if(wave->wFormatTag==WAVE_FORMAT_EXTENSIBLE && wave->cbSize>=22) {
        const WAVEFORMATEXTENSIBLE *ext=(const WAVEFORMATEXTENSIBLE *)wave;
        f.valid_bits=ext->Samples.wValidBitsPerSample; f.channel_mask=ext->dwChannelMask;
        if(IsEqualGUID(&ext->SubFormat,&KSDATAFORMAT_SUBTYPE_PCM)) f.kind=SAMPLE_PCM;
        else if(IsEqualGUID(&ext->SubFormat,&KSDATAFORMAT_SUBTYPE_IEEE_FLOAT)) f.kind=SAMPLE_FLOAT;
        else { snprintf(error,256,"Unsupported endpoint audio subformat"); return false; }
    } else { snprintf(error,256,"Unsupported endpoint audio format"); return false; }
    if(!format_validate(&f,error)) return false;
    *out=f; return true;
}
static bool op_initialize(void *ctx,uint32_t flags,int64_t duration,int64_t period) {
    Stream *s=ctx; owner(s); assert(flags==AUDIO_SHARED_EVENT);
    s->hr=IAudioClient_Initialize(s->client,AUDCLNT_SHAREMODE_SHARED,
        AUDCLNT_STREAMFLAGS_EVENTCALLBACK|AUDCLNT_STREAMFLAGS_NOPERSIST,
        duration,period,s->wave,NULL);
    if(FAILED(s->hr)) return false;
    return true;
}
static bool op_event(void *ctx) {
    Stream *s=ctx; owner(s);
    s->hr=IAudioClient_GetBufferSize(s->client,&s->capacity);
    if(FAILED(s->hr) || !s->capacity || s->capacity>MAX_ENDPOINT_FRAMES) { if(SUCCEEDED(s->hr)) s->hr=E_INVALIDARG; return false; }
    s->scratch=calloc((size_t)s->capacity*2,sizeof(float));
    if(!s->scratch) { s->hr=E_OUTOFMEMORY; return false; }
    if(s->role==ROLE_MIC) s->hr=IAudioClient_GetService(s->client,&IID_IAudioCaptureClient,(void **)&s->capture);
    else s->hr=IAudioClient_GetService(s->client,&IID_IAudioRenderClient,(void **)&s->render);
    if(FAILED(s->hr)) return false;
    s->hr=IAudioClient_SetEventHandle(s->client,s->event); return SUCCEEDED(s->hr);
}
static bool op_start(void *ctx) {
    Stream *s=ctx; owner(s);
    bool valid;
    EnterCriticalSection(&s->worker->queue_lock);
    valid=s->worker->accepting && audio_queue_sequence_current(&s->worker->queue,s->sequence) && audio_queue_generation_current(&s->worker->queue,s->role,s->generation,s->queue_epoch);
    LeaveCriticalSection(&s->worker->queue_lock);
    if(!valid || WaitForSingleObject(s->worker->quit_event,0)==WAIT_OBJECT_0) { s->hr=HRESULT_FROM_WIN32(ERROR_CANCELLED); return false; }
    s->hr=IAudioClient_Start(s->client); return SUCCEEDED(s->hr);
}
static void op_stop(void *ctx) { Stream *s=ctx; owner(s); (void)IAudioClient_Stop(s->client); }
static void op_reset(void *ctx) { Stream *s=ctx; owner(s); (void)IAudioClient_Reset(s->client); }
static void op_release(void *ctx) {
    Stream *s=ctx; owner(s);
    if(s->capture) { IAudioCaptureClient_Release(s->capture); s->capture=NULL; }
    if(s->render) { IAudioRenderClient_Release(s->render); s->render=NULL; }
    if(s->client) { IAudioClient_Release(s->client); s->client=NULL; }
    if(s->device) { IMMDevice_Release(s->device); s->device=NULL; }
    CoTaskMemFree(s->wave); s->wave=NULL; free(s->scratch); s->scratch=NULL; s->capacity=0;
}
static void op_close_event(void *ctx) { Stream *s=ctx; owner(s); if(s->event) { CloseHandle(s->event); s->event=NULL; } }
static bool op_padding(void *ctx,size_t *padding) {
    Stream *s=ctx; UINT32 n=0; owner(s); s->hr=IAudioClient_GetCurrentPadding(s->client,&n); *padding=n; return SUCCEEDED(s->hr);
}
static bool op_get_render(void *ctx,size_t frames,void **data) {
    Stream *s=ctx; owner(s); s->hr=IAudioRenderClient_GetBuffer(s->render,(UINT32)frames,(BYTE **)data); return SUCCEEDED(s->hr);
}
static bool op_release_render(void *ctx,size_t frames,bool silent) {
    Stream *s=ctx; owner(s); s->hr=IAudioRenderClient_ReleaseBuffer(s->render,(UINT32)frames,silent?AUDCLNT_BUFFERFLAGS_SILENT:0); return SUCCEEDED(s->hr);
}
static bool op_get_capture(void *ctx,const void **data,size_t *frames,unsigned *flags) {
    Stream *s=ctx; UINT32 n=0; DWORD f=0; owner(s);
    s->hr=IAudioCaptureClient_GetBuffer(s->capture,(BYTE **)data,&n,&f,NULL,NULL);
    *frames=n; *flags=f; return SUCCEEDED(s->hr);
}
static bool op_release_capture(void *ctx,size_t frames) {
    Stream *s=ctx; HRESULT previous=s->hr,hr; owner(s); hr=IAudioCaptureClient_ReleaseBuffer(s->capture,(UINT32)frames); s->hr=FAILED(hr)?hr:previous; return SUCCEEDED(hr);
}
static AudioStreamOps stream_ops(Stream *s) {
    AudioStreamOps o={0}; o.ctx=s; o.initialize=op_initialize; o.set_event=op_event; o.start=op_start;
    o.stop=op_stop; o.reset=op_reset; o.release_objects=op_release; o.close_event=op_close_event;
    o.padding=op_padding; o.get_render=op_get_render; o.release_render=op_release_render;
    o.get_capture=op_get_capture; o.release_capture=op_release_capture; return o;
}
static void close_stream(Stream *s) { AudioStreamOps ops=stream_ops(s); audio_stream_close(&s->life,&ops); }
static void close_send(Worker *w) {
    close_stream(&w->send); close_stream(&w->mic); mixer_clear_send(&w->mixer); w->state.sending=false;
}
static void close_monitor(Worker *w) { close_stream(&w->monitor); mixer_clear_monitor(&w->mixer); w->state.monitoring=false; }
static bool prepare_device(Worker *w,const char *id,EndpointRole role,IMMDevice **device,IAudioClient **client,WAVEFORMATEX **wave,AudioFormat *format,char error[256]) {
    HRESULT hr;
    if(!device_lookup(w->enumerator,id,role,device,error)) return false;
    hr=IMMDevice_Activate(*device,&IID_IAudioClient,CLSCTX_INPROC_SERVER,NULL,(void **)client);
    if(FAILED(hr)) { win_error(error,"Activate audio endpoint",hr); return false; }
    hr=IAudioClient_GetMixFormat(*client,wave);
    if(FAILED(hr)) { win_error(error,"Get endpoint native mix format",hr); return false; }
    return wave_format(*wave,format,error);
}
static bool validate_route(Worker *w,const char *id,EndpointRole role,char error[256]) {
    IMMDevice *d=NULL; IAudioClient *c=NULL; WAVEFORMATEX *wave=NULL; AudioFormat f;
    bool ok=prepare_device(w,id,role,&d,&c,&wave,&f,error);
    CoTaskMemFree(wave); if(c) IAudioClient_Release(c); if(d) IMMDevice_Release(d); return ok;
}
static bool open_stream(Worker *w,Stream *s,const char *id,EndpointRole role,uint64_t generation,uint64_t sequence,char error[256]) {
    AudioStreamOps ops;
    s->worker=w; s->role=role; s->generation=generation; s->sequence=sequence;
    EnterCriticalSection(&w->queue_lock); s->queue_epoch=w->queue.epoch; LeaveCriticalSection(&w->queue_lock);
    if(!prepare_device(w,id,role,&s->device,&s->client,&s->wave,&s->format,error)) { close_stream(s); return false; }
    s->event=CreateEventW(NULL,FALSE,FALSE,NULL);
    if(!s->event) { win_error(error,"Create audio event",HRESULT_FROM_WIN32(GetLastError())); close_stream(s); return false; }
    ops=stream_ops(s);
    if(!audio_stream_begin(&s->life,&ops)) {
        win_error(error,s->hr==E_ACCESSDENIED?"Microphone access denied; check Windows desktop microphone privacy":"Open shared event audio stream",s->hr);
        close_stream(s); return false;
    }
    return true;
}
static bool render_fill(void *ctx,void *data,size_t frames) {
    Stream *s=ctx; assert(frames<=s->capacity);
    mixer_render(&s->worker->mixer,s->role==ROLE_SEND?SEND:MONITOR,s->scratch,frames,s->format.rate);
    format_encode_stereo(&s->format,s->scratch,frames,data); return true;
}
static bool capture_copy(void *ctx,const void *data,size_t frames,unsigned flags) {
    Stream *s=ctx;
    if(frames>s->capacity) { s->hr=E_INVALIDARG; return false; }
    /* No WASAPI packet pointer escapes ReleaseBuffer. */
    format_decode_stereo(&s->format,data,frames,(flags&AUDCLNT_BUFFERFLAGS_SILENT)!=0,s->scratch);
    mixer_push_mic(&s->worker->mixer,s->scratch,frames,s->format.rate,(flags&AUDCLNT_BUFFERFLAGS_DATA_DISCONTINUITY)!=0);
    return true;
}
static void process_render(Worker *w,Stream *s) {
    char error[256]; AudioStreamOps ops=stream_ops(s);
    if(!s->life.started) return;
    if(!audio_render_block(&ops,s->capacity,render_fill,s)) {
        win_error(error,"Render endpoint invalidated or unavailable",s->hr);
        if(s->role==ROLE_MONITOR) { close_monitor(w); audio_status_finish(&w->state,ROLE_MONITOR,error); }
        else { close_send(w); w->state.ready=false; audio_status_finish(&w->state,ROLE_SEND,error); }
        report(w,NULL);
    }
}
static void process_capture(Worker *w) {
    Stream *s=&w->mic; UINT32 pending=0; char error[256]; AudioStreamOps ops=stream_ops(s);
    if(!s->life.started) return;
    /* Drain currently available packets before render; each successful packet
       is copied while owned and released exactly once in the shared adapter. */
    for(;;) {
        s->hr=IAudioCaptureClient_GetNextPacketSize(s->capture,&pending);
        if(FAILED(s->hr)) break;
        if(!pending) return;
        if(!audio_capture_packet(&ops,capture_copy,s)) break;
        if(WaitForSingleObject(w->quit_event,0)==WAIT_OBJECT_0) return;
    }
    win_error(error,"Capture endpoint invalidated or unavailable",s->hr);
    close_send(w); w->state.ready=false; audio_status_finish(&w->state,ROLE_SEND,error); report(w,NULL);
}
static HRESULT STDMETHODCALLTYPE notify_query(IMMNotificationClient *iface,REFIID iid,void **out) {
    if(!out) return E_POINTER;
    *out=NULL;
    if(IsEqualIID(iid,&IID_IUnknown) || IsEqualIID(iid,&IID_IMMNotificationClient)) {
        *out=iface; IMMNotificationClient_AddRef(iface); return S_OK;
    }
    return E_NOINTERFACE;
}
static ULONG STDMETHODCALLTYPE notify_addref(IMMNotificationClient *iface) { return (ULONG)InterlockedIncrement(&((Notification *)iface)->refs); }
static ULONG STDMETHODCALLTYPE notify_release(IMMNotificationClient *iface) {
    Notification *n=(Notification *)iface; ULONG refs=(ULONG)InterlockedDecrement(&n->refs); if(!refs) free(n); return refs;
}
static HRESULT notification_signal(IMMNotificationClient *iface) {
    Notification *n=(Notification *)iface;
    InterlockedIncrement(&n->in_flight);
    if(!InterlockedCompareExchange(&n->closing,0,0)) SetEvent(n->event);
    InterlockedDecrement(&n->in_flight); return S_OK;
}
static HRESULT STDMETHODCALLTYPE notify_state(IMMNotificationClient *i,LPCWSTR id,DWORD state) { (void)id; (void)state; return notification_signal(i); }
static HRESULT STDMETHODCALLTYPE notify_added(IMMNotificationClient *i,LPCWSTR id) { (void)id; return notification_signal(i); }
static HRESULT STDMETHODCALLTYPE notify_removed(IMMNotificationClient *i,LPCWSTR id) { (void)id; return notification_signal(i); }
static HRESULT STDMETHODCALLTYPE notify_default(IMMNotificationClient *i,EDataFlow flow,ERole role,LPCWSTR id) { (void)i; (void)flow; (void)role; (void)id; return S_OK; }
static HRESULT STDMETHODCALLTYPE notify_property(IMMNotificationClient *i,LPCWSTR id,const PROPERTYKEY key) { (void)id; (void)key; return notification_signal(i); }
static IMMNotificationClientVtbl notification_vtable={notify_query,notify_addref,notify_release,notify_state,notify_added,notify_removed,notify_default,notify_property};
static bool stream_still_matches(Worker *w,Stream *s,const char *id,EndpointRole role,char error[256]) {
    IMMDevice *d=NULL; IAudioClient *c=NULL; WAVEFORMATEX *wave=NULL; AudioFormat f; bool ok;
    ok=prepare_device(w,id,role,&d,&c,&wave,&f,error);
    if(ok && s->life.started) {
        ok=f.rate==s->format.rate && f.channels==s->format.channels && f.kind==s->format.kind && f.container_bits==s->format.container_bits && f.valid_bits==s->format.valid_bits && f.channel_mask==s->format.channel_mask;
        if(!ok) snprintf(error,256,"Selected endpoint was reconfigured; reconnect explicitly");
    }
    CoTaskMemFree(wave); if(c) IAudioClient_Release(c); if(d) IMMDevice_Release(d); return ok;
}
static void process_device_change(Worker *w) {
    char error[256]={0};
    if(w->state.ready && (!stream_still_matches(w,&w->mic,w->settings.mic_id,ROLE_MIC,error) ||
                          !stream_still_matches(w,&w->send,w->settings.send_id,ROLE_SEND,error))) {
        close_send(w); w->state.ready=false; audio_status_finish(&w->state,ROLE_SEND,error); report(w,NULL);
    }
    if(w->state.monitoring && !stream_still_matches(w,&w->monitor,w->settings.monitor_id,ROLE_MONITOR,error)) { close_monitor(w); audio_status_finish(&w->state,ROLE_MONITOR,error); report(w,NULL); }
}
static bool dequeue(Worker *w,Command *out) {
    bool have=false; EnterCriticalSection(&w->queue_lock);
    have=audio_queue_pop(&w->queue,out);
    LeaveCriticalSection(&w->queue_lock); return have;
}
static void process_command(Worker *w,const Command *incoming) {
    char error[256]={0}; Command filtered=*incoming; const Command *cmd=&filtered;
    if(cmd->kind!=COMMAND_SHUTDOWN) {
        bool valid;
        EnterCriticalSection(&w->queue_lock);
        valid=audio_queue_sequence_current(&w->queue,cmd->sequence);
        if(cmd->kind==COMMAND_ROUTE_UPDATE || cmd->kind==COMMAND_DEVICE_CHANGED) filtered.changed_roles=audio_queue_filter_roles(&w->queue,cmd);
        LeaveCriticalSection(&w->queue_lock);
        if(!valid || ((cmd->kind==COMMAND_ROUTE_UPDATE || cmd->kind==COMMAND_DEVICE_CHANGED) && !cmd->changed_roles)) return;
    }
    /* A canceled CONNECT can still carry a valid independent headphone edit. */
    if(cmd->kind==COMMAND_CONNECT && strcmp(w->settings.monitor_id,cmd->settings.monitor_id) && current_generation(w,ROLE_MONITOR,cmd->generation,cmd->sequence)) {
        audio_status_begin(&w->state,ROLE_MONITOR,cmd->generation); close_monitor(w);
        memcpy(w->settings.monitor_id,cmd->settings.monitor_id,sizeof(w->settings.monitor_id));
        audio_status_finish(&w->state,ROLE_MONITOR,NULL);
    }
    if(cmd->kind==COMMAND_CONNECT || cmd->kind==COMMAND_SEND ||
       ((cmd->kind==COMMAND_ROUTE_UPDATE || cmd->kind==COMMAND_DEVICE_CHANGED) && (cmd->changed_roles&(DEVICE_ROLE_MIC|DEVICE_ROLE_SEND)))) {
        if(!current_generation(w,ROLE_SEND,cmd->generation,cmd->sequence)) { report(w,NULL); return; }
        audio_status_begin(&w->state,ROLE_SEND,cmd->generation);
    }
    if(cmd->kind==COMMAND_MONITOR || ((cmd->kind==COMMAND_ROUTE_UPDATE || cmd->kind==COMMAND_DEVICE_CHANGED) && (cmd->changed_roles&DEVICE_ROLE_MONITOR))) {
        if(!current_generation(w,ROLE_MONITOR,cmd->generation,cmd->sequence)) return;
        audio_status_begin(&w->state,ROLE_MONITOR,cmd->generation);
    }
    switch(cmd->kind) {
    case COMMAND_CONNECT:
        close_send(w); w->state.ready=false;
        EnterCriticalSection(&w->queue_lock);
        if(cmd->sequence==w->queue.latest_gains) {
            w->settings.sound_gain=cmd->settings.sound_gain; w->settings.mic_gain=cmd->settings.mic_gain; w->settings.mic_muted=cmd->settings.mic_muted;
            mixer_set_gains(&w->mixer,w->settings.sound_gain/100.0f,w->settings.mic_gain/100.0f,w->settings.mic_muted);
        }
        LeaveCriticalSection(&w->queue_lock);
        memcpy(w->settings.mic_id,cmd->settings.mic_id,sizeof(w->settings.mic_id));
        memcpy(w->settings.send_id,cmd->settings.send_id,sizeof(w->settings.send_id));
        if(validate_route(w,w->settings.mic_id,ROLE_MIC,error) && validate_route(w,w->settings.send_id,ROLE_SEND,error) && current_generation(w,ROLE_SEND,cmd->generation,cmd->sequence)) w->state.ready=true;
        EnterCriticalSection(&w->queue_lock); audio_queue_complete_connect(&w->queue,cmd->generation); LeaveCriticalSection(&w->queue_lock);
        audio_status_finish(&w->state,ROLE_SEND,error[0]?error:NULL); report(w,NULL); break;
    case COMMAND_SEND:
        close_send(w);
        EnterCriticalSection(&w->queue_lock); if(audio_queue_clear_ready(&w->queue,cmd)) w->state.ready=false; LeaveCriticalSection(&w->queue_lock);
        if(strcmp(w->settings.mic_id,cmd->settings.mic_id) || strcmp(w->settings.send_id,cmd->settings.send_id)) w->state.ready=false;
        memcpy(w->settings.mic_id,cmd->settings.mic_id,sizeof(w->settings.mic_id));
        memcpy(w->settings.send_id,cmd->settings.send_id,sizeof(w->settings.send_id));
        if(cmd->enabled && w->state.ready) {
            if(open_stream(w,&w->mic,w->settings.mic_id,ROLE_MIC,cmd->generation,cmd->sequence,error) &&
               open_stream(w,&w->send,w->settings.send_id,ROLE_SEND,cmd->generation,cmd->sequence,error) && current_generation(w,ROLE_SEND,cmd->generation,cmd->sequence)) w->state.sending=true;
            else { close_send(w); w->state.ready=false; }
        } else if(cmd->enabled) snprintf(error,256,"Route is no longer ready; reconnect selected devices explicitly");
        audio_status_finish(&w->state,ROLE_SEND,error[0]?error:NULL); report(w,NULL); break;
    case COMMAND_MONITOR:
        close_monitor(w); memcpy(w->settings.monitor_id,cmd->settings.monitor_id,sizeof(w->settings.monitor_id));
        if(cmd->enabled && open_stream(w,&w->monitor,w->settings.monitor_id,ROLE_MONITOR,cmd->generation,cmd->sequence,error) && current_generation(w,ROLE_MONITOR,cmd->generation,cmd->sequence)) w->state.monitoring=true;
        else if(cmd->enabled) close_monitor(w);
        audio_status_finish(&w->state,ROLE_MONITOR,error[0]?error:NULL); report(w,NULL); break;
    case COMMAND_PLAY:
    case COMMAND_STOP:
        EnterCriticalSection(&w->queue_lock);
        if(cmd->generation!=w->queue.latest_play) { LeaveCriticalSection(&w->queue_lock); return; }
        LeaveCriticalSection(&w->queue_lock);
        w->state.playback_generation=cmd->generation;
        if(cmd->kind==COMMAND_STOP) mixer_stop_clip(&w->mixer);
        else mixer_play(&w->mixer,cmd->clip_index,w->state.sending,w->state.monitoring);
        break;
    case COMMAND_GAINS:
        EnterCriticalSection(&w->queue_lock);
        if(cmd->sequence!=w->queue.latest_gains) { LeaveCriticalSection(&w->queue_lock); return; }
        LeaveCriticalSection(&w->queue_lock);
        w->settings.sound_gain=cmd->settings.sound_gain; w->settings.mic_gain=cmd->settings.mic_gain; w->settings.mic_muted=cmd->settings.mic_muted;
        mixer_set_gains(&w->mixer,w->settings.sound_gain/100.0f,w->settings.mic_gain/100.0f,w->settings.mic_muted); break;
    case COMMAND_ROUTE_UPDATE:
    case COMMAND_DEVICE_CHANGED:
        if(cmd->changed_roles&(DEVICE_ROLE_MIC|DEVICE_ROLE_SEND)) {
            close_send(w); w->state.ready=false;
            if(cmd->kind==COMMAND_ROUTE_UPDATE) {
                if(cmd->changed_roles&DEVICE_ROLE_MIC) memcpy(w->settings.mic_id,cmd->settings.mic_id,sizeof(w->settings.mic_id));
                if(cmd->changed_roles&DEVICE_ROLE_SEND) memcpy(w->settings.send_id,cmd->settings.send_id,sizeof(w->settings.send_id));
            }
            audio_status_finish(&w->state,ROLE_SEND,cmd->kind==COMMAND_DEVICE_CHANGED?"Selected endpoint changed; reconnect explicitly":NULL);
        }
        if(cmd->changed_roles&DEVICE_ROLE_MONITOR) {
            close_monitor(w);
            if(cmd->kind==COMMAND_ROUTE_UPDATE) memcpy(w->settings.monitor_id,cmd->settings.monitor_id,sizeof(w->settings.monitor_id));
            audio_status_finish(&w->state,ROLE_MONITOR,cmd->kind==COMMAND_DEVICE_CHANGED?"Selected headphone endpoint changed; re-enable explicitly":NULL);
        }
        report(w,NULL); break;
    case COMMAND_SHUTDOWN: SetEvent(w->quit_event); break;
    }
}
static bool load_clips(Worker *w,char error[256]) {
    wchar_t path[32768]; char *utf8; size_t i;
    for(i=0;i<CLIP_COUNT;++i) {
        wchar_t relative[128]; wchar_t *id=win_utf8_to_wide(clip_catalog_id((int)i));
        if(!id) { snprintf(error,256,"Cannot encode bundled sound name"); return false; }
        swprintf(relative,sizeof(relative)/sizeof(relative[0]),L"assets\\%ls.wav",id); free(id);
        if(!win_root_path(path,sizeof(path)/sizeof(path[0]),relative) || !(utf8=win_wide_to_utf8(path))) { snprintf(error,256,"Cannot locate bundled sounds"); return false; }
        if(!clip_load_wav(utf8,&w->clips[i],error)) { free(utf8); return false; }
        free(utf8);
    }
    mixer_init(&w->mixer,w->clips); return true;
}
static void pump_messages(void) { MSG msg; while(PeekMessageW(&msg,NULL,0,0,PM_REMOVE)) { TranslateMessage(&msg); DispatchMessageW(&msg); } }
static DWORD WINAPI worker_main(void *ctx) {
    Worker *w=ctx; HRESULT hr; DWORD task=0; char error[256]={0}; bool com=false; size_t i;
    w->owner=GetCurrentThreadId(); w->mic.worker=w; w->send.worker=w; w->monitor.worker=w;
    hr=CoInitializeEx(NULL,COINIT_APARTMENTTHREADED); if(FAILED(hr)) { win_error(error,"Initialize audio STA",hr); goto finished; } com=true;
    /* Create the STA message queue before activating any audio interfaces. */
    pump_messages();
    hr=CoCreateInstance(&CLSID_MMDeviceEnumerator,NULL,CLSCTX_INPROC_SERVER,&IID_IMMDeviceEnumerator,(void **)&w->enumerator);
    if(FAILED(hr)) { win_error(error,"Create audio endpoint enumerator",hr); goto finished; }
    w->notification=calloc(1,sizeof(*w->notification));
    if(!w->notification) { snprintf(error,256,"Cannot allocate endpoint notification"); goto finished; }
    w->notification->iface.lpVtbl=&notification_vtable; w->notification->refs=1; w->notification->event=w->changed_event;
    hr=IMMDeviceEnumerator_RegisterEndpointNotificationCallback(w->enumerator,&w->notification->iface);
    if(FAILED(hr)) { win_error(error,"Register endpoint notifications",hr); goto finished; } w->notification_registered=true;
    if(!load_clips(w,error)) goto finished;
    settings_defaults(&w->settings);
    w->mmcss=AvSetMmThreadCharacteristicsW(L"Audio",&task); w->mmcss_warning=w->mmcss==NULL;
    w->initialized=true; SetEvent(w->ready_event); report(w,NULL);
    while(WaitForSingleObject(w->quit_event,0)!=WAIT_OBJECT_0) {
        HANDLE handles[6]; DWORD n=0,result; Command cmd;
        handles[n++]=w->quit_event; handles[n++]=w->command_event; handles[n++]=w->changed_event;
        if(w->mic.event) handles[n++]=w->mic.event;
        if(w->send.event) handles[n++]=w->send.event;
        if(w->monitor.event) handles[n++]=w->monitor.event;
        result=MsgWaitForMultipleObjectsEx(n,handles,INFINITE,QS_ALLINPUT,MWMO_INPUTAVAILABLE);
        if(result==WAIT_FAILED) { win_error(error,"Wait audio events",HRESULT_FROM_WIN32(GetLastError())); report(w,error); break; }
        pump_messages();
        /* Commands precede audio work, and handles are rebuilt after closures. */
        while(dequeue(w,&cmd)) process_command(w,&cmd);
        if(WaitForSingleObject(w->quit_event,0)==WAIT_OBJECT_0) break;
        if(result==WAIT_OBJECT_0+2 || WaitForSingleObject(w->changed_event,0)==WAIT_OBJECT_0) process_device_change(w);
        if(w->mic.life.started) process_capture(w);
        if(w->send.life.started && (result<n+WAIT_OBJECT_0)) process_render(w,&w->send);
        if(w->monitor.life.started && (result<n+WAIT_OBJECT_0)) process_render(w,&w->monitor);
    }
finished:
    close_send(w); close_monitor(w);
    if(w->notification) {
        InterlockedExchange(&w->notification->closing,1);
        if(w->notification_registered) { IMMDeviceEnumerator_UnregisterEndpointNotificationCallback(w->enumerator,&w->notification->iface); w->notification_registered=false; }
        while(InterlockedCompareExchange(&w->notification->in_flight,0,0)) { MsgWaitForMultipleObjectsEx(0,NULL,5,QS_ALLINPUT,MWMO_INPUTAVAILABLE); pump_messages(); }
    }
    if(w->enumerator) { IMMDeviceEnumerator_Release(w->enumerator); w->enumerator=NULL; }
    if(w->notification) { IMMNotificationClient_Release(&w->notification->iface); w->notification=NULL; }
    if(w->mmcss) AvRevertMmThreadCharacteristics(w->mmcss);
    for(i=0;i<CLIP_COUNT;++i) clip_free(&w->clips[i]);
    if(com) CoUninitialize();
    if(!w->initialized) { snprintf(w->startup_error,256,"%s",error); SetEvent(w->ready_event); }
    w->state.ready=false; w->state.sending=false; w->state.monitoring=false; w->state.shutting_down=true; report(w,error[0]?error:NULL);
    return 0;
}
bool audio_worker_start(AudioWorker *out,AudioStatusCallback cb,void *ctx,char error[256]) {
    Worker *w=calloc(1,sizeof(*w)); out->impl=NULL;
    if(!w) { snprintf(error,256,"Cannot allocate audio worker"); return false; }
    InitializeCriticalSection(&w->queue_lock); w->accepting=true; w->callback=cb; w->callback_ctx=ctx;
    w->command_event=CreateEventW(NULL,FALSE,FALSE,NULL); w->quit_event=CreateEventW(NULL,TRUE,FALSE,NULL);
    w->ready_event=CreateEventW(NULL,TRUE,FALSE,NULL); w->changed_event=CreateEventW(NULL,FALSE,FALSE,NULL);
    if(!w->command_event || !w->quit_event || !w->ready_event || !w->changed_event) { win_error(error,"Create audio worker event",HRESULT_FROM_WIN32(GetLastError())); out->impl=w; audio_worker_shutdown(out); return false; }
    w->thread=CreateThread(NULL,0,worker_main,w,0,NULL);
    if(!w->thread) { win_error(error,"Create audio worker thread",HRESULT_FROM_WIN32(GetLastError())); out->impl=w; audio_worker_shutdown(out); return false; }
    /* UI STA continues to dispatch messages while startup activates COM. */
    for(;;) {
        DWORD wait=MsgWaitForMultipleObjectsEx(1,&w->ready_event,INFINITE,QS_ALLINPUT,MWMO_INPUTAVAILABLE);
        if(wait==WAIT_OBJECT_0) break;
        if(wait==WAIT_FAILED) break;
        pump_messages();
    }
    out->impl=w;
    if(!w->initialized) { snprintf(error,256,"%s",w->startup_error); audio_worker_shutdown(out); return false; }
    error[0]=0; return true;
}
bool audio_worker_submit(AudioWorker *worker,const Command *cmd,char error[256]) {
    Worker *w=worker?worker->impl:NULL;
    if(!w || !cmd) { snprintf(error,256,"Audio worker unavailable"); return false; }
    EnterCriticalSection(&w->queue_lock);
    if(!w->accepting || !audio_queue_push(&w->queue,cmd)) {
        LeaveCriticalSection(&w->queue_lock); snprintf(error,256,"Audio command rejected: queue full, closing, or stale sequence"); return false;
    }
    LeaveCriticalSection(&w->queue_lock);
    if(cmd->kind==COMMAND_SHUTDOWN) SetEvent(w->quit_event);
    SetEvent(w->command_event); error[0]=0; return true;
}
void audio_worker_shutdown(AudioWorker *worker) {
    Worker *w=worker?worker->impl:NULL;
    if(!w) return;
    EnterCriticalSection(&w->queue_lock); w->accepting=false; w->queue.shutdown=true; LeaveCriticalSection(&w->queue_lock);
    if(w->quit_event) SetEvent(w->quit_event);
    if(w->thread) {
        for(;;) { DWORD result=MsgWaitForMultipleObjectsEx(1,&w->thread,INFINITE,QS_ALLINPUT,MWMO_INPUTAVAILABLE); if(result==WAIT_OBJECT_0 || result==WAIT_FAILED) break; pump_messages(); }
        CloseHandle(w->thread);
    }
    if(w->command_event) CloseHandle(w->command_event);
    if(w->quit_event) CloseHandle(w->quit_event);
    if(w->ready_event) CloseHandle(w->ready_event);
    if(w->changed_event) CloseHandle(w->changed_event);
    DeleteCriticalSection(&w->queue_lock); free(w); worker->impl=NULL;
}
