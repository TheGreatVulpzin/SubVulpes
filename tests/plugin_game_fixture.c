#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdio.h>
#include <string.h>
#include "sv_game_read.h"
#include "sv_game_write.h"
#include "sv_game_props.h"
#include "sv_game_events.h"

static const SvHost *host;
static const SvGameRead *game;
static const SvGameWrite *change;
static const SvGameProps *props;
static uint32_t pid_godmode, pid_admin, pid_team, pid_health, pid_bleeding, pid_stun, pid_protect, pid_bone_velocity,
    pid_object_velocity;
static SvOwner plugin_owner;
static SvEntity player, human, item, object, account;
static uint32_t stage;
static uint64_t reset_sequence;
static int runtime_probe;
/* What the fixture saw of the game events. */
static uint32_t seen_spawn, seen_death, seen_leave, seen_lost, seen_other, spawn_before_lost, seen_n, seen_ticks[8];
static int ref_ok[8];
static uint32_t seen_chat, seen_chat_denied, seen_join;
static char last_chat[64];
static int chat_player_ok;
/* What the fixture saw of the damage decisions: the amounts that the test program sends, in order, and the checks that run
 * inside the callback. */
static uint32_t damage_log_n, damage_log_source[8], damage_log_amount[8];
static int damage_refs_ok = 1, damage_guard_ok;

#define CHECK(test) do { if (!(test)) { \
    char text[160]; \
    snprintf(text, sizeof text, "Read API fixture failed at line %d.", __LINE__); \
    host->log(owner, text); return SV_FAILED; \
} } while (0)

static DWORD WINAPI wrong_thread(void *argument)
{
    SvWorldState state = { sizeof state };
    const void *table = NULL;
    (void)argument;
    return game->world(plugin_owner, &state) == SV_STATE &&
        host->get_api(plugin_owner, SV_API_GAME_READ, SV_GAME_READ_VERSION, &table) == SV_STATE ? 0 : 1;
}

/* A chat command logs its name and text. The actor is never zero, since zero is the local operator. */
static SvResult report(SvOwner owner, const char *name, uint64_t actor, const char *arguments)
{
    char text[128];
    if (!actor)
        return SV_DENIED;
    snprintf(text, sizeof text, "COMMAND %s %s", name, arguments);
    return host->log(owner, text);
}

static SvResult SV_CALL echo_command(SvOwner owner, uint64_t actor, const char *arguments)
{
    return report(owner, "echo", actor, arguments);
}

static SvResult SV_CALL secret_command(SvOwner owner, uint64_t actor, const char *arguments)
{
    return report(owner, "secret", actor, arguments);
}

static SvResult SV_CALL localonly_command(SvOwner owner, uint64_t actor, const char *arguments)
{
    return report(owner, "localonly", actor, arguments);
}

/* The test program leaves too few bonds for a human, then runs this command. A bot must not stay behind. */
static SvResult SV_CALL npclimit_command(SvOwner owner, uint64_t actor, const char *arguments)
{
    SvEntity page[SV_ENTITY_PAGE_MAX], none_player = 0, none_human = 0;
    const SvVector at = { 10.0f, 20.0f, 30.0f };
    uint32_t before = 0, after = 0, next = 0;
    (void)actor;
    (void)arguments;
    if (game->entities(owner, SV_ENTITY_PLAYER, 0, page, SV_ENTITY_PAGE_MAX, &before, &next) != SV_OK ||
        change->npc_create(owner, at, 0.0f, &none_player, &none_human) != SV_LIMIT || none_player || none_human ||
        game->entities(owner, SV_ENTITY_PLAYER, 0, page, SV_ENTITY_PAGE_MAX, &after, &next) != SV_OK || before != after)
        return host->log(owner, "NPC_LIMIT_FAILURE");
    return host->log(owner, "NPC_LIMIT_PASS");
}

/* The test program sets a mark in the shared hit record of the game, then runs this command. The queries must put it back. */
static SvResult SV_CALL linemark_command(SvOwner owner, uint64_t actor, const char *arguments)
{
    const SvVector from = { 600.0f, 10.0f, 600.0f }, to = { 600.0f, -10.0f, 600.0f };
    SvLineHit hit = { sizeof hit };
    (void)actor;
    (void)arguments;
    if (game->line(owner, from, to, SV_LINE_ALL, 0, &hit) != SV_OK || hit.kind != SV_HIT_LEVEL ||
        game->aim(owner, human, 10.0f, SV_LINE_ALL, &hit) != SV_OK)
        return host->log(owner, "LINE_MARK_FAILURE");
    return host->log(owner, "LINE_MARK_PASS");
}

/* The ids of the properties. The first call that can find them runs in a tick, when the world is ready. */
static int find_props(SvOwner owner)
{
    return props->find(owner, "player.godmode", &pid_godmode) == SV_OK && props->find(owner, "player.admin", &pid_admin) == SV_OK &&
        props->find(owner, "player.team", &pid_team) == SV_OK && props->find(owner, "human.health", &pid_health) == SV_OK &&
        props->find(owner, "human.bleeding", &pid_bleeding) == SV_OK && props->find(owner, "human.stun", &pid_stun) == SV_OK &&
        props->find(owner, "human.protect", &pid_protect) == SV_OK &&
        props->find(owner, "human.bone_velocity", &pid_bone_velocity) == SV_OK &&
        props->find(owner, "object.velocity", &pid_object_velocity) == SV_OK;
}

static SvResult put(SvOwner owner, uint32_t id, SvEntity entity, int32_t value)
{
    SvValue v = { 0 };
    v.i = value;
    return props->set(owner, id, entity, 0, v);
}

static SvResult push_to(SvOwner owner, uint32_t id, SvEntity entity, uint32_t slot, SvVector change_by)
{
    SvValue v;
    v.v = change_by;
    return props->add(owner, id, entity, slot, v);
}

static int get_int(SvOwner owner, uint32_t id, SvEntity entity, int32_t *value)
{
    SvValue v;
    if (props->get(owner, id, entity, 0, &v) != SV_OK)
        return 0;
    *value = v.i;
    return 1;
}

static DWORD WINAPI write_wrong_thread(void *argument)
{
    (void)argument;
    return change->message(plugin_owner, SV_MESSAGE_SERVER, "wrong thread") == SV_STATE &&
        put(plugin_owner, pid_health, human, 50) == SV_STATE ? 0 : 1;
}

static SvEntity first(SvOwner owner, uint32_t kind)
{
    SvEntity entity = 0;
    uint32_t count = 0, next = 0;
    return game->entities(owner, kind, 0, &entity, 1, &count, &next) == SV_OK && count == 1 ? entity : 0;
}

