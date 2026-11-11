/* Voice routing (N48). The game gives each listener 8 voice slots. A direct voice is heard within 256 units. A rule says that one
 * speaker is never heard by one listener, or is heard at any distance. The rules live in a short list, and the voice code asks
 * sv_voice_mode for each pair of players. */
#include "plugin_host.h"
#include "plugin_entities.h"
#include "sv_game_write.h"
#include "plugin_game_internal.h"
#include "player.h"
#include "server.h"
#include "sr_runtime.h"
#include "sr_server.h"
#include "voice_encode.h"
#include <windows.h>
#include <stdlib.h>
#include <string.h>

#define RULE_MAX 256

typedef struct {
    SvOwner owner;
    SvEntity speaker, listener;
    int32_t speaker_index, listener_index;
    uint64_t reset;
    uint32_t mode;
} Rule;

static Rule rules[RULE_MAX];
static int rules_used;

static int rule_live(const Rule *r)
{
    return r->owner && r->reset == sv_game_reset_sequence() &&
        sv_entity_resolve(SV_ENTITY_PLAYER, r->speaker) == r->speaker_index &&
        sv_entity_resolve(SV_ENTITY_PLAYER, r->listener) == r->listener_index;
}

SvResult SV_CALL sv_voice_route(SvOwner owner, SvEntity speaker, SvEntity listener, uint32_t mode)
{
    int32_t s, l;
    int i, free_slot = -1, found = -1;
    if (!sv_plugins_write_context(owner) || !sv_game_is_ready())
        return SV_STATE;
    s = sv_entity_resolve(SV_ENTITY_PLAYER, speaker);
    l = sv_entity_resolve(SV_ENTITY_PLAYER, listener);
    if (s < 0 || l < 0)
        return SV_NOT_FOUND;
    if (s == l || mode > SV_VOICE_FORCE)
        return SV_INVALID;
    /* Only a real player has a connection to listen with. */
    if (sr_players[l].is_bot)
        return SV_DENIED;
    for (i = 0; i < RULE_MAX; ++i) {
        if (rules[i].owner && !rule_live(&rules[i]))
            memset(&rules[i], 0, sizeof rules[i]);
        if (!rules[i].owner) {
            if (free_slot < 0)
                free_slot = i;
        } else if (rules[i].speaker_index == s && rules[i].listener_index == l) {
            found = i;
        }
    }
    if (found >= 0) {
        if (rules[found].owner != owner)
            return SV_CONFLICT;
        if (mode == SV_VOICE_DEFAULT) {
            memset(&rules[found], 0, sizeof rules[found]);
            return SV_OK;
        }
        rules[found].mode = mode;
        return SV_OK;
    }
    if (mode == SV_VOICE_DEFAULT)
        return SV_OK;
    if (free_slot < 0)
        return SV_LIMIT;
    rules[free_slot] = (Rule){owner, speaker, listener, s, l, sv_game_reset_sequence(), mode};
    if (free_slot >= rules_used)
        rules_used = free_slot + 1;
    return SV_OK;
}

/* ------------------------------------------------------------ clips (N47)
 * A plugin gives a WAV file. A worker thread turns it into voice frames (voice_encode.c), and the result goes in a cache, so the same sound
 * starts at once the next time. The player sends one frame every 20 ms into the ring of a bot, or into the ring of an emitter when the
 * source is an item. The server tick is 16 ms, so a tick sends 0, 1 or 2 frames. The worker thread touches only its own copy of the file. */
#define PLAY_MAX 8
#define FRAME_MS 20
#define TICK_MS 16
#define VOICE_MODE 1
#define CACHE_MAX 8
#define CACHE_BYTES_MAX (16u << 20)

typedef struct {
    SvOwner owner;
    SvEntity source, listener;
    int32_t index;
    int is_item;
    uint64_t reset;
    uint32_t flags, pos;
    size_t size;
    float range;
    int credit;
    uint8_t *clip;
    /* While the file is in work: a copy of it, the worker thread and its result. */
    uint8_t *wav;
    size_t wav_size;
    uint64_t key;
    HANDLE thread;
    volatile LONG done; /* 0 in work, 1 done, 2 failed */
    uint8_t *result;
    size_t result_size;
} Play;

typedef struct {
    uint64_t key, used;
    size_t wav_size, bytes;
    uint8_t *clip;
} CacheEntry;

static Play plays[PLAY_MAX];
static VoiceRing item_rings[PLAY_MAX];
static CacheEntry cache[CACHE_MAX];
static uint64_t cache_clock;

static uint64_t hash_bytes(const uint8_t *data, size_t size)
{
    uint64_t h = 1469598103934665603ull;
    size_t i;
    for (i = 0; i < size; ++i)
        h = (h ^ data[i]) * 1099511628211ull;
    return h ^ (uint64_t)size;
}

static CacheEntry *cache_find(uint64_t key, size_t wav_size)
{
    int i;
    for (i = 0; i < CACHE_MAX; ++i)
        if (cache[i].clip && cache[i].key == key && cache[i].wav_size == wav_size)
            return &cache[i];
    return NULL;
}

