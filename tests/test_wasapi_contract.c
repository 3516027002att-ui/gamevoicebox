/* Platform call substitutes: this is NOT evidence of Windows audio runtime. */
#include "../src/windows/audio_contract.h"
#include "../src/windows/status_contract.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

typedef struct Fake {
    int owner, thread, fail_stage, stage, outstanding, releases, fills;
    size_t requested, padding, capacity, frames;
    unsigned flags;
    unsigned char data[256];
    char order[80];
    size_t order_len;
} Fake;
static void log_op(Fake *f, char c) { f->order[f->order_len++]=c; f->order[f->order_len]=0; }
static bool init(void *ctx, uint32_t flags, int64_t duration, int64_t period) {
    Fake *f=ctx; assert(flags==AUDIO_SHARED_EVENT); assert(duration==0 && period==0);
    assert(f->owner==f->thread); log_op(f,'I'); return f->fail_stage!=1;
}
static bool event(void *ctx) { Fake *f=ctx; log_op(f,'E'); return f->fail_stage!=2; }
static bool start(void *ctx) { Fake *f=ctx; log_op(f,'S'); return f->fail_stage!=3; }
static void stop(void *ctx) { Fake *f=ctx; assert(!f->outstanding); log_op(f,'s'); }
static void reset(void *ctx) { log_op(ctx,'r'); }
static void release(void *ctx) { log_op(ctx,'l'); }
static void close_event(void *ctx) { log_op(ctx,'c'); }
static bool padding(void *ctx,size_t *n) { Fake *f=ctx; *n=f->padding; return true; }
static bool get_render(void *ctx,size_t n,void **data) {
    Fake *f=ctx; assert(f->owner==f->thread); assert(!f->outstanding);
    assert(n<=f->capacity-f->padding); f->requested=n; f->outstanding=1; *data=f->data; return true;
}
static bool put_render(void *ctx,size_t n,bool silent) {
    Fake *f=ctx; (void)silent; assert(f->owner==f->thread && f->outstanding);
    assert(n==f->requested); f->outstanding=0; ++f->releases; return true;
}
static bool fill(void *ctx,void *data,size_t n) {
    Fake *f=ctx; assert(f->outstanding); assert(data==f->data && n==f->requested);
    ++f->fills; return f->fail_stage!=4;
}
static bool get_capture(void *ctx,const void **data,size_t *n,unsigned *flags) {
    Fake *f=ctx; assert(!f->outstanding); f->outstanding=f->frames?1:0;
    *data=(f->flags&AUDIO_PACKET_SILENT)?NULL:f->data; *n=f->frames; *flags=f->flags; return true;
}
static bool copy_capture(void *ctx,const void *data,size_t n,unsigned flags) {
    Fake *f=ctx; assert(f->outstanding); assert(n==f->frames);
    assert(data!=NULL || (flags&AUDIO_PACKET_SILENT)); ++f->fills; return f->fail_stage!=4;
}
static bool put_capture(void *ctx,size_t n) {
    Fake *f=ctx; assert(f->owner==f->thread && f->outstanding && n==f->frames);
    f->outstanding=0; ++f->releases; return true;
}
static AudioStreamOps ops(Fake *f) {
    AudioStreamOps o={0}; o.ctx=f; o.initialize=init; o.set_event=event; o.start=start;
    o.stop=stop; o.reset=reset; o.release_objects=release; o.close_event=close_event;
    o.padding=padding; o.get_render=get_render; o.release_render=put_render;
    o.get_capture=get_capture; o.release_capture=put_capture; return o;
}
static void test_lifecycle(void) {
    int i;
    for(i=0;i<4;++i) {
        Fake f={0}; AudioStreamLife l={0}; AudioStreamOps o=ops(&f); f.fail_stage=i;
        assert(audio_stream_begin(&l,&o)==(i==0));
        audio_stream_close(&l,&o); assert(!f.outstanding);
        assert(strstr(f.order,"lc")!=NULL); assert(f.order[f.order_len-1]=='c');
        if(i==0) assert(strcmp(f.order,"IESsrlc")==0);
        if(i==1) assert(strcmp(f.order,"Ilc")==0);
        if(i==2) assert(strcmp(f.order,"IErlc")==0);
        if(i==3) assert(strcmp(f.order,"IESrlc")==0);
    }
}
static void test_render_and_capture(void) {
    Fake f={0}; AudioStreamOps o=ops(&f); f.capacity=32; f.padding=7;
    assert(audio_render_block(&o,f.capacity,fill,&f));
    assert(f.requested==25 && f.releases==1 && f.fills==1);
    f.padding=32; assert(audio_render_block(&o,32,fill,&f)); assert(f.releases==1);
    f.padding=33; assert(!audio_render_block(&o,32,fill,&f)); assert(f.releases==1);
    f.padding=0; f.fail_stage=4; assert(!audio_render_block(&o,32,fill,&f)); assert(f.releases==2);
    f.fail_stage=0; f.frames=13; f.flags=AUDIO_PACKET_SILENT;
    assert(audio_capture_packet(&o,copy_capture,&f)); assert(f.releases==3);
    f.fail_stage=4; assert(!audio_capture_packet(&o,copy_capture,&f)); assert(f.releases==4);
    f.frames=0; assert(audio_capture_packet(&o,copy_capture,&f)); assert(f.releases==4);
}
static void test_endpoint_policy(void) {
    EndpointView mic={"mic-id","USB mic",DEVICE_CAPTURE,true,CABLE_NONE};
    EndpointView cable={"send-id","CABLE Input (VB-Audio Virtual Cable)",DEVICE_RENDER,true,CABLE_STANDARD_INPUT};
    EndpointView hp={"hp-id","same very long friendly name",DEVICE_RENDER,true,CABLE_NONE};
    char error[256]={0};
    assert(endpoint_validate(&mic,ROLE_MIC,error)); assert(endpoint_validate(&cable,ROLE_SEND,error));
    assert(endpoint_validate(&hp,ROLE_MONITOR,error));
    assert(!endpoint_validate(&cable,ROLE_MONITOR,error));
    cable.active=false; assert(!endpoint_validate(&cable,ROLE_SEND,error)); cable.active=true;
    mic.cable=CABLE_OTHER; assert(!endpoint_validate(&mic,ROLE_MIC,error));
    mic.cable=CABLE_NONE; mic.direction=DEVICE_RENDER; assert(!endpoint_validate(&mic,ROLE_MIC,error));
    hp.id="send-id"; assert(!endpoint_validate(&hp,ROLE_SEND,error));
    assert(endpoint_classify_metadata("CABLE Input","VB-Audio Virtual Cable")==CABLE_STANDARD_INPUT);
    assert(endpoint_classify_metadata("CABLE Output","VB-Audio Virtual Cable")==CABLE_OTHER);
    assert(endpoint_classify_metadata("CABLE-A Input","VB-Audio Cable A")==CABLE_OTHER);
    assert(endpoint_classify_metadata("Cable Headphones","USB Cable Audio")==CABLE_NONE);
    assert(endpoint_classify_metadata(NULL,"USB Audio")==CABLE_UNVERIFIED);
    assert(endpoint_classify_metadata("Microphone",NULL)==CABLE_UNVERIFIED);
    /* Editable display names never enter classification. */
    mic.name="Renamed real-looking microphone";
    mic.direction=DEVICE_CAPTURE;
    mic.cable=endpoint_classify_metadata("CABLE Output","VB-Audio Virtual Cable");
    assert(!endpoint_validate(&mic,ROLE_MIC,error));
    hp.name="Renamed headphones";
    hp.cable=endpoint_classify_metadata("CABLE Input","VB-Audio Virtual Cable");
    assert(!endpoint_validate(&hp,ROLE_MONITOR,error));
    cable.name="Renamed send endpoint";
    cable.cable=endpoint_classify_metadata("CABLE Input","VB-Audio Virtual Cable");
    assert(endpoint_validate(&cable,ROLE_SEND,error));
    mic.cable=CABLE_UNVERIFIED; assert(!endpoint_validate(&mic,ROLE_MIC,error));

}
static void test_duplicate_labels(void) {
    char *a=endpoint_display_label("Identical full friendly name","opaque ID A",true);
    char *b=endpoint_display_label("Identical full friendly name","opaque ID B",true);
    char *reordered=endpoint_display_label("Identical full friendly name","opaque ID A",true);
    char long_name[2048]; char *long_label;
    assert(a && b && reordered && strcmp(a,b) && !strcmp(a,reordered));
    assert(strstr(a,"Identical full friendly name")==a);
    memset(long_name,'x',sizeof(long_name)-1); long_name[sizeof(long_name)-1]=0;
    long_label=endpoint_display_label(long_name,"ID",true);
    assert(long_label && strlen(long_label)>strlen(long_name));
    assert(!memcmp(long_label,long_name,strlen(long_name)));
    free(a); free(b); free(reordered); free(long_label);
}
static void test_durable_domain_status(void) {
    ControllerState state={0};
    audio_status_begin(&state,ROLE_SEND,10);
    state.ready=false;
    audio_status_finish(&state,ROLE_SEND,"Microphone privacy denied");
    audio_status_begin(&state,ROLE_MONITOR,20);
    state.monitoring=true;
    audio_status_finish(&state,ROLE_MONITOR,NULL);
    audio_status_error(&state,NULL);
    assert(state.route_complete && state.monitor_complete);
    assert(!strcmp(state.route_error,"Microphone privacy denied"));
    assert(!strcmp(state.last_error,"Microphone privacy denied"));
    audio_status_begin(&state,ROLE_SEND,11);
    assert(!state.route_complete && !state.route_error[0]);
    assert(state.monitor_complete && state.monitoring);
}
static void test_mailbox_close_restart(void) {
    AudioStatusMailbox box={0}; ControllerState state={0},out;
    state.routing_generation=10; state.sending=true;
    assert(audio_mailbox_write(&box,&state));
    state.sending=false; state.shutting_down=true;
    assert(!audio_mailbox_write(&box,&state));
    audio_mailbox_drain(&box,&out); /* Still drain during close/modal. */
    assert(!box.posted && !out.sending && out.shutting_down);
    audio_mailbox_reset(&box);
    state=(ControllerState){0};
    assert(audio_mailbox_write(&box,&state)); /* Restart must post fresh state. */
    audio_mailbox_drain(&box,&out); assert(!out.shutting_down && !box.posted);
}
int main(void) { test_mailbox_close_restart(); test_durable_domain_status(); test_duplicate_labels(); test_lifecycle(); test_render_and_capture(); test_endpoint_policy(); puts("WASAPI shared adapters: 6 contract groups passed (platform substitutes, NOT Windows runtime)"); return 0; }