static SvResult initial_reads(SvOwner owner)
{
    SvPlayerState p = { sizeof p };
    SvHumanState h = { sizeof h };
    SvItemState it = { sizeof it };
    SvObjectState o = { sizeof o };
    SvAccountState a = { sizeof a };
    SvBoneState bone = { sizeof bone };
    SvInventoryState inventory = { sizeof inventory };
    SvWorldState world = { sizeof world };
    char news[SV_HEADLINE_BUFFER];
    struct { SvPlayerState state; uint32_t tail[4]; } larger;
    SvEntity page[2] = {0, 0};
    uint32_t count = 777, next = 888;
    HANDLE thread;
    DWORD code;
    player = first(owner, SV_ENTITY_PLAYER); human = first(owner, SV_ENTITY_HUMAN);
    item = first(owner, SV_ENTITY_ITEM); object = first(owner, SV_ENTITY_OBJECT);
    account = first(owner, SV_ENTITY_ACCOUNT);
    CHECK(player && human && item && object && account);
    CHECK(game->player(owner, player, &p) == SV_OK);
    CHECK(p.money == 4242 && p.name[30] == 'P' && p.name[31] == 0);
    CHECK(p.human == human && p.account == account);
    memset(&larger, 0xa5, sizeof larger);
    larger.state.size = sizeof larger;
    CHECK(game->player(owner, player, &larger.state) == SV_OK);
    CHECK(larger.tail[0] == 0xa5a5a5a5u && larger.tail[3] == 0xa5a5a5a5u);
    p.money = -100;
    CHECK(game->player(owner, player, &p) == SV_OK && p.money == 4242);
    CHECK(game->human(owner, human, &h) == SV_OK && h.health == 37);
    CHECK(h.player == player && h.account == account && h.vehicle == object);
    CHECK(game->item(owner, item, &it) == SV_OK && it.parent_human == human);
    CHECK(game->object(owner, object, &o) == SV_OK && o.driver == player);
    CHECK(game->account(owner, account, &a) == SV_OK && a.money == 1000);
    CHECK(game->bone(owner, human, 14, &bone) == SV_OK && bone.bone == 14);
    CHECK(game->bone(owner, human, 15, &bone) == SV_INVALID);
    CHECK(game->inventory(owner, human, 0, &inventory) == SV_OK);
    CHECK(inventory.count == 1 && inventory.items[0] == item && inventory.items[1] == 0);
    CHECK(game->inventory(owner, human, 8, &inventory) == SV_INVALID);
    CHECK(game->world(owner, &world) == SV_OK && world.world_clock_raw == 123456);
    CHECK(world.max_players == 128 && world.item_type_count == 27 && world.object_type_count == 14);
    /* The test program put three news lines in the game: "first", "second" and "third". */
    CHECK(world.headline_count == 3);
    CHECK(game->headline(owner, 0, news, SV_HEADLINE_BUFFER) == SV_OK && !strcmp(news, "first"));
    CHECK(game->headline(owner, 2, news, SV_HEADLINE_BUFFER) == SV_OK && !strcmp(news, "third"));
    CHECK(game->headline(owner, 3, news, SV_HEADLINE_BUFFER) == SV_NOT_FOUND && game->headline(owner, 1024, news, SV_HEADLINE_BUFFER) == SV_NOT_FOUND);
    CHECK(game->headline(owner, UINT32_MAX, news, SV_HEADLINE_BUFFER) == SV_NOT_FOUND);
    CHECK(game->headline(owner, 0, news, SV_HEADLINE_BUFFER - 1) == SV_INVALID && game->headline(owner, 0, NULL, SV_HEADLINE_BUFFER) == SV_INVALID);
    CHECK(game->headline(owner + 1, 0, news, SV_HEADLINE_BUFFER) == SV_STATE);
    reset_sequence = world.reset_sequence;
    CHECK(game->entities(owner, SV_ENTITY_PLAYER, 0, page, 2, &count, &next) == SV_OK);
    CHECK(count == 1 && page[0] == player && next == 256);
    CHECK(game->entities(owner, SV_ENTITY_PLAYER, next, page, 2, &count, &next) == SV_OK && count == 0);
    count = 777; next = 888;
    CHECK(game->entities(owner, 0, 0, page, 2, &count, &next) == SV_INVALID);
    CHECK(count == 777 && next == 888);
    CHECK(game->entities(owner, SV_ENTITY_PLAYER, 257, page, 2, &count, &next) == SV_INVALID);
    CHECK(game->entities(owner, SV_ENTITY_PLAYER, 0, page, 257, &count, &next) == SV_INVALID);
    CHECK(game->entities(owner, SV_ENTITY_PLAYER, 0, NULL, 1, &count, &next) == SV_INVALID);
    CHECK(game->player(owner + 1, player, &p) == SV_STATE);
    CHECK(game->player(owner, human, &p) == SV_NOT_FOUND);
    CHECK(game->player(owner, 0, &p) == SV_NOT_FOUND);
    CHECK(game->player(owner, player, NULL) == SV_INVALID);
    p.size = sizeof p - 1; p.money = -789;
    CHECK(game->player(owner, player, &p) == SV_INVALID && p.money == -789);
    thread = CreateThread(NULL, 0, wrong_thread, NULL, 0, NULL);
    CHECK(thread != NULL);
    CHECK(WaitForSingleObject(thread, 5000) == WAIT_OBJECT_0);
    CHECK(GetExitCodeThread(thread, &code));
    CloseHandle(thread);
    CHECK(code == 0);
    /* The human sits in a vehicle in this tick, so a teleport must refuse and change nothing. */
    CHECK(change->teleport(owner, human, (SvVector){ 1.0f, 2.0f, 3.0f }) == SV_STATE);
    CHECK(game->human(owner, human, &h) == SV_OK && h.position.x != 1.0f);
    return SV_OK;
}

/* The admin flag, the team, the looks and the private message. Returns 1 when all checks pass. */
static int change_player_fields(SvOwner owner)
{
    static const int32_t highest[SV_LOOK_COUNT] = { 2, 7, 3, 1, 1 };
    const int32_t looks[SV_LOOK_COUNT] = { 2, 5, 3, 1, 1 };
    int32_t bad[SV_LOOK_COUNT];
    SvPlayerState ps = { sizeof ps };
    SvHumanState hs = { sizeof hs };
    SvEntity page[2] = { 0, 0 };
    uint32_t count = 0, next = 0, i;
    if (game->entities(owner, SV_ENTITY_PLAYER, 0, page, 2, &count, &next) != SV_OK || count != 2 || page[0] != player)
        return 0;
    if (put(owner, pid_admin, player, 2) != SV_INVALID || put(owner, pid_admin, page[1], 1) != SV_INVALID ||
        put(owner, pid_admin, human, 1) != SV_NOT_FOUND || put(owner, pid_admin, player, 1) != SV_OK ||
        game->player(owner, player, &ps) != SV_OK || ps.is_admin != 1)
        return 0;
    if (put(owner, pid_team, player, 8) != SV_INVALID || put(owner, pid_team, player, -1) != SV_INVALID ||
        put(owner, pid_team, human, 2) != SV_NOT_FOUND || put(owner, pid_team, player, 3) != SV_OK ||
        game->player(owner, player, &ps) != SV_OK || ps.team != 3 || game->human(owner, human, &hs) != SV_OK ||
        hs.team != 3)
        return 0;
    if (change->set_looks(owner, player, NULL) != SV_INVALID || change->set_looks(owner, human, looks) != SV_NOT_FOUND)
        return 0;
    for (i = 0; i < SV_LOOK_COUNT; ++i) {
        memcpy(bad, looks, sizeof bad);
        bad[i] = highest[i] + 1;
        if (change->set_looks(owner, player, bad) != SV_INVALID)
            return 0;
        bad[i] = -1;
        if (change->set_looks(owner, player, bad) != SV_INVALID)
            return 0;
    }
    if (change->set_looks(owner, player, looks) != SV_OK || game->player(owner, player, &ps) != SV_OK ||
        game->human(owner, human, &hs) != SV_OK)
        return 0;
    for (i = 0; i < SV_LOOK_COUNT; ++i)
        if (ps.looks[i] != looks[i] || hs.looks[i] != looks[i])
            return 0;
    return change->message_to(owner, 1, player, "kind") == SV_INVALID &&
        change->message_to(owner, SV_MESSAGE_SERVER, human, "target") == SV_NOT_FOUND &&
        change->message_to(owner, SV_MESSAGE_SERVER, player, "") == SV_INVALID &&
        change->message_to(owner, SV_MESSAGE_SERVER, player, "private") == SV_OK;
}

/* Reads one item, or returns 0. */
static int read_item(SvOwner owner, SvEntity entity, SvItemState *state)
{
    state->size = sizeof *state;
    return game->item(owner, entity, state) == SV_OK;
}

