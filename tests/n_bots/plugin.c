#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include "systems_plugin.h"
#include "sv_game_ai.h"
#include "sv_game_write.h"

static const SvHost *host;
static const SvGameAi *ai;
static const SvGameProps *props;
static const SvGameWrite *change;
static SvEntity bot, driver, scenario, normal, stale;
static SvEntity dead, released, living;
static SvEntity receiver, donor, rider;

static int death_and_street(SvOwner owner)
{
    uint32_t id;
    SvValue value = {0};
    SvAiStreetPosition out = {sizeof out}, saved;
    STEP(props->find(owner, "human.despawn", &id) == SV_OK);
    value.i = -1;
    STEP(props->set(owner, id, dead, 0, value) == SV_INVALID);
    value.i = 3601;
    STEP(props->set(owner, id, dead, 0, value) == SV_INVALID);
    value.i = 100;
    STEP(props->set(owner, id, living, 0, value) == SV_STATE);
    STEP(props->set(owner, id, dead ^ ((uint64_t)1 << 32), 0, value) == SV_NOT_FOUND);
    STEP(props->set(owner, id, dead, 1, value) == SV_INVALID);
    STEP(props->set(owner, id, dead, 0, value) == SV_OK);
    STEP(props->get(owner, id, dead, 0, &value) == SV_OK && value.i == 100);
    value.i = 2699;
    STEP(props->set(owner, id, released, 0, value) == SV_OK);
    STEP(ai->street_near(owner, (SvVector){80, 5.5f, 2}, 16, &out) == SV_OK);
    STEP(out.point.road == 0 && out.point.direction <= 1 && out.point.lane == 0);
    STEP(out.next_node == (uint32_t)(out.point.direction ^ 1));
    STEP(out.direction.x == (out.point.direction ? -1.0f : 1.0f));
    STEP(out.position.x == 80 && out.position.y == 5.5f);
    saved = out;
    STEP(ai->street_near(owner, (SvVector){3000, 0, 3000}, 16, &out) == SV_NOT_FOUND);
    STEP(!memcmp(&out, &saved, sizeof out));
    STEP(ai->street_near(owner, (SvVector){0}, 0, &out) == SV_INVALID);
    STEP(ai->street_near(owner, (SvVector){0}, 513, &out) == SV_INVALID);
    STEP(ai->street_near(owner, (SvVector){32769, 0, 0}, 16, &out) == SV_INVALID);
    STEP(ai->street_near(owner, (SvVector){0}, 16, NULL) == SV_INVALID);
    out.size = 0;
    saved = out;
    STEP(ai->street_near(owner, (SvVector){80, 5.5f, 2}, 16, &out) == SV_INVALID);
    STEP(!memcmp(&out, &saved, sizeof out));
    return 0;
}

static DWORD WINAPI wrong_thread(void *argument)
{
    SvOwner owner = *(SvOwner *)argument;
    STEP(ai->aim_mode(owner, bot, 1) == SV_STATE);
    STEP(ai->chase_mode(owner, driver, 1) == SV_STATE);
    STEP(ai->unpossess(owner, receiver) == SV_STATE);
    STEP(change->input_lock(owner, receiver, SV_INPUT_LOCK_ALL) == SV_STATE);
    STEP(change->voice_route(owner, bot, receiver, SV_VOICE_DENY) == SV_STATE);
    {
        SvAiStreetPosition out = {sizeof out};
        STEP(ai->street_near(owner, (SvVector){0}, 16, &out) == SV_STATE);
    }
    return 0;
}

