#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include "systems_plugin.h"
#include "sv_game_mission.h"

static const SvHost *host;
static const SvGameMission *mission;
static const SvGameProps *props;
static const SvGameRead *game;
static SvEntity normal;
static SvMissionRef started;

static int errors(SvOwner owner)
{
    SvMissionRef out = {sizeof out}, saved;
    saved = out;
    STEP(mission->acquisition(owner, 2, 1, &out) == SV_INVALID);
    STEP(mission->acquisition(owner, 0, 0, &out) == SV_INVALID);
    STEP(mission->acquisition(owner, 0, 1u << 5, &out) == SV_INVALID);
    STEP(mission->acquisition(owner, 1, 1, NULL) == SV_INVALID);
    out.size = sizeof out - 1;
    saved = out;
    STEP(mission->acquisition(owner, 1, 1, &out) == SV_INVALID && !memcmp(&out, &saved, sizeof out));
    out.size = sizeof out;
    STEP(mission->acquisition(0, 1, 1, &out) == SV_STATE);
    return 0;
}

static DWORD WINAPI wrong_thread(void *argument)
{
    SvOwner owner = *(SvOwner *)argument;
    SvMissionRef out = {sizeof out};
    STEP(mission->acquisition(owner, 1, 1, &out) == SV_STATE);
    return 0;
}

static int setup(SvOwner owner, const char *text)
{
    const void *table;
    SvEntity list[256];
    uint32_t count, next, i;
    HANDLE thread;
    DWORD code;
    SvResult result = host->get_api(owner, SV_API_MISSION, 1, &table);
    STEP(result == (!strcmp(text, "missing") ? SV_DENIED : SV_OK));
    mission = table;
    STEP(host->get_api(owner, SV_API_GAME_PROPS, 1, &table) == SV_OK);
    props = table;
    STEP(host->get_api(owner, SV_API_GAME_READ, 1, &table) == SV_OK);
    game = table;
    STEP(game->entities(owner, SV_ENTITY_PLAYER, 0, list, 256, &count, &next) == SV_OK);
    for (i = 0; i < count; ++i) {
        SvPlayerState p = {sizeof p};
        STEP(game->player(owner, list[i], &p) == SV_OK);
        if (!strcmp(p.name, "Systems test")) normal = list[i];
    }
    if (!strcmp(text, "missing")) return 0;
    STEP(normal && mission->size >= offsetof(SvGameMission, acquisition) + sizeof mission->acquisition);
    {
        int line = errors(owner);
        if (line) return line;
    }
    thread = CreateThread(NULL, 0, wrong_thread, &owner, 0, NULL);
    STEP(thread != NULL);
    STEP(WaitForSingleObject(thread, 5000) == WAIT_OBJECT_0);
    STEP(GetExitCodeThread(thread, &code));
    CloseHandle(thread);
    STEP(code == 0);
    return 0;
}

/* Both teams hold one active slot of the scenario in the reference. */
static int shared(SvOwner owner, const SvMissionRef *r, uint32_t teams)
{
    uint32_t t, j;
    for (t = 0; t < 5; ++t) {
        SvMissionSlots slots = {sizeof slots};
        int found = 0;
        STEP(mission->slots(owner, t, &slots) == SV_OK);
        for (j = 0; j < SV_MISSION_SLOTS; ++j)
            if (slots.slots[j].active && slots.slots[j].reference.scenario == r->scenario) {
                STEP(slots.slots[j].kind == 1 && slots.slots[j].site == 255 && slots.slots[j].stake == 0);
                STEP(slots.slots[j].item == r->item);
                ++found;
            }
        STEP(found == ((teams >> t) & 1u ? 1 : 0));
    }
    return 0;
}

static int run(SvOwner owner, const char *text)
{
    if (!normal) {
        int line = setup(owner, text);
        if (line || !strcmp(text, "missing")) return line;
    }
    if (!strcmp(text, "no-site")) {
        SvMissionRef out = {sizeof out}, saved = out;
        STEP(mission->acquisition(owner, 0, 3, &out) == SV_FAILED && !memcmp(&out, &saved, sizeof out));
        STEP(mission->acquisition(owner, 1, 3, &out) == SV_FAILED && !memcmp(&out, &saved, sizeof out));
    } else if (!strcmp(text, "chase")) {
        started.size = sizeof started;
        STEP(mission->acquisition(owner, 1, 3, &started) == SV_OK);
        STEP(started.team == 0 && started.item != 0);
        {
            int line = shared(owner, &started, 3);
            if (line) return line;
        }
    } else if (!strcmp(text, "full")) {
        SvMissionRef out = {sizeof out};
        STEP(mission->acquisition(owner, 1, 1u << 2, &out) == SV_LIMIT);
        STEP(mission->acquisition(owner, 1, (1u << 2) | 1u, &out) == SV_LIMIT);
    } else if (!strcmp(text, "cancel")) {
        STEP(mission->cancel(owner, &started) == SV_OK);
        {
            int line = shared(owner, &started, 0);
            if (line) return line;
        }
    }
    return 0;
}

static SvResult SV_CALL command(SvOwner owner, uint64_t actor, const char *text)
{
    int line = run(owner, text);
    char message[96];
    (void)actor;
    if (line) {
        snprintf(message, sizeof message, "N mission test failed at line %d.", line);
        host->log(owner, message);
        return SV_FAILED;
    }
    return systems_godmode(props, owner, normal, 1);
}

static SvResult SV_CALL enable(const SvHost *api, SvOwner owner)
{
    host = api;
    return host->command(owner, "nmission", SV_COMMAND_LOCAL, command);
}

SV_EXPORT SvResult SV_CALL sv_plugin_entry(uint32_t abi, SvPlugin *plugin)
{
    if (abi != SV_PLUGIN_ABI || !plugin || plugin->size < sizeof *plugin)
        return SV_INCOMPATIBLE;
    memset(plugin, 0, sizeof *plugin);
    plugin->size = sizeof *plugin;
    plugin->abi = abi;
    strcpy(plugin->id, "test.nmission");
    strcpy(plugin->version, "0.1.0");
    plugin->flags = SV_PLUGIN_LIVE;
    plugin->enable = enable;
    return SV_OK;
}