/* Make, equip, attach, detach and remove items. Returns 1 when all checks pass. */
static int change_items(SvOwner owner)
{
    const SvVector at = { 12.0f, 20.0f, 30.0f };
    SvVector bad = at;
    SvItemState state;
    SvEntity gun = 0, gun2 = 0, mag = 0, bandage = 0, none = 0;
    uint32_t nan_bits = 0x7fc00000u;
    memcpy(&bad.x, &nan_bits, sizeof bad.x);
    if (change->item_create(owner, SV_ITEM_TYPE_COUNT, at, &none) != SV_INVALID ||
        change->item_create(owner, 26, at, &none) != SV_INVALID || change->item_create(owner, 0, at, NULL) != SV_INVALID ||
        change->item_create(owner, 0, bad, &none) != SV_INVALID || none != 0)
        return 0;
    if (change->item_create(owner, 0, at, &gun) != SV_OK || change->item_create(owner, 0, at, &gun2) != SV_OK ||
        change->item_create(owner, 1, at, &mag) != SV_OK || change->item_create(owner, 12, at, &bandage) != SV_OK ||
        !gun || !gun2 || !mag || !bandage)
        return 0;
    if (!read_item(owner, bandage, &state) || state.type != 12 || state.position.x != 12.0f || state.parent_human != 0)
        return 0;
    if (change->item_equip(owner, bandage, human, SV_ITEM_SLOT_COUNT) != SV_INVALID ||
        change->item_equip(owner, bandage, player, 3) != SV_NOT_FOUND ||
        change->item_equip(owner, player, human, 3) != SV_NOT_FOUND || change->item_equip(owner, bandage, human, 3) != SV_OK)
        return 0;
    if (!read_item(owner, bandage, &state) || state.parent_human != human || state.parent_slot != 3)
        return 0;
    /* Slot 2 takes only guns, so the bandage stays in slot 3. */
    if (change->item_equip(owner, bandage, human, 2) != SV_CONFLICT || !read_item(owner, bandage, &state) ||
        state.parent_slot != 3 || change->item_equip(owner, gun, human, 0) != SV_OK)
        return 0;
    if (change->item_attach(owner, gun, gun) != SV_INVALID || change->item_attach(owner, human, mag) != SV_NOT_FOUND ||
        change->item_attach(owner, gun, mag) != SV_OK)
        return 0;
    if (!read_item(owner, gun, &state) || state.child_count != 1 || state.children[0] != mag ||
        !read_item(owner, mag, &state) || state.parent_item != gun)
        return 0;
    /* The magazine already sits in a gun. A second gun must refuse it, or it would have two parents. */
    if (change->item_attach(owner, gun2, mag) != SV_CONFLICT || change->item_attach(owner, gun, mag) != SV_CONFLICT)
        return 0;
    if (!read_item(owner, gun2, &state) || state.child_count != 0)
        return 0;
    if (change->item_detach(owner, mag) != SV_OK || !read_item(owner, gun, &state) || state.child_count != 0 ||
        change->item_detach(owner, mag) != SV_STATE)
        return 0;
    return change->item_remove(owner, bandage) == SV_OK && change->item_remove(owner, player) == SV_NOT_FOUND;
}

/* Make and remove objects. The train piece (type 12) comes from the test program. Returns 1 when all checks pass. */
static int change_objects(SvOwner owner)
{
    const SvVector at = { 40.0f, 21.0f, 60.0f };
    SvVector bad = at;
    SvObjectState state = { sizeof state };
    SvEntity made = 0, none = 0, train = 0, page[8];
    uint32_t nan_bits = 0x7fc00000u, count = 0, next = 0, i;
    float yaw_nan;
    memcpy(&bad.x, &nan_bits, sizeof bad.x);
    memcpy(&yaw_nan, &nan_bits, sizeof yaw_nan);
    if (change->object_create(owner, 12, at, 0.0f, 0, &none) != SV_INVALID ||
        change->object_create(owner, 13, at, 0.0f, 0, &none) != SV_INVALID ||
        change->object_create(owner, SV_OBJECT_TYPE_COUNT + 100, at, 0.0f, 0, &none) != SV_INVALID ||
        change->object_create(owner, 0, at, 0.0f, SV_OBJECT_COLOR_MAX + 1, &none) != SV_INVALID ||
        change->object_create(owner, 0, at, 0.0f, -1, &none) != SV_INVALID ||
        change->object_create(owner, 0, at, 0.0f, 0, NULL) != SV_INVALID ||
        change->object_create(owner, 0, bad, 0.0f, 0, &none) != SV_INVALID ||
        change->object_create(owner, 0, at, yaw_nan, 0, &none) != SV_INVALID || none != 0)
        return 0;
    if (change->object_create(owner, 0, at, 1.5f, 3, &made) != SV_OK || !made || game->object(owner, made, &state) != SV_OK ||
        state.type != 0 || state.color != 3 || state.health != 100 || state.position.x != 40.0f ||
        state.position.y != 21.0f || state.position.z != 60.0f)
        return 0;
    if (game->entities(owner, SV_ENTITY_OBJECT, 0, page, 8, &count, &next) != SV_OK)
        return 0;
    for (i = 0; i < count; ++i)
        if (game->object(owner, page[i], &state) == SV_OK && state.type == 12)
            train = page[i];
    if (!train || change->object_remove(owner, train) != SV_DENIED || change->object_remove(owner, player) != SV_NOT_FOUND)
        return 0;
    return change->object_remove(owner, made) == SV_OK;
}

/* Make and remove a bot. Returns 0 when all checks pass, or the line of the check that failed. */
#define STEP(condition) do { if (!(condition)) return __LINE__; } while (0)
static int change_npc(SvOwner owner)
{
    const SvVector at = { 70.0f, 22.0f, 80.0f };
    SvVector bad = at;
    SvPlayerState ps = { sizeof ps };
    SvHumanState hs = { sizeof hs };
    SvBoneState bs = { sizeof bs };
    SvEntity npc_player = 0, npc_human = 0, none = 0;
    uint32_t nan_bits = 0x7fc00000u;
    float yaw_nan;
    memcpy(&bad.x, &nan_bits, sizeof bad.x);
    memcpy(&yaw_nan, &nan_bits, sizeof yaw_nan);
    STEP(change->npc_create(owner, at, 0.0f, NULL, &none) == SV_INVALID);
    STEP(change->npc_create(owner, at, 0.0f, &none, NULL) == SV_INVALID);
    STEP(change->npc_create(owner, bad, 0.0f, &none, &none) == SV_INVALID);
    STEP(change->npc_create(owner, at, yaw_nan, &none, &none) == SV_INVALID && none == 0);
    STEP(change->npc_create(owner, at, 1.0f, &npc_player, &npc_human) == SV_OK && npc_player && npc_human);
    STEP(game->player(owner, npc_player, &ps) == SV_OK && ps.is_bot == 1 && ps.team == 6 && ps.human == npc_human);
    STEP(strcmp(ps.name, "NPC") == 0);
    STEP(game->human(owner, npc_human, &hs) == SV_OK && hs.player == npc_player && hs.health == 100);
    STEP(game->bone(owner, npc_human, 0, &bs) == SV_OK && hs.position.x == bs.position.x && hs.position.y == bs.position.y &&
         hs.position.z == bs.position.z && hs.position.x != 0.0f);
    STEP(change->npc_remove(owner, npc_human) == SV_NOT_FOUND);
    STEP(change->npc_remove(owner, player) == SV_DENIED);
    STEP(change->npc_remove(owner, npc_player) == SV_OK);
    return 0;
}

/* The test program seats two humans in one car. The first rider leaves with stop 1 and the second with stop 0.
 * Returns 0 when all checks pass, or the line of the check that failed. */
static int vehicle_checks(SvOwner owner)
{
    SvEntity page[16], rider_a = 0, rider_b = 0;
    SvHumanState hs = { sizeof hs };
    uint32_t count = 0, next = 0, i;
    STEP(change->vehicle_exit(owner, human, 0) == SV_STATE);
    STEP(change->vehicle_exit(owner, human, 2) == SV_INVALID && change->vehicle_exit(owner, human, -1) == SV_INVALID);
    STEP(change->vehicle_exit(owner, player, 0) == SV_NOT_FOUND && change->vehicle_exit(owner, 0, 0) == SV_NOT_FOUND);
    STEP(change->vehicle_exit(owner + 1, human, 0) == SV_STATE);
    STEP(game->entities(owner, SV_ENTITY_HUMAN, 0, page, 16, &count, &next) == SV_OK);
    for (i = 0; i < count; ++i)
        if (game->human(owner, page[i], &hs) == SV_OK && hs.vehicle) {
            if (hs.seat == 0)
                rider_a = page[i];
            else if (hs.seat == 1)
                rider_b = page[i];
        }
    STEP(rider_a && rider_b);
    STEP(change->vehicle_exit(owner, rider_a, 1) == SV_OK);
    STEP(game->human(owner, rider_a, &hs) == SV_OK && hs.vehicle == 0);
    STEP(change->vehicle_exit(owner, rider_a, 0) == SV_STATE);
    STEP(change->vehicle_exit(owner, rider_b, 0) == SV_OK);
    return 0;
}

/* Sound, pushes and spawn protection. The test program reads the events and the records after the tick.
 * Returns 0 when all checks pass, or the line of the check that failed. */
