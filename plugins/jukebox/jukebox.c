/* Native SDK plugin. Playback advances only when the host sends a server tick. */
#include "sv_game_read.h"
#include "sv_game_write.h"
#include "sv_config.h"
#include "files.h"
#include "gm.h"
#include <errno.h>
#include <math.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum { IDLE, MIDI, DEMO, SCALE, CHORD, TRY };
static const SvHost *host;
static const SvGameRead *game;
static const SvGameWrite *write_api;
static const SvConfig *config;
static SvOwner self;
static JbSong song;
static JbInstruments instruments;
static JbSounds sounds;
static int fixed_sound = 39;
static int auto_channels = 3;
static int default_bpm, default_speed = 100, default_key;
static int bpm, speed = 100, song_key;
static int default_master = 100, master = 100;
/* Whole PPQ ticks and billionths avoid a product with the full song length. */
typedef struct { uint64_t whole, fraction, remainder; size_t tempo; } MusicPosition;
static MusicPosition music;
static uint64_t busy_until[16], server_ticks;
static JbRoute routes[16];
static int resume_output[16];
static char song_name[JB_NAME_MAX + 1];
static int bank = JB_VOICES, everyone, max_per_tick = 8, trace;
static struct {
    int mode, keypad;
    SvEntity starter;
    size_t next;
    unsigned step, chord;
    int try_sound, try_root;
    uint64_t elapsed, played, dropped_notes, dropped_calls, moved, failed, skipped_mono;
} play;

typedef struct {
    JbNote note;
    uint32_t order;
    int output, bank, transpose, voice, root;
} Candidate;
typedef struct {
    const Candidate *candidate;
    SvVector position;
    unsigned target;
    JbTone tone;
} SoundCall;

static void tell(SvEntity actor, const char *format, ...)
{
    char text[256];
    const char *at;
    va_list args;
    va_start(args, format);
    vsnprintf(text, sizeof text, format, args);
    va_end(args);
    host->log(self, text);
    at = text;
    while (actor && *at) {
        char line[61];
        size_t length = strlen(at), take = length > 60 ? 60 : length;
        if (take < length) {
            size_t cut = take;
            while (cut > 30 && at[cut] != ' ')
                --cut;
            if (at[cut] == ' ')
                take = cut;
        }
        memcpy(line, at, take);
        line[take] = 0;
        write_api->message_to(self, SV_MESSAGE_SERVER, actor, line);
        at += take;
        while (*at == ' ')
            ++at;
    }
}

static int integer(const char *text, long minimum, long maximum, int *out)
{
    char *end;
    long value;
    errno = 0;
    value = strtol(text, &end, 10);
    if (!*text || *end || errno || value < minimum || value > maximum)
        return 0;
    *out = (int)value;
    return 1;
}

static int position(SvEntity actor, SvVector *out)
{
    SvPlayerState player = { sizeof player };
    SvHumanState human = { sizeof human };
    if (game->player(self, actor, &player) != SV_OK || !player.human ||
        game->human(self, player.human, &human) != SV_OK || human.health <= 0)
        return 0;
    *out = human.position;
    return 1;
}

static size_t listeners(SvVector positions[256])
{
    uint32_t cursor = 0;
    size_t total = 0;
    if (!everyone)
        return position(play.starter, &positions[0]) ? 1 : 0;
    while (cursor < 256) {
        SvEntity page[64];
        uint32_t count, next, i;
        if (game->entities(self, SV_ENTITY_PLAYER, cursor, page, 64, &count, &next) != SV_OK || count > 64)
            break;
        for (i = 0; i < count && total < 256; ++i) {
            SvPlayerState player = { sizeof player };
            if (game->player(self, page[i], &player) == SV_OK && !player.is_bot && position(page[i], &positions[total]))
                ++total;
        }
        if (next <= cursor || next > 256)
            break;
        cursor = next;
    }
    return total;
}

static void reset_routes(void)
{
    int i;
    for (i = 0; i < 16; ++i) {
        routes[i].output = i == 9 ? JB_DRUMS : JB_SYNTH;
        routes[i].muted = routes[i].transpose = 0;
        resume_output[i] = routes[i].output;
        routes[i].voice = -1;
        routes[i].mono = i != 9;
        routes[i].automatic = 0;
        routes[i].volume = 100;
        routes[i].root = sounds.sounds[39].root;
    }
}

static void select_channels(const JbSong *next)
{
    unsigned chosen = 0;
    int rank, i, best;
    if (!auto_channels) return;
    for (rank = 0; rank < auto_channels; ++rank) {
        best = -1;
        for (i = 0; i < 16; ++i)
            if (i != 9 && !(chosen & (1u << i)) && next->channels[i].notes &&
                (best < 0 || next->channels[i].notes > next->channels[best].notes)) best = i;
        if (best < 0) break;
        chosen |= 1u << best;
    }
    for (i = 0; i < 16; ++i)
        if (i != 9 && next->channels[i].notes && !(chosen & (1u << i)))
            routes[i].muted = routes[i].automatic = 1;
}

static void keypad(void)
{
    JbRoute active[16];
    int i;
    memcpy(active, routes, sizeof active);
    for (i = 0; i < 16; ++i) active[i].transpose += song_key;
    play.keypad = bank == JB_BANK_KEYPAD ? jb_keypad_octaves(&instruments, &song, active) : 0;
}

