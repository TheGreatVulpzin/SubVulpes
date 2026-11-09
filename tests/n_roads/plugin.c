#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include "systems_plugin.h"
#include "sv_game_ai.h"

static const SvHost *host;
static const SvGameAi *ai;
static const SvGameProps *props;
static const SvGameRead *game;
static SvEntity bot, normal, target, gun;
static uint32_t trigger;

static int road_reads(SvOwner owner)
{
    SvAiRoad road = {sizeof road}, saved_road;
    SvAiIntersection cross = {sizeof cross}, saved_cross;
    STEP(ai->road(owner, 0, &road) == SV_OK);
    STEP(road.size == sizeof road && road.count == 1);
    STEP(road.ends[0] == 0 && road.ends[1] == 1 && road.kind == 0);
    STEP(road.lanes[0] == 1 && road.lanes[1] == 1);
    STEP(road.direction.x == 1.0f && road.direction.y == 0.0f && road.direction.z == 0.0f);
    STEP(ai->intersection(owner, 1, &cross) == SV_OK);
    STEP(cross.size == sizeof cross && cross.count == 2);
    STEP(cross.position.x == 160.0f && cross.position.y == 4.0f && cross.position.z == 0.0f);
    STEP(cross.roads[0] == 0 || cross.roads[1] == 0 || cross.roads[2] == 0 || cross.roads[3] == 0);
    STEP(cross.light[0] == 0 && cross.light[1] == 1 && cross.light[2] == 2 && cross.light[3] == 0);
    /* Errors leave the output unchanged. */
    saved_road = road;
    STEP(ai->road(owner, 1, &road) == SV_NOT_FOUND && !memcmp(&road, &saved_road, sizeof road));
    STEP(ai->road(owner, UINT32_MAX, &road) == SV_NOT_FOUND && !memcmp(&road, &saved_road, sizeof road));
    saved_cross = cross;
    STEP(ai->intersection(owner, 2, &cross) == SV_NOT_FOUND && !memcmp(&cross, &saved_cross, sizeof cross));
    STEP(ai->road(owner, 0, NULL) == SV_INVALID);
    STEP(ai->intersection(owner, 0, NULL) == SV_INVALID);
    road.size = sizeof road - 1;
    saved_road = road;
    STEP(ai->road(owner, 0, &road) == SV_INVALID && !memcmp(&road, &saved_road, sizeof road));
    cross.size = 0;
    saved_cross = cross;
    STEP(ai->intersection(owner, 0, &cross) == SV_INVALID && !memcmp(&cross, &saved_cross, sizeof cross));
    STEP(ai->road(0, 0, &road) == SV_STATE);
    return 0;
}

static int trigger_rows(SvOwner owner)
{
    SvValue value = {0};
    SvItemState item = {sizeof item};
    STEP(props->find(owner, "item.trigger_ticks", &trigger) == SV_OK);
    value.i = -1;
    STEP(props->set(owner, trigger, gun, 0, value) == SV_INVALID);
    value.i = 3601;
    STEP(props->set(owner, trigger, gun, 0, value) == SV_INVALID);
    value.i = 1;
    STEP(props->set(owner, trigger, gun, 1, value) == SV_INVALID);
    STEP(props->add(owner, trigger, gun, 0, value) == SV_DENIED);
    STEP(props->set(owner, trigger, bot, 0, value) == SV_NOT_FOUND);
    STEP(props->set(owner, trigger, gun ^ ((uint64_t)1 << 32), 0, value) == SV_NOT_FOUND);
    STEP(props->set(owner, trigger, gun, 0, value) == SV_OK);
    STEP(game->item(owner, gun, &item) == SV_OK && item.trigger_ticks == 1);
    value.i = 0;
    STEP(props->set(owner, trigger, gun, 0, value) == SV_OK);
    STEP(props->get(owner, trigger, gun, 0, &value) == SV_OK && value.i == 0);
    return 0;
}

static int snap_errors(SvOwner owner)
{
    STEP(ai->aim_snap(owner, bot ^ ((uint64_t)1 << 32), 1) == SV_NOT_FOUND);
    STEP(ai->aim_snap(owner, normal, 1) == SV_DENIED);
    STEP(ai->aim_snap(owner, bot, -1) == SV_INVALID);
    STEP(ai->aim_snap(owner, bot, 2) == SV_INVALID);
    STEP(ai->aim_snap(0, bot, 1) == SV_STATE);
    return 0;
}