static int effect_checks(SvOwner owner)
{
    const SvVector at = { 100.0f, 50.0f, 200.0f }, push = { 1.0f, 2.0f, 3.0f }, strong = { 60.0f, 30.0f, 0.0f };
    SvVector bad = at;
    SvEntity page[16], car = 0, train = 0;
    SvObjectState os = { sizeof os };
    uint32_t nan_bits = 0x7fc00000u, count = 0, next = 0, i;
    float nan_value;
    memcpy(&nan_value, &nan_bits, sizeof nan_value);
    bad.x = nan_value;
    STEP(change->sound(owner + 1, 21, at, 1.0f, 0.15f) == SV_STATE);
    STEP(change->sound(owner, 47, at, 1.0f, 0.15f) == SV_INVALID && change->sound(owner, UINT32_MAX, at, 1.0f, 0.15f) == SV_INVALID);
    STEP(change->sound(owner, 21, bad, 1.0f, 0.15f) == SV_INVALID);
    STEP(change->sound(owner, 21, at, nan_value, 0.15f) == SV_INVALID && change->sound(owner, 21, at, 1.0f, nan_value) == SV_INVALID);
    STEP(change->sound(owner, 21, at, -0.5f, 0.15f) == SV_INVALID && change->sound(owner, 21, at, SV_SOUND_VOLUME_MAX + 0.5f, 0.15f) == SV_INVALID);
    STEP(change->sound(owner, 21, at, 1.0f, 0.05f) == SV_INVALID && change->sound(owner, 21, at, 1.0f, SV_SOUND_PITCH_MAX + 1.0f) == SV_INVALID);
    STEP(change->sound(owner, 21, at, 1.0f, 0.15f) == SV_OK);
    STEP(push_to(owner + 1, pid_bone_velocity, human, 3, push) == SV_STATE && push_to(owner, pid_bone_velocity, player, 3, push) == SV_NOT_FOUND);
    STEP(push_to(owner, pid_bone_velocity, human, 15, push) == SV_INVALID && push_to(owner, pid_bone_velocity, human, 3, strong) == SV_INVALID);
    STEP(push_to(owner, pid_bone_velocity, human, 3, bad) == SV_INVALID);
    STEP(push_to(owner, pid_bone_velocity, human, 3, push) == SV_OK);
    STEP(game->entities(owner, SV_ENTITY_OBJECT, 0, page, 16, &count, &next) == SV_OK);
    for (i = 0; i < count; ++i)
        if (game->object(owner, page[i], &os) == SV_OK) {
            if (os.type == 12)
                train = page[i];
            else if (os.color == 4)
                car = page[i];
        }
    STEP(car && train);
    STEP(push_to(owner, pid_object_velocity, train, 0, push) == SV_DENIED && push_to(owner, pid_object_velocity, human, 0, push) == SV_NOT_FOUND);
    STEP(push_to(owner, pid_object_velocity, car, 0, strong) == SV_INVALID && push_to(owner, pid_object_velocity, car, 0, bad) == SV_INVALID);
    STEP(push_to(owner, pid_object_velocity, car, 0, push) == SV_OK);
    STEP(put(owner, pid_protect, human, -1) == SV_INVALID && put(owner, pid_protect, human, 21601) == SV_INVALID);
    STEP(put(owner, pid_protect, player, 5) == SV_NOT_FOUND);
    STEP(put(owner, pid_protect, human, 500) == SV_OK);
    return 0;
}

/* News lines. The test program starts with three lines and reads the events and the storage after the tick.
 * Returns 0 when all checks pass, or the line of the check that failed. */
static int headline_checks(SvOwner owner)
{
    char news[SV_HEADLINE_BUFFER], long_text[SV_HEADLINE_BUFFER + 1], text[SV_HEADLINE_BUFFER];
    SvWorldState world = { sizeof world };
    uint32_t n;
    memset(long_text, 'x', SV_HEADLINE_TEXT_MAX + 1);
    long_text[SV_HEADLINE_TEXT_MAX + 1] = 0;
    STEP(change->headline(owner + 1, 0, "late") == SV_STATE);
    STEP(change->headline(owner, 0, NULL) == SV_INVALID);
    STEP(change->headline(owner, 0, "bad\ttext") == SV_INVALID && change->headline(owner, 0, "caf\xc3\xa9") == SV_INVALID);
    STEP(change->headline(owner, 0, long_text) == SV_INVALID);
    /* A gap is refused, and so is an index beyond the limit. */
    STEP(change->headline(owner, 4, "gap") == SV_INVALID && change->headline(owner, 1025, "far") == SV_INVALID);
    STEP(change->headline(owner, UINT32_MAX, "far") == SV_INVALID);
    /* A refusal changes nothing. */
    STEP(game->world(owner, &world) == SV_OK && world.headline_count == 3);
    STEP(game->headline(owner, 0, news, sizeof news) == SV_OK && !strcmp(news, "first"));
    /* Replace a line, add one with 100 characters, and add an empty one. */
    STEP(change->headline(owner, 1, "second replaced") == SV_OK);
    memset(text, 'a', 100);
    memset(text + 63, 'b', 37);
    text[100] = 0;
    STEP(change->headline(owner, 3, text) == SV_OK);
    STEP(change->headline(owner, 4, "") == SV_OK);
    STEP(game->world(owner, &world) == SV_OK && world.headline_count == 5);
    STEP(game->headline(owner, 1, news, sizeof news) == SV_OK && !strcmp(news, "second replaced"));
    STEP(game->headline(owner, 3, news, sizeof news) == SV_OK && !strcmp(news, text));
    STEP(game->headline(owner, 4, news, sizeof news) == SV_OK && !news[0]);
    /* Fill the list. The game keeps 1,024 lines. */
    for (n = 5; n < SV_HEADLINE_COUNT_MAX; ++n)
        STEP(change->headline(owner, n, "fill") == SV_OK);
    STEP(game->world(owner, &world) == SV_OK && world.headline_count == SV_HEADLINE_COUNT_MAX);
    STEP(change->headline(owner, SV_HEADLINE_COUNT_MAX, "full") == SV_LIMIT);
    STEP(change->headline(owner, SV_HEADLINE_COUNT_MAX - 1, "last") == SV_OK);
    STEP(game->headline(owner, SV_HEADLINE_COUNT_MAX - 1, news, sizeof news) == SV_OK && !strcmp(news, "last"));
    STEP(game->headline(owner, SV_HEADLINE_COUNT_MAX, news, sizeof news) == SV_NOT_FOUND);
    return 0;
}

/* The property table itself: names, descriptions, operations and the checks that do not belong to one row.
 * Returns 0 when all checks pass, or the line of the check that failed. */
