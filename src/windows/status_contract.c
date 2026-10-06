#include "status_contract.h"
#include <stdio.h>
#include <string.h>
void audio_status_begin(ControllerState *s,EndpointRole role,uint64_t generation) {
    if(role==ROLE_MONITOR) { s->monitor_generation=generation; s->monitor_complete=false; s->monitor_error[0]=0; }
    else { s->routing_generation=generation; s->route_complete=false; s->route_error[0]=0; }
}
void audio_status_finish(ControllerState *s,EndpointRole role,const char *error) {
    if(role==ROLE_MONITOR) { s->monitor_complete=true; snprintf(s->monitor_error,sizeof(s->monitor_error),"%s",error?error:""); }
    else { s->route_complete=true; snprintf(s->route_error,sizeof(s->route_error),"%s",error?error:""); }
}
void audio_status_error(ControllerState *s,const char *fallback) {
    snprintf(s->last_error,sizeof(s->last_error),"%s",s->route_error[0]?s->route_error:s->monitor_error[0]?s->monitor_error:fallback?fallback:"");
}

bool audio_mailbox_write(AudioStatusMailbox *box,const ControllerState *state) {
    bool post=!box->posted; box->state=*state; box->posted=true; return post;
}
void audio_mailbox_drain(AudioStatusMailbox *box,ControllerState *out) { *out=box->state; box->posted=false; }
void audio_mailbox_reset(AudioStatusMailbox *box) { memset(box,0,sizeof(*box)); }