static DWORD WINAPI wrong_thread(void *argument)
{
    SvOwner owner = *(SvOwner *)argument;
    SvAiRoad road = {sizeof road};
    SvAiIntersection cross = {sizeof cross};
    SvValue value = {0};
    STEP(ai->road(owner, 0, &road) == SV_STATE);
    STEP(ai->intersection(owner, 0, &cross) == SV_STATE);
    STEP(ai->aim_snap(owner, bot, 1) == SV_STATE);
    STEP(props->set(owner, trigger, gun, 0, value) == SV_STATE);
    return 0;
}

static int setup(SvOwner owner, const char *text)
{
    const void *table;
    SvEntity list[256];
    uint32_t count, next, i;
    HANDLE thread;
    DWORD code;
    SvResult result = host->get_api(owner, SV_API_AI, 1, &table);
    STEP(result == (!strcmp(text, "missing") ? SV_DENIED : SV_OK));
    ai = table;
    STEP(host->get_api(owner, SV_API_GAME_PROPS, 1, &table) == SV_OK);
    props = table;
    STEP(host->get_api(owner, SV_API_GAME_READ, 1, &table) == SV_OK);
    game = table;
    STEP(game->entities(owner, SV_ENTITY_PLAYER, 0, list, 256, &count, &next) == SV_OK);
    for (i = 0; i < count; ++i) {
        SvPlayerState p = {sizeof p};
        STEP(game->player(owner, list[i], &p) == SV_OK);
        if (!strcmp(p.name, "Aim")) bot = list[i];
        if (!strcmp(p.name, "Systems test")) {
            normal = list[i];
            target = p.human;
        }
    }
    STEP(game->entities(owner, SV_ENTITY_ITEM, 0, list, 256, &count, &next) == SV_OK);
    for (i = 0; i < count; ++i) {
        SvItemState item = {sizeof item};
        STEP(game->item(owner, list[i], &item) == SV_OK);
        if (item.type == 10) gun = list[i];
    }
    if (!strcmp(text, "missing")) return 0;
    STEP(ai->size >= sizeof *ai && bot && normal && target && gun);
    {
        int line = road_reads(owner);
        if (!line) line = trigger_rows(owner);
        if (!line) line = snap_errors(owner);
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

static int run(SvOwner owner, const char *text)
{
    if (!normal) {
        int line = setup(owner, text);
        if (line || !strcmp(text, "missing")) return line;
    }
    if (!strcmp(text, "snap-on")) {
        STEP(ai->target(owner, bot, target) == SV_OK);
        STEP(ai->aim_snap(owner, bot, 1) == SV_OK);
    } else if (!strcmp(text, "snap-off")) {
        STEP(ai->aim_snap(owner, bot, 0) == SV_OK);
    } else if (!strcmp(text, "trigger-zero")) {
        SvValue value = {0};
        STEP(props->set(owner, trigger, gun, 0, value) == SV_OK);
    } else if (!strcmp(text, "bad-graph")) {
        SvAiRoad road = {sizeof road}, saved = road;
        SvAiIntersection cross = {sizeof cross}, saved_cross = cross;
        STEP(ai->road(owner, 0, &road) == SV_FAILED && !memcmp(&road, &saved, sizeof road));
        STEP(ai->intersection(owner, 0, &cross) == SV_FAILED && !memcmp(&cross, &saved_cross, sizeof cross));
    }
    return 0;
}

static SvResult SV_CALL command(SvOwner owner, uint64_t actor, const char *text)
{
    int line = run(owner, text);
    char message[96];
    (void)actor;
    if (line) {
        snprintf(message, sizeof message, "N roads test failed at line %d.", line);
        host->log(owner, message);
        return SV_FAILED;
    }
    return systems_godmode(props, owner, normal, 1);
}

static SvResult SV_CALL enable(const SvHost *api, SvOwner owner)
{
    host = api;
    return host->command(owner, "nroads", SV_COMMAND_LOCAL, command);
}

SV_EXPORT SvResult SV_CALL sv_plugin_entry(uint32_t abi, SvPlugin *plugin)
{
    if (abi != SV_PLUGIN_ABI || !plugin || plugin->size < sizeof *plugin)
        return SV_INCOMPATIBLE;
    memset(plugin, 0, sizeof *plugin);
    plugin->size = sizeof *plugin;
    plugin->abi = abi;
    strcpy(plugin->id, "test.nroads");
    strcpy(plugin->version, "0.1.0");
    plugin->flags = SV_PLUGIN_LIVE;
    plugin->enable = enable;
    return SV_OK;
}
