/* Portable production queue: no Windows runtime claims. */
#include "../src/windows/command_queue.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static void fill(AudioCommandQueue *q,uint64_t start) {
    Command c={0}; unsigned i; c.kind=COMMAND_GAINS;
    for(i=0;i<AUDIO_COMMAND_CAPACITY;++i) { c.sequence=start+i; assert(audio_queue_push(q,&c)); }
}
int main(void) {
    AudioCommandQueue q={0}; Command c={0},out;
    q.latest_route=10; q.latest_monitor=20; fill(&q,1);
    c.kind=COMMAND_PLAY; c.sequence=100; assert(!audio_queue_push(&q,&c));
    c.kind=COMMAND_GAINS; c.sequence=101; c.settings.mic_muted=true;
    assert(audio_queue_push(&q,&c)); /* Mute must never be dropped under load. */
    assert(audio_queue_pop(&q,&out) && out.kind==COMMAND_GAINS && out.settings.mic_muted);
    c.sequence=102; c.settings.mic_muted=false; assert(audio_queue_push(&q,&c));
    c.sequence=103; c.settings.mic_muted=true; assert(audio_queue_push(&q,&c));
    assert(audio_queue_pop(&q,&out) && out.sequence==103 && out.settings.mic_muted);
    c.kind=COMMAND_STOP; c.sequence=104; c.generation=30;
    assert(audio_queue_push(&q,&c));
    assert(q.count==AUDIO_COMMAND_CAPACITY); /* No unrelated work discarded. */
    assert(q.latest_route==10 && q.latest_monitor==20 && q.epoch==0);
    assert(audio_queue_pop(&q,&out) && out.kind==COMMAND_STOP); /* Priority first. */
    c.kind=COMMAND_SEND; c.enabled=false; c.generation=11; c.sequence=105;
    assert(audio_queue_push(&q,&c));
    assert(q.count==AUDIO_COMMAND_CAPACITY && q.latest_route==11 && q.latest_monitor==20);
    assert(!audio_queue_generation_current(&q,ROLE_SEND,10,0));
    assert(audio_queue_generation_current(&q,ROLE_MONITOR,20,0));
    c.kind=COMMAND_MONITOR; c.enabled=false; c.generation=21; c.sequence=106;
    assert(audio_queue_push(&q,&c));
    assert(q.latest_route==11 && q.latest_monitor==21);
    assert(audio_queue_pop(&q,&out) && out.kind==COMMAND_SEND);
    assert(audio_queue_pop(&q,&out) && out.kind==COMMAND_MONITOR);
    assert(audio_queue_pop(&q,&out) && out.kind==COMMAND_GAINS); /* Original ordinary work remains. */
    c.kind=COMMAND_MONITOR; c.enabled=true; c.generation=22; c.sequence=107; strcpy(c.settings.monitor_id,"headphone-A");
    assert(audio_queue_push(&q,&c));
    c.kind=COMMAND_CONNECT; c.generation=12; c.sequence=108; strcpy(c.settings.monitor_id,"headphone-B");
    assert(audio_queue_push(&q,&c)); assert(q.latest_monitor==12);
    assert(!audio_queue_generation_current(&q,ROLE_MONITOR,22,0));
    c.kind=COMMAND_ROUTE_UPDATE; c.changed_roles=DEVICE_ROLE_ALL; c.generation=13; c.sequence=109;
    assert(audio_queue_push(&q,&c)); assert(q.latest_route==13 && q.latest_monitor==13);
    assert(!audio_queue_push(&q,&c));
    {
        Command mixed=c;
        c.kind=COMMAND_SEND; c.enabled=false; c.generation=14; c.sequence=110;
        assert(audio_queue_push(&q,&c));
        assert(audio_queue_filter_roles(&q,&mixed)==DEVICE_ROLE_MONITOR);
    }
    c.kind=COMMAND_SHUTDOWN; c.sequence=111; assert(audio_queue_push(&q,&c)); assert(q.shutdown);
    assert(!audio_queue_generation_current(&q,ROLE_SEND,13,0));
    assert(!audio_queue_sequence_current(&q,107));
    {
        AudioCommandQueue pending={0}; Command connect={0},off={0};
        connect.kind=COMMAND_CONNECT; connect.sequence=1; connect.generation=1;
        assert(audio_queue_push(&pending,&connect));
        off.kind=COMMAND_SEND; off.enabled=false; off.sequence=2; off.generation=2;
        assert(audio_queue_push(&pending,&off));
        assert(audio_queue_clear_ready(&pending,&off)); /* Same-ID pending re-CONNECT canceled. */
        audio_queue_complete_connect(&pending,1);
        connect.sequence=3; connect.generation=3; assert(audio_queue_push(&pending,&connect));
        audio_queue_complete_connect(&pending,3);
        off.sequence=4; off.generation=4; assert(audio_queue_push(&pending,&off));
        assert(!audio_queue_clear_ready(&pending,&off)); /* Established route survives SEND_OFF. */
    }
    puts("Production command queue: full STOP/send-off/monitor-off preserve independent domains; stale opens canceled; priority bounded"); return 0;
}
