#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include "systems_plugin.h"
#include "sv_game_mission.h"
#include "sv_game_props.h"

static const SvGameMission *mission_api;
static const SvGameRead *reader;
static const SvGameProps *writer;
static SvEntity reporter;
static SvMissionRef old_reference;

static SvMissionStart specification(SvEntity item, int reward, int expire)
{
    SvMissionStart s = {sizeof s, 5, 1, reward, expire, 0, 0};
    s.item = item;
    return s;
}

static SvEntity disk(SvOwner owner, int type)
{
    SvEntity entities[256], found = 0;
    SvItemState item = {sizeof item};
    uint32_t count, next, j;
    if (reader->entities(owner, SV_ENTITY_ITEM, 0, entities, 256, &count, &next) != SV_OK)
        return 0;
    for (j = 0; j < count; ++j)
        if (reader->item(owner, entities[j], &item) == SV_OK && item.type == type && item.time_to_live > 0)
            found = entities[j];
    return found;
}

static SvResult run_command(SvOwner owner, const char *text)
{
    SvMissionStart s;
    SvMissionRef r;
    if (!strcmp(text, "stale")) {
        STEP(mission_api->cancel(owner, &old_reference) == SV_NOT_FOUND);
        return SV_OK;
    }
    if (!strcmp(text, "post-reset")) {
        s = specification(disk(owner, 20), 100, 300);
        STEP(s.item && mission_api->start(owner, 2, &s, &r) == SV_OK);
        s = specification(disk(owner, 21), 0, 120);
        STEP(s.item && mission_api->start(owner, 2, &s, &r) == SV_OK);
        s = specification(disk(owner, 22), 0, 300);
        STEP(s.item && mission_api->start(owner, 2, &s, &r) == SV_OK);
        return SV_OK;
    }
    if (!strcmp(text, "limit-id")) {
        s = specification(disk(owner, 17), 0, 300);
        STEP(s.item && mission_api->start(owner, 2, &s, &r) == SV_LIMIT);
        return SV_OK;
    }
    if (!strcmp(text, "owner")) {
        s = specification(disk(owner, 22), 0, 300);
        return mission_api->start(owner, 2, &s, &r);
    }
    return SV_INVALID;
}

static SvResult SV_CALL command(SvOwner owner, uint64_t actor, const char *text)
{
    SvResult result = run_command(owner, text);
    (void)actor;
    STEP(systems_godmode(writer, owner, reporter, result == SV_OK) == SV_OK);
    return result;
}

typedef struct { const SvGameMission *api; SvOwner owner; } Args;

static DWORD WINAPI wrong_thread(void *data)
{
    Args *a = data;
    SvMissionSlots out = {sizeof out};
    SvMissionStart s = {sizeof s};
    SvMissionRef r = {0};
    STEP(a->api->slots(a->owner, 0, &out) == SV_STATE);
    STEP(a->api->start(a->owner, 0, &s, &r) == SV_STATE);
    STEP(a->api->cancel(a->owner, &r) == SV_STATE);
    return 0;
}

