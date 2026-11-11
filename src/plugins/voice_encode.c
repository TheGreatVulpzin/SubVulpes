/* WAV to voice frames (N47). See voice_encode.h. The steps: read the header, mix the channels to one, resample to 8000 Hz with a windowed
 * sinc filter, and encode each 160 samples with Speex narrowband at quality 8 (15 kbps, 38 bytes a frame). */
#include "voice_encode.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

#ifdef SV_HAVE_SPEEX
#include <speex/speex.h>
#endif

#define OUT_RATE 8000
#define FRAME_SAMPLES 160
#define SINC_ZEROS 10 /* zero crossings on each side of the filter */

typedef struct {
    unsigned format, channels, rate, bits;
    const uint8_t *data;
    size_t data_size, frames;
} Wav;

static uint32_t read32(const uint8_t *p)
{
    return (uint32_t)p[0] | (uint32_t)p[1] << 8 | (uint32_t)p[2] << 16 | (uint32_t)p[3] << 24;
}

static unsigned read16(const uint8_t *p)
{
    return (unsigned)p[0] | (unsigned)p[1] << 8;
}

static int wav_parse(const uint8_t *wav, size_t size, Wav *out)
{
    size_t pos = 12;
    int have_fmt = 0, have_data = 0;
    Wav w;
    memset(&w, 0, sizeof w);
    if (!wav || size < 12 || size > SV_ENCODE_INPUT_MAX || memcmp(wav, "RIFF", 4) || memcmp(wav + 8, "WAVE", 4))
        return SV_ENCODE_BAD;
    while (pos + 8 <= size) {
        size_t length = read32(wav + pos + 4), body = pos + 8;
        if (!memcmp(wav + pos, "fmt ", 4)) {
            if (length < 16 || length > size - body)
                return SV_ENCODE_BAD;
            w.format = read16(wav + body);
            w.channels = read16(wav + body + 2);
            w.rate = read32(wav + body + 4);
            w.bits = read16(wav + body + 14);
            if (w.format == 0xfffe && length >= 26)
                w.format = read16(wav + body + 24);
            have_fmt = 1;
        } else if (!memcmp(wav + pos, "data", 4)) {
            /* A writer that streams may leave a size that is too large. Use the bytes that exist. */
            w.data = wav + body;
            w.data_size = length > size - body ? size - body : length;
            have_data = 1;
        }
        if (length > size - body)
            break;
        pos = body + length + (length & 1u);
    }
    if (!have_fmt || !have_data || w.channels < 1 || w.channels > 8 || w.rate < 1000 || w.rate > 384000)
        return SV_ENCODE_BAD;
    if (!((w.format == 1 && (w.bits == 8 || w.bits == 16 || w.bits == 24 || w.bits == 32)) ||
          (w.format == 3 && (w.bits == 32 || w.bits == 64))))
        return SV_ENCODE_BAD;
    w.frames = w.data_size / ((size_t)w.channels * (w.bits / 8u));
    if (w.frames < 1 || (double)w.frames * OUT_RATE / w.rate > (double)SV_ENCODE_FRAMES_MAX * FRAME_SAMPLES)
        return SV_ENCODE_BAD;
    *out = w;
    return SV_ENCODE_OK;
}

int sv_voice_wav_check(const uint8_t *wav, size_t size)
{
    Wav w;
    return wav_parse(wav, size, &w);
}

int sv_voice_clip_check(const uint8_t *clip, size_t size)
{
    size_t pos = 0, frames = 0;
    if (!clip)
        return 0;
    while (pos < size) {
        pos += 1u + clip[pos];
        if (pos > size)
            return 0;
        ++frames;
    }
    return frames >= 1 && frames <= SV_ENCODE_FRAMES_MAX;
}

#ifndef SV_HAVE_SPEEX

int sv_voice_available(void)
{
    return 0;
}

int sv_voice_encode(const uint8_t *wav, size_t size, uint8_t **clip, size_t *bytes, size_t *frames)
{
    (void)wav;
    (void)size;
    *clip = NULL;
    *bytes = *frames = 0;
    return SV_ENCODE_MISSING;
}

#else

int sv_voice_available(void)
{
    return 1;
}