static int props_checks(SvOwner owner)
{
    SvPropInfo info = { sizeof info }, small = { 4 };
    SvValue v = { 0 }, vel;
    uint32_t id = 0, count = 0;
    STEP(props->find(owner + 1, "human.health", &id) == SV_STATE);
    STEP(props->find(owner, NULL, &id) == SV_INVALID && props->find(owner, "human.health", NULL) == SV_INVALID);
    STEP(props->find(owner, "", &id) == SV_INVALID && props->find(owner, "no.such", &id) == SV_NOT_FOUND);
    STEP(props->find(owner, "0123456789012345678901234567890123456789", &id) == SV_INVALID);
    STEP(props->find(owner, "human.health", &id) == SV_OK && id == pid_health);
    STEP(props->info(owner, 0, &info) == SV_NOT_FOUND && props->info(owner, 9999, &info) == SV_NOT_FOUND);
    STEP(props->info(owner, id, NULL) == SV_INVALID && props->info(owner, id, &small) == SV_INVALID);
    STEP(props->info(owner + 1, id, &info) == SV_STATE);
    STEP(props->info(owner, id, &info) == SV_OK && info.id == id && info.entity_kind == SV_ENTITY_HUMAN &&
         info.type == SV_PROP_INT && info.count == 1 && info.min == -100.0 && info.max == 100.0 &&
         !strcmp(info.name, "human.health") && info.ops == (SV_PROP_GET | SV_PROP_SET | SV_PROP_ADD));
    for (id = 1; props->info(owner, id, &info) == SV_OK; ++id) {
        STEP(info.id == id);
        ++count;
    }
    STEP(count == 13);
    /* Read. */
    STEP(props->get(owner, pid_health, human, 0, &v) == SV_OK && v.i == 80);
    STEP(props->get(owner, pid_health, player, 0, &v) == SV_NOT_FOUND && props->get(owner, pid_health, human, 1, &v) == SV_INVALID);
    STEP(props->get(owner, pid_health, human, 0, NULL) == SV_INVALID && props->get(owner, 0, human, 0, &v) == SV_NOT_FOUND);
    STEP(props->get(owner + 1, pid_health, human, 0, &v) == SV_STATE);
    STEP(props->get(owner, pid_bone_velocity, human, 14, &vel) == SV_OK && props->get(owner, pid_bone_velocity, human, 15, &vel) == SV_INVALID);
    STEP(props->get(owner, pid_object_velocity, object, 0, &vel) == SV_OK);
    /* An operation that the row does not offer. */
    v.i = 1;
    STEP(props->set(owner, pid_bone_velocity, human, 0, v) == SV_DENIED && props->set(owner, pid_object_velocity, object, 0, v) == SV_DENIED);
    STEP(props->add(owner, pid_godmode, player, 0, v) == SV_DENIED && props->add(owner, pid_bleeding, human, 0, v) == SV_DENIED);
    /* Add to a number: the sum must stay in the range, and an error changes nothing. */
    v.i = 5;
    STEP(props->add(owner, pid_health, human, 0, v) == SV_OK && props->get(owner, pid_health, human, 0, &v) == SV_OK && v.i == 85);
    v.i = 100;
    STEP(props->add(owner, pid_health, human, 0, v) == SV_INVALID && props->get(owner, pid_health, human, 0, &v) == SV_OK && v.i == 85);
    v.i = INT32_MAX;
    STEP(props->add(owner, pid_health, human, 0, v) == SV_INVALID);
    v.i = INT32_MIN;
    STEP(props->add(owner, pid_health, human, 0, v) == SV_INVALID);
    v.i = -5;
    STEP(props->add(owner, pid_health, human, 0, v) == SV_OK && props->get(owner, pid_health, human, 0, &v) == SV_OK && v.i == 80);
    /* A row that a plugin area added: a world property, with no entity. */
    STEP(props->find(owner, "test.world", &id) == SV_OK && props->info(owner, id, &info) == SV_OK && info.entity_kind == 0 &&
         info.max == 10.0);
    STEP(props->get(owner, id, 0, 0, &v) == SV_OK && v.i == 0 && props->get(owner, id, player, 0, &v) == SV_INVALID);
    v.i = 11;
    STEP(props->set(owner, id, 0, 0, v) == SV_INVALID);
    v.i = 3;
    STEP(props->add(owner, id, 0, 0, v) == SV_OK && props->add(owner, id, 0, 0, v) == SV_OK);
    STEP(props->get(owner, id, 0, 0, &v) == SV_OK && v.i == 6);
    v.i = 5;
    STEP(props->add(owner, id, 0, 0, v) == SV_INVALID && props->get(owner, id, 0, 0, &v) == SV_OK && v.i == 6);
    v.i = 5;
    STEP(props->set(owner, id, 0, 0, v) == SV_OK);
    return 0;
}

/* The damage scales: float rows with a range, an add, and the kind check. The test program sets the values that it
 * reads after the tick: taken 0.5 and dealt 2 for the human, taken 0.5 for the car. */
static int damage_checks(SvOwner owner)
{
    uint32_t taken = 0, dealt = 0, scale = 0, i, count = 0, next = 0;
    SvEntity page[16], car = 0;
    SvObjectState os = { sizeof os };
    SvPropInfo info = { sizeof info };
    SvValue v;
    uint32_t nan_bits = 0x7fc00000u;
    STEP(props->find(owner, "human.damage_taken_scale", &taken) == SV_OK && props->find(owner, "human.damage_dealt_scale", &dealt) == SV_OK);
    STEP(props->find(owner, "object.damage_taken_scale", &scale) == SV_OK);
    STEP(props->info(owner, taken, &info) == SV_OK && info.type == SV_PROP_FLOAT && info.entity_kind == SV_ENTITY_HUMAN &&
         info.min == 0.0 && info.max == 100.0 && info.ops == (SV_PROP_GET | SV_PROP_SET | SV_PROP_ADD));
    STEP(game->entities(owner, SV_ENTITY_OBJECT, 0, page, 16, &count, &next) == SV_OK);
    for (i = 0; i < count; ++i)
        if (game->object(owner, page[i], &os) == SV_OK && os.type != 12 && os.color == 4)
            car = page[i];
    STEP(car);
    STEP(props->get(owner, taken, human, 0, &v) == SV_OK && v.f == 1.0f && props->get(owner, scale, car, 0, &v) == SV_OK && v.f == 1.0f);
    v.f = -0.1f;
    STEP(props->set(owner, taken, human, 0, v) == SV_INVALID);
    v.f = 100.5f;
    STEP(props->set(owner, taken, human, 0, v) == SV_INVALID);
    memcpy(&v.f, &nan_bits, sizeof v.f);
    STEP(props->set(owner, taken, human, 0, v) == SV_INVALID && props->add(owner, taken, human, 0, v) == SV_INVALID);
    v.f = 0.25f;
    STEP(props->set(owner, taken, human, 0, v) == SV_OK && props->add(owner, taken, human, 0, v) == SV_OK);
    v.f = 99.9f;
    STEP(props->add(owner, taken, human, 0, v) == SV_INVALID);
    STEP(props->get(owner, taken, human, 0, &v) == SV_OK && v.f == 0.5f);
    v.f = 2.0f;
    STEP(props->set(owner, dealt, human, 0, v) == SV_OK);
    v.f = 0.5f;
    STEP(props->set(owner, scale, car, 0, v) == SV_OK);
    STEP(props->set(owner, taken, player, 0, v) == SV_NOT_FOUND && props->set(owner, scale, human, 0, v) == SV_NOT_FOUND);
    return 0;
}

/* The test program puts a car with two faces in the world. Face 0 is the plane x = 500 and face 1 is the plane z = 150.
 * The head of the human faces -z from (100, 51, 200). The level is the flat ground at height 0. */
