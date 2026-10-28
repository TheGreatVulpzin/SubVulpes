#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include "systems_plugin.h"
#include "sv_game_ai.h"
#include "sv_game_props.h"

typedef struct {
    const SvGameAi *api;
    SvOwner owner;
    SvEntity bot;
} ThreadArgs;

static const SvGameAi *control_api;
static const SvGameProps *control_write;
static SvEntity control_bot, control_human, control_policy, control_driver, control_shooter, control_player;
static const SvAiStreetPoint street[4] = {{0, 0, 0}, {1, 0, 0}, {2, 1, 0}, {3, 1, 0}};

static SvResult run_command(SvOwner owner, uint64_t actor, const char *arguments)
{
    SvAiInput input = {sizeof input, 8, {0, 0, 0, 1}, 0, 0};
    (void)actor;
    if (!strcmp(arguments, "release"))
        return control_api->release(owner, control_bot);
    if (!strcmp(arguments, "renew")) {
        STEP(control_api->target(owner, control_bot, control_human) == SV_OK);
        return control_api->override_input(owner, control_bot, &input, 2);
    }
    if (!strcmp(arguments, "policy-off")) {
        STEP(control_api->perception(owner, control_policy, 2) == SV_OK);
        STEP(control_api->aim_mode(owner, control_policy, 0) == SV_OK);
        return control_api->shot_chance(owner, control_policy, 0);
    }
    if (!strcmp(arguments, "policy-half"))
        return control_api->shot_chance(owner, control_policy, 50);
    if (!strcmp(arguments, "route"))
        return control_api->street_route(owner, control_driver, street, 4);
    if (!strcmp(arguments, "bad-route")) {
        STEP(control_api->street_route(owner, control_driver, street, 4) == SV_INVALID);
        return SV_OK;
    }
    if (!strcmp(arguments, "bad-road-index")) {
        SvAiStreetPoint copy[4];
        memcpy(copy, street, sizeof copy);
        copy[0].road = 4;
        STEP(control_api->street_route(owner, control_driver, copy, 4) == SV_INVALID);
        return SV_OK;
    }
    if (!strcmp(arguments, "bad-seat")) {
        STEP(control_api->seated_target(owner, control_shooter, control_player) == SV_STATE);
        return SV_OK;
    }
    return SV_INVALID;
}

static SvResult SV_CALL control_command(SvOwner owner, uint64_t actor, const char *arguments)
{
    SvResult result = run_command(owner, actor, arguments);
    /* Command dispatch handles errors without failing the module. Publish the result for the host check. */
    STEP(systems_godmode(control_write, owner, control_policy, result == SV_OK) == SV_OK);
    return result;
}

static DWORD WINAPI wrong_thread(void *argument)
{
    ThreadArgs *a = argument;
    SvAiThreats out = {sizeof out};
    SvAiInput input = {sizeof input};
    STEP(a->api->threats(a->owner, a->bot, &out) == SV_STATE);
    STEP(a->api->destination(a->owner, a->bot, (SvVector){0}) == SV_STATE);
    STEP(a->api->waypoints(a->owner, a->bot, NULL, 0) == SV_STATE);
    STEP(a->api->target(a->owner, a->bot, 0) == SV_STATE);
    STEP(a->api->override_input(a->owner, a->bot, &input, 1) == SV_STATE);
    STEP(a->api->release(a->owner, a->bot) == SV_STATE);
    STEP(a->api->perception(a->owner, a->bot, 1) == SV_STATE);
    STEP(a->api->aim_mode(a->owner, a->bot, 1) == SV_STATE);
    STEP(a->api->shot_chance(a->owner, a->bot, 100) == SV_STATE);
    STEP(a->api->street_route(a->owner, a->bot, street, 4) == SV_STATE);
    STEP(a->api->seated_target(a->owner, a->bot, 0) == SV_STATE);
    return 0;
}

