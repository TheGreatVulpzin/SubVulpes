/* Capture the actual public sound method calls from the built DLL. */
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include "sv_game_read.h"
#include "sv_game_write.h"
#include "sv_config.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CHECK(x) do { if (!(x)) { fprintf(stderr, "CHECK line %d: %s\n", __LINE__, #x); exit(1); } } while (0)
static SvCommandFn command_fn;
static SvEventFn event_fn;
static char bank[64] = "nearest", logs[65536];
static int32_t everyone, maximum = 8, trace = 1;
static unsigned tick_number, player_count = 1;
static int gone;
static struct { unsigned tick, sound; float pitch, volume; SvVector position; } calls[4096];
static size_t call_count;

static SvResult SV_CALL log_line(SvOwner owner, const char *text)
{
    size_t used = strlen(logs), length = strlen(text);
    CHECK(owner == 1 && used + length + 2 < sizeof logs);
    memcpy(logs + used, text, length);
    logs[used + length] = '\n';
    logs[used + length + 1] = 0;
    printf("LOG %s\n", text);
    return SV_OK;
}

static SvResult SV_CALL player(SvOwner owner, SvEntity entity, SvPlayerState *out)
{
    CHECK(owner == 1 && out->size == sizeof *out);
    if (!entity || entity > player_count)
        return SV_NOT_FOUND;
    memset(out, 0, sizeof *out);
    out->size = sizeof *out;
    out->entity = entity;
    out->human = (gone && entity == 1) || entity == 4 ? 0 : 1000 + entity;
    out->is_admin = entity == 1;
    out->is_bot = entity == 3;
    return SV_OK;
}

static SvResult SV_CALL human(SvOwner owner, SvEntity entity, SvHumanState *out)
{
    CHECK(owner == 1 && out->size == sizeof *out);
    if (entity < 1001 || entity > 1000 + player_count)
        return SV_NOT_FOUND;
    memset(out, 0, sizeof *out);
    out->size = sizeof *out;
    out->health = 100;
    out->position.x = (float)(entity - 1000) * 10;
    out->position.y = 5;
    out->position.z = 7;
    return SV_OK;
}

static SvResult SV_CALL entities(SvOwner owner, uint32_t kind, uint32_t cursor, SvEntity *page,
    uint32_t capacity, uint32_t *count, uint32_t *next)
{
    uint32_t i = cursor;
    CHECK(owner == 1 && kind == SV_ENTITY_PLAYER && cursor < 256 && capacity >= 1 && capacity <= 256);
    *count = 0;
    while (i < player_count && *count < capacity)
        page[(*count)++] = ++i;
    *next = i >= player_count ? 256 : i;
    return SV_OK;
}

static SvResult SV_CALL sound(SvOwner owner, uint32_t index, SvVector position, float volume, float pitch)
{
    CHECK(owner == 1 && call_count < sizeof calls / sizeof calls[0]);
    CHECK(index < 47 && isfinite(pitch) && pitch >= 0.125f && pitch <= 2 && volume > 0 && volume <= 1);
    calls[call_count].tick = tick_number;
    calls[call_count].sound = index;
    calls[call_count].pitch = pitch;
    calls[call_count].volume = volume;
    calls[call_count].position = position;
    ++call_count;
    printf("CALL tick=%u sound=%u x=%.1f y=%.1f z=%.1f volume=%.6f pitch=%.6f\n",
        tick_number, index, position.x, position.y, position.z, volume, pitch);
    return SV_OK;
}

static SvResult SV_CALL message(SvOwner owner, uint32_t kind, SvEntity target, const char *text)
{
    CHECK(owner == 1 && kind == SV_MESSAGE_SERVER && target >= 1 && target <= player_count);
    CHECK(strlen(text) >= 1 && strlen(text) <= 63);
    return SV_OK;
}