static uint64_t gcd(uint64_t a, uint64_t b)
{
    while (b) { uint64_t r = a % b; a = b; b = r; }
    return a;
}

static uint32_t file_tempo(const MusicPosition *p)
{
    return song.tempo_count ? song.tempos[p->tempo].usec : 500000;
}

static void tempo_position(MusicPosition *p)
{
    p->tempo = 0;
    while (p->tempo + 1 < song.tempo_count && song.tempos[p->tempo + 1].tick <= p->whole) ++p->tempo;
}

static MusicPosition next_position(void)
{
    const uint64_t unit = 1000000000u;
    uint64_t budget = unit;
    MusicPosition p = music;
    tempo_position(&p);
    while (budget) {
        uint64_t n = bpm ? (uint64_t)song.division * bpm : (uint64_t)song.division * 1000000u * speed;
        uint64_t d = bpm ? 3600u : (uint64_t)file_tempo(&p) * 6000u;
        uint64_t common = gcd(n, d), delta, whole, fraction;
        n /= common;
        d /= common;
        /* Reduced n is at most 6553400000. budget*n plus a remainder fits uint64_t. */
        delta = (budget * n + p.remainder) / d;
        whole = p.whole + delta / unit;
        fraction = p.fraction + delta % unit;
        whole += fraction / unit;
        fraction %= unit;
        if (!bpm && p.tempo + 1 < song.tempo_count && song.tempos[p.tempo + 1].tick <= whole) {
            uint64_t boundary = song.tempos[p.tempo + 1].tick;
            uint64_t distance = (boundary - p.whole) * unit - p.fraction;
            uint64_t consumed = (distance / n) * d + ((distance % n) * d + n - 1) / n;
            if (consumed > budget) consumed = budget;
            budget -= consumed;
            p.whole = boundary;
            p.fraction = p.remainder = 0;
            ++p.tempo;
        } else {
            p.remainder = (budget * n + p.remainder) % d;
            p.whole = whole;
            p.fraction = fraction;
            budget = 0;
        }
    }
    return p;
}

static const char *mode_name(void)
{
    if (bank == JB_VOICES) return "voices";
    if (bank == JB_FIXED) return "fixed";
    if (bank == JB_BANK_NEAREST) return "phone";
    return jb_bank_name(bank);
}

static int mode_value(const char *text, int *next_bank, int *next_sound)
{
    if (!strcmp(text, "voices")) { *next_bank = JB_VOICES; return 1; }
    if (!strcmp(text, "phone")) { *next_bank = JB_BANK_NEAREST; return 1; }
    if (!strncmp(text, "fixed ", 6) && jb_sound_index(&sounds, text + 6, next_sound)) {
        *next_bank = JB_FIXED;
        return 1;
    }
    return jb_bank_index(text, next_bank);
}

static int read_settings(void)
{
    char value[64], legacy[64];
    int next_bank, next_sound = fixed_sound;
    int32_t next_everyone, next_max, next_trace, next_auto, next_bpm, next_speed, next_key, next_master;
    if (config->get_text(self, "mode", value, sizeof value) != SV_OK || !mode_value(value, &next_bank, &next_sound)) {
        tell(0, "Invalid mode. Use voices, phone, keypad or fixed SOUND.");
        return 0;
    }
    if (config->get_int(self, "everyone", &next_everyone) != SV_OK || next_everyone < 0 || next_everyone > 1 ||
        config->get_int(self, "max_per_tick", &next_max) != SV_OK || next_max < 1 || next_max > 32 ||
        config->get_int(self, "trace", &next_trace) != SV_OK || next_trace < 0 || next_trace > 1) {
        tell(0, "Invalid jukebox settings. Check everyone, max_per_tick and trace.");
        return 0;
    }
    if (config->get_text(self, "bank", legacy, sizeof legacy) == SV_OK && *legacy &&
        !strcmp(value, "voices")) {
        if (!mode_value(legacy, &next_bank, &next_sound)) {
            tell(0, "Invalid legacy bank. Clear bank to use mode.");
            return 0;
        }
    }
    if (config->get_int(self, "auto_channels", &next_auto) != SV_OK || next_auto < 0 || next_auto > 16) {
        tell(0, "Auto channels must be a number from 0 to 16.");
        return 0;
    }
    if (config->get_int(self, "default_bpm", &next_bpm) != SV_OK || next_bpm < 0 || next_bpm > 300 ||
        (next_bpm && next_bpm < 20) || config->get_int(self, "default_speed", &next_speed) != SV_OK ||
        next_speed < 25 || next_speed > 400 || config->get_int(self, "default_key", &next_key) != SV_OK ||
        next_key < -48 || next_key > 48) {
        tell(0, "Invalid tempo or key defaults. BPM: 0 or 20..300, speed: 25..400, key: -48..48.");
        return 0;
    }
    if (config->get_int(self, "default_master", &next_master) != SV_OK || next_master < 0 || next_master > 200) {
        tell(0, "Default master must be 0 to 200 percent.");
        return 0;
    }
    default_master = next_master;
    default_bpm = next_bpm;
    default_speed = next_speed;
    default_key = next_key;
    auto_channels = next_auto;
    bank = next_bank;
    fixed_sound = next_sound;
    everyone = next_everyone;
    max_per_tick = next_max;
    trace = next_trace;
    keypad();
    return 1;
}