static int run(SvOwner owner, const char *text)
{
    const void *table;
    const SvGameRead *read;
    SvEntity list[256];
    SvPlayerState p = {sizeof p};
    uint32_t count, next, i;
    if (!normal) {
        HANDLE thread;
        DWORD code;
        SvResult result = host->get_api(owner, SV_API_AI, 1, &table);
        STEP(result == (!strcmp(text, "missing") ? SV_DENIED : SV_OK));
        ai = table;
        if (strcmp(text, "missing")) STEP(ai->size >= sizeof *ai);
        STEP(host->get_api(owner, SV_API_GAME_PROPS, 1, &table) == SV_OK);
        props = table;
        STEP(host->get_api(owner, SV_API_GAME_READ, 1, &table) == SV_OK);
        read = table;
        STEP(read->entities(owner, SV_ENTITY_PLAYER, 0, list, 256, &count, &next) == SV_OK);
        for (i = 0; i < count; ++i) {
            STEP(read->player(owner, list[i], &p) == SV_OK);
            if (!strcmp(p.name, "Aim")) bot = list[i];
            if (!strcmp(p.name, "Driver")) driver = list[i];
            if (!strcmp(p.name, "Scenario")) scenario = list[i];
            if (!strcmp(p.name, "Systems test")) normal = list[i];
            if (!strcmp(p.name, "Aim")) living = p.human;
            if (!strcmp(p.name, "Dead")) dead = p.human;
            if (!strcmp(p.name, "Release")) released = p.human;
            if (!strcmp(p.name, "Receiver")) receiver = list[i];
            if (!strcmp(p.name, "Donor")) donor = list[i];
            if (!strcmp(p.name, "Rider")) rider = list[i];
        }
        if (!strcmp(text, "missing")) return 0;
        STEP(host->get_api(owner, SV_API_GAME_WRITE, 1, &table) == SV_OK);
        change = table;
        STEP(bot && driver && scenario && normal && receiver && donor && rider);
        stale = bot ^ ((uint64_t)1 << 32);
        STEP(ai->aim_mode(owner, stale, 1) == SV_NOT_FOUND);
        STEP(ai->chase_mode(owner, stale, 1) == SV_NOT_FOUND);
        STEP(ai->aim_mode(owner, normal, 1) == SV_DENIED);
        STEP(ai->chase_mode(owner, normal, 1) == SV_DENIED);
        STEP(ai->chase_mode(owner, bot, 1) == SV_STATE);
        STEP(ai->chase_mode(owner, driver, -1) == SV_INVALID);
        STEP(ai->chase_mode(owner, driver, 2) == SV_INVALID);
        STEP(ai->aim_mode(owner, bot, -1) == SV_INVALID);
        STEP(ai->aim_mode(owner, bot, 2) == SV_INVALID);
        STEP(ai->chase_mode(0, driver, 1) == SV_STATE);
        thread = CreateThread(NULL, 0, wrong_thread, &owner, 0, NULL);
        STEP(thread != NULL);
        STEP(WaitForSingleObject(thread, 5000) == WAIT_OBJECT_0);
        STEP(GetExitCodeThread(thread, &code));
        CloseHandle(thread);
        STEP(code == 0);
        {
            int line = death_and_street(owner);
            if (line) return line;
        }
    }
    if (!strcmp(text, "street-occupied")) {
        SvAiStreetPosition out = {sizeof out};
        STEP(ai->street_near(owner, (SvVector){80, 5.5f, 2}, 40, &out) == SV_OK);
        STEP(out.position.x <= 64 || out.position.x >= 96);
    } else if (!strcmp(text, "street-blocked")) {
        SvAiStreetPosition out = {sizeof out};
        STEP(ai->street_near(owner, (SvVector){80, 5.5f, 2}, 1, &out) == SV_NOT_FOUND);
    } else if (!strcmp(text, "street-slope")) {
        SvAiStreetPosition out = {sizeof out};
        STEP(ai->street_near(owner, (SvVector){80, 7.5f, 2}, 1, &out) == SV_OK);
        STEP(out.position.y == 7.5f);
    } else if (!strcmp(text, "lock")) {
        STEP(change->input_lock(owner, bot, SV_INPUT_LOCK_BODY) == SV_DENIED);
        STEP(change->input_lock(owner, receiver, 4) == SV_INVALID);
        STEP(change->input_lock(owner, receiver ^ ((uint64_t)1 << 32), SV_INPUT_LOCK_BODY) == SV_NOT_FOUND);
        STEP(change->input_lock(owner, receiver, SV_INPUT_LOCK_ALL) == SV_OK);
    } else if (!strcmp(text, "voice")) {
        STEP(change->voice_route(owner, bot, receiver, SV_VOICE_DENY) == SV_OK);
        STEP(change->voice_route(owner, receiver, receiver, SV_VOICE_DENY) == SV_INVALID);
        STEP(change->voice_route(owner, bot, receiver, 3) == SV_INVALID);
        STEP(change->voice_route(owner, bot, donor, SV_VOICE_DENY) == SV_DENIED);
        STEP(change->voice_route(owner, stale, receiver, SV_VOICE_DENY) == SV_NOT_FOUND);
    } else if (!strcmp(text, "voice-force")) {
        STEP(change->voice_route(owner, bot, receiver, SV_VOICE_FORCE) == SV_OK);
    } else if (!strcmp(text, "voice-clear")) {
        STEP(change->voice_route(owner, bot, receiver, SV_VOICE_DEFAULT) == SV_OK);
    } else if (!strcmp(text, "unlock")) {
        STEP(change->input_lock(owner, receiver, 0) == SV_OK);
    } else if (!strcmp(text, "aim-at")) {
        SvAiAimTarget t = {sizeof t, SV_AIM_POINT, {10, 20, 30}, 0};
        STEP(ai->aim_at(owner, bot, NULL, 2) == SV_INVALID);
        t.kind = 0;
        STEP(ai->aim_at(owner, bot, &t, 2) == SV_INVALID);
        t.kind = 3;
        STEP(ai->aim_at(owner, bot, &t, 2) == SV_INVALID);
        t.kind = SV_AIM_POINT;
        STEP(ai->aim_at(owner, bot, &t, 0) == SV_INVALID);
        STEP(ai->aim_at(owner, bot, &t, 3601) == SV_INVALID);
        t.point.x = 40000;
        STEP(ai->aim_at(owner, bot, &t, 2) == SV_INVALID);
        t.point.x = 10;
        STEP(ai->aim_at(owner, stale, &t, 2) == SV_NOT_FOUND);
        STEP(ai->aim_at(owner, driver, &t, 2) == SV_STATE);
        t.kind = SV_AIM_ENTITY;
        t.entity = 0;
        STEP(ai->aim_at(owner, bot, &t, 2) == SV_NOT_FOUND);
        t.size = 0;
        t.kind = SV_AIM_POINT;
        STEP(ai->aim_at(owner, bot, &t, 2) == SV_INVALID);
        t.size = sizeof t;
        STEP(ai->aim_at(owner, bot, &t, 2) == SV_OK);
    } else if (!strcmp(text, "aim-entity")) {
        SvAiAimTarget t = {sizeof t, SV_AIM_ENTITY, {0, 0, 0}, 0};
        t.entity = dead;
        STEP(ai->aim_at(owner, bot, &t, 1) == SV_OK);
    } else if (!strcmp(text, "aim-rider")) {
        SvAiAimTarget t = {sizeof t, SV_AIM_POINT, {10, 20, 30}, 0};
        SvAiInput in = {sizeof in};
        in.buttons = 1;
        STEP(ai->aim_at(owner, rider, &t, 2) == SV_OK);
        STEP(ai->override_input_partial(owner, rider, &in, SV_AI_PART_AXES, 0, 1) == SV_CONFLICT);
        STEP(ai->override_input_partial(owner, rider, &in, SV_AI_PART_ALL, SV_AI_BUTTONS_ALL, 1) == SV_CONFLICT);
        STEP(ai->override_input_partial(owner, rider, &in, SV_AI_PART_BUTTONS | SV_AI_PART_VIEW, 1, 1) == SV_OK);
    } else if (!strcmp(text, "street-list")) {
        SvAiStreetPosition cand[8], one = {sizeof one};
        SvAiStreetFilter filter = {sizeof filter, SV_STREET_ANY, 4.0f, 16.0f};
        const SvVector at = {80, 5.5f, 2};
        uint32_t got = 99, k;
        double previous = 0;
        for (k = 0; k < 8; ++k) cand[k].size = sizeof cand[k];
        /* The object at this place keeps every candidate 16 units away, the cand is sorted, and the first one is street_near. */
        STEP(ai->street_list(owner, at, 40, NULL, cand, 8, &got) == SV_OK);
        STEP(got >= 1 && got <= 8);
        STEP(ai->street_near(owner, at, 40, &one) == SV_OK);
        STEP(!memcmp(&cand[0], &one, sizeof one));
        for (k = 0; k < got; ++k) {
            double dx = cand[k].position.x - at.x, dy = cand[k].position.y - at.y, dz = cand[k].position.z - at.z;
            double d = dx * dx + dy * dy + dz * dz;
            STEP(d >= 256.0 && d >= previous);
            previous = d;
        }
        /* The road of the test runs along x. */
        filter.kinds = SV_STREET_ALONG_Z;
        STEP(ai->street_list(owner, at, 40, &filter, cand, 8, &got) == SV_NOT_FOUND && got == 0);
        filter.kinds = SV_STREET_ALONG_X;
        STEP(ai->street_list(owner, at, 40, &filter, cand, 8, &got) == SV_OK && got >= 1);
        /* Gaps of 0 turn the checks off. The default filter finds nothing within 1 unit of the object. */
        STEP(ai->street_list(owner, at, 1, NULL, cand, 1, &got) == SV_NOT_FOUND && got == 0);
        filter.kinds = SV_STREET_ANY; filter.human_gap = 0; filter.object_gap = 0;
        STEP(ai->street_list(owner, at, 1, &filter, cand, 1, &got) == SV_OK && got == 1);
        filter.human_gap = 4.0f; filter.object_gap = 16.0f;
        STEP(ai->street_list(owner, at, 40, &filter, cand, 0, &got) == SV_INVALID);
        STEP(ai->street_list(owner, at, 40, &filter, cand, 9, &got) == SV_INVALID);
        STEP(ai->street_list(owner, at, 40, &filter, cand, 8, NULL) == SV_INVALID);
        STEP(ai->street_list(owner, at, 40, &filter, NULL, 8, &got) == SV_INVALID);
        STEP(ai->street_list(owner, at, 0, &filter, cand, 8, &got) == SV_INVALID);
        filter.kinds = 0;
        STEP(ai->street_list(owner, at, 40, &filter, cand, 8, &got) == SV_INVALID);
        filter.kinds = 4;
        STEP(ai->street_list(owner, at, 40, &filter, cand, 8, &got) == SV_INVALID);
        filter.kinds = SV_STREET_ANY; filter.human_gap = -1;
        STEP(ai->street_list(owner, at, 40, &filter, cand, 8, &got) == SV_INVALID);
        filter.human_gap = 4.0f; filter.object_gap = 65;
        STEP(ai->street_list(owner, at, 40, &filter, cand, 8, &got) == SV_INVALID);
        filter.object_gap = 16.0f; filter.size = 0;
        STEP(ai->street_list(owner, at, 40, &filter, cand, 8, &got) == SV_INVALID);
        filter.size = sizeof filter; cand[1].size = 0;
        STEP(ai->street_list(owner, at, 40, &filter, cand, 8, &got) == SV_INVALID);
    } else if (!strcmp(text, "bad-seat")) {
        STEP(ai->chase_mode(owner, driver, 1) == SV_STATE);
    } else if (!strcmp(text, "bad-graph")) {
        SvAiStreetPosition out = {sizeof out}, saved = out;
        STEP(ai->street_near(owner, (SvVector){80, 5.5f, 2}, 16, &out) == SV_FAILED);
        STEP(!memcmp(&out, &saved, sizeof out));
    } else if (!strcmp(text, "possess")) {
        STEP(ai->unpossess(owner, receiver) == SV_NOT_FOUND);
        STEP(ai->unpossess(owner, receiver ^ ((uint64_t)1 << 32)) == SV_NOT_FOUND);
        STEP(ai->possess(owner, receiver, donor) == SV_OK);
    } else if (!strcmp(text, "unpossess")) {
        STEP(ai->unpossess(owner, receiver) == SV_OK);
        STEP(ai->unpossess(owner, receiver) == SV_NOT_FOUND);
    } else if (!strcmp(text, "speed-cap")) {
        uint32_t speed_bits = 0x7fc00000u;
        float nan_speed;
        memcpy(&nan_speed, &speed_bits, sizeof nan_speed);
        STEP(ai->driver_speed(owner, driver, nan_speed) == SV_INVALID && ai->driver_speed(owner, driver, -0.5f) == SV_INVALID);
        STEP(ai->driver_speed(owner, stale, 0.0f) == SV_NOT_FOUND);
        STEP(ai->driver_speed(owner, normal, 0.0f) != SV_OK);
        STEP(ai->driver_speed(owner, driver, 0.0f) == SV_OK);
    } else if (!strcmp(text, "accuracy")) {
        uint32_t scale_bits = 0x7fc00000u;
        float scale_nan;
        memcpy(&scale_nan, &scale_bits, sizeof scale_nan);
        STEP(ai->accuracy(owner, bot, scale_nan) == SV_INVALID && ai->accuracy(owner, bot, -0.5f) == SV_INVALID);
        STEP(ai->sight_range(owner, bot, scale_nan) == SV_INVALID && ai->sight_range(owner, bot, -2.0f) == SV_INVALID);
        STEP(ai->accuracy(owner, stale, 0.0f) == SV_NOT_FOUND && ai->sight_range(owner, stale, 10.0f) == SV_NOT_FOUND);
        STEP(ai->accuracy(owner, bot, 0.0f) == SV_OK && ai->sight_range(owner, bot, 500.0f) == SV_OK);
    } else if (!strcmp(text, "accuracy-free")) {
        STEP(ai->accuracy(owner, bot, -1.0f) == SV_OK && ai->sight_range(owner, bot, -1.0f) == SV_OK);
    } else if (!strcmp(text, "speed-fast")) {
        STEP(ai->driver_speed(owner, driver, 10.0f) == SV_OK);
    } else if (!strcmp(text, "speed-free")) {
        STEP(ai->driver_speed(owner, driver, -1.0f) == SV_OK);
    } else if (!strcmp(text, "parts-lights")) {
        STEP(ai->chase_effects(owner, driver, 16) == SV_INVALID);
        STEP(ai->chase_effects(owner, stale, SV_CHASE_LIGHTS) == SV_NOT_FOUND);
        STEP(ai->chase_effects(owner, normal, SV_CHASE_LIGHTS) == SV_DENIED);
        STEP(ai->chase_effects(owner, driver, SV_CHASE_LIGHTS) == SV_OK);
    } else if (!strcmp(text, "parts-speed")) {
        STEP(ai->chase_effects(owner, driver, SV_CHASE_SPEED | SV_CHASE_LANE) == SV_OK);
    } else if (!strcmp(text, "parts-all")) {
        STEP(ai->chase_effects(owner, driver, SV_CHASE_ALL) == SV_OK);
    } else if (!strcmp(text, "parts-input")) {
        SvAiInput in = {sizeof in};
        in.buttons = 4; in.axes[1] = 0.5f; in.yaw = 1.0f;
        STEP(ai->override_input_partial(owner, bot, &in, 0, 4, 2) == SV_INVALID);
        STEP(ai->override_input_partial(owner, bot, &in, 8, 4, 2) == SV_INVALID);
        STEP(ai->override_input_partial(owner, bot, &in, SV_AI_PART_BUTTONS, 0x80000u, 2) == SV_INVALID);
        STEP(ai->override_input_partial(owner, bot, &in, SV_AI_PART_BUTTONS, 4, 0) == SV_INVALID);
        STEP(ai->override_input_partial(owner, stale, &in, SV_AI_PART_BUTTONS, 4, 2) == SV_NOT_FOUND);
        STEP(ai->override_input_partial(owner, bot, NULL, SV_AI_PART_BUTTONS, 4, 2) == SV_INVALID);
        STEP(ai->override_input_partial(owner, bot, &in, SV_AI_PART_BUTTONS | SV_AI_PART_AXES, 4, 2) == SV_OK);
    } else if (!strcmp(text, "parts-full")) {
        SvAiInput in = {sizeof in};
        in.buttons = 8;
        STEP(ai->override_input_partial(owner, bot, &in, SV_AI_PART_ALL, 0x7ffffu, 3) == SV_OK);
    } else if (!strcmp(text, "scenario")) {
        STEP(ai->chase_mode(owner, scenario, 0) == SV_DENIED);
        STEP(ai->chase_mode(owner, scenario, 1) == SV_OK);
    } else {
        int enabled = !strcmp(text, "on");
        STEP(ai->aim_mode(owner, bot, enabled) == SV_OK);
        STEP(ai->chase_mode(owner, driver, enabled) == SV_OK);
    }
    return 0;
}

static SvResult SV_CALL command(SvOwner owner, uint64_t actor, const char *text)
{
    int line = run(owner, text);
    char message[96];
    (void)actor;
    if (line) {
        snprintf(message, sizeof message, "N bots test failed at line %d.", line);
        host->log(owner, message);
        return SV_FAILED;
    }
    return systems_godmode(props, owner, normal, 1);
}

static SvResult SV_CALL enable(const SvHost *api, SvOwner owner)
{
    host = api;
    return host->command(owner, "nbots", SV_COMMAND_LOCAL, command);
}

SV_EXPORT SvResult SV_CALL sv_plugin_entry(uint32_t abi, SvPlugin *plugin)
{
    if (abi != SV_PLUGIN_ABI || !plugin || plugin->size < sizeof *plugin)
        return SV_INCOMPATIBLE;
    memset(plugin, 0, sizeof *plugin);
    plugin->size = sizeof *plugin;
    plugin->abi = abi;
    strcpy(plugin->id, "test.nbots");
    strcpy(plugin->version, "0.1.0");
    plugin->flags = SV_PLUGIN_LIVE;
    plugin->enable = enable;
    return SV_OK;
}