static SvResult SV_CALL get_int(SvOwner owner, const char *key, int32_t *out)
{
    CHECK(owner == 1);
    if (!strcmp(key, "everyone")) *out = everyone;
    else if (!strcmp(key, "max_per_tick")) *out = maximum;
    else if (!strcmp(key, "trace")) *out = trace;
    else return SV_NOT_FOUND;
    return SV_OK;
}

static SvResult SV_CALL get_text(SvOwner owner, const char *key, char *out, uint32_t capacity)
{
    CHECK(owner == 1 && capacity >= 64);
    if (strcmp(key, "bank")) return SV_NOT_FOUND;
    strcpy(out, bank);
    return SV_OK;
}

static SvGameRead read_api;
static SvGameWrite write_api;
static SvConfig config_api;
static SvResult SV_CALL get_api(SvOwner owner, uint32_t capability, uint32_t version, const void **api)
{
    CHECK(owner == 1 && version == 1);
    if (capability == SV_API_GAME_READ) *api = &read_api;
    else if (capability == SV_API_GAME_WRITE) *api = &write_api;
    else if (capability == SV_API_CONFIG) *api = &config_api;
    else { CHECK(0); return SV_NOT_FOUND; }
    return SV_OK;
}

static SvResult SV_CALL register_command(SvOwner owner, const char *name, uint32_t permission, SvCommandFn callback)
{
    CHECK(owner == 1 && !strcmp(name, "jukebox") && permission == SV_COMMAND_ADMIN);
    command_fn = callback;
    return SV_OK;
}

static SvResult SV_CALL subscribe(SvOwner owner, uint64_t mask, SvEventFn callback)
{
    CHECK(owner == 1 && mask == (SV_EVENT_TICK | SV_EVENT_STOP | SV_EVENT_CONFIG));
    event_fn = callback;
    return SV_OK;
}

static void send(const char *text)
{
    CHECK(command_fn && command_fn(1, 1, text) == SV_OK);
}

static void ticks(unsigned count)
{
    unsigned i;
    for (i = 0; i < count; ++i) {
        SvEvent value = { sizeof value, SV_EVENT_TICK, ++tick_number };
        CHECK(event_fn(1, &value) == SV_OK);
    }
}

static void settings(void)
{
    SvEvent value = { sizeof value, SV_EVENT_CONFIG, tick_number };
    CHECK(event_fn(1, &value) == SV_OK);
}

static void test_demo(void)
{
    const unsigned expected_ticks[] = {1, 1, 16, 31, 31, 46};
    const unsigned expected_sounds[] = {29, 21, 29, 29, 21, 29};
    const double expected_pitches[] = {0.375359491, 0.25, 0.472923323, 0.562403782, 2.0, 0.750718983};
    size_t i;
    send("demo");
    ticks(46);
    CHECK(call_count == 6);
    for (i = 0; i < 6; ++i) {
        CHECK(calls[i].tick == expected_ticks[i] && calls[i].sound == expected_sounds[i]);
        CHECK(fabs(calls[i].pitch - expected_pitches[i]) < 0.000002);
        CHECK(calls[i].position.x == 10 && calls[i].position.y == 5 && calls[i].position.z == 7);
    }
}

static void test_channels(void)
{
    maximum = 32;
    settings();
    send("play fifteen");
    send("channels");
    CHECK(strstr(logs, "Channel 1: notes=3 program=1 Acoustic Grand Piano range=60..64 out=synth"));
    CHECK(strstr(logs, "Channel 10: notes=3 program=10 Glockenspiel range=69..73 out=drums"));
    CHECK(strstr(logs, "Channel 15: notes=3 program=15 Tubular Bells range=74..78 out=synth"));
    send("solo 1 3 5");
    ticks(1);
    CHECK(call_count == 3);
    send("mute 3");
    send("transpose 1 12");
    send("solo 0"); send("solo 17"); send("solo text");
    send("mute 0"); send("unmute 17"); send("out text off");
    send("transpose 1 37"); send("out 2 unknown");
    CHECK(strstr(logs, "Channel must be a number from 1 to 16."));
    CHECK(strstr(logs, "Transpose must be a number from -36 to 36."));
    CHECK(strstr(logs, "Output must be synth, drums or off."));
    ticks(30);
    CHECK(call_count == 5 && calls[3].tick == 31 && calls[4].tick == 31);
    CHECK(fabs(calls[3].pitch - 0.842653124) < 0.000002);
    send("out 5 off");
    ticks(30);
    CHECK(call_count == 6 && calls[5].tick == 61);
    send("all");
    send("play fifteen");
    ticks(1);
    CHECK(call_count == 21);
    send("channels");
    CHECK(strstr(logs, "transpose=12."));
    send("play other");
    send("channels");
    CHECK(strstr(logs, "Channel 1: notes=1 program=1 Acoustic Grand Piano range=60..60 out=synth transpose=0."));
}