static void start(int mode, SvEntity actor)
{
    memset(&play, 0, sizeof play);
    memset(&music, 0, sizeof music);
    play.mode = mode;
    play.starter = actor;
    keypad();
}

static int prepare(SvEntity actor, JbInstruments *next, JbSounds *next_sounds)
{
    char error[160];
    SvVector point;
    if (!position(actor, &point)) {
        tell(actor, "You need a live body to start the jukebox.");
        return 0;
    }
    if (!jb_load_instruments(next, error, sizeof error) || !jb_load_sounds(next_sounds, error, sizeof error)) {
        tell(actor, "%s", error);
        return 0;
    }
    return 1;
}

static void mix_state(JbMix *mix)
{
    memcpy(mix->routes, routes, sizeof routes);
    memcpy(mix->resume, resume_output, sizeof resume_output);
    mix->bank = bank;
    mix->fixed_sound = fixed_sound;
    mix->bpm = bpm;
    mix->speed = speed;
    mix->key = song_key;
    mix->master = master;
}

static void restore_mix(const JbMix *mix)
{
    memcpy(routes, mix->routes, sizeof routes);
    memcpy(resume_output, mix->resume, sizeof resume_output);
    bank = mix->bank;
    fixed_sound = mix->fixed_sound;
    bpm = mix->bpm;
    speed = mix->speed;
    song_key = mix->key;
    master = mix->master;
    keypad();
}

static void load_song(SvEntity actor, const char *input)
{
    JbSong next = {0};
    JbInstruments voices;
    JbSounds next_sounds;
    char name[JB_NAME_MAX + 1], error[160];
    char matches[8][JB_NAME_MAX + 1];
    size_t found, i;
    int resolved = jb_resolve_song(input, name, matches, &found, error, sizeof error);
    if (resolved != 1) {
        if (resolved < 0) tell(actor, "%s", error);
        else if (!resolved) tell(actor, "No song matches. Use /jb list.");
        else {
            tell(actor, "More than one song matches:");
            for (i = 0; i < found && i < 8; ++i) tell(actor, "%s", matches[i]);
            if (found > 8) tell(actor, "More matches exist. Type more of the name.");
        }
        return;
    }
    if (!prepare(actor, &voices, &next_sounds))
        return;
    if (!jb_load_song(name, &next, error, sizeof error)) {
        tell(actor, "%s", error);
        return;
    }
    if (!next.count) {
        jb_song_free(&next);
        tell(actor, "The song has no note-on events.");
        return;
    }
    if (strcmp(name, song_name)) {
        reset_routes();
        select_channels(&next);
    }
    jb_song_free(&song);
    song = next;
    instruments = voices;
    sounds = next_sounds;
    strcpy(song_name, name);
    bpm = default_bpm;
    speed = bpm ? 100 : default_speed;
    song_key = default_key;
    master = default_master;
    {
        JbMix mix;
        int loaded = jb_load_mix(name, &mix, error, sizeof error);
        if (loaded > 0) { restore_mix(&mix); tell(actor, "Loaded preset for %s.", name); }
        else if (loaded < 0) tell(actor, "Preset ignored: %s", error);
    }
    start(MIDI, actor);
    tell(actor, "Playing %s: %zu notes, keypad octave shift=%d.", name, song.count, play.keypad);
}

static void demo(SvEntity actor)
{
    /* A short public-domain note pattern, not a MIDI or audio file. */
    static const JbNote notes[] = {
        { 0, 0, 60, 127, 0 }, { 0, 9, 36, 100, 0 }, { 15, 0, 64, 96, 0 },
        { 30, 0, 67, 64, 0 }, { 30, 9, 42, 80, 0 }, { 45, 0, 72, 127, 0 }
    };
    JbInstruments voices;
    JbSounds next_sounds;
    JbSong next = {0};
    size_t i;
    if (!prepare(actor, &voices, &next_sounds))
        return;
    next.notes = malloc(sizeof notes);
    if (!next.notes) {
        tell(actor, "Not enough memory for the demo.");
        return;
    }
    memcpy(next.notes, notes, sizeof notes);
    next.count = sizeof notes / sizeof notes[0];
    next.end_tick = 45;
    next.division = 480;
    next.end_midi_tick = 720;
    for (i = 0; i < next.count; ++i) {
        const JbNote *note = &next.notes[i];
        next.notes[i].midi_tick = note->tick * 16;
        JbChannel *c = &next.channels[note->channel];
        if (!c->notes)
            c->low = c->high = note->key;
        if (note->key < c->low)
            c->low = note->key;
        if (note->key > c->high)
            c->high = note->key;
        ++c->notes;
    }
    if (strcmp(song_name, "#demo")) {
        reset_routes();
        select_channels(&next);
        /* Tune this short listening pattern so the ping tails finish between notes. */
        routes[0].voice = 39;
        routes[0].root = 60;
    }
    jb_song_free(&song);
    song = next;
    instruments = voices;
    sounds = next_sounds;
    strcpy(song_name, "#demo");
    bpm = default_bpm;
    speed = bpm ? 100 : default_speed;
    song_key = default_key;
    master = default_master;
    start(DEMO, actor);
    tell(actor, "Playing built-in demo: 6 notes over 45 server ticks.");
}

