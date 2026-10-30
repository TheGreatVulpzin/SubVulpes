/* Native SDK plugin. Playback advances only when the host sends a server tick. */
#include "sv_game_read.h"
#include "sv_game_write.h"
#include "sv_config.h"
#include "files.h"
#include "gm.h"
#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum { IDLE, MIDI, DEMO, SCALE, CHORD };
static const SvHost *host;
static const SvGameRead *game;
static const SvGameWrite *write_api;
static const SvConfig *config;
static SvOwner self;
static JbSong song;
static JbInstruments instruments;
static JbRoute routes[16];
static int resume_output[16];
static char song_name[JB_NAME_MAX + 1];
static int bank = JB_BANK_NEAREST, everyone, max_per_tick = 8, trace;
static struct {
    int mode, keypad;
    SvEntity starter;
    size_t next;
    unsigned step, chord;
    uint64_t elapsed, played, dropped_notes, dropped_calls, moved, failed;
} play;

typedef struct {
    JbNote note;
    uint32_t order;
    int output, bank, transpose;
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
    }
}

static void keypad(void)
{
    play.keypad = bank == JB_BANK_KEYPAD ? jb_keypad_octaves(&instruments, &song, routes) : 0;
}

static int read_settings(void)
{
    char value[64];
    int next_bank;
    int32_t next_everyone, next_max, next_trace;
    if (config->get_text(self, "bank", value, sizeof value) != SV_OK || !jb_bank_index(value, &next_bank)) {
        tell(0, "Invalid bank. Use nearest, keypad, ring, or digit0 to digit9.");
        return 0;
    }
    if (config->get_int(self, "everyone", &next_everyone) != SV_OK || next_everyone < 0 || next_everyone > 1 ||
        config->get_int(self, "max_per_tick", &next_max) != SV_OK || next_max < 1 || next_max > 32 ||
        config->get_int(self, "trace", &next_trace) != SV_OK || next_trace < 0 || next_trace > 1) {
        tell(0, "Invalid jukebox settings. Check everyone, max_per_tick and trace.");
        return 0;
    }
    bank = next_bank;
    everyone = next_everyone;
    max_per_tick = next_max;
    trace = next_trace;
    keypad();
    return 1;
}

static void start(int mode, SvEntity actor)
{
    memset(&play, 0, sizeof play);
    play.mode = mode;
    play.starter = actor;
    keypad();
}

static int prepare(SvEntity actor, JbInstruments *next)
{
    char error[160];
    SvVector point;
    if (!position(actor, &point)) {
        tell(actor, "You need a live body to start the jukebox.");
        return 0;
    }
    if (!jb_load_instruments(next, error, sizeof error)) {
        tell(actor, "%s", error);
        return 0;
    }
    return 1;
}

