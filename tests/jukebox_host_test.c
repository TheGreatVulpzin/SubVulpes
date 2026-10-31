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
static char bank[64] = "nearest", logs[1048576];
static int32_t everyone, maximum = 8, trace = 1;
static unsigned tick_number, player_count = 1;
static int gone, modern, automatic;
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
    else if (!strcmp(key, "auto_channels")) *out = automatic;
    else if (!strcmp(key, "default_bpm") || !strcmp(key, "default_key")) *out = 0;
    else if ((!strcmp(key, "default_speed") || !strcmp(key, "default_master"))) *out = 100;
    else return SV_NOT_FOUND;
    return SV_OK;
}

static SvResult SV_CALL get_text(SvOwner owner, const char *key, char *out, uint32_t capacity)
{
    CHECK(owner == 1 && capacity >= 64);
    if (!strcmp(key, "mode")) strcpy(out, "voices");
    else if (!strcmp(key, "bank")) strcpy(out, bank);
    else return SV_NOT_FOUND;
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
    CHECK(owner == 1 && (!strcmp(name, "jukebox") || !strcmp(name, "jb")) && permission == SV_COMMAND_ADMIN);
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
    if (!modern && (!strcmp(text, "demo") || !strncmp(text, "play ", 5))) {
        int ch;
        for (ch = 1; ch <= 16; ++ch) {
            char change[24];
            snprintf(change, sizeof change, "mono %d 0", ch);
            CHECK(command_fn(1, 1, change) == SV_OK);
        }
    }
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
    const unsigned expected_sounds[] = {39, 21, 39, 39, 21, 39};
    const double expected_pitches[] = {1, 0.25, 1.25992105, 1.49830708, 2.0, 2};
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
    CHECK(strstr(logs, "[x] 1 Acoustic Grand 3 notes -> phone"));
    CHECK(strstr(logs, "[x] 10 Glockenspiel 3 notes -> drums"));
    CHECK(strstr(logs, "[x] 15 Tubular Bells 3 notes -> phone"));
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
    CHECK(strstr(logs, "Channel settings apply now."));
    send("play other");
    send("channels");
    CHECK(strstr(logs, "[x] 1 Acoustic Grand 1 notes -> phone"));
}

static void test_budget(void)
{
    maximum = 3;
    settings();
    send("play burst");
    send("mono 1 0");
    ticks(1);
    CHECK(call_count == 3);
    CHECK(fabs(calls[0].volume - 19.0 / 127 * 0.4) < 0.000001);
    CHECK(fabs(calls[1].volume - 20.0 / 127 * 0.4) < 0.000001);
    CHECK(fabs(calls[2].volume - 21.0 / 127 * 0.4) < 0.000001);
    send("status");
    CHECK(strstr(logs, "dropped_notes=9 dropped_calls=9"));
    everyone = 1;
    player_count = 4;
    settings();
    send("play burst");
    send("mono 1 0");
    ticks(1);
    CHECK(call_count == 6);
    CHECK(fabs(calls[3].volume - 20.0 / 127 * 0.4) < 0.000001);
    CHECK(fabs(calls[4].volume - 21.0 / 127 * 0.4) < 0.000001 && fabs(calls[5].volume - 21.0 / 127 * 0.4) < 0.000001);
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
    send("play extreme"); send("mono 1 0"); ticks(1);
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
    CHECK(strstr(logs, "No song matches. Use /jb list."));
}

/* Exercise the v2 controls through the public DLL entry point and sound API. */
static void near_pitch(size_t index, double expected)
{
    CHECK(index < call_count && fabs(calls[index].pitch - expected) < 0.000002);
}

static void melody(void)
{
    send("mono 1 0");
    send("voice 1 ping 60");
}