static void offer(Candidate top[32], size_t *count, const Candidate *candidate)
{
    size_t i, worst = 0;
    if (*count < 32) {
        top[(*count)++] = *candidate;
        return;
    }
    for (i = 1; i < *count; ++i)
        if (top[i].note.velocity < top[worst].note.velocity ||
            (top[i].note.velocity == top[worst].note.velocity && top[i].order > top[worst].order))
            worst = i;
    if (candidate->note.velocity > top[worst].note.velocity)
        top[worst] = *candidate;
}

static int priority(const void *left, const void *right)
{
    const Candidate *a = left, *b = right;
    if (a->note.velocity != b->note.velocity)
        return (int)b->note.velocity - a->note.velocity;
    return a->order < b->order ? -1 : a->order != b->order;
}

static int call_order(const void *left, const void *right)
{
    const SoundCall *a = left, *b = right;
    if (a->candidate->order != b->candidate->order)
        return a->candidate->order < b->candidate->order ? -1 : 1;
    return a->target < b->target ? -1 : a->target != b->target;
}

static JbTone candidate_tone(const Candidate *c)
{
    if (c->output == JB_DRUMS) return jb_drum(&instruments, c->note.key, c->transpose);
    if (c->voice >= 0) return jb_voice(c->voice, c->root, c->note.key + c->transpose);
    if (c->bank == JB_VOICES || c->bank == JB_FIXED) {
        int sound = c->bank == JB_FIXED ? fixed_sound : 39;
        return jb_voice(sound, sounds.sounds[sound].root, c->note.key + c->transpose);
    }
    return jb_synth(&instruments, c->note.key + c->transpose, c->bank, play.keypad);
}

static void emit(Candidate top[32], size_t count, uint64_t wanted, uint32_t tick)
{
    SvVector positions[256];
    SoundCall calls[32];
    size_t targets = listeners(positions), used = 0, unique = 0, i, j;
    if (!targets && !everyone) {
        play.mode = IDLE;
        tell(play.starter, "Starter needs a live body. Playback stopped.");
        return;
    }
    qsort(top, count, sizeof top[0], priority);
    for (i = 0; i < count && used < (size_t)max_per_tick; ++i) {
        const Candidate *c = &top[i];
        JbTone tone = candidate_tone(c);
        if (targets) {
            ++unique;
            play.moved += tone.octaves != 0;
        }
        for (j = 0; j < targets && used < (size_t)max_per_tick; ++j) {
            SoundCall *call = &calls[used++];
            call->candidate = c;
            call->position = positions[j];
            call->target = (unsigned)j;
            call->tone = tone;
        }
    }
    play.dropped_notes += wanted - unique;
    play.dropped_calls += wanted * targets - used;
    qsort(calls, used, sizeof calls[0], call_order);
    for (i = 0; i < used; ++i) {
        const SoundCall *call = &calls[i];
        const JbNote *note = &call->candidate->note;
        float volume = note->velocity / 127.0f * sounds.sounds[call->tone.sound].gain *
            (routes[note->channel].volume / 100.0f) * (master / 100.0f);
        if (volume > 1.0f) volume = 1.0f;
        if (volume <= 0) continue;
        SvResult result = write_api->sound(self, call->tone.sound, call->position, volume, call->tone.pitch);
        if (result == SV_OK) {
            ++play.played;
            if ((play.mode == MIDI || play.mode == DEMO) && routes[note->channel].mono)
                busy_until[note->channel] = server_ticks + (uint64_t)ceil(sounds.sounds[call->tone.sound].tail / (double)call->tone.pitch);
            if (trace || play.mode != MIDI)
                tell(0, "JUKEBOX_SOUND tick=%u at=%llu ch=%u note=%u sound=%u pitch=%.6f volume=%.6f",
                    tick, (unsigned long long)play.elapsed, note->channel + 1u, note->key,
                    call->tone.sound, call->tone.pitch, volume);
        } else {
            ++play.failed;
            tell(0, "Jukebox sound failed with API result %d.", (int)result);
        }
    }
}

