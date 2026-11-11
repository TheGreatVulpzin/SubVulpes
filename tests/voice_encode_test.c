/* Tests of the WAV to voice encoder (N47). The frames must be what the client sends: Speex narrowband, mode 5, 38 bytes. The test also
 * decodes the frames with the Speex decoder and checks that the tone survives, so a wrong sample rate or a wrong channel mix shows. */
#include "voice_encode.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <speex/speex.h>

static int failures;

#define CHECK(condition) \
    do { \
        if (!(condition)) { \
            printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #condition); \
            ++failures; \
        } \
    } while (0)

static void put32(uint8_t *p, uint32_t v)
{
    p[0] = (uint8_t)v;
    p[1] = (uint8_t)(v >> 8);
    p[2] = (uint8_t)(v >> 16);
    p[3] = (uint8_t)(v >> 24);
}

/* A WAV file with a tone. format 1 is PCM and 3 is float. */
static uint8_t *make_wav(size_t *size, unsigned format, unsigned bits, unsigned channels, unsigned rate, double seconds, double hertz)
{
    size_t frames = (size_t)(seconds * rate), data = frames * channels * (bits / 8u), i;
    unsigned c;
    uint8_t *wav = (uint8_t *)calloc(1, 44 + data);
    memcpy(wav, "RIFF", 4);
    put32(wav + 4, (uint32_t)(36 + data));
    memcpy(wav + 8, "WAVEfmt ", 8);
    put32(wav + 16, 16);
    wav[20] = (uint8_t)format;
    wav[22] = (uint8_t)channels;
    put32(wav + 24, rate);
    put32(wav + 28, rate * channels * (bits / 8u));
    wav[32] = (uint8_t)(channels * (bits / 8u));
    wav[34] = (uint8_t)bits;
    memcpy(wav + 36, "data", 4);
    put32(wav + 40, (uint32_t)data);
    for (i = 0; i < frames; ++i) {
        double v = 0.3 * sin(6.283185307179586 * hertz * (double)i / rate);
        for (c = 0; c < channels; ++c) {
            uint8_t *p = wav + 44 + (i * channels + c) * (bits / 8u);
            if (format == 3 && bits == 32) {
                float f = (float)v;
                memcpy(p, &f, 4);
            } else if (format == 3) {
                memcpy(p, &v, 8);
            } else if (bits == 8) {
                p[0] = (uint8_t)(128 + (int)(v * 127.0));
            } else if (bits == 16) {
                short x = (short)(v * 32767.0);
                memcpy(p, &x, 2);
            } else if (bits == 24) {
                int x = (int)(v * 8388607.0);
                p[0] = (uint8_t)x;
                p[1] = (uint8_t)(x >> 8);
                p[2] = (uint8_t)(x >> 16);
            } else {
                int x = (int)(v * 2147483647.0);
                memcpy(p, &x, 4);
            }
        }
    }
    *size = 44 + data;
    return wav;
}

/* Decodes the clip. The result is the samples at 8000 Hz. */
static short *decode(const uint8_t *clip, size_t bytes, size_t frames)
{
    void *state = speex_decoder_init(&speex_nb_mode);
    SpeexBits bits;
    short *out = (short *)calloc(frames * 160, sizeof *out);
    size_t pos = 0, f;
    speex_bits_init(&bits);
    for (f = 0; f < frames && pos < bytes; ++f) {
        unsigned n = clip[pos];
        speex_bits_read_from(&bits, (char *)(clip + pos + 1), (int)n);
        speex_decode_int(state, &bits, out + f * 160);
        pos += 1u + n;
    }
    speex_bits_destroy(&bits);
    speex_decoder_destroy(state);
    return out;
}

/* The tone that comes out must have the pitch and the loudness that went in. */
static void check_tone(const uint8_t *clip, size_t bytes, size_t frames, double hertz)
{
    short *pcm = decode(clip, bytes, frames);
    size_t from = 10 * 160, to = frames * 160, i, crossings = 0;
    double sum = 0.0, seconds, measured;
    for (i = from; i < to; ++i) {
        sum += (double)pcm[i] * pcm[i];
        if ((pcm[i - 1] < 0) != (pcm[i] < 0))
            ++crossings;
    }
    seconds = (double)(to - from) / 8000.0;
    measured = crossings / 2.0 / seconds;
    CHECK(sqrt(sum / (double)(to - from)) > 3000.0);
    CHECK(fabs(measured - hertz) < hertz * 0.08);
    free(pcm);
}

