#ifndef RONGYU_AUDIO_COMMAND_QUEUE_H
#define RONGYU_AUDIO_COMMAND_QUEUE_H
#include "../app/controller.h"
#include "device_contract.h"
#include <stddef.h>
#define AUDIO_COMMAND_CAPACITY 64u
/* Caller supplies synchronization. This exact queue is used by WASAPI worker. */
typedef struct {
    Command commands[AUDIO_COMMAND_CAPACITY];
    Command priority[4];
    unsigned priority_mask;
    size_t head,count;
    uint64_t latest_route,latest_monitor,latest_play,last_sequence,latest_gains,epoch,connect_generation,cancel_ready_generation;
    bool shutdown,connect_pending;
    char monitor_id[SETTINGS_ENDPOINT_CAP];
} AudioCommandQueue;
bool audio_queue_push(AudioCommandQueue *queue,const Command *command);
bool audio_queue_pop(AudioCommandQueue *queue,Command *out);
void audio_queue_complete_connect(AudioCommandQueue *,uint64_t generation);
bool audio_queue_clear_ready(const AudioCommandQueue *,const Command *);
uint32_t audio_queue_filter_roles(const AudioCommandQueue *,const Command *);
bool audio_queue_sequence_current(const AudioCommandQueue *,uint64_t sequence);
bool audio_queue_generation_current(const AudioCommandQueue *,EndpointRole,uint64_t generation,uint64_t epoch);
#endif