static void tick(uint32_t tick_number)
{
    static const unsigned char major[8] = {0, 2, 4, 5, 7, 9, 11, 12};
    Candidate top[32];
    size_t count = 0;
    uint64_t wanted = 0;
    int done = 0;
    unsigned pending = 0;
    MusicPosition horizon = music;
    if (play.mode == MIDI || play.mode == DEMO) {
        horizon = next_position();
        while (play.next < song.count && (song.notes[play.next].midi_tick <= music.whole ||
            song.notes[play.next].midi_tick < horizon.whole ||
            (song.notes[play.next].midi_tick == horizon.whole && horizon.fraction))) {
            Candidate c = {0};
            c.voice = -1;
            const JbNote *note = &song.notes[play.next];
            const JbRoute *route = &routes[note->channel];
            c.note = *note;
            c.order = (uint32_t)play.next++;
            if (route->output == JB_OFF || route->muted || !route->volume || !master)
                continue;
            if (route->mono && (server_ticks < busy_until[note->channel] || (pending & (1u << note->channel)))) {
                ++play.skipped_mono;
                continue;
            }
            c.output = note->channel == 9 ? JB_DRUMS : route->output;
            c.transpose = route->transpose + (c.output == JB_DRUMS ? 0 : song_key);
            c.bank = bank;
            c.voice = route->voice;
            c.root = route->root;
            if (route->mono && sounds.sounds[candidate_tone(&c).sound].tail) pending |= 1u << note->channel;
            offer(top, &count, &c);
            ++wanted;
        }
        done = play.next == song.count && (song.end_midi_tick <= music.whole || song.end_midi_tick < horizon.whole);
        music = horizon;
    } else if (play.mode == SCALE) {
        if (play.elapsed % 30u == 0 && play.step < (JB_MEMBERS + 2u) * 8u) {
            Candidate c = {0};
            c.voice = -1;
            unsigned group = play.step / 8u, key = play.step % 8u;
            int drums = group == JB_MEMBERS + 1;
            if (!key)
                tell(play.starter, "Scale voice: %s.", drums ? "drums" : jb_bank_name(group == JB_MEMBERS ? JB_BANK_NEAREST : (int)group));
            c.note.key = drums ? 36 : (unsigned char)(60 + major[key]);
            c.note.velocity = 100;
            c.note.channel = drums ? 9 : 0;
            c.output = drums ? JB_DRUMS : JB_SYNTH;
            c.transpose = drums ? major[key] : 0;
            c.bank = group == JB_MEMBERS ? JB_BANK_NEAREST : (int)group;
            c.order = play.step++;
            offer(top, &count, &c);
            wanted = 1;
        }
        done = play.step == (JB_MEMBERS + 2u) * 8u;
    } else if (play.mode == TRY) {
        if (play.elapsed % 15u == 0 && play.step < 12) {
            Candidate c = {0};
            c.note.key = (unsigned char)play.try_root;
            c.transpose = (int)play.step;
            c.note.velocity = 100;
            c.voice = play.try_sound;
            c.root = play.try_root;
            c.output = JB_SYNTH;
            c.order = play.step++;
            offer(top, &count, &c);
            wanted = 1;
        }
        done = play.step == 12;
    } else if (play.mode == CHORD) {
        unsigned i;
        for (i = 0; i < play.chord; ++i) {
            Candidate c = {0};
            c.voice = -1;
            c.note.key = (unsigned char)(60 + i);
            c.note.velocity = 100;
            c.output = JB_SYNTH;
            c.bank = bank;
            c.order = i;
            offer(top, &count, &c);
            ++wanted;
        }
        done = 1;
    }
    if (wanted)
        emit(top, count, wanted, tick_number);
    if (done && play.mode != IDLE) {
        tell(play.starter, "Finished: played=%llu dropped_notes=%llu dropped_calls=%llu moved=%llu failed=%llu.",
            (unsigned long long)play.played, (unsigned long long)play.dropped_notes,
            (unsigned long long)play.dropped_calls, (unsigned long long)play.moved, (unsigned long long)play.failed);
        play.mode = IDLE;
    }
    ++play.elapsed;
}

static void channels(SvEntity actor)
{
    int i;
    if (!song.count) {
        tell(actor, "No song loaded. Use /jukebox play NAME or /jukebox demo.");
        return;
    }
    for (i = 0; i < 16; ++i) {
        const JbChannel *c = &song.channels[i];
        const JbRoute *r = &routes[i];
        const char *voice = i == 9 || r->output == JB_DRUMS ? "drums" :
            r->voice >= 0 ? sounds.sounds[r->voice].name :
            bank == JB_VOICES ? sounds.sounds[39].name :
            bank == JB_FIXED ? sounds.sounds[fixed_sound].name : mode_name();
        if (c->notes)
            tell(actor, "[%c] %d %.14s %u notes -> %.12s%s", r->muted || r->output == JB_OFF ? ' ' : 'x',
                i + 1, jb_gm_names[c->first_program], c->notes, voice, r->automatic ? " auto" : "");
    }
}

static int turn_on(int channel)
{
    routes[channel].muted = routes[channel].automatic = 0;
    if (routes[channel].output == JB_OFF)
        routes[channel].output = resume_output[channel];
    return channel;
}

