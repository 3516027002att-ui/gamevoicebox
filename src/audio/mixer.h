#ifndef RONGYU_AUDIO_MIXER_H
#define RONGYU_AUDIO_MIXER_H

#include "clip.h"
#include "../app/clip_catalog.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define MIXER_BUS_RATE UINT32_C(48000)
#define MIXER_MIC_TARGET 1920u
#define MIXER_MIC_CAPACITY 4800u
typedef enum { SEND = 0, MONITOR = 1 } OutputKind;
typedef struct {
    const Clip *clips;
    int clip_index[2];
    double clip_phase[2];
    uint64_t play_generation;
    float sound_gain, mic_gain;
    bool mic_muted;
    float mic_queue[MIXER_MIC_CAPACITY * 2u];
    size_t mic_head, mic_count;
    double mic_phase;
    bool mic_warm;
    uint32_t capture_rate;
    bool capture_previous_valid;
    float capture_previous[2];
    double capture_next;
} Mixer;

/* All mixer operations run on the audio owner's single thread.
 * Clip samples remain owned by the caller for the Mixer's lifetime. */
void mixer_init(Mixer *m, const Clip clips[CLIP_COUNT]);
void mixer_reset(Mixer *m);
void mixer_play(Mixer *m, int clip_index, bool send_active, bool monitor_active);
void mixer_stop_clip(Mixer *m);
/* Route shutdown clears only its own output; send clear also resets capture. */
void mixer_clear_send(Mixer *m);
void mixer_clear_monitor(Mixer *m);
void mixer_set_gains(Mixer *m, float sound, float mic, bool mic_muted);
void mixer_push_mic(Mixer *m, const float *stereo, size_t frames,
                    uint32_t capture_rate, bool discontinuity);
void mixer_render(Mixer *m, OutputKind out, float *stereo, size_t frames,
                  uint32_t render_rate);
size_t mixer_mic_queued_frames(const Mixer *m);

#endif
