#ifndef SV_VOICE_ENCODE_H
#define SV_VOICE_ENCODE_H

#include <stddef.h>
#include <stdint.h>

/* WAV to voice frames (N47). The client sends Speex narrowband, 15 kbps, 160 samples (20 ms) for each frame. This file makes the same
 * frames from a WAV file, so a plugin can play any sound that it has as a WAV file. The result is a clip: for each frame one byte with
 * the size, then that many bytes. The Speex source is not part of the repository (see tools/voice/fetch_speex.ps1). A build without it
 * still checks the header of a WAV file, and it cannot encode. */

enum {
    SV_ENCODE_OK = 0,
    SV_ENCODE_BAD = 1,     /* the bytes are not a WAV file that the encoder can use */
    SV_ENCODE_MISSING = 2, /* this build has no Speex encoder */
    SV_ENCODE_MEMORY = 3
};

#define SV_ENCODE_INPUT_MAX (64u << 20) /* bytes of the WAV file */
#define SV_ENCODE_FRAMES_MAX 90000u     /* 30 minutes */

/* 1 when this build can encode. */
int sv_voice_available(void);

/* Reads the header only. SV_ENCODE_OK for a file that sv_voice_encode accepts (PCM of 8, 16, 24 or 32 bits, or float of 32 or 64 bits,
 * 1 to 8 channels, 1000 to 384000 Hz, at most 30 minutes). */
int sv_voice_wav_check(const uint8_t *wav, size_t size);

/* Makes the clip. On success *clip is memory from malloc (free it), and *frames and *bytes tell the size. */
int sv_voice_encode(const uint8_t *wav, size_t size, uint8_t **clip, size_t *bytes, size_t *frames);

/* A clip that the voice code can play: frames of one size byte and the bytes, 1 to SV_ENCODE_FRAMES_MAX frames. */
int sv_voice_clip_check(const uint8_t *clip, size_t size);

#endif