static void channel_command(SvEntity actor, int count, char *words[])
{
    int channel, value, i;
    if (!song.count) {
        tell(actor, "No song loaded. Use /jukebox play NAME or /jukebox demo.");
        return;
    }
    if (!strcmp(words[0], "all") && count == 1) {
        for (i = 0; i < 16; ++i)
            turn_on(i);
    } else if ((!strcmp(words[0], "solo") || !strcmp(words[0], "t") ||
        !strcmp(words[0], "on") || !strcmp(words[0], "off")) && count >= 2 && count <= 17) {
        unsigned selected = 0;
        int solo = !strcmp(words[0], "solo"), toggle = !strcmp(words[0], "t"), on = !strcmp(words[0], "on");
        for (i = 1; i < count; ++i) {
            if (!integer(words[i], 1, 16, &channel)) {
                tell(actor, "Channel must be a number from 1 to 16.");
                return;
            }
            selected |= 1u << (channel - 1);
        }
        for (i = 0; i < 16; ++i) {
            if (selected & (1u << i)) {
                if (solo || on || (toggle && (routes[i].muted || routes[i].output == JB_OFF))) turn_on(i);
                else routes[i].muted = 1;
                routes[i].automatic = 0;
            } else if (solo) { routes[i].muted = 1; routes[i].automatic = 0; }
        }
    } else if (count >= 2) {
        if (!integer(words[1], 1, 16, &channel)) {
            tell(actor, "Channel must be a number from 1 to 16.");
            return;
        }
        --channel;
        if (!strcmp(words[0], "mute") && count == 2)
            routes[channel].muted = 1;
        else if (!strcmp(words[0], "unmute") && count == 2)
            turn_on(channel);
        else if (!strcmp(words[0], "out") && count == 3) {
            if (!strcmp(words[2], "off"))
                routes[channel].output = JB_OFF;
            else if (!strcmp(words[2], "synth") || !strcmp(words[2], "drums"))
                resume_output[channel] = routes[channel].output = !strcmp(words[2], "synth") ? JB_SYNTH : JB_DRUMS;
            else {
                tell(actor, "Output must be synth, drums or off.");
                return;
            }
        } else if (!strcmp(words[0], "transpose") && count == 3) {
            if (!integer(words[2], -36, 36, &value)) {
                tell(actor, "Transpose must be a number from -36 to 36.");
                return;
            }
            routes[channel].transpose = value;
        } else {
            tell(actor, "Use mute N, unmute N, solo N [N ...], all, out N VOICE, or transpose N SEMITONES.");
            return;
        }
    } else {
        tell(actor, "Use mute N, unmute N, solo N [N ...], all, out N VOICE, or transpose N SEMITONES.");
        return;
    }
    keypad();
    tell(actor, "Channel settings apply now. Keypad octave shift=%d.", play.keypad);
}

