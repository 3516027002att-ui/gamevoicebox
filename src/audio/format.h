#ifndef RONGYU_AUDIO_FORMAT_H
#define RONGYU_AUDIO_FORMAT_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef enum { SAMPLE_PCM = 1, SAMPLE_FLOAT = 3 } SampleKind;
typedef struct {
    uint32_t rate;
    uint16_t channels;
    SampleKind kind;
    uint16_t container_bits;
    uint16_t valid_bits;
    uint16_t block_align;
    uint32_t channel_mask;
} AudioFormat;

#define AUDIO_SPEAKER_FRONT_LEFT UINT32_C(0x1)
#define AUDIO_SPEAKER_FRONT_RIGHT UINT32_C(0x2)
#define AUDIO_SPEAKER_FRONT_CENTER UINT32_C(0x4)

bool format_validate(const AudioFormat *f, char error[256]);
/* Capture is downmixed by averaging all input channels, then duplicated.
 * SILENT accepts src == NULL. All input samples are bounded and finite. */
void format_decode_stereo(const AudioFormat *f, const void *src, size_t frames,
                          bool silent, float *dst);
/* Mono averages L/R; otherwise only front L/R speakers receive audio.
 * A zero mask uses the conventional first L/R channel pair. */
void format_encode_stereo(const AudioFormat *f, const float *src, size_t frames,
                          void *dst);

#endif