static void test_budget(void)
{
    maximum = 3;
    settings();
    send("play burst");
    ticks(1);
    CHECK(call_count == 3);
    CHECK(fabs(calls[0].volume - 19.0 / 127) < 0.000001);
    CHECK(fabs(calls[1].volume - 20.0 / 127) < 0.000001);
    CHECK(fabs(calls[2].volume - 21.0 / 127) < 0.000001);
    send("status");
    CHECK(strstr(logs, "dropped_notes=9 dropped_calls=9"));
    everyone = 1;
    player_count = 4;
    settings();
    send("play burst");
    ticks(1);
    CHECK(call_count == 6);
    CHECK(fabs(calls[3].volume - 20.0 / 127) < 0.000001);
    CHECK(fabs(calls[4].volume - 21.0 / 127) < 0.000001 && fabs(calls[5].volume - 21.0 / 127) < 0.000001);
    CHECK(calls[3].position.x == 10 && calls[4].position.x == 10 && calls[5].position.x == 20);
    send("status");
    CHECK(strstr(logs, "dropped_notes=10 dropped_calls=21"));
}

static void test_keypad(void)
{
    strcpy(bank, "keypad");
    settings();
    send("play keypad");
    ticks(46);
    CHECK(call_count == 4 && calls[0].sound == 29 && calls[1].sound == 29 && calls[2].sound == 32 && calls[3].sound == 28);
    CHECK(calls[0].pitch == 1 && calls[1].pitch == 1 && calls[2].pitch == 1 && calls[3].pitch == 1);
    CHECK(strstr(logs, "keypad octave shift=1."));
    send("status");
    CHECK(strstr(logs, "moved=4"));
    strcpy(bank, "ring"); settings();
    send("play keypad"); ticks(1);
    CHECK(call_count == 5 && calls[4].sound == 27 && fabs(calls[4].pitch - 0.131009297) < 0.000002);
    strcpy(bank, "nearest"); settings();
    send("play extreme"); ticks(1);
    CHECK(call_count == 7 && calls[5].pitch >= 0.125 && calls[6].pitch <= 2);
    send("status");
    CHECK(strstr(logs, "moved=2"));
}

static void test_scale(void)
{
    unsigned i;
    send("scale"); ticks(3091);
    CHECK(call_count == 104);
    for (i = 0; i < 88; ++i)
        CHECK(calls[i].sound == 27 + i / 8 && calls[i].tick == i * 30 + 1);
    for (i = 88; i < 96; ++i)
        CHECK(calls[i].sound == 29);
    for (i = 96; i < 104; ++i)
        CHECK(calls[i].sound == 21);
    CHECK(fabs(calls[96].pitch - 0.25) < 0.000001 && fabs(calls[103].pitch - 0.5) < 0.000001);
    send("chord 0"); send("chord 33"); send("chord text"); ticks(1);
    CHECK(call_count == 104 && strstr(logs, "Chord size must be a number from 1 to 32."));
    send("chord 32"); ticks(1);
    CHECK(call_count == 112 && strstr(logs, "dropped_notes=24 dropped_calls=24"));
    maximum = 32; settings();
    send("chord 32"); ticks(1);
    CHECK(call_count == 144);
}