static int line_checks(SvOwner owner)
{
    const SvVector across_a = { 490.0f, 2.0f, 2.0f }, across_b = { 510.0f, 2.0f, 2.0f };
    const SvVector level_a = { 600.0f, 10.0f, 600.0f }, level_b = { 600.0f, -10.0f, 600.0f };
    const SvVector body_a = { 100.0f, 50.0f, 190.0f }, body_b = { 100.0f, 50.0f, 210.0f };
    SvVector bad = across_a, far_end = { 490.0f, 2.0f, 5000.0f };
    SvLineHit hit = { sizeof hit }, small = { 4 };
    SvObjectState os = { sizeof os };
    uint32_t nan_bits = 0x7fc00000u;
    float nan_value;
    memcpy(&nan_value, &nan_bits, sizeof nan_value);
    /* Bad input. */
    STEP(game->line(owner + 1, across_a, across_b, SV_LINE_OBJECTS, 0, &hit) == SV_STATE);
    STEP(game->line(owner, across_a, across_b, SV_LINE_OBJECTS, 0, NULL) == SV_INVALID);
    STEP(game->line(owner, across_a, across_b, SV_LINE_OBJECTS, 0, &small) == SV_INVALID);
    STEP(game->line(owner, across_a, across_b, 0, 0, &hit) == SV_INVALID);
    STEP(game->line(owner, across_a, across_b, SV_LINE_ALL + 1, 0, &hit) == SV_INVALID);
    STEP(game->line(owner, across_a, across_a, SV_LINE_ALL, 0, &hit) == SV_INVALID);
    STEP(game->line(owner, across_a, far_end, SV_LINE_ALL, 0, &hit) == SV_INVALID);
    bad.y = nan_value;
    STEP(game->line(owner, bad, across_b, SV_LINE_ALL, 0, &hit) == SV_INVALID);
    bad = across_a;
    bad.x = SV_POSITION_LIMIT + 1.0f;
    STEP(game->line(owner, bad, across_b, SV_LINE_ALL, 0, &hit) == SV_INVALID);
    STEP(game->line(owner, across_a, across_b, SV_LINE_ALL, player, &hit) == SV_NOT_FOUND);
    /* The car. */
    hit.kind = 99;
    STEP(game->line(owner, across_a, across_b, SV_LINE_OBJECTS, 0, &hit) == SV_OK && hit.kind == SV_HIT_OBJECT);
    STEP(hit.entity && game->object(owner, hit.entity, &os) == SV_OK && os.color == 4 && hit.bone == -1);
    STEP(hit.position.x > 499.99f && hit.position.x < 500.01f && hit.fraction > 0.49f && hit.fraction < 0.51f);
    STEP(hit.distance > 9.9f && hit.distance < 10.1f);
    STEP(game->line(owner, across_a, across_b, SV_LINE_LEVEL | SV_LINE_HUMANS, 0, &hit) == SV_OK && hit.kind == SV_HIT_NONE &&
         hit.entity == 0 && hit.fraction == 1.0f && hit.position.x == 0.0f);
    /* The level. */
    STEP(game->line(owner, level_a, level_b, SV_LINE_LEVEL, 0, &hit) == SV_OK && hit.kind == SV_HIT_LEVEL && hit.entity == 0);
    STEP(hit.position.y > -0.01f && hit.position.y < 0.01f && hit.fraction > 0.49f && hit.fraction < 0.51f);
    STEP(game->line(owner, level_a, level_b, SV_LINE_OBJECTS | SV_LINE_HUMANS, 0, &hit) == SV_OK && hit.kind == SV_HIT_NONE);
    /* A human, and the ignore rule. */
    STEP(game->line(owner, body_a, body_b, SV_LINE_HUMANS, 0, &hit) == SV_OK && hit.kind == SV_HIT_HUMAN && hit.entity == human);
    STEP(hit.bone >= 0 && hit.bone < 15 && hit.fraction > 0.3f && hit.fraction < 0.7f);
    STEP(game->line(owner, body_a, body_b, SV_LINE_HUMANS, human, &hit) == SV_OK && hit.kind == SV_HIT_NONE);
    /* The nearest of all kinds: the car plane z = 150 stands before the human when the line starts at z = 100. */
    STEP(game->line(owner, (SvVector){ 100.0f, 51.0f, 100.0f }, body_b, SV_LINE_ALL, 0, &hit) == SV_OK &&
         hit.kind == SV_HIT_OBJECT && hit.position.z > 149.99f && hit.position.z < 150.01f);
    /* The aim of the human. */
    STEP(game->aim(owner, human, 100.0f, SV_LINE_OBJECTS, &hit) == SV_OK && hit.kind == SV_HIT_OBJECT);
    STEP(hit.distance > 49.0f && hit.distance < 51.0f && hit.position.z > 149.99f && hit.position.z < 150.01f);
    STEP(game->aim(owner, human, 40.0f, SV_LINE_ALL, &hit) == SV_OK && hit.kind == SV_HIT_NONE);
    STEP(game->aim(owner, human, 0.0f, SV_LINE_ALL, &hit) == SV_INVALID && game->aim(owner, human, nan_value, SV_LINE_ALL, &hit) == SV_INVALID);
    STEP(game->aim(owner, human, SV_LINE_LENGTH_MAX + 1.0f, SV_LINE_ALL, &hit) == SV_INVALID);
    STEP(game->aim(owner, player, 10.0f, SV_LINE_ALL, &hit) == SV_NOT_FOUND && game->aim(owner, human, 10.0f, 0, &hit) == SV_INVALID);
    STEP(game->aim(owner + 1, human, 10.0f, SV_LINE_ALL, &hit) == SV_STATE);
    return 0;
}

/* Bad input must change nothing. The test program checks the game state after the tick. */
static SvResult write_checks(SvOwner owner)
{
    static const char longest[] = "0123456789012345678901234567890123456789012345678901234567890123";
    SvHumanState h = { sizeof h };
    SvVector target = { 100.0f, 50.0f, 200.0f }, bad = target;
    uint32_t nan_bits = 0x7fc00000u, inf_bits = 0x7f800000u;
    HANDLE thread;
    DWORD code;
    CHECK(change->message(owner, SV_MESSAGE_SERVER, NULL) == SV_INVALID);
    CHECK(change->message(owner, 1, "kind") == SV_INVALID);
    CHECK(change->message(owner, SV_MESSAGE_SERVER, "") == SV_INVALID);
    CHECK(change->message(owner, SV_MESSAGE_SERVER, "bad\ntext") == SV_INVALID);
    CHECK(change->message(owner, SV_MESSAGE_SERVER, longest) == SV_INVALID);
    CHECK(change->message(owner + 1, SV_MESSAGE_SERVER, "owner") == SV_STATE);
    CHECK(change->message(owner, SV_MESSAGE_SERVER, longest + 1) == SV_OK);
    CHECK(change->message(owner, SV_MESSAGE_ADMIN, "admin line") == SV_OK);
    CHECK(change->add_money(owner, account, INT32_MAX) == SV_INVALID);
    CHECK(change->add_money(owner, human, 5) == SV_NOT_FOUND);
    CHECK(change->add_money(owner, account, 500) == SV_OK);
    CHECK(put(owner, pid_godmode, player, 2) == SV_INVALID);
    CHECK(put(owner, pid_godmode, human, 1) == SV_NOT_FOUND);
    CHECK(put(owner, pid_godmode, player, 1) == SV_OK);
    CHECK(put(owner, pid_health, human, 101) == SV_INVALID);
    CHECK(put(owner, pid_health, human, -101) == SV_INVALID);
    CHECK(put(owner, pid_health, player, 5) == SV_NOT_FOUND);
    CHECK(put(owner, pid_health, human, 80) == SV_OK);
    memcpy(&bad.x, &nan_bits, sizeof bad.x);
    CHECK(change->teleport(owner, human, bad) == SV_INVALID);
    bad = target;
    memcpy(&bad.y, &inf_bits, sizeof bad.y);
    CHECK(change->teleport(owner, human, bad) == SV_INVALID);
    bad = target;
    bad.z = SV_POSITION_LIMIT + 1.0f;
    CHECK(change->teleport(owner, human, bad) == SV_INVALID);
    CHECK(change->teleport(owner, player, target) == SV_NOT_FOUND);
    CHECK(change->teleport(owner, human, target) == SV_OK);
    CHECK(put(owner, pid_bleeding, human, 2) == SV_INVALID && put(owner, pid_bleeding, player, 1) == SV_NOT_FOUND);
    CHECK(put(owner, pid_bleeding, human, 1) == SV_OK && game->human(owner, human, &h) == SV_OK && h.bleeding == 1);
    CHECK(put(owner, pid_bleeding, human, 0) == SV_OK && game->human(owner, human, &h) == SV_OK && h.bleeding == 0);
    CHECK(put(owner, pid_bleeding, human, 1) == SV_OK);
    CHECK(put(owner, pid_stun, human, -1) == SV_INVALID && put(owner, pid_stun, human, 3601) == SV_INVALID);
    CHECK(put(owner, pid_stun, player, 5) == SV_NOT_FOUND);
    CHECK(put(owner, pid_stun, human, 3600) == SV_OK && game->human(owner, human, &h) == SV_OK &&
          h.stun_ticks == 3600);
    CHECK(put(owner, pid_stun, human, 120) == SV_OK);
    CHECK(game->human(owner, human, &h) == SV_OK);
    CHECK(h.position.x == 100.0f && h.position.y == 50.0f && h.position.z == 200.0f && h.health == 80);
    CHECK(change_player_fields(owner));
    CHECK(change_items(owner));
    CHECK(change_objects(owner));
    {
        int bad_line = change_npc(owner);
        if (!bad_line)
            bad_line = vehicle_checks(owner);
        if (!bad_line)
            bad_line = line_checks(owner);
        if (!bad_line)
            bad_line = effect_checks(owner);
        if (!bad_line)
            bad_line = headline_checks(owner);
        if (!bad_line)
            bad_line = props_checks(owner);
        if (!bad_line)
            bad_line = damage_checks(owner);
        if (bad_line) {
            char text[80];
            snprintf(text, sizeof text, "Write check failed at line %d.", bad_line);
            host->log(owner, text);
            return SV_FAILED;
        }
    }
    CHECK(change->kick(owner, human) == SV_NOT_FOUND);
    CHECK(change->kick(owner, player) == SV_OK);
    thread = CreateThread(NULL, 0, write_wrong_thread, NULL, 0, NULL);
    CHECK(thread != NULL);
    CHECK(WaitForSingleObject(thread, 5000) == WAIT_OBJECT_0);
    CHECK(GetExitCodeThread(thread, &code));
    CloseHandle(thread);
    CHECK(code == 0);
    return host->log(owner, "PUBLIC_WRITE_API_PASS");
}