static SvResult SV_CALL command(SvOwner owner, uint64_t actor, const char *arguments)
{
    char copy[256], *words[20], *context = NULL, *word;
    int count = 0;
    SvPlayerState player = { sizeof player };
    (void)owner;
    if (!actor) {
        tell(0, "The jukebox runs from the chat of an administrator.");
        return SV_OK;
    }
    if (game->player(self, actor, &player) != SV_OK || !player.is_admin) {
        tell(actor, "Administrator only.");
        return SV_OK;
    }
    if (strlen(arguments) >= sizeof copy) {
        tell(actor, "Jukebox command is too long.");
        return SV_OK;
    }
    strcpy(copy, arguments);
    word = strtok_s(copy, " \t", &context);
    while (word && count < 20) {
        words[count++] = word;
        word = strtok_s(NULL, " \t", &context);
    }
    if (word || !count) {
        tell(actor, "Use list, play NAME, demo, stop, status, channels, scale or chord N.");
        tell(actor, "Channels: mute, unmute, solo, all, out and transpose.");
    } else if (!strcmp(words[0], "list") && count == 1) {
        char names[JB_LIST_MAX][JB_NAME_MAX + 1], error[160];
        size_t files, i;
        int more;
        if (!jb_list_songs(names, &files, &more, error, sizeof error))
            tell(actor, "%s", error);
        else {
            tell(actor, "Songs in user/jukebox: %zu%s. Built-in song: /jukebox demo.", files, more ? "+" : "");
            for (i = 0; i < files; ++i)
                tell(actor, "%s.mid", names[i]);
        }
    } else if (!strcmp(words[0], "play")) {
        if (count == 2) load_song(actor, words[1]);
        else tell(actor, "Song name needs 1 to 48 letters, digits, underscores or hyphens.");
    } else if (!strcmp(words[0], "demo") && count == 1) {
        demo(actor);
    } else if (!strcmp(words[0], "save") && count == 1) {
        JbMix mix;
        char error[160];
        mix_state(&mix);
        if (jb_save_mix(song_name, &mix, error, sizeof error)) tell(actor, "Saved preset for %s.", song_name);
        else tell(actor, "%s", error);
    } else if (!strcmp(words[0], "stop") && count == 1) {
        play.mode = IDLE;
        tell(actor, "Stopped. Existing sounds play to their end.");
    } else if (!strcmp(words[0], "status") && count == 1) {
        tempo_position(&music);
        tell(actor, "Jukebox %s song=%s at=%llu mode=%s keypad_octaves=%d limit=%d everyone=%d.",
            play.mode == IDLE ? "stopped" : play.mode == SCALE ? "scale" : play.mode == CHORD ? "chord" : "playing",
            *song_name ? song_name : "none", (unsigned long long)play.elapsed, mode_name(), play.keypad, max_per_tick, everyone);
        tell(actor, "Counts: played=%llu dropped_notes=%llu dropped_calls=%llu moved=%llu failed=%llu.",
            (unsigned long long)play.played, (unsigned long long)play.dropped_notes,
            (unsigned long long)play.dropped_calls, (unsigned long long)play.moved, (unsigned long long)play.failed);
        tell(actor, "skipped_mono=%llu. Limit drops=%llu notes, %llu calls.",
            (unsigned long long)play.skipped_mono, (unsigned long long)play.dropped_notes,
            (unsigned long long)play.dropped_calls);
        tell(actor, "Tempo: %s BPM=%u speed=%d%% key=%d.", bpm ? "constant" : "file",
            bpm ? (unsigned)bpm : (unsigned)((60000000ull * speed) / ((uint64_t)file_tempo(&music) * 100)),
            speed, song_key);
        tell(actor, "Master=%d%%.", master);
        if (bank == JB_FIXED) tell(actor, "Fixed voice: %s.", sounds.sounds[fixed_sound].name);
    } else if ((!strcmp(words[0], "bpm") || !strcmp(words[0], "speed") || !strcmp(words[0], "key")) && count == 2) {
        int value;
        if (!strcmp(words[0], "bpm")) {
            if (!strcmp(words[1], "file")) { bpm = 0; speed = 100; music.remainder = 0; }
            else if (!integer(words[1], 20, 300, &value)) /* CONTROL_BOUND: bpm */
                tell(actor, "BPM must be 20 to 300 or file.");
            else { bpm = value; speed = 100; music.remainder = 0; }
        } else if (!strcmp(words[0], "speed")) {
            if (!integer(words[1], 25, 400, &value)) /* CONTROL_BOUND: speed */
                tell(actor, "Speed must be 25 to 400 percent.");
            else { speed = value; bpm = 0; music.remainder = 0; }
        } else {
            if (!integer(words[1], -48, 48, &value)) /* CONTROL_BOUND: key */
                tell(actor, "Key must be -48 to 48 semitones.");
            else { song_key = value; keypad(); }
        }
    } else if (!strcmp(words[0], "vol") && count == 3) {
        int ch, value;
        if (!integer(words[1], 1, 16, &ch)) tell(actor, "Channel must be a number from 1 to 16.");
        else if (!integer(words[2], 0, 200, &value)) /* CONTROL_BOUND: volume */
            tell(actor, "Volume must be 0 to 200 percent.");
        else { routes[ch - 1].volume = value; tell(actor, "Channel %d volume=%d%%.", ch, value); }
    } else if (!strcmp(words[0], "master") && count == 2) {
        int value;
        if (!integer(words[1], 0, 200, &value)) /* CONTROL_BOUND: master */
            tell(actor, "Master must be 0 to 200 percent.");
        else { master = value; tell(actor, "Master=%d%%.", value); }
    } else if (!strcmp(words[0], "sounds") && count == 1) {
        int i;
        for (i = 0; i < 47; ++i)
            tell(actor, "%d %s %s root=%d tail=%d gain=%.2f", i, sounds.sounds[i].name,
                sounds.sounds[i].kind, sounds.sounds[i].root, sounds.sounds[i].tail, sounds.sounds[i].gain);
    } else if (!strcmp(words[0], "mode")) {
        int next_bank, next_sound = fixed_sound;
        char text[64];
        if (count == 1) {
            tell(actor, "Modes: voices, phone, keypad, fixed SOUND. Active: %s %s.", mode_name(),
                bank == JB_FIXED ? sounds.sounds[fixed_sound].name : "");
        } else {
            snprintf(text, sizeof text, "%s%s%s", words[1], count == 3 ? " " : "", count == 3 ? words[2] : "");
            if (count > 3 || !mode_value(text, &next_bank, &next_sound))
                tell(actor, "Invalid mode. Use voices, phone, keypad or fixed SOUND.");
            else { bank = next_bank; fixed_sound = next_sound; keypad(); tell(actor, "Mode: %s.", mode_name()); }
        }
    } else if (!strcmp(words[0], "mono") && count == 3) {
        int ch, value;
        if (!integer(words[1], 1, 16, &ch)) tell(actor, "Channel must be a number from 1 to 16.");
        else if (!integer(words[2], 0, 1, &value)) tell(actor, "Mono must be 0 or 1.");
        else { routes[ch - 1].mono = value; tell(actor, "Channel %d mono=%d.", ch, value); }
    } else if (!strcmp(words[0], "voice") && count >= 3 && count <= 4) {
        int ch, id, root = 0;
        if (!integer(words[1], 1, 16, &ch))
            tell(actor, "Channel must be a number from 1 to 16.");
        else if (!strcmp(words[2], "default") && count == 3) {
            routes[ch - 1].voice = -1;
            keypad();
        } else if (!jb_sound_index(&sounds, words[2], &id))
            tell(actor, "Sound needs an ID from 0 to 46 or a name from /jb sounds.");
        else if (count == 4 && !integer(words[3], 0, 127, &root))
            tell(actor, "Root must be a note number from 0 to 127.");
        else {
            routes[ch - 1].voice = id;
            routes[ch - 1].root = count == 4 ? root : sounds.sounds[id].root;
            keypad();
            tell(actor, "Channel %d voice=%s root=%d.", ch, sounds.sounds[id].name, routes[ch - 1].root);
        }
    } else if (!strcmp(words[0], "try") && count >= 2 && count <= 3) {
        int id, root = 0;
        JbInstruments voices;
        JbSounds next_sounds;
        if (!jb_sound_index(&sounds, words[1], &id))
            tell(actor, "Sound needs an ID from 0 to 46 or a name from /jb sounds.");
        else if (count == 3 && !integer(words[2], 0, 127, &root))
            tell(actor, "Root must be a note number from 0 to 127.");
        else if (prepare(actor, &voices, &next_sounds)) {
            instruments = voices;
            sounds = next_sounds;
            start(TRY, actor);
            play.try_sound = id;
            play.try_root = count == 3 ? root : sounds.sounds[id].root;
            tell(actor, "Trying %s: 12 notes, one every 15 ticks.", sounds.sounds[id].name);
        }
    } else if (!strcmp(words[0], "channels") && count == 1) {
        channels(actor);
    } else if ((!strcmp(words[0], "scale") && count == 1) || (!strcmp(words[0], "chord") && count == 2)) {
        JbInstruments voices;
        JbSounds next_sounds;
        int notes = 0;
        if (!strcmp(words[0], "chord") && !integer(words[1], 1, 32, &notes)) {
            tell(actor, "Chord size must be a number from 1 to 32.");
        } else if (prepare(actor, &voices, &next_sounds)) {
            instruments = voices;
            sounds = next_sounds;
            start(notes ? CHORD : SCALE, actor);
            play.chord = (unsigned)notes;
            tell(actor, notes ? "Chord queued. The sound-call limit still applies." : "Scale queued: ring, ten digits, nearest, then drums.");
        }
    } else if (!strcmp(words[0], "mute") || !strcmp(words[0], "unmute") || !strcmp(words[0], "solo") ||
        !strcmp(words[0], "all") || !strcmp(words[0], "t") || !strcmp(words[0], "on") ||
        !strcmp(words[0], "off") || !strcmp(words[0], "out") || !strcmp(words[0], "transpose")) {
        channel_command(actor, count, words);
    } else {
        tell(actor, "Unknown jukebox command or arguments. Use /jukebox for help.");
    }
    return SV_OK;
}

