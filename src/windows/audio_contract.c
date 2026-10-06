#include "audio_contract.h"
bool audio_stream_begin(AudioStreamLife *life,const AudioStreamOps *ops) {
    life->initialized=false; life->started=false;
    if(!ops->initialize(ops->ctx,AUDIO_SHARED_EVENT,0,0)) return false;
    life->initialized=true;
    if(!ops->set_event(ops->ctx) || !ops->start(ops->ctx)) return false;
    life->started=true; return true;
}
void audio_stream_close(AudioStreamLife *life,const AudioStreamOps *ops) {
    if(life->started) ops->stop(ops->ctx);
    if(life->initialized) ops->reset(ops->ctx);
    /* Interfaces stop referencing event before the event is destroyed. */
    ops->release_objects(ops->ctx); ops->close_event(ops->ctx);
    life->initialized=false; life->started=false;
}
bool audio_render_block(const AudioStreamOps *ops,size_t capacity,AudioFill fill,void *ctx) {
    size_t padding,frames; void *data=NULL; bool ok,released;
    if(!ops->padding(ops->ctx,&padding) || padding>capacity) return false;
    frames=capacity-padding;
    if(!frames) return true;
    if(!ops->get_render(ops->ctx,frames,&data)) return false;
    ok=data && fill(ctx,data,frames);
    released=ops->release_render(ops->ctx,frames,!ok);
    return ok && released;
}
bool audio_capture_packet(const AudioStreamOps *ops,AudioCopy copy,void *ctx) {
    const void *data=NULL; size_t frames=0; unsigned flags=0; bool ok,released;
    if(!ops->get_capture(ops->ctx,&data,&frames,&flags)) return false;
    if(!frames) return true;
    ok=(data || (flags&AUDIO_PACKET_SILENT)) && copy(ctx,data,frames,flags);
    released=ops->release_capture(ops->ctx,frames);
    return ok && released;
}
