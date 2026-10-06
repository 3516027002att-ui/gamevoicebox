#include "command_queue.h"
#include <string.h>
static bool safety_command(const Command *c) {
    return c->kind==COMMAND_GAINS || c->kind==COMMAND_STOP || c->kind==COMMAND_SHUTDOWN || c->kind==COMMAND_CONNECT ||
           c->kind==COMMAND_DEVICE_CHANGED || c->kind==COMMAND_ROUTE_UPDATE ||
           (c->kind==COMMAND_SEND && !c->enabled) || (c->kind==COMMAND_MONITOR && !c->enabled);
}
static void priority_store(AudioCommandQueue *q,unsigned slot,const Command *c,unsigned roles) {
    q->priority[slot]=*c;
    if(roles) q->priority[slot].changed_roles=roles;
    q->priority_mask|=1u<<slot;
}
bool audio_queue_push(AudioCommandQueue *q,const Command *c) {
    bool priority;
    if(!q || !c || q->shutdown || c->sequence<=q->last_sequence) return false;
    priority=q->count==AUDIO_COMMAND_CAPACITY;
    if(priority && !safety_command(c)) return false;
    q->last_sequence=c->sequence;
    if(c->kind==COMMAND_CONNECT) { q->connect_pending=true; q->connect_generation=c->generation; q->latest_gains=c->sequence; }
    else if(c->kind==COMMAND_SEND && !c->enabled) {
        if(q->connect_pending) q->cancel_ready_generation=c->generation;
        q->connect_pending=false;
    } else if((c->kind==COMMAND_ROUTE_UPDATE || c->kind==COMMAND_DEVICE_CHANGED) && (c->changed_roles&(DEVICE_ROLE_MIC|DEVICE_ROLE_SEND))) q->connect_pending=false;
    if(c->kind==COMMAND_CONNECT || c->kind==COMMAND_SEND ||
       ((c->kind==COMMAND_ROUTE_UPDATE || c->kind==COMMAND_DEVICE_CHANGED) && (c->changed_roles&(DEVICE_ROLE_MIC|DEVICE_ROLE_SEND)))) q->latest_route=c->generation;
    if(c->kind==COMMAND_MONITOR ||
       ((c->kind==COMMAND_ROUTE_UPDATE || c->kind==COMMAND_DEVICE_CHANGED) && (c->changed_roles&DEVICE_ROLE_MONITOR))) q->latest_monitor=c->generation;
    if(c->kind==COMMAND_CONNECT && strcmp(q->monitor_id,c->settings.monitor_id)) q->latest_monitor=c->generation;
    if(c->kind==COMMAND_MONITOR || c->kind==COMMAND_CONNECT ||
       (c->kind==COMMAND_ROUTE_UPDATE && (c->changed_roles&DEVICE_ROLE_MONITOR))) memcpy(q->monitor_id,c->settings.monitor_id,sizeof(q->monitor_id));
    if(c->kind==COMMAND_PLAY || c->kind==COMMAND_STOP) q->latest_play=c->generation;
    if(c->kind==COMMAND_GAINS) q->latest_gains=c->sequence;
    if(c->kind==COMMAND_SHUTDOWN) q->shutdown=true;
    if(priority) {
        /* One bounded safety slot per domain, without discarding unrelated
           work or resetting independent streams. New generations supersede
           older same-domain work; mixed endpoint closures are split by role. */
        if(c->kind==COMMAND_STOP) priority_store(q,2,c,0);
        else if(c->kind==COMMAND_MONITOR) priority_store(q,1,c,0);
        else if(c->kind==COMMAND_SEND || c->kind==COMMAND_CONNECT) priority_store(q,0,c,0);
        else if(c->kind==COMMAND_DEVICE_CHANGED || c->kind==COMMAND_ROUTE_UPDATE) {
            if(c->changed_roles&(DEVICE_ROLE_MIC|DEVICE_ROLE_SEND)) priority_store(q,0,c,c->changed_roles&(DEVICE_ROLE_MIC|DEVICE_ROLE_SEND));
            if(c->changed_roles&DEVICE_ROLE_MONITOR) priority_store(q,1,c,DEVICE_ROLE_MONITOR);
            if(!c->changed_roles) priority_store(q,3,c,0);
        } else priority_store(q,3,c,0);
    } else {
        q->commands[(q->head+q->count)%AUDIO_COMMAND_CAPACITY]=*c; ++q->count;
    }
    return true;
}
bool audio_queue_pop(AudioCommandQueue *q,Command *out) {
    unsigned i,best=4;
    if(!q || !out) return false;
    if(q->priority_mask) {
        for(i=0;i<4;++i) if(q->priority_mask&(1u<<i)) {
            if(best==4 || q->priority[i].sequence<q->priority[best].sequence) best=i;
        }
        *out=q->priority[best]; q->priority_mask&=~(1u<<best); return true;
    }
    if(!q->count) return false;
    *out=q->commands[q->head]; q->head=(q->head+1)%AUDIO_COMMAND_CAPACITY; --q->count; return true;
}
bool audio_queue_generation_current(const AudioCommandQueue *q,EndpointRole role,uint64_t generation,uint64_t epoch) {
    return q && !q->shutdown && q->epoch==epoch && generation==(role==ROLE_MONITOR?q->latest_monitor:q->latest_route);
}

bool audio_queue_sequence_current(const AudioCommandQueue *q,uint64_t sequence) { return q && !q->shutdown && sequence>0; }

uint32_t audio_queue_filter_roles(const AudioCommandQueue *q,const Command *c) {
    uint32_t roles=c->changed_roles;
    if(!q || q->shutdown) return 0;
    if(c->generation!=q->latest_route) roles&=~(DEVICE_ROLE_MIC|DEVICE_ROLE_SEND);
    if(c->generation!=q->latest_monitor) roles&=~DEVICE_ROLE_MONITOR;
    return roles;
}

void audio_queue_complete_connect(AudioCommandQueue *q,uint64_t generation) {
    if(q->connect_pending && q->connect_generation==generation) q->connect_pending=false;
}
bool audio_queue_clear_ready(const AudioCommandQueue *q,const Command *c) {
    return c->kind==COMMAND_SEND && !c->enabled && q->cancel_ready_generation==c->generation;
}