static void test_v2(const char *mode)
{
    if (!strcmp(mode, "v2-voices")) {
        unsigned id;
        send("play timeline"); ticks(1);
        CHECK(call_count == 2 && calls[0].sound == 39 && calls[1].sound == 21);
        near_pitch(0, pow(2.0, -25.0 / 12));
        send("voice 0 thud"); send("voice 17 thud"); send("voice text thud");
        send("voice 1 47"); send("voice 1 -1"); send("voice 1 no-sound");
        send("voice 1 ping 128"); send("voice 1 ping -1");
        CHECK(strstr(logs, "Channel must be") && strstr(logs, "Sound needs") && strstr(logs, "Root must be"));
        for (id = 0; id <= 46; ++id) {
            char text[40];
            size_t first = call_count;
            snprintf(text, sizeof text, "try %u 60", id);
            send(text); ticks(166);
            CHECK(call_count == first + 12 && calls[first].sound == id);
            near_pitch(first, 1);
            near_pitch(first + 11, pow(2.0, 11.0 / 12));
            CHECK(calls[first + 11].tick - calls[first].tick == 165);
        }
        send("play timeline"); melody(); send("voice 1 thud 60"); send("mode keypad"); ticks(1);
        CHECK(calls[call_count - 2].sound == 21 && calls[call_count - 2].pitch == 1);
        send("voice 1 default"); send("mode fixed glass"); ticks(30);
        CHECK(calls[call_count - 2].sound == 25);
        send("mode phone"); send("mode"); ticks(30);
        CHECK(calls[call_count - 2].sound == 29 && strstr(logs, "Active: phone"));
        send("sounds"); CHECK(strstr(logs, "46 silent46 silent root=60"));
    } else if (!strcmp(mode, "v2-mono")) {
        send("play mono"); ticks(11);
        CHECK(call_count == 1);
        send("status"); CHECK(strstr(logs, "skipped_mono=1"));
        send("play mono"); send("mono 1 0"); ticks(11);
        CHECK(call_count == 3);
        send("play low"); ticks(57);
        CHECK(call_count == 5 && calls[4].tick - calls[3].tick == 56);
        ticks(56);
        send("play high"); ticks(8);
        CHECK(call_count == 7 && calls[6].tick - calls[5].tick == 7);
    } else if (!strcmp(mode, "v2-restart")) {
        send("play mono"); ticks(1); CHECK(call_count == 1);
        send("stop"); ticks(5); send("play mono"); ticks(1);
        CHECK(call_count == 1);
        ticks(10); CHECK(call_count == 2 && calls[1].tick == 17);
        send("status"); CHECK(strstr(logs, "skipped_mono=1"));
    } else if (!strcmp(mode, "v2-transaction")) {
        char text[4096], *row;
        const char *old = "39=ping,tonal,14,109,0.79";
        FILE *file;
        size_t size;
        send("play timeline"); send("mono 1 0"); ticks(1);
        file = fopen("user/jukebox/sounds.ini", "rb"); CHECK(file);
        size = fread(text, 1, sizeof text - 1, file); fclose(file); text[size] = 0;
        row = strstr(text, old); CHECK(row);
        file = fopen("user/jukebox/sounds.ini", "wb"); CHECK(file);
        CHECK(fwrite(text, 1, (size_t)(row - text), file) == (size_t)(row - text));
        fputs("39=ping,tonal,14,60,0.05", file); fputs(row + strlen(old), file); fclose(file);
        send("play broken"); ticks(30);
        CHECK(call_count == 4 && fabs(calls[2].volume - 0.79) < 0.000002);
        near_pitch(2, pow(2.0, -25.0 / 12));
        send("play timeline"); send("mono 1 0"); ticks(1);
        CHECK(call_count == 6 && fabs(calls[4].volume - 0.05) < 0.000002);
        near_pitch(4, 1);
    } else if (!strcmp(mode, "v2-channels")) {
        send("play six"); send("channels"); ticks(1);
        CHECK(call_count == 3 && strstr(logs, "[ ] 1 Acoustic Grand 1 notes -> ping auto"));
        send("all"); send("off 4 5 6"); send("t 1 2 3");
        send("on 1 2 3"); send("solo 1 2"); ticks(30);
        CHECK(call_count == 4);
        send("on 0 3"); send("off 17"); send("t text");
        CHECK(strstr(logs, "Channel must be a number from 1 to 16."));
        send("all"); ticks(14); send("play six"); ticks(1); CHECK(call_count == 10);
    } else if (!strcmp(mode, "v2-mix")) {
        send("play mixsong"); send("off 2 3"); send("voice 1 glass 60");
        send("transpose 1 5"); send("mono 1 0"); send("mode fixed thud");
        send("bpm 60"); send("key 7"); send("vol 1 50"); send("master 80"); send("save");
        send("play other"); send("mode voices");
        send("play MIXS"); send("status"); ticks(1);
        CHECK(strstr(logs, "Loaded preset for mixsong.") && strstr(logs, "mode=fixed"));
        CHECK(strstr(logs, "BPM=60 speed=100% key=7") && strstr(logs, "Master=80%"));
        CHECK(call_count == 2 && calls[0].sound == 25 && calls[1].sound == 21);
        near_pitch(0, 2);
        CHECK(fabs(calls[0].volume - 0.53 * 0.5 * 0.8) < 0.000002);
        send("save");
    } else if (!strcmp(mode, "v2-tempo")) {
        size_t first;
        unsigned anchor;
        send("play timeline"); melody(); send("bpm 60"); ticks(121);
        CHECK(call_count == 6 && calls[2].tick - calls[0].tick == 60 && calls[4].tick - calls[2].tick == 60);
        first = call_count;
        send("play timeline"); melody(); send("speed 50"); ticks(121);
        CHECK(call_count == first + 6 && calls[first + 2].tick - calls[first].tick == 60);
        first = call_count;
        send("play timeline"); melody(); send("bpm 60"); send("bpm file"); ticks(61);
        CHECK(call_count == first + 6 && calls[first + 2].tick - calls[first].tick == 30);
        first = call_count;
        send("play timeline"); melody(); ticks(15); anchor = tick_number;
        send("bpm 60"); ticks(91);
        CHECK(call_count == first + 6 && calls[first + 2].tick == anchor + 31 && calls[first + 4].tick == anchor + 91);
        /* The last event needs 105 ticks in total: 15 at normal speed and 90 at half speed. */
        send("play mapped"); melody(); first = call_count; send("speed 50"); ticks(181);
        CHECK(call_count == first + 3 && calls[first + 1].tick - calls[first].tick == 60 && calls[first + 2].tick - calls[first + 1].tick == 120);
        send("play mapped"); melody(); send("bpm 300"); ticks(12);
        send("bpm file"); send("status"); CHECK(strstr(logs, "BPM=60 speed=100% key=0"));
        first = call_count; ticks(61); CHECK(call_count == first + 2);
        send("bpm 0"); send("bpm 301"); send("bpm text");
        send("speed 24"); send("speed 401"); send("speed text");
        send("status"); CHECK(strstr(logs, "BPM must be") && strstr(logs, "Speed must be"));
    } else if (!strcmp(mode, "v2-key")) {
        send("play timeline"); melody(); ticks(1);
        near_pitch(0, 1); near_pitch(1, 0.25);
        send("key 12"); ticks(30);
        near_pitch(2, 2); near_pitch(3, 0.25);
        send("key 7"); send("transpose 1 5"); ticks(30);
        near_pitch(4, 2); near_pitch(5, 0.25);
        send("key -49"); send("key 49"); send("key text"); send("status");
        CHECK(strstr(logs, "Key must be -48 to 48") && strstr(logs, "key=7"));
        send("play timeline"); send("out 1 drums"); send("key 48"); ticks(1);
        CHECK(calls[6].sound == 21);
    } else if (!strcmp(mode, "v2-names")) {
        send("play MOON.mid"); CHECK(strstr(logs, "Playing moonlight-sonata:"));
        send("play sonata"); CHECK(strstr(logs, "Playing moonlight-sonata:"));
        send("play cirno"); CHECK(strstr(logs, "Playing cirno:"));
        send("play cirno1"); CHECK(strstr(logs, "Playing cirno1:"));
        CHECK(DeleteFileA("user/jukebox/cirno.mid"));
        send("play cirno"); CHECK(strstr(logs, "More than one song matches"));
        send("play alpha"); CHECK(strstr(logs, "Playing alphabeta:"));
        send("play zz-last"); CHECK(strstr(logs, "Playing zz-last:"));
        send("play many"); CHECK(strstr(logs, "More than one song matches") && strstr(logs, "More matches exist"));
        send("play unknown"); CHECK(strstr(logs, "No song matches. Use /jb list."));
        send("play a/b"); send("play a.b"); send("play bad name");
        CHECK(strstr(logs, "Song name needs 1 to 48"));
        send("stop"); ticks(200); CHECK(!call_count);
    } else if (!strcmp(mode, "v2-volume")) {
        send("play timeline"); melody(); send("vol 1 50"); send("master 80"); ticks(1);
        CHECK(fabs(calls[0].volume - 0.79 * 0.5 * 0.8) < 0.000002);
        CHECK(fabs(calls[1].volume - 0.58 * 0.8) < 0.000002);
        send("vol 1 200"); send("master 200"); ticks(30);
        CHECK(calls[2].volume == 1 && calls[3].volume == 1);
        send("vol 1 -1"); send("vol 1 201"); send("vol 1 text");
        send("master -1"); send("master 201"); send("master text");
        CHECK(strstr(logs, "Volume must be") && strstr(logs, "Master must be"));
        send("master 0"); ticks(30); CHECK(call_count == 4);
    } else if (!strcmp(mode, "v2-control-bad")) {
        FILE *file = fopen("control.txt", "r");
        char text[128], error[128];
        CHECK(file && fgets(text, sizeof text, file) && fgets(error, sizeof error, file));
        fclose(file);
        text[strcspn(text, "\r\n")] = 0;
        error[strcspn(error, "\r\n")] = 0;
        send("play timeline"); melody(); send(text); send("status");
        CHECK(strstr(logs, error));
        CHECK(strstr(logs, "BPM=120 speed=100% key=0") && strstr(logs, "Master=100%"));
        ticks(1); CHECK(call_count == 2 && fabs(calls[0].volume - 0.79) < 0.000002);
    } else if (!strcmp(mode, "v2-mix-bad")) {
        send("play timeline"); send("status");
        CHECK(strstr(logs, "Preset ignored:") && strstr(logs, "mode=voices"));
        CHECK(strstr(logs, "speed=100% key=0") && strstr(logs, "Master=100%"));
        ticks(1); CHECK(call_count == 2 && calls[0].sound == 39);
    } else { CHECK(0); }
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
    modern = !strncmp(mode, "v2", 2);
    if (modern) { bank[0] = 0; automatic = 3; maximum = 32; }
    library = LoadLibraryA(argv[1]);
    CHECK(library);
    symbol = GetProcAddress(library, "sv_plugin_entry");
    CHECK(symbol && sizeof symbol == sizeof entry);
    memcpy(&entry, &symbol, sizeof entry);
    CHECK(entry(2, &plugin) == SV_INCOMPATIBLE);
    CHECK(entry(1, &plugin) == SV_OK && !strcmp(plugin.id, "sv.jukebox") && !strcmp(plugin.version, "0.2.0"));
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
        else if (!strcmp(mode, "preserve")) { send("scale"); ticks(1); CHECK(call_count == 1 && fabs(calls[0].pitch - 0.1376976659) < 0.000002); plugin.disable(1); CHECK(plugin.enable(&h, 1) == SV_OK); }
        else if (modern) test_v2(mode);
        else if (!strcmp(mode, "bad-midi")) { send("play bad"); CHECK(strstr(logs, "MIDI ")); ticks(1); CHECK(!call_count); }
        else { CHECK(0); }
        plugin.disable(1);
    }
    FreeLibrary(library);
    puts("JUKEBOX_HOST_PASS");
    return 0;
}