static SvResult runtime_reads(SvOwner owner)
{
    SvWorldState world = { sizeof world };
    SvPlayerState p = { sizeof p };
    SvHumanState h = { sizeof h };
    SvItemState it = { sizeof it };
    SvObjectState o = { sizeof o };
    SvAccountState a = { sizeof a };
    const uint32_t limits[] = { 0, 256, 512, 1024, 1024, 65536 };
    uint32_t kind, seen = 0;
    CHECK(game->world(owner, &world) == SV_OK);
    CHECK(world.reset_sequence && world.server_active && world.max_players >= 2 && world.max_players <= 128);
    CHECK(world.mode == 3 || world.mode == 4);
    for (kind = SV_ENTITY_PLAYER; kind <= SV_ENTITY_ACCOUNT; ++kind) {
        uint32_t cursor = 0;
        while (cursor < limits[kind]) {
            SvEntity page[16];
            uint32_t count = 0, next = 0, i;
            CHECK(game->entities(owner, kind, cursor, page, 16, &count, &next) == SV_OK);
            CHECK(next > cursor && next <= limits[kind] && next - cursor <= SV_ENTITY_PAGE_MAX);
            for (i = 0; i < count; ++i) {
                SvResult result = SV_FAILED;
                switch (kind) {
                case SV_ENTITY_PLAYER: result = game->player(owner, page[i], &p); break;
                case SV_ENTITY_HUMAN: result = game->human(owner, page[i], &h); break;
                case SV_ENTITY_ITEM: result = game->item(owner, page[i], &it); break;
                case SV_ENTITY_OBJECT: result = game->object(owner, page[i], &o); break;
                case SV_ENTITY_ACCOUNT: result = game->account(owner, page[i], &a); break;
                }
                CHECK(result == SV_OK);
                ++seen;
            }
            cursor = next;
        }
    }
    CHECK(seen > 0);
    return host->log(owner, "PUBLIC_READ_RUNTIME_PASS");
}

static SvResult SV_CALL tick_forward(SvOwner owner, const SvEvent *event);

/* A game event record: count it, and note the tick and whether its human reference is still valid. */
static SvResult game_event(SvOwner owner, const SvEvent *event)
{
    const SvGameEvent *record = (const SvGameEvent *)event;
    SvHumanState h = { sizeof h };
    if (event->kind == SV_EVENT_DAMAGE) {
        /* A decision inside the simulation. The amounts 77, 55, 66, 67, 88 and 5 are the ones that the test program uses. The
         * record is writable, and new_amount is the only field that the fixture writes. */
        SvDamageEvent *damage = (SvDamageEvent *)event;
        SvValue value = { 0 };
        SvObjectState object_state = { sizeof object_state };
        if (event->size != sizeof *damage || damage->reserved || damage->new_amount != damage->amount)
            return SV_FAILED;
        if (damage->amount != 77 && damage->amount != 55 && damage->amount != 66 && damage->amount != 67 &&
            damage->amount != 88 && damage->amount != 5)
            return SV_OK;
        if (damage_log_n < 8) {
            damage_log_source[damage_log_n] = damage->source;
            damage_log_amount[damage_log_n] = (uint32_t)damage->amount;
            ++damage_log_n;
        }
        /* The references must resolve, and a read is allowed inside the decision. */
        if (damage->human && game->human(owner, damage->human, &h) != SV_OK)
            damage_refs_ok = 0;
        if (damage->object && game->object(owner, damage->object, &object_state) != SV_OK)
            damage_refs_ok = 0;
        if (damage->attacker && game->human(owner, damage->attacker, &h) != SV_OK)
            damage_refs_ok = 0;
        if (damage->amount == 88) {
            /* Every call that changes the game or the host is closed. */
            value.i = 5;
            damage_guard_ok = change->message(owner, SV_MESSAGE_SERVER, "late") == SV_STATE &&
                change->sound(owner, 21, (SvVector){ 1.0f, 2.0f, 3.0f }, 1.0f, 1.0f) == SV_STATE &&
                change->vehicle_exit(owner, human, 0) == SV_STATE && props->set(owner, pid_health, human, 0, value) == SV_STATE &&
                props->add(owner, pid_health, human, 0, value) == SV_STATE &&
                host->command(owner, "late", SV_COMMAND_PUBLIC, echo_command) == SV_STATE &&
                host->subscribe(owner, SV_EVENT_TICK, tick_forward) == SV_STATE &&
                props->get(owner, pid_health, human, 0, &value) == SV_OK && host->log(owner, "decision log") == SV_OK;
            return SV_OK;
        }
        if (damage->amount == 77)
            return SV_DENIED;
        if (damage->amount == 55)
            damage->new_amount = 11;
        else if (damage->amount == 66)
            damage->new_amount = -5;
        else if (damage->amount == 67)
            damage->new_amount = SV_DAMAGE_MAX + 1;
        return SV_OK;
    }
    if (event->kind == SV_EVENT_CHAT) {
        /* A decision: drop a text that has the word deny. */
        const SvChatEvent *chat = (const SvChatEvent *)event;
        SvPlayerState ps = { sizeof ps };
        if (event->size != sizeof *chat || chat->text[63] != 0)
            return SV_FAILED;
        ++seen_chat;
        memcpy(last_chat, chat->text, sizeof last_chat);
        chat_player_ok = game->player(owner, chat->player, &ps) == SV_OK;
        if (strstr(chat->text, "deny")) {
            ++seen_chat_denied;
            return SV_DENIED;
        }
        return SV_OK;
    }
    if (event->kind == SV_EVENT_JOIN) {
        /* A decision: drop a player named BANNED. The name needs a ready world, and a plugin that cannot read lets it pass. */
        SvPlayerState ps = { sizeof ps };
        SvAccountState acc = { sizeof acc };
        if (event->size != sizeof *record || record->reserved)
            return SV_FAILED;
        ++seen_join;
        if (game->player(owner, record->player, &ps) != SV_OK)
            return SV_OK;
        if (record->account && game->account(owner, record->account, &acc) != SV_OK)
            return SV_FAILED;
        return strcmp(ps.name, "BANNED") ? SV_OK : SV_DENIED;
    }
    if (event->size != sizeof *record || record->reserved)
        return SV_FAILED;
    switch (event->kind) {
    case SV_EVENT_SPAWN: ++seen_spawn; break;
    case SV_EVENT_DEATH: ++seen_death; break;
    case SV_EVENT_LEAVE: ++seen_leave; break;
    case SV_EVENT_LOST:
        seen_lost += record->lost;
        spawn_before_lost = seen_spawn;
        break;
    default: ++seen_other;
    }
    if (seen_n < 8 && event->kind != SV_EVENT_LOST) {
        seen_ticks[seen_n] = event->tick;
        ref_ok[seen_n] = record->human ? game->human(owner, record->human, &h) == SV_OK : -1;
        ++seen_n;
    }
    return SV_OK;
}

/* The test program posted a spawn, two deaths and a leave. The second death has a human that is gone. */
static int event_checks(void)
{
    return seen_spawn == 1 && seen_death == 2 && seen_leave == 1 && seen_lost == 0 && seen_other == 0 && seen_n == 4 &&
        seen_ticks[0] == 11 && seen_ticks[1] == 12 && seen_ticks[2] == 13 && seen_ticks[3] == 14 &&
        ref_ok[0] == 1 && ref_ok[1] == 1 && ref_ok[2] == -1 && ref_ok[3] == 0;
}

static SvResult SV_CALL tick(SvOwner owner, const SvEvent *event);
/* The same function with the name that the guard test passes to subscribe. */
static SvResult SV_CALL tick_forward(SvOwner owner, const SvEvent *event)
{
    return tick(owner, event);
}