static void check_frames(const uint8_t *clip, size_t bytes, size_t frames)
{
    size_t pos = 0, f;
    for (f = 0; f < frames; ++f) {
        CHECK(clip[pos] == 38);
        /* The first bit is 0 for narrowband, then 4 bits with the mode 5. The client frames start in the same way. */
        CHECK((clip[pos + 1] >> 7) == 0 && ((clip[pos + 1] >> 3) & 15) == 5);
        pos += 1u + clip[pos];
    }
    CHECK(pos == bytes);
    CHECK(sv_voice_clip_check(clip, bytes) == 1);
    CHECK(sv_voice_clip_check(clip, bytes - 1) == 0);
}

static void encode_case(unsigned format, unsigned bits, unsigned channels, unsigned rate, double seconds, double hertz)
{
    size_t size, bytes = 0, frames = 0;
    uint8_t *wav = make_wav(&size, format, bits, channels, rate, seconds, hertz), *clip = NULL;
    size_t expected = (size_t)ceil((double)(size_t)(seconds * rate) * 8000.0 / rate / 160.0);
    CHECK(sv_voice_wav_check(wav, size) == SV_ENCODE_OK);
    CHECK(sv_voice_encode(wav, size, &clip, &bytes, &frames) == SV_ENCODE_OK);
    CHECK(clip != NULL && frames + 1 >= expected && frames <= expected + 1);
    if (clip) {
        check_frames(clip, bytes, frames);
        check_tone(clip, bytes, frames, hertz);
    }
    free(clip);
    free(wav);
}

int main(void)
{
    size_t size, bytes, frames;
    uint8_t *wav, *clip = NULL;
    CHECK(sv_voice_available() == 1);

    /* The common cases: 8 kHz mono, CD stereo, 48 kHz, 8-bit, 24-bit, 32-bit and both float widths. */
    encode_case(1, 16, 1, 8000, 1.5, 440.0);
    encode_case(1, 16, 2, 44100, 1.5, 440.0);
    encode_case(1, 16, 2, 48000, 1.0, 600.0);
    encode_case(1, 16, 1, 22050, 1.0, 300.0);
    encode_case(1, 8, 1, 11025, 1.0, 500.0);
    encode_case(1, 24, 1, 44100, 1.0, 440.0);
    encode_case(1, 32, 1, 16000, 1.0, 440.0);
    encode_case(3, 32, 2, 44100, 1.0, 440.0);
    encode_case(3, 64, 1, 8000, 1.0, 440.0);

    /* A sound above the voice band does not fold into it: a 15 kHz tone at 44.1 kHz leaves a quiet signal. */
    wav = make_wav(&size, 1, 16, 1, 44100, 1.0, 15000.0);
    CHECK(sv_voice_encode(wav, size, &clip, &bytes, &frames) == SV_ENCODE_OK);
    if (clip) {
        short *pcm = decode(clip, bytes, frames);
        size_t i;
        double sum = 0.0;
        for (i = 10 * 160; i < frames * 160; ++i)
            sum += (double)pcm[i] * pcm[i];
        CHECK(sqrt(sum / (double)(frames * 160 - 1600)) < 600.0);
        free(pcm);
    }
    free(clip);
    free(wav);

    /* Bad input. */
    wav = make_wav(&size, 1, 16, 1, 8000, 1.0, 440.0);
    CHECK(sv_voice_wav_check(NULL, 100) == SV_ENCODE_BAD);
    CHECK(sv_voice_wav_check(wav, 11) == SV_ENCODE_BAD);
    CHECK(sv_voice_wav_check(wav, 40) == SV_ENCODE_BAD);
    wav[0] = 'X';
    CHECK(sv_voice_wav_check(wav, size) == SV_ENCODE_BAD);
    wav[0] = 'R';
    wav[20] = 2; /* ADPCM */
    CHECK(sv_voice_wav_check(wav, size) == SV_ENCODE_BAD);
    wav[20] = 1;
    wav[22] = 9; /* 9 channels */
    CHECK(sv_voice_wav_check(wav, size) == SV_ENCODE_BAD);
    wav[22] = 1;
    put32(wav + 24, 500); /* 500 Hz */
    CHECK(sv_voice_wav_check(wav, size) == SV_ENCODE_BAD);
    put32(wav + 24, 8000);
    put32(wav + 40, 0); /* no data */
    CHECK(sv_voice_wav_check(wav, size) == SV_ENCODE_BAD);
    CHECK(sv_voice_encode(wav, size, &clip, &bytes, &frames) == SV_ENCODE_BAD && clip == NULL && bytes == 0 && frames == 0);
    put32(wav + 40, 0xffffffffu); /* a streaming writer: the size is too large, so the bytes that exist count */
    CHECK(sv_voice_wav_check(wav, size) == SV_ENCODE_OK);
    free(wav);

    printf(failures ? "voice encoder: %d failures\n" : "voice encoder: all checks passed\n", failures);
    return failures ? 1 : 0;
}
