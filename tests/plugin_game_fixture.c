#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdio.h>
#include <string.h>
#include "sv_game_read.h"

static const SvHost *host;
static const SvGameRead *game;
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
    return SV_OK;
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
    CHECK(host->get_api(owner, UINT32_MAX, 1, &table) == SV_NOT_FOUND && table == NULL);
    CHECK(host->get_api(owner, SV_API_GAME_READ, UINT32_MAX, &table) == SV_INCOMPATIBLE && table == NULL);
    CHECK(host->get_api(owner, SV_API_GAME_READ, SV_GAME_READ_VERSION, NULL) == SV_INVALID);
    return host->subscribe(owner, SV_EVENT_TICK, tick);
}

static void SV_CALL disable(SvOwner owner)
{
    SvWorldState world = { sizeof world };
    host->log(owner, game->world(owner, &world) == SV_STATE ? "STOP_READ_DENIED" : "STOP_READ_FAILURE");
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