static void load_song(SvEntity actor, const char *input)
{
    JbSong next = {0};
    JbInstruments voices;
    char name[JB_NAME_MAX + 1], error[160];
    if (!jb_song_name(input, name)) {
        tell(actor, "Song name needs 1 to 48 letters, digits, underscores or hyphens.");
        return;
    }
    if (!prepare(actor, &voices))
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
    if (strcmp(name, song_name))
        reset_routes();
    jb_song_free(&song);
    song = next;
    instruments = voices;
    strcpy(song_name, name);
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
    JbSong next = {0};
    size_t i;
    if (!prepare(actor, &voices))
        return;
    next.notes = malloc(sizeof notes);
    if (!next.notes) {
        tell(actor, "Not enough memory for the demo.");
        return;
    }
    memcpy(next.notes, notes, sizeof notes);
    next.count = sizeof notes / sizeof notes[0];
    next.end_tick = 45;
    for (i = 0; i < next.count; ++i) {
        const JbNote *note = &next.notes[i];
        JbChannel *c = &next.channels[note->channel];
        if (!c->notes)
            c->low = c->high = note->key;
        if (note->key < c->low)
            c->low = note->key;
        if (note->key > c->high)
            c->high = note->key;
        ++c->notes;
    }
    if (strcmp(song_name, "#demo"))
        reset_routes();
    jb_song_free(&song);
    song = next;
    instruments = voices;
    strcpy(song_name, "#demo");
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
        JbTone tone = c->output == JB_DRUMS ? jb_drum(&instruments, c->note.key, c->transpose) :
            jb_synth(&instruments, c->note.key + c->transpose, c->bank, play.keypad);
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
        float volume = note->velocity / 127.0f;
        SvResult result = write_api->sound(self, call->tone.sound, call->position, volume, call->tone.pitch);
        if (result == SV_OK) {
            ++play.played;
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
    if (play.mode == MIDI || play.mode == DEMO) {
        while (play.next < song.count && song.notes[play.next].tick <= play.elapsed) {
            Candidate c = {0};
            const JbNote *note = &song.notes[play.next];
            const JbRoute *route = &routes[note->channel];
            c.note = *note;
            c.order = (uint32_t)play.next++;
            if (route->output == JB_OFF || route->muted)
                continue;
            c.output = route->output;
            c.transpose = route->transpose;
            c.bank = bank;
            offer(top, &count, &c);
            ++wanted;
        }
        done = play.next == song.count && play.elapsed >= song.end_tick;
    } else if (play.mode == SCALE) {
        if (play.elapsed % 30u == 0 && play.step < (JB_MEMBERS + 2u) * 8u) {
            Candidate c = {0};
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
    } else if (play.mode == CHORD) {
        unsigned i;
        for (i = 0; i < play.chord; ++i) {
            Candidate c = {0};
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
        int output = routes[i].muted ? JB_OFF : routes[i].output;
        if (c->notes)
            tell(actor, "Channel %d: notes=%u program=%u %s range=%u..%u out=%s transpose=%d.", i + 1, c->notes,
                c->first_program + 1u, jb_gm_names[c->first_program], c->low, c->high,
                output == JB_OFF ? "off" : output == JB_DRUMS ? "drums" : "synth", routes[i].transpose);
    }
}

static int turn_on(int channel)
{
    routes[channel].muted = 0;
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
    } else if (!strcmp(words[0], "solo") && count >= 2 && count <= 17) {
        unsigned selected = 0;
        for (i = 1; i < count; ++i) {
            if (!integer(words[i], 1, 16, &channel)) {
                tell(actor, "Channel must be a number from 1 to 16.");
                return;
            }
            selected |= 1u << (channel - 1);
        }
        for (i = 0; i < 16; ++i) {
            if (selected & (1u << i))
                turn_on(i);
            else
                routes[i].muted = 1;
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
    } else if (!strcmp(words[0], "play") && count == 2) {
        load_song(actor, words[1]);
    } else if (!strcmp(words[0], "demo") && count == 1) {
        demo(actor);
    } else if (!strcmp(words[0], "stop") && count == 1) {
        play.mode = IDLE;
        tell(actor, "Stopped. Existing sounds play to their end.");
    } else if (!strcmp(words[0], "status") && count == 1) {
        tell(actor, "Jukebox %s song=%s at=%llu bank=%s keypad_octaves=%d limit=%d everyone=%d.",
            play.mode == IDLE ? "stopped" : play.mode == SCALE ? "scale" : play.mode == CHORD ? "chord" : "playing",
            *song_name ? song_name : "none", (unsigned long long)play.elapsed, jb_bank_name(bank), play.keypad, max_per_tick, everyone);
        tell(actor, "Counts: played=%llu dropped_notes=%llu dropped_calls=%llu moved=%llu failed=%llu.",
            (unsigned long long)play.played, (unsigned long long)play.dropped_notes,
            (unsigned long long)play.dropped_calls, (unsigned long long)play.moved, (unsigned long long)play.failed);
    } else if (!strcmp(words[0], "channels") && count == 1) {
        channels(actor);
    } else if ((!strcmp(words[0], "scale") && count == 1) || (!strcmp(words[0], "chord") && count == 2)) {
        JbInstruments voices;
        int notes = 0;
        if (!strcmp(words[0], "chord") && !integer(words[1], 1, 32, &notes)) {
            tell(actor, "Chord size must be a number from 1 to 32.");
        } else if (prepare(actor, &voices)) {
            instruments = voices;
            start(notes ? CHORD : SCALE, actor);
            play.chord = (unsigned)notes;
            tell(actor, notes ? "Chord queued. The sound-call limit still applies." : "Scale queued: ring, ten digits, nearest, then drums.");
        }
    } else if (!strcmp(words[0], "mute") || !strcmp(words[0], "unmute") || !strcmp(words[0], "solo") ||
        !strcmp(words[0], "all") || !strcmp(words[0], "out") || !strcmp(words[0], "transpose")) {
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
    if (value->kind == SV_EVENT_TICK && play.mode != IDLE)
        tick(value->tick);
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
    reset_routes();
    if (!jb_load_instruments(&instruments, error, sizeof error)) {
        tell(0, "%s", error);
        return SV_IO;
    }
    if (!read_settings())
        return SV_INVALID;
    result = h->subscribe(owner, SV_EVENT_TICK | SV_EVENT_CONFIG | SV_EVENT_STOP, event);
    if (result == SV_OK)
        result = h->command(owner, "jukebox", SV_COMMAND_ADMIN, command);
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
    strcpy(plugin->version, "0.1.0");
    plugin->flags = SV_PLUGIN_LIVE;
    plugin->enable = enable;
    plugin->disable = disable;
    return SV_OK;
}