static SvResult SV_CALL event(SvOwner owner, const SvEvent *value)
{
    (void)owner;
    if (!value || value->size < sizeof *value)
        return SV_INVALID;
    if (value->kind == SV_EVENT_TICK) {
        ++server_ticks;
        if (play.mode != IDLE) tick(value->tick);
    }
    else if (value->kind == SV_EVENT_CONFIG)
        read_settings();
    else if (value->kind == SV_EVENT_STOP)
        play.mode = IDLE;
    return SV_OK;
}

static void SV_CALL disable(SvOwner owner)
{
    (void)owner;
    play.mode = IDLE;
    jb_song_free(&song);
    song_name[0] = 0;
}

static SvResult SV_CALL enable(const SvHost *h, SvOwner owner)
{
    char error[160];
    SvResult result;
    if (!h || h->abi != SV_PLUGIN_ABI || h->size < sizeof *h || !h->get_api)
        return SV_INCOMPATIBLE;
    host = h;
    self = owner;
    result = h->get_api(owner, SV_API_GAME_READ, SV_GAME_READ_VERSION, (const void **)&game);
    if (result == SV_OK)
        result = h->get_api(owner, SV_API_GAME_WRITE, SV_GAME_WRITE_VERSION, (const void **)&write_api);
    if (result == SV_OK)
        result = h->get_api(owner, SV_API_CONFIG, SV_CONFIG_VERSION, (const void **)&config);
    if (result != SV_OK)
        return result;
    if (!game || game->size < sizeof *game || game->version < SV_GAME_READ_VERSION ||
        !write_api || write_api->size < offsetof(SvGameWrite, sound) + sizeof write_api->sound ||
        write_api->version < SV_GAME_WRITE_VERSION || !config || config->size < sizeof *config || config->version < SV_CONFIG_VERSION)
        return SV_INCOMPATIBLE;
    memset(&play, 0, sizeof play);
    memset(busy_until, 0, sizeof busy_until);
    server_ticks = 0;
    if (!jb_load_instruments(&instruments, error, sizeof error) || !jb_load_sounds(&sounds, error, sizeof error)) {
        tell(0, "%s", error);
        return SV_IO;
    }
    reset_routes();
    if (!read_settings())
        return SV_INVALID;
    result = h->subscribe(owner, SV_EVENT_TICK | SV_EVENT_CONFIG | SV_EVENT_STOP, event);
    if (result == SV_OK)
        result = h->command(owner, "jukebox", SV_COMMAND_ADMIN, command);
    if (result == SV_OK)
        result = h->command(owner, "jb", SV_COMMAND_ADMIN, command);
    return result;
}

SV_EXPORT SvResult SV_CALL sv_plugin_entry(uint32_t abi, SvPlugin *plugin)
{
    if (abi != SV_PLUGIN_ABI || !plugin || plugin->size < sizeof *plugin)
        return SV_INCOMPATIBLE;
    memset(plugin, 0, sizeof *plugin);
    plugin->size = sizeof *plugin;
    plugin->abi = SV_PLUGIN_ABI;
    strcpy(plugin->id, "sv.jukebox");
    strcpy(plugin->version, "0.2.0");
    plugin->flags = SV_PLUGIN_LIVE;
    plugin->enable = enable;
    plugin->disable = disable;
    return SV_OK;
}
