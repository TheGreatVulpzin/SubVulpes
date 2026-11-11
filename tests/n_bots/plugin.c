#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include "systems_plugin.h"
#include "sv_game_ai.h"

static const SvHost *host;
static const SvGameAi *ai;
static const SvGameProps *props;
static SvEntity bot, driver, scenario, normal, stale;
static SvEntity dead, released, living;
static SvEntity receiver, donor;

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
        }
        if (!strcmp(text, "missing")) return 0;
        STEP(bot && driver && scenario && normal && receiver && donor);
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
    } else if (!strcmp(text, "parts-lights")) {
        STEP(ai->chase_parts(owner, driver, 16) == SV_INVALID);
        STEP(ai->chase_parts(owner, stale, SV_CHASE_LIGHTS) == SV_NOT_FOUND);
        STEP(ai->chase_parts(owner, normal, SV_CHASE_LIGHTS) == SV_DENIED);
        STEP(ai->chase_parts(owner, driver, SV_CHASE_LIGHTS) == SV_OK);
    } else if (!strcmp(text, "parts-speed")) {
        STEP(ai->chase_parts(owner, driver, SV_CHASE_SPEED | SV_CHASE_LANE) == SV_OK);
    } else if (!strcmp(text, "parts-all")) {
        STEP(ai->chase_parts(owner, driver, SV_CHASE_ALL) == SV_OK);
    } else if (!strcmp(text, "parts-input")) {
        SvAiInput in = {sizeof in};
        in.buttons = 4; in.axes[1] = 0.5f; in.yaw = 1.0f;
        STEP(ai->override_parts(owner, bot, &in, 0, 4, 2) == SV_INVALID);
        STEP(ai->override_parts(owner, bot, &in, 8, 4, 2) == SV_INVALID);
        STEP(ai->override_parts(owner, bot, &in, SV_AI_PART_BUTTONS, 0x80000u, 2) == SV_INVALID);
        STEP(ai->override_parts(owner, bot, &in, SV_AI_PART_BUTTONS, 4, 0) == SV_INVALID);
        STEP(ai->override_parts(owner, stale, &in, SV_AI_PART_BUTTONS, 4, 2) == SV_NOT_FOUND);
        STEP(ai->override_parts(owner, bot, NULL, SV_AI_PART_BUTTONS, 4, 2) == SV_INVALID);
        STEP(ai->override_parts(owner, bot, &in, SV_AI_PART_BUTTONS | SV_AI_PART_AXES, 4, 2) == SV_OK);
    } else if (!strcmp(text, "parts-full")) {
        SvAiInput in = {sizeof in};
        in.buttons = 8;
        STEP(ai->override_parts(owner, bot, &in, SV_AI_PART_ALL, 0x7ffffu, 3) == SV_OK);
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