int ai_fixture(const SvHost *host, SvOwner owner)
{
    const void *table = NULL;
    const SvGameAi *api;
    const SvGameRead *read;
    SvEntity entities[256], bot = 0, malformed = 0, vehicle = 0, empty = 0, normal = 0, human = 0;
    SvEntity policy = 0, driver = 0, shooter = 0, bad_body = 0;
    uint32_t count, next, i;
    SvPlayerState player = {sizeof player};
    SvAiThreats out = {sizeof out}, saved;
    SvAiInput input = {sizeof input};
    SvVector points[65] = {{0}};
    SvAiStreetPoint roads[68];
    ThreadArgs args;
    HANDLE thread;
    DWORD code;
    STEP(host->get_api(owner, SV_API_AI, SV_GAME_AI_VERSION, &table) == SV_OK);
    api = (const SvGameAi *)table;
    STEP(api && api->size >= sizeof *api && api->version == SV_GAME_AI_VERSION);
    STEP(host->get_api(owner, SV_API_GAME_READ, SV_GAME_READ_VERSION, &table) == SV_OK);
    read = (const SvGameRead *)table;
    STEP(read->entities(owner, SV_ENTITY_PLAYER, 0, entities, 256, &count, &next) == SV_OK);
    for (i = 0; i < count; ++i) {
        STEP(read->player(owner, entities[i], &player) == SV_OK);
        if (!strcmp(player.name, "AI bot")) bot = entities[i];
        if (!strcmp(player.name, "AI malformed")) malformed = entities[i];
        if (!strcmp(player.name, "AI vehicle")) vehicle = entities[i];
        if (!strcmp(player.name, "AI empty")) empty = entities[i];
        if (!strcmp(player.name, "AI policy")) policy = entities[i];
        if (!strcmp(player.name, "AI driver")) driver = entities[i];
        if (!strcmp(player.name, "AI shooter")) shooter = entities[i];
        if (!strcmp(player.name, "AI bad body")) bad_body = entities[i];
        if (!strcmp(player.name, "Systems test")) { normal = entities[i]; human = player.human; }
    }
    STEP(bot && malformed && vehicle && empty && normal && human && policy && driver && shooter && bad_body);
    args = (ThreadArgs){api, owner, bot};
    thread = CreateThread(NULL, 0, wrong_thread, &args, 0, NULL);
    STEP(thread != NULL);
    STEP(WaitForSingleObject(thread, 5000) == WAIT_OBJECT_0);
    STEP(GetExitCodeThread(thread, &code));
    CloseHandle(thread);
    STEP(code == 0);
    STEP(api->threats(0, bot, &out) == SV_STATE);
    STEP(api->destination(0, bot, points[0]) == SV_STATE);
    STEP(api->waypoints(0, bot, points, 1) == SV_STATE);
    STEP(api->target(0, bot, human) == SV_STATE);
    STEP(api->override_input(0, bot, &input, 1) == SV_STATE);
    STEP(api->release(0, bot) == SV_STATE);
    STEP(api->perception(0, bot, 1) == SV_STATE);
    STEP(api->aim_mode(0, bot, 1) == SV_STATE);
    STEP(api->shot_chance(0, bot, 100) == SV_STATE);
    STEP(api->street_route(0, driver, street, 4) == SV_STATE);
    STEP(api->seated_target(0, shooter, normal) == SV_STATE);
    STEP(api->threats(owner, human, &out) == SV_NOT_FOUND);
    STEP(api->destination(owner, human, points[0]) == SV_NOT_FOUND);
    STEP(api->waypoints(owner, human, points, 1) == SV_NOT_FOUND);
    STEP(api->target(owner, human, human) == SV_NOT_FOUND);
    STEP(api->override_input(owner, human, &input, 1) == SV_NOT_FOUND);
    STEP(api->release(owner, human) == SV_NOT_FOUND);
    STEP(api->perception(owner, human, 1) == SV_NOT_FOUND);
    STEP(api->aim_mode(owner, human, 1) == SV_NOT_FOUND);
    STEP(api->shot_chance(owner, human, 100) == SV_NOT_FOUND);
    STEP(api->street_route(owner, human, street, 4) == SV_NOT_FOUND);
    STEP(api->seated_target(owner, human, normal) == SV_NOT_FOUND);
    STEP(api->threats(owner, normal, &out) == SV_DENIED);
    STEP(api->destination(owner, normal, points[0]) == SV_DENIED);
    STEP(api->waypoints(owner, normal, points, 1) == SV_DENIED);
    STEP(api->target(owner, normal, human) == SV_DENIED);
    STEP(api->override_input(owner, normal, &input, 1) == SV_DENIED);
    STEP(api->release(owner, normal) == SV_DENIED);
    STEP(api->perception(owner, normal, 1) == SV_DENIED);
    STEP(api->aim_mode(owner, normal, 1) == SV_DENIED);
    STEP(api->shot_chance(owner, normal, 100) == SV_DENIED);
    STEP(api->street_route(owner, normal, street, 4) == SV_DENIED);
    STEP(api->seated_target(owner, normal, bot) == SV_DENIED);
    STEP(api->perception(owner, policy, 0) == SV_INVALID);
    STEP(api->perception(owner, policy, 3) == SV_INVALID);
    STEP(api->perception(owner, policy, UINT32_MAX) == SV_INVALID);
    STEP(api->aim_mode(owner, policy, -1) == SV_INVALID);
    STEP(api->aim_mode(owner, policy, 2) == SV_INVALID);
    STEP(api->shot_chance(owner, policy, 101) == SV_INVALID);
    STEP(api->shot_chance(owner, policy, UINT32_MAX) == SV_INVALID);
    STEP(api->perception(owner, empty, 1) == SV_STATE);
    STEP(api->perception(owner, bad_body, 1) == SV_STATE);
    STEP(api->destination(owner, bad_body, points[0]) == SV_STATE);
    STEP(api->aim_mode(owner, empty, 1) == SV_STATE);
    STEP(api->shot_chance(owner, empty, 100) == SV_STATE);
    STEP(api->perception(owner, policy, 1) == SV_OK);
    STEP(api->aim_mode(owner, policy, 1) == SV_OK);
    STEP(api->shot_chance(owner, policy, 100) == SV_OK);
    STEP(api->street_route(owner, driver, NULL, 4) == SV_INVALID);
    STEP(api->street_route(owner, driver, street, 1) == SV_INVALID);
    for (i = 0; i < 68; ++i) roads[i] = street[i % 4];
    STEP(api->street_route(owner, driver, roads, 68) == SV_INVALID);
    roads[0].road = 512;
    STEP(api->street_route(owner, driver, roads, 4) == SV_INVALID);
    roads[0] = street[0];
    roads[0].direction = 2;
    STEP(api->street_route(owner, driver, roads, 4) == SV_INVALID);
    roads[0] = street[0];
    roads[0].lane = 2;
    STEP(api->street_route(owner, driver, roads, 4) == SV_INVALID);
    roads[0] = street[2];
    STEP(api->street_route(owner, driver, roads, 4) == SV_INVALID);
    STEP(api->street_route(owner, bot, street, 4) == SV_STATE);
    STEP(api->street_route(owner, shooter, street, 4) == SV_STATE);
    STEP(api->street_route(owner, driver, street, 4) == SV_OK);
    STEP(api->seated_target(owner, shooter, human) == SV_NOT_FOUND);
    STEP(api->seated_target(owner, shooter, shooter) == SV_INVALID);
    STEP(api->seated_target(owner, bot, normal) == SV_STATE);
    STEP(api->seated_target(owner, driver, normal) == SV_STATE);
    STEP(api->seated_target(owner, shooter, empty) == SV_STATE);
    STEP(api->perception(owner, shooter, 1) == SV_OK);
    STEP(api->seated_target(owner, shooter, normal) == SV_OK);
    STEP(api->threats(owner, bot, NULL) == SV_INVALID);
    out.size = 0;
    saved = out;
    STEP(api->threats(owner, bot, &out) == SV_INVALID);
    STEP(!memcmp(&out, &saved, sizeof out));
    out.size = sizeof out;
    saved = out;
    STEP(api->threats(owner, malformed, &out) == SV_FAILED);
    STEP(!memcmp(&out, &saved, sizeof out));
    STEP(api->threats(owner, bot, &out) == SV_OK);
    STEP(out.item_size == sizeof(SvAiThreat));
    STEP(out.count == 1 && out.threats[0].human == human && out.threats[0].level == 9);
    STEP(api->waypoints(owner, bot, points, 65) == SV_INVALID);
    STEP(api->waypoints(owner, bot, NULL, 1) == SV_INVALID);
    points[1].x = 32769;
    STEP(api->waypoints(owner, bot, points, 2) == SV_INVALID);
    STEP(api->destination(owner, bot, points[1]) == SV_INVALID);
    STEP(api->destination(owner, vehicle, points[0]) == SV_CONFLICT);
    STEP(api->destination(owner, empty, points[0]) == SV_STATE);
    STEP(api->waypoints(owner, bot, NULL, 0) == SV_OK);
    STEP(api->destination(owner, bot, (SvVector){4, 5, 6}) == SV_OK);
    STEP(api->target(owner, bot, bot) == SV_NOT_FOUND);
    STEP(read->player(owner, bot, &player) == SV_OK);
    STEP(api->target(owner, bot, player.human) == SV_INVALID);
    STEP(api->target(owner, vehicle, human) == SV_CONFLICT);
    STEP(api->target(owner, empty, human) == SV_STATE);
    STEP(api->target(owner, bot, human) == SV_OK);
    STEP(api->release(owner, bot) == SV_OK);
    STEP(api->override_input(owner, bot, NULL, 1) == SV_INVALID);
    input.size = 0;
    STEP(api->override_input(owner, bot, &input, 1) == SV_INVALID);
    input.size = sizeof input;
    STEP(api->override_input(owner, bot, &input, 0) == SV_INVALID);
    STEP(api->override_input(owner, bot, &input, 3601) == SV_INVALID);
    input.axes[3] = 2;
    STEP(api->override_input(owner, bot, &input, 1) == SV_INVALID);
    input.axes[3] = 1;
    input.buttons = 0x80000u;
    STEP(api->override_input(owner, bot, &input, 1) == SV_INVALID);
    input.buttons = 8;
    STEP(api->override_input(owner, vehicle, &input, 1) == SV_CONFLICT);
    STEP(api->override_input(owner, empty, &input, 1) == SV_STATE);
    STEP(api->override_input(owner, bot, &input, 1) == SV_OK);
    STEP(api->release(owner, bot) == SV_OK);
    STEP(api->release(owner, bot) == SV_OK);
    for (i = 0; i < 64; ++i) points[i] = (SvVector){(float)i, 20, 30};
    STEP(api->waypoints(owner, bot, points, 64) == SV_OK);
    STEP(api->target(owner, bot, human) == SV_OK);
    STEP(api->override_input(owner, bot, &input, 2) == SV_OK);
    control_api = api;
    control_bot = bot;
    control_human = human;
    control_policy = policy;
    control_driver = driver;
    control_shooter = shooter;
    control_player = normal;
    STEP(host->get_api(owner, SV_API_GAME_PROPS, SV_GAME_PROPS_VERSION, &table) == SV_OK);
    control_write = (const SvGameProps *)table;
    STEP(host->command(owner, "ai.control", SV_COMMAND_LOCAL, control_command) == SV_OK);
    return 0;
}
