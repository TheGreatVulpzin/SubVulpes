#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdio.h>
#include <string.h>
#include "sv_game_read.h"
#include "sv_game_write.h"

static const SvHost *host;
static const SvGameRead *game;
static const SvGameWrite *change;
static SvOwner plugin_owner;
static SvEntity player, human, item, object, account;
static uint32_t stage;
static uint64_t reset_sequence;
static int runtime_probe;

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

static DWORD WINAPI write_wrong_thread(void *argument)
{
    (void)argument;
    return change->message(plugin_owner, SV_MESSAGE_SERVER, "wrong thread") == SV_STATE &&
        change->set_health(plugin_owner, human, 50) == SV_STATE ? 0 : 1;
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
    if (change->set_admin(owner, player, 2) != SV_INVALID || change->set_admin(owner, page[1], 1) != SV_INVALID ||
        change->set_admin(owner, human, 1) != SV_NOT_FOUND || change->set_admin(owner, player, 1) != SV_OK ||
        game->player(owner, player, &ps) != SV_OK || ps.is_admin != 1)
        return 0;
    if (change->set_team(owner, player, SV_TEAM_MAX + 1) != SV_INVALID || change->set_team(owner, player, -1) != SV_INVALID ||
        change->set_team(owner, human, 2) != SV_NOT_FOUND || change->set_team(owner, player, 3) != SV_OK ||
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
    CHECK(change->set_godmode(owner, player, 2) == SV_INVALID);
    CHECK(change->set_godmode(owner, human, 1) == SV_NOT_FOUND);
    CHECK(change->set_godmode(owner, player, 1) == SV_OK);
    CHECK(change->set_health(owner, human, SV_HEALTH_MAX + 1) == SV_INVALID);
    CHECK(change->set_health(owner, human, SV_HEALTH_MIN - 1) == SV_INVALID);
    CHECK(change->set_health(owner, player, 5) == SV_NOT_FOUND);
    CHECK(change->set_health(owner, human, 80) == SV_OK);
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
    CHECK(game->human(owner, human, &h) == SV_OK);
    CHECK(h.position.x == 100.0f && h.position.y == 50.0f && h.position.z == 200.0f && h.health == 80);
    CHECK(change_player_fields(owner));
    CHECK(change_items(owner));
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

static SvResult SV_CALL tick(SvOwner owner, const SvEvent *event)
{
    (void)event;
    ++stage;
    if (runtime_probe)
        return stage == 1 ? runtime_reads(owner) : SV_OK;
    if (stage == 1)
        return initial_reads(owner);
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
    CHECK(change->message(owner, SV_MESSAGE_SERVER, "before ready") == SV_STATE);
    CHECK(host->command(owner, "admin", SV_COMMAND_PUBLIC, echo_command) == SV_INVALID);
    CHECK(host->command(owner, "echo", SV_COMMAND_PUBLIC, echo_command) == SV_OK);
    CHECK(host->command(owner, "secret", SV_COMMAND_ADMIN, secret_command) == SV_OK);
    CHECK(host->command(owner, "localonly", SV_COMMAND_LOCAL, localonly_command) == SV_OK);
    CHECK(host->get_api(owner, UINT32_MAX, 1, &table) == SV_NOT_FOUND && table == NULL);
    CHECK(host->get_api(owner, SV_API_GAME_READ, UINT32_MAX, &table) == SV_INCOMPATIBLE && table == NULL);
    CHECK(host->get_api(owner, SV_API_GAME_READ, SV_GAME_READ_VERSION, NULL) == SV_INVALID);
    return host->subscribe(owner, SV_EVENT_TICK, tick);
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