/* The cache keeps its own copy of the clip. The oldest entries go when it holds too much. */
static void cache_add(uint64_t key, size_t wav_size, const uint8_t *clip, size_t bytes)
{
    size_t total = 0;
    int i, slot = -1;
    uint8_t *copy;
    if (bytes > CACHE_BYTES_MAX)
        return;
    for (;;) {
        total = 0;
        slot = -1;
        for (i = 0; i < CACHE_MAX; ++i) {
            if (cache[i].clip)
                total += cache[i].bytes;
            else if (slot < 0)
                slot = i;
        }
        if (slot >= 0 && total + bytes <= CACHE_BYTES_MAX)
            break;
        {
            int oldest = -1;
            for (i = 0; i < CACHE_MAX; ++i)
                if (cache[i].clip && (oldest < 0 || cache[i].used < cache[oldest].used))
                    oldest = i;
            if (oldest < 0)
                return;
            free(cache[oldest].clip);
            memset(&cache[oldest], 0, sizeof cache[oldest]);
        }
    }
    copy = (uint8_t *)malloc(bytes);
    if (!copy)
        return;
    memcpy(copy, clip, bytes);
    cache[slot] = (CacheEntry){key, ++cache_clock, wav_size, bytes, copy};
}

static int play_live(const Play *p)
{
    if (!p->owner || p->reset != sv_game_reset_sequence())
        return 0;
    if (p->is_item)
        return sv_entity_resolve(SV_ENTITY_ITEM, p->source) == p->index && sr_items[p->index].active;
    return sv_entity_resolve(SV_ENTITY_PLAYER, p->source) == p->index && sr_players[p->index].is_bot;
}

/* The worker thread waits first, so its memory is never freed under it. */
static void play_free(Play *p)
{
    if (p->thread) {
        WaitForSingleObject(p->thread, INFINITE);
        CloseHandle(p->thread);
    }
    free(p->result);
    free(p->wav);
    free(p->clip);
    memset(p, 0, sizeof *p);
}

static DWORD WINAPI encode_thread(LPVOID argument)
{
    Play *p = (Play *)argument;
    size_t frames = 0;
    int result = sv_voice_encode(p->wav, p->wav_size, &p->result, &p->result_size, &frames);
    InterlockedExchange(&p->done, result == SV_ENCODE_OK ? 1 : 2);
    return 0;
}

static void ring_put(VoiceRing *ring, const uint8_t *data, uint32_t n)
{
    int k = ring->head & 0x3f;
    memcpy(ring->data[k], data, n);
    ring->size[k] = (int)n;
    ring->mode[k] = VOICE_MODE;
    ring->head = (k + 1) & 0x3f;
}

/* The play of a source, or -1. */
static int play_find(SvEntity source)
{
    int i;
    for (i = 0; i < PLAY_MAX; ++i)
        if (plays[i].owner && plays[i].source == source)
            return i;
    return -1;
}

SvResult SV_CALL sv_voice_play(SvOwner owner, SvEntity source, const void *file, uint32_t size, uint32_t flags, float range,
    SvEntity listener)
{
    int32_t player = -1, item = -1;
    int i, found, slot = -1;
    uint64_t key;
    uint8_t *copy = NULL;
    const CacheEntry *hit;
    if (!sv_plugins_write_context(owner) || !sv_game_is_ready())
        return SV_STATE;
    player = sv_entity_resolve(SV_ENTITY_PLAYER, source);
    if (player < 0)
        item = sv_entity_resolve(SV_ENTITY_ITEM, source);
    if (player < 0 && (item < 0 || !sr_items[item].active))
        return SV_NOT_FOUND;
    if (!file || size < 1 || (flags & ~(uint32_t)SV_VOICE_LOOP) || !(range - range == 0.0f) || range < 0.0f ||
        sv_voice_wav_check((const uint8_t *)file, size) != SV_ENCODE_OK)
        return SV_INVALID;
    if (player >= 0) {
        if (!sr_players[player].is_bot)
            return SV_DENIED;
        if (range != 0.0f || listener != 0)
            return SV_INVALID;
    } else if (listener != 0) {
        int32_t l = sv_entity_resolve(SV_ENTITY_PLAYER, listener);
        if (l < 0)
            return SV_NOT_FOUND;
        if (sr_players[l].is_bot)
            return SV_DENIED;
    }
    if (!sv_voice_available())
        return SV_INCOMPATIBLE;
    for (i = 0; i < PLAY_MAX; ++i)
        if (plays[i].owner && !play_live(&plays[i]))
            play_free(&plays[i]);
    found = play_find(source);
    if (found >= 0 && plays[found].owner != owner)
        return SV_CONFLICT;
    if (found >= 0) {
        slot = found;
    } else {
        for (i = 0; i < PLAY_MAX; ++i)
            if (!plays[i].owner) {
                slot = i;
                break;
            }
    }
    if (slot < 0)
        return SV_LIMIT;
    key = hash_bytes((const uint8_t *)file, size);
    hit = cache_find(key, size);
    if (hit) {
        copy = (uint8_t *)malloc(hit->bytes);
        if (!copy)
            return SV_LIMIT;
        memcpy(copy, hit->clip, hit->bytes);
        ((CacheEntry *)hit)->used = ++cache_clock;
    }
    if (found >= 0)
        play_free(&plays[slot]);
    plays[slot] = (Play){owner, source, listener, player >= 0 ? player : item, player < 0, sv_game_reset_sequence(), flags, 0, 0, range,
        FRAME_MS};
    if (player < 0)
        memset(&item_rings[slot], 0, sizeof item_rings[slot]);
    if (hit) {
        plays[slot].clip = copy;
        plays[slot].size = hit->bytes;
        return SV_OK;
    }
    plays[slot].wav = (uint8_t *)malloc(size);
    if (!plays[slot].wav) {
        play_free(&plays[slot]);
        return SV_LIMIT;
    }
    memcpy(plays[slot].wav, file, size);
    plays[slot].wav_size = size;
    plays[slot].key = key;
    plays[slot].thread = CreateThread(NULL, 0, encode_thread, &plays[slot], 0, NULL);
    if (!plays[slot].thread) {
        play_free(&plays[slot]);
        return SV_FAILED;
    }
    return SV_OK;
}