static void test_stop(void)
{
    SvEvent stopped = { sizeof stopped, SV_EVENT_STOP, 0 };
    player_count = 2;
    CHECK(command_fn(1, 2, "demo") == SV_OK);
    CHECK(command_fn(1, 0, "demo") == SV_OK);
    ticks(1);
    CHECK(call_count == 0 && strstr(logs, "Administrator only.") && strstr(logs, "chat of an administrator"));
    send("demo"); ticks(1);
    CHECK(call_count == 2);
    send("stop"); ticks(100);
    CHECK(call_count == 2);
    send("demo"); gone = 1; ticks(1);
    CHECK(call_count == 2 && strstr(logs, "Playback stopped."));
    gone = 0; send("demo");
    CHECK(event_fn(1, &stopped) == SV_OK);
    ticks(100);
    CHECK(call_count == 2);
    send("play ../secret"); send("play CON"); send("play name.mid:stream");
    CHECK(strstr(logs, "Song name needs 1 to 48"));
    send("play missing");
    CHECK(strstr(logs, "Cannot open the jukebox file."));
}

int main(int argc, char **argv)
{
    HMODULE library;
    FARPROC symbol;
    SvPluginEntry entry;
    SvPlugin plugin = { sizeof plugin };
    SvHost h = {0};
    const char *mode;
    SetErrorMode(SEM_NOGPFAULTERRORBOX | SEM_FAILCRITICALERRORS);
    CHECK(argc == 3);
    mode = argv[2];
    library = LoadLibraryA(argv[1]);
    CHECK(library);
    symbol = GetProcAddress(library, "sv_plugin_entry");
    CHECK(symbol && sizeof symbol == sizeof entry);
    memcpy(&entry, &symbol, sizeof entry);
    CHECK(entry(2, &plugin) == SV_INCOMPATIBLE);
    CHECK(entry(1, &plugin) == SV_OK && !strcmp(plugin.id, "sv.jukebox") && !strcmp(plugin.version, "0.1.0"));
    read_api.size = sizeof read_api; read_api.version = 1;
    read_api.player = player; read_api.human = human; read_api.entities = entities;
    write_api.size = sizeof write_api; write_api.version = 1;
    write_api.sound = sound; write_api.message_to = message;
    config_api.size = sizeof config_api; config_api.version = 1;
    config_api.get_int = get_int; config_api.get_text = get_text;
    h.size = sizeof h; h.abi = 1; h.log = log_line; h.command = register_command; h.subscribe = subscribe; h.get_api = get_api;
    h.size = SV_HOST_V1_SIZE;
    CHECK(plugin.enable(&h, 1) == SV_INCOMPATIBLE);
    h.size = sizeof h;
    if (!strcmp(mode, "instruments-bad")) {
        CHECK(plugin.enable(&h, 1) == SV_IO && !command_fn && !event_fn);
    } else {
        CHECK(plugin.enable(&h, 1) == SV_OK && command_fn && event_fn);
        if (!strcmp(mode, "demo")) test_demo();
        else if (!strcmp(mode, "channels")) test_channels();
        else if (!strcmp(mode, "budget")) test_budget();
        else if (!strcmp(mode, "keypad")) test_keypad();
        else if (!strcmp(mode, "scale")) test_scale();
        else if (!strcmp(mode, "stop")) test_stop();
        else if (!strcmp(mode, "preserve")) { send("demo"); ticks(1); CHECK(call_count == 2 && fabs(calls[0].pitch - 0.3737508076) < 0.000002); plugin.disable(1); CHECK(plugin.enable(&h, 1) == SV_OK); }
        else if (!strcmp(mode, "bad-midi")) { send("play bad"); CHECK(strstr(logs, "MIDI ")); ticks(1); CHECK(!call_count); }
        else { CHECK(0); }
        plugin.disable(1);
    }
    FreeLibrary(library);
    puts("JUKEBOX_HOST_PASS");
    return 0;
}