/* Runs inside the tick callback. Returns 0, or the line of the check that failed. */
int mission_fixture(const SvHost *host, SvOwner owner)
{
    const void *table = NULL;
    const SvGameMission *api;
    SvMissionSlots out = {sizeof out}, saved;
    SvMissionStart s;
    SvMissionRef r, untouched, bad;
    SvEntity first, final, entities[256];
    SvPlayerState player = {sizeof player};
    uint32_t count, next, j;
    Args args;
    HANDLE thread;
    DWORD code;
    STEP(host->get_api(owner, SV_API_MISSION, SV_GAME_MISSION_VERSION, &table) == SV_OK);
    api = (const SvGameMission *)table;
    STEP(api && api->size >= sizeof *api && api->version == SV_GAME_MISSION_VERSION);
    mission_api = api;
    STEP(host->get_api(owner, SV_API_GAME_READ, SV_GAME_READ_VERSION, &table) == SV_OK);
    reader = (const SvGameRead *)table;
    STEP(host->get_api(owner, SV_API_GAME_PROPS, SV_GAME_PROPS_VERSION, &table) == SV_OK);
    writer = (const SvGameProps *)table;
    STEP(reader->entities(owner, SV_ENTITY_PLAYER, 0, entities, 256, &count, &next) == SV_OK);
    for (j = 0; j < count; ++j) {
        STEP(reader->player(owner, entities[j], &player) == SV_OK);
        if (!strcmp(player.name, "Systems test")) reporter = entities[j];
    }
    first = disk(owner, 17);
    final = disk(owner, 18);
    STEP(reporter && first && final);
    s = specification(first, 0, 300);
    args = (Args){api, owner};
    thread = CreateThread(NULL, 0, wrong_thread, &args, 0, NULL);
    STEP(thread != NULL);
    STEP(WaitForSingleObject(thread, 5000) == WAIT_OBJECT_0);
    STEP(GetExitCodeThread(thread, &code));
    CloseHandle(thread);
    STEP(code == 0);
    STEP(api->slots(0, 0, &out) == SV_STATE);
    STEP(api->start(0, 0, &s, &r) == SV_STATE);
    STEP(api->cancel(0, NULL) == SV_STATE);
    STEP(api->slots(owner, 5, &out) == SV_INVALID);
    STEP(api->slots(owner, 0, NULL) == SV_INVALID);
    out.size = 0;
    saved = out;
    STEP(api->slots(owner, 0, &out) == SV_INVALID);
    STEP(!memcmp(&saved, &out, sizeof out));
    out.size = sizeof out;
    STEP(api->slots(owner, 4, &out) == SV_OK);
    STEP(out.item_size == sizeof(SvMissionState));
    for (j = 0; j < 16; ++j) STEP(out.slots[j].active == 1);
    memset(&r, 0xa5, sizeof r);
    untouched = r;
    STEP(api->start(owner, 4, &s, &r) == SV_LIMIT);
    STEP(!memcmp(&r, &untouched, sizeof r));
    STEP(api->cancel(owner, &out.slots[8].reference) == SV_STATE);
    STEP(api->start(owner, 1, &s, &r) == SV_LIMIT);
    STEP(!memcmp(&r, &untouched, sizeof r));
    STEP(api->start(owner, 5, &s, &r) == SV_INVALID);
    STEP(api->start(owner, 0, NULL, &r) == SV_INVALID);
    STEP(api->start(owner, 0, &s, NULL) == SV_INVALID);
    s.size = 0;
    STEP(api->start(owner, 0, &s, &r) == SV_INVALID);
    s = specification(first, 0, 300);
    s.kind = 7;
    STEP(api->start(owner, 0, &s, &r) == SV_INVALID);
    s = specification(first, 0, 300);
    s.partner = 5;
    STEP(api->start(owner, 0, &s, &r) == SV_INVALID);
    s = specification(first, -1, 300);
    STEP(api->start(owner, 0, &s, &r) == SV_INVALID);
    s = specification(first, SV_MISSION_REWARD_MAX + 1, 300);
    STEP(api->start(owner, 0, &s, &r) == SV_INVALID);
    s = specification(first, 0, 100);
    STEP(api->start(owner, 0, &s, &r) == SV_INVALID);
    s = specification(first, 0, SV_MISSION_EXPIRE_MAX + 1);
    STEP(api->start(owner, 0, &s, &r) == SV_INVALID);
    s = specification(first, 0, 300);
    s.site = -1;
    STEP(api->start(owner, 0, &s, &r) == SV_INVALID);
    s = specification(reporter, 0, 300);
    STEP(api->start(owner, 0, &s, &r) == SV_NOT_FOUND);
    s = specification(first, 0, 300);
    STEP(api->cancel(owner, NULL) == SV_INVALID);
    STEP(api->start(owner, 0, &s, &r) == SV_OK);
    STEP(r.size == sizeof r && r.team == 0 && r.slot == 0 && r.item == first);
    STEP(api->start(owner, 2, &s, &bad) == SV_CONFLICT);
    bad = r;
    bad.size = 0;
    STEP(api->cancel(owner, &bad) == SV_INVALID);
    bad = r;
    bad.team = 5;
    STEP(api->cancel(owner, &bad) == SV_INVALID);
    bad = r;
    bad.slot = 16;
    STEP(api->cancel(owner, &bad) == SV_INVALID);
    bad = r;
    bad.item = reporter;
    STEP(api->cancel(owner, &bad) == SV_NOT_FOUND);
    bad.item = final;
    STEP(api->cancel(owner, &bad) == SV_NOT_FOUND);
    bad = r;
    ++bad.scenario;
    STEP(api->cancel(owner, &bad) == SV_NOT_FOUND);
    bad = r;
    ++bad.reset;
    STEP(api->cancel(owner, &bad) == SV_NOT_FOUND);
    STEP(api->cancel(owner, &r) == SV_OK);
    STEP(api->cancel(owner, &r) == SV_NOT_FOUND);
    STEP(api->start(owner, 0, &s, &r) == SV_STATE);
    STEP(api->slots(owner, 3, &out) == SV_OK);
    STEP(api->cancel(owner, &out.slots[1].reference) == SV_STATE);
    s = specification(final, 100, 300);
    STEP(api->start(owner, 0, &s, &old_reference) == SV_OK);
    STEP(api->slots(owner, 0, &out) == SV_OK);
    STEP(out.slots[0].active && out.slots[0].reward == 100 && out.slots[0].item == final);
    STEP(host->command(owner, "mission.control", SV_COMMAND_LOCAL, command) == SV_OK);
    return 0;
}