static SvResult SV_CALL tick(SvOwner owner, const SvEvent *event)
{
    if (event->kind != SV_EVENT_TICK)
        return game_event(owner, event);
    ++stage;
    if (runtime_probe)
        return stage == 1 ? runtime_reads(owner) : SV_OK;
    if (stage == 1)
        return find_props(owner) ? initial_reads(owner) : SV_FAILED;
    if (stage == 2) {
        SvInventoryState inventory = { sizeof inventory };
        SvItemState it = { sizeof it };
        SvObjectState o = { sizeof o };
        SvHumanState h = { sizeof h };
        inventory.count = 777; it.type = 888; o.type = 999;
        CHECK(game->inventory(owner, human, 0, &inventory) == SV_FAILED && inventory.count == 777);
        CHECK(game->item(owner, item, &it) == SV_FAILED && it.type == 888);
        CHECK(game->object(owner, object, &o) == SV_FAILED && o.type == 999);
        CHECK(game->human(owner, human, &h) == SV_OK && h.vehicle == 0 && h.seat == -1);
    } else if (stage == 3) {
        SvObjectState o = { sizeof o };
        CHECK(game->object(owner, object, &o) == SV_OK && o.driver == 0);
        CHECK(event_checks());
        return write_checks(owner);
    } else if (stage == 4) {
        SvPlayerState p = { sizeof p };
        SvHumanState h = { sizeof h };
        SvItemState it = { sizeof it };
        SvObjectState o = { sizeof o };
        SvAccountState a = { sizeof a };
        SvWorldState world = { sizeof world };
        SvEntity page[1] = {0};
        uint32_t count = 777, next = 888;
        CHECK(seen_spawn == 1025 && seen_lost == 76 && spawn_before_lost == 1 && seen_other == 0);
        /* The chat texts that reached the plugin: hello, deny me (dropped), 63 letters and 99 letters cut to 63. */
        CHECK(seen_chat == 4 && seen_chat_denied == 1 && strlen(last_chat) == 63 && last_chat[0] == 'b' && chat_player_ok);
        CHECK(seen_join == 2);
        /* The decisions of the test program, in order: deny, change, two bad answers, an object change and deny, a vehicle
         * hit with a shooter, and the write guard. */
        CHECK(damage_log_n == 8 && damage_refs_ok && damage_guard_ok);
        CHECK(damage_log_source[0] == SV_DAMAGE_BULLET && damage_log_amount[0] == 77 && damage_log_amount[1] == 55 &&
              damage_log_amount[2] == 66 && damage_log_amount[3] == 67);
        CHECK(damage_log_source[4] == SV_DAMAGE_OBJECT && damage_log_amount[4] == 55 && damage_log_source[5] == SV_DAMAGE_OBJECT &&
              damage_log_amount[5] == 77 && damage_log_source[6] == SV_DAMAGE_VEHICLE && damage_log_amount[6] == 5 &&
              damage_log_source[7] == SV_DAMAGE_BULLET && damage_log_amount[7] == 88);
        CHECK(game->player(owner, player, &p) == SV_NOT_FOUND);
        CHECK(game->human(owner, human, &h) == SV_NOT_FOUND);
        CHECK(game->item(owner, item, &it) == SV_NOT_FOUND);
        CHECK(game->object(owner, object, &o) == SV_NOT_FOUND);
        CHECK(game->account(owner, account, &a) == SV_NOT_FOUND);
        CHECK(game->world(owner, &world) == SV_OK && world.reset_sequence == reset_sequence + 1);
        CHECK(first(owner, SV_ENTITY_PLAYER) != player);
        CHECK(game->entities(owner, SV_ENTITY_ACCOUNT, 0, page, 1, &count, &next) == SV_OK);
        CHECK(count == 0 && next == SV_ENTITY_PAGE_MAX);
        CHECK(game->object(owner, first(owner, SV_ENTITY_OBJECT), &o) == SV_NOT_FOUND);
        host->log(owner, "PUBLIC_READ_API_PASS");
    }
    return SV_OK;
}

static SvResult SV_CALL enable(const SvHost *api, SvOwner owner)
{
    SvWorldState world = { sizeof world };
    const void *table = NULL;
    char mode[4];
    if (!api || api->size < sizeof *api || api->abi != SV_PLUGIN_ABI)
        return SV_INCOMPATIBLE;
    host = api; plugin_owner = owner;
    runtime_probe = GetEnvironmentVariableA("SV_GAME_READ_RUNTIME", mode, sizeof mode) == 1 && mode[0] == '1';
    CHECK(host->get_api(owner, SV_API_GAME_READ, SV_GAME_READ_VERSION, &table) == SV_OK);
    game = (const SvGameRead *)table;
    CHECK(game && game->size == sizeof *game && game->version == SV_GAME_READ_VERSION);
    CHECK(game->world(owner, &world) == SV_STATE);
    CHECK(host->get_api(owner, SV_API_GAME_WRITE, SV_GAME_WRITE_VERSION + 1, &table) == SV_INCOMPATIBLE && table == NULL);
    CHECK(host->get_api(owner, SV_API_GAME_WRITE, SV_GAME_WRITE_VERSION, &table) == SV_OK);
    change = (const SvGameWrite *)table;
    CHECK(change && change->size == sizeof *change && change->version == SV_GAME_WRITE_VERSION);
    CHECK(host->get_api(owner, SV_API_GAME_PROPS, SV_GAME_PROPS_VERSION + 1, &table) == SV_INCOMPATIBLE && table == NULL);
    CHECK(host->get_api(owner, SV_API_GAME_PROPS, SV_GAME_PROPS_VERSION, &table) == SV_OK);
    props = (const SvGameProps *)table;
    CHECK(props && props->size == sizeof *props && props->version == SV_GAME_PROPS_VERSION);
    CHECK(change->message(owner, SV_MESSAGE_SERVER, "before ready") == SV_STATE);
    CHECK(host->command(owner, "admin", SV_COMMAND_PUBLIC, echo_command) == SV_INVALID);
    CHECK(host->command(owner, "echo", SV_COMMAND_PUBLIC, echo_command) == SV_OK);
    CHECK(host->command(owner, "secret", SV_COMMAND_ADMIN, secret_command) == SV_OK);
    CHECK(host->command(owner, "localonly", SV_COMMAND_LOCAL, localonly_command) == SV_OK);
    CHECK(host->command(owner, "npclimit", SV_COMMAND_LOCAL, npclimit_command) == SV_OK);
    CHECK(host->command(owner, "linemark", SV_COMMAND_LOCAL, linemark_command) == SV_OK);
    CHECK(host->get_api(owner, UINT32_MAX, 1, &table) == SV_NOT_FOUND && table == NULL);
    CHECK(host->get_api(owner, SV_API_GAME_READ, UINT32_MAX, &table) == SV_INCOMPATIBLE && table == NULL);
    CHECK(host->get_api(owner, SV_API_GAME_READ, SV_GAME_READ_VERSION, NULL) == SV_INVALID);
    CHECK(host->subscribe(owner, 0x80000000u, tick) == SV_INVALID && host->subscribe(owner, SV_EVENT_TICK | 0x4000u, tick) == SV_INVALID);
    return host->subscribe(owner, SV_EVENT_TICK | SV_EVENT_LOST | SV_EVENT_SPAWN | SV_EVENT_DEATH | SV_EVENT_LEAVE |
        SV_EVENT_CHAT | SV_EVENT_JOIN | SV_EVENT_DAMAGE, tick);
}

static void SV_CALL disable(SvOwner owner)
{
    SvWorldState world = { sizeof world };
    host->log(owner, game->world(owner, &world) == SV_STATE ? "STOP_READ_DENIED" : "STOP_READ_FAILURE");
    host->log(owner, change->message(owner, SV_MESSAGE_SERVER, "stopping") == SV_STATE ?
        "STOP_WRITE_DENIED" : "STOP_WRITE_FAILURE");
}

SV_EXPORT SvResult SV_CALL sv_plugin_entry(uint32_t abi, SvPlugin *plugin)
{
    if (abi != SV_PLUGIN_ABI || !plugin || plugin->size < sizeof *plugin)
        return SV_INCOMPATIBLE;
    memset(plugin, 0, sizeof *plugin);
    plugin->size = sizeof *plugin; plugin->abi = SV_PLUGIN_ABI;
    strcpy(plugin->id, "test.read"); strcpy(plugin->version, "0.1.0");
    plugin->flags = SV_PLUGIN_LIVE; plugin->enable = enable; plugin->disable = disable;
    return SV_OK;
}