SvResult SV_CALL sv_voice_stop(SvOwner owner, SvEntity source)
{
    int found;
    if (!sv_plugins_write_context(owner) || !sv_game_is_ready())
        return SV_STATE;
    if (sv_entity_resolve(SV_ENTITY_PLAYER, source) < 0 && sv_entity_resolve(SV_ENTITY_ITEM, source) < 0)
        return SV_NOT_FOUND;
    found = play_find(source);
    if (found < 0)
        return SV_OK;
    if (plays[found].owner != owner)
        return SV_CONFLICT;
    play_free(&plays[found]);
    return SV_OK;
}

/* The worker thread is done: take its clip, keep a copy in the cache, and let the play start. */
static void play_ready(Play *p)
{
    WaitForSingleObject(p->thread, INFINITE);
    CloseHandle(p->thread);
    p->thread = NULL;
    free(p->wav);
    p->wav = NULL;
    if (p->done != 1) {
        free(p->result);
        p->result = NULL;
        sv_log("A voice clip could not be made from the WAV file.");
        play_free(p);
        return;
    }
    cache_add(p->key, p->wav_size, p->result, p->result_size);
    p->clip = p->result;
    p->size = p->result_size;
    p->result = NULL;
    p->credit = FRAME_MS;
}

void sv_voice_tick(void)
{
    int i;
    for (i = 0; i < PLAY_MAX; ++i) {
        Play *p = &plays[i];
        if (!p->owner)
            continue;
        if (!play_live(p)) {
            play_free(p);
            continue;
        }
        if (p->wav) {
            if (!p->done)
                continue;
            play_ready(p);
            if (!p->owner)
                continue;
        }
        p->credit += TICK_MS;
        while (p->credit >= FRAME_MS) {
            uint32_t n;
            p->credit -= FRAME_MS;
            if (p->pos >= p->size) {
                if (!(p->flags & SV_VOICE_LOOP)) {
                    play_free(p);
                    break;
                }
                p->pos = 0;
            }
            n = p->clip[p->pos];
            ring_put(p->is_item ? &item_rings[i] : &sr_speex.voice_ring[p->index], p->clip + p->pos + 1, n);
            p->pos += 1u + n;
        }
    }
}

/* 1 while a clip is in work or plays on the source. */
int sv_voice_busy(SvEntity source)
{
    return play_find(source) >= 0;
}

int sv_voice_emitter_item(int e)
{
    /* An emitter is free until the worker thread has made the clip. */
    if (e < 0 || e >= PLAY_MAX || !plays[e].owner || !plays[e].is_item || !plays[e].clip || !play_live(&plays[e]))
        return -1;
    return plays[e].index;
}

float sv_voice_emitter_range(int e)
{
    return e >= 0 && e < PLAY_MAX && plays[e].range > 0.0f ? plays[e].range : 256.0f;
}

int sv_voice_emitter_hears(int e, int player)
{
    return e >= 0 && e < PLAY_MAX && (plays[e].listener == 0 || sv_entity_resolve(SV_ENTITY_PLAYER, plays[e].listener) == player);
}

const void *sv_voice_emitter_ring(int e)
{
    return &item_rings[e >= 0 && e < PLAY_MAX ? e : 0];
}

int sv_voice_mode(int speaker, int listener)
{
    int i;
    for (i = 0; i < rules_used; ++i)
        if (rules[i].owner && rules[i].speaker_index == speaker && rules[i].listener_index == listener)
            return rule_live(&rules[i]) ? (int)rules[i].mode : SV_VOICE_DEFAULT;
    return SV_VOICE_DEFAULT;
}

void sv_voice_release(SvOwner owner)
{
    int i;
    for (i = 0; i < RULE_MAX; ++i)
        if (rules[i].owner == owner)
            memset(&rules[i], 0, sizeof rules[i]);
    for (i = 0; i < PLAY_MAX; ++i)
        if (plays[i].owner == owner)
            play_free(&plays[i]);
}