/* One sample of the WAV file, with the channels mixed to one. The range is -1 to 1. */
static float mixed_sample(const Wav *w, size_t frame)
{
    const uint8_t *p = w->data + frame * w->channels * (w->bits / 8u);
    double sum = 0.0;
    unsigned c;
    for (c = 0; c < w->channels; ++c, p += w->bits / 8u) {
        if (w->format == 3 && w->bits == 32) {
            float f;
            memcpy(&f, p, sizeof f);
            sum += f;
        } else if (w->format == 3) {
            double d;
            memcpy(&d, p, sizeof d);
            sum += d;
        } else if (w->bits == 8) {
            sum += ((int)p[0] - 128) / 128.0;
        } else if (w->bits == 16) {
            sum += (int16_t)read16(p) / 32768.0;
        } else if (w->bits == 24) {
            int32_t v = (int32_t)((uint32_t)p[0] << 8 | (uint32_t)p[1] << 16 | (uint32_t)p[2] << 24) >> 8;
            sum += v / 8388608.0;
        } else {
            sum += (int32_t)read32(p) / 2147483648.0;
        }
    }
    sum /= w->channels;
    if (!(sum == sum))
        return 0.0f;
    return (float)(sum > 1.0 ? 1.0 : sum < -1.0 ? -1.0 : sum);
}

/* The samples at 8000 Hz. A low-pass filter at 3600 Hz comes first, so a high sound does not fold into the voice band. */
static float *resample(const Wav *w, size_t *count)
{
    size_t out_n = (size_t)((double)w->frames * OUT_RATE / w->rate), i;
    double ratio = (double)w->rate / OUT_RATE;
    double cutoff = (3600.0 < 0.45 * w->rate ? 3600.0 : 0.45 * w->rate) / w->rate; /* cycles for each input sample */
    double half = SINC_ZEROS / (2.0 * cutoff);
    float *out;
    if (out_n < 1)
        out_n = 1;
    out = (float *)malloc(out_n * sizeof *out);
    if (!out)
        return NULL;
    for (i = 0; i < out_n; ++i) {
        double t = i * ratio, weight = 0.0, sum = 0.0;
        long first = (long)ceil(t - half), last = (long)floor(t + half), k;
        if (w->rate == OUT_RATE) {
            out[i] = mixed_sample(w, i < w->frames ? i : w->frames - 1);
            continue;
        }
        for (k = first; k <= last; ++k) {
            double x = k - t, arg = 2.0 * cutoff * x, h, window;
            if (k < 0 || (size_t)k >= w->frames)
                continue;
            h = fabs(arg) < 1e-9 ? 1.0 : sin(3.14159265358979323846 * arg) / (3.14159265358979323846 * arg);
            window = 0.5 + 0.5 * cos(3.14159265358979323846 * x / half);
            h *= window;
            sum += h * mixed_sample(w, (size_t)k);
            weight += h;
        }
        out[i] = weight > 1e-9 ? (float)(sum / weight) : 0.0f;
    }
    *count = out_n;
    return out;
}

int sv_voice_encode(const uint8_t *wav, size_t size, uint8_t **clip, size_t *bytes, size_t *frames)
{
    Wav w;
    float *samples;
    size_t count = 0, total, f;
    uint8_t *out;
    void *state;
    SpeexBits bits;
    int quality = 8, result;
    *clip = NULL;
    *bytes = *frames = 0;
    result = wav_parse(wav, size, &w);
    if (result != SV_ENCODE_OK)
        return result;
    samples = resample(&w, &count);
    if (!samples)
        return SV_ENCODE_MEMORY;
    total = (count + FRAME_SAMPLES - 1) / FRAME_SAMPLES;
    out = (uint8_t *)malloc(total * 256);
    state = speex_encoder_init(&speex_nb_mode);
    if (!out || !state) {
        free(out);
        free(samples);
        if (state)
            speex_encoder_destroy(state);
        return SV_ENCODE_MEMORY;
    }
    speex_encoder_ctl(state, SPEEX_SET_QUALITY, &quality);
    speex_bits_init(&bits);
    *bytes = 0;
    for (f = 0; f < total; ++f) {
        spx_int16_t in[FRAME_SAMPLES];
        int n, i;
        for (i = 0; i < FRAME_SAMPLES; ++i) {
            size_t at = f * FRAME_SAMPLES + (size_t)i;
            double v = at < count ? samples[at] * 32767.0 : 0.0;
            in[i] = (spx_int16_t)(v > 32767.0 ? 32767.0 : v < -32768.0 ? -32768.0 : v);
        }
        speex_bits_reset(&bits);
        speex_encode_int(state, in, &bits);
        n = speex_bits_write(&bits, (char *)out + *bytes + 1, 255);
        out[*bytes] = (uint8_t)n;
        *bytes += 1u + (size_t)n;
    }
    speex_bits_destroy(&bits);
    speex_encoder_destroy(state);
    free(samples);
    *clip = out;
    *frames = total;
    return SV_ENCODE_OK;
}

#endif
