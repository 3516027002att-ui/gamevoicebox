#ifndef RONGYU_AUDIO_CONTRACT_H
#define RONGYU_AUDIO_CONTRACT_H
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "device_contract.h"
#define AUDIO_SHARED_EVENT 1u
#define AUDIO_PACKET_SILENT 2u
#define AUDIO_PACKET_DISCONTINUITY 1u
/* Real WASAPI wrappers and platform substitutes both consume these adapters.
   They are synchronous: every call occurs on the sole stream owner thread. */
typedef struct {
    void *ctx;
    bool (*initialize)(void *,uint32_t,int64_t,int64_t);
    bool (*set_event)(void *);
    bool (*start)(void *);
    void (*stop)(void *);
    void (*reset)(void *);
    void (*release_objects)(void *);
    void (*close_event)(void *);
    bool (*padding)(void *,size_t *);
    bool (*get_render)(void *,size_t,void **);
    bool (*release_render)(void *,size_t,bool);
    bool (*get_capture)(void *,const void **,size_t *,unsigned *);
    bool (*release_capture)(void *,size_t);
} AudioStreamOps;
typedef struct { bool initialized,started; } AudioStreamLife;
typedef bool (*AudioFill)(void *,void *,size_t);
typedef bool (*AudioCopy)(void *,const void *,size_t,unsigned);
bool audio_stream_begin(AudioStreamLife *,const AudioStreamOps *);
void audio_stream_close(AudioStreamLife *,const AudioStreamOps *);
bool audio_render_block(const AudioStreamOps *,size_t,AudioFill,void *);
bool audio_capture_packet(const AudioStreamOps *,AudioCopy,void *);
#endif
