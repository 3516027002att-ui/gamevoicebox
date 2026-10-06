#ifndef RONGYU_AUDIO_CLIP_H
#define RONGYU_AUDIO_CLIP_H

#include <stdbool.h>
#include <stddef.h>

typedef struct { float *samples; size_t frames; } Clip;

/* Success publishes an owned, complete 48 kHz stereo float clip.
 * Failure leaves *out unchanged. Free an old clip before loading into it. */
bool clip_load_wav(const char *path, Clip *out, char error[256]);
void clip_free(Clip *clip);

#endif
