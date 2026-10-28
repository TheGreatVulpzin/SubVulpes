#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include "systems_plugin.h"
#include "sv_game_shop.h"
#include "sv_game_write.h"
#include "sv_game_engine.h"

static const SvGameShop *shop;
static const SvGameRead *read_api;
static const SvGameWrite *write_api;
static const SvHost *saved_host;
static SvEntity player, human, phone, nonphone;
static SvEntity first_phone;

static int refresh(SvOwner owner)
{
    SvEntity entities[SV_ENTITY_PAGE_MAX];
    uint32_t count, next, cursor = 0, i;
    SvPlayerState p = {sizeof p};
    SvItemState item = {sizeof item};
    phone = nonphone = player = human = 0;
    STEP(read_api->entities(owner, SV_ENTITY_PLAYER, 0, entities, SV_ENTITY_PAGE_MAX, &count, &next) == SV_OK);
    for (i = 0; i < count; ++i) {
        STEP(read_api->player(owner, entities[i], &p) == SV_OK);
        if (!strcmp(p.name, "Systems test")) {
            player = entities[i];
            human = p.human;
        }
    }
    do {
        STEP(read_api->entities(owner, SV_ENTITY_ITEM, cursor, entities, SV_ENTITY_PAGE_MAX, &count, &next) == SV_OK);
        for (i = 0; i < count; ++i) {
            STEP(read_api->item(owner, entities[i], &item) == SV_OK);
            if (item.type == 23 && item.phone_id == 7001)
                phone = entities[i];
            if (item.type != 23)
                nonphone = entities[i];
        }
        if ((phone && nonphone) || cursor == next)
            break;
        cursor = next;
    } while (1);
    STEP(player && human && phone && nonphone);
    return 0;
}

static int phone_failure(SvOwner owner, SvEntity reference, SvResult expected)
{
    SvPhoneState out, before;
    memset(&out, 0xa5, sizeof out);
    out.size = sizeof out;
    before = out;
    STEP(shop->phone(owner, reference, &out) == expected);
    STEP(!memcmp(&out, &before, sizeof out));
    return 0;
}

static int catalog_failure(SvOwner owner, uint32_t building, SvResult expected)
{
    SvShopCatalog out, before;
    memset(&out, 0xa5, sizeof out);
    out.size = sizeof out;
    before = out;
    STEP(shop->catalog(owner, building, &out) == expected);
    STEP(!memcmp(&out, &before, sizeof out));
    return 0;
}

static DWORD WINAPI wrong_thread(void *data)
{
    SvOwner owner = *(SvOwner *)data;
    STEP(phone_failure(owner, phone, SV_STATE) == 0);
    STEP(shop->phone_idle(owner, phone) == SV_STATE);
    STEP(catalog_failure(owner, 0, SV_STATE) == 0);
    STEP(shop->catalog_replace(owner, 0, NULL) == SV_STATE);
    return 0;
}

static int invalid_inputs(SvOwner owner)
{
    SvPhoneState state, old_state;
    SvShopCatalog out, old_out;
    SvPhoneState initial_phone = {sizeof initial_phone}, final_phone = {sizeof final_phone};
    SvShopCatalog initial_catalog = {sizeof initial_catalog}, final_catalog = {sizeof final_catalog};
    SvShopCatalogWrite input = {sizeof input, 0, {{0}}};
    HANDLE thread;
    DWORD code;
    STEP(shop->phone(owner, phone, &initial_phone) == SV_OK);
    STEP(shop->catalog(owner, 0, &initial_catalog) == SV_OK);
    thread = CreateThread(NULL, 0, wrong_thread, &owner, 0, NULL);
    STEP(thread != NULL);
    STEP(WaitForSingleObject(thread, 5000) == WAIT_OBJECT_0);
    STEP(GetExitCodeThread(thread, &code));
    CloseHandle(thread);
    STEP(code == 0);
    STEP(phone_failure(owner + 1, phone, SV_STATE) == 0);
    STEP(shop->phone_idle(owner + 1, phone) == SV_STATE);
    STEP(catalog_failure(owner + 1, 0, SV_STATE) == 0);
    STEP(shop->catalog_replace(owner + 1, 0, NULL) == SV_STATE);
    STEP(phone_failure(owner, 0, SV_NOT_FOUND) == 0);
    STEP(shop->phone_idle(owner, 0) == SV_NOT_FOUND);
    STEP(phone_failure(owner, human, SV_NOT_FOUND) == 0);
    STEP(shop->phone_idle(owner, human) == SV_NOT_FOUND);
    STEP(phone_failure(owner, nonphone, SV_INVALID) == 0);
    STEP(shop->phone_idle(owner, nonphone) == SV_INVALID);
    STEP(shop->phone(owner, phone, NULL) == SV_INVALID);
    memset(&state, 0xa5, sizeof state);
    state.size = sizeof state - 1;
    old_state = state;
    STEP(shop->phone(owner, phone, &state) == SV_INVALID);
    STEP(!memcmp(&state, &old_state, sizeof state));
    STEP(shop->catalog(owner, 0, NULL) == SV_INVALID);
    memset(&out, 0xa5, sizeof out);
    out.size = sizeof out - 1;
    old_out = out;
    STEP(shop->catalog(owner, 0, &out) == SV_INVALID);
    STEP(!memcmp(&out, &old_out, sizeof out));
    STEP(shop->catalog_replace(owner, 0, NULL) == SV_INVALID);
    input.size = sizeof input - 1;
    STEP(shop->catalog_replace(owner, 0, &input) == SV_INVALID);
    input.size = sizeof input;
    STEP(catalog_failure(owner, SV_SHOP_BUILDINGS, SV_INVALID) == 0);
    STEP(shop->catalog_replace(owner, SV_SHOP_BUILDINGS, &input) == SV_INVALID);
    STEP(catalog_failure(owner, UINT32_MAX, SV_INVALID) == 0);
    STEP(shop->catalog_replace(owner, UINT32_MAX, &input) == SV_INVALID);
    STEP(catalog_failure(owner, 4, SV_NOT_FOUND) == 0);
    STEP(shop->catalog_replace(owner, 4, &input) == SV_NOT_FOUND);
    STEP(catalog_failure(owner, 3, SV_STATE) == 0);
    STEP(shop->catalog_replace(owner, 3, &input) == SV_STATE);
    STEP(shop->phone(owner, phone, &final_phone) == SV_OK);
    STEP(!memcmp(&initial_phone, &final_phone, sizeof initial_phone));
    STEP(shop->catalog(owner, 0, &final_catalog) == SV_OK);
    STEP(!memcmp(&initial_catalog, &final_catalog, sizeof initial_catalog));
    return 0;
}

static int replace_failure(SvOwner owner, uint32_t building, const SvShopCatalogWrite *input)
{
    SvShopCatalog before = {sizeof before}, after = {sizeof after};
    STEP(shop->catalog(owner, building, &before) == SV_OK);
    STEP(shop->catalog_replace(owner, building, input) == SV_INVALID);
    STEP(shop->catalog(owner, building, &after) == SV_OK);
    STEP(!memcmp(&before, &after, sizeof before));
    return 0;
}

static int replace_check(SvOwner owner, uint32_t building, uint32_t kind, uint32_t capacity,
    const SvShopCatalogWrite *input)
{
    SvShopCatalog out = {sizeof out};
    SvShopRow zero = {0};
    uint32_t i;
    STEP(shop->catalog_replace(owner, building, input) == SV_OK);
    STEP(shop->catalog(owner, building, &out) == SV_OK);
    STEP(out.size == sizeof out && out.kind == kind && out.capacity == capacity && out.count == input->count);
    STEP(!memcmp(out.rows, input->rows, input->count * sizeof(SvShopRow)));
    for (i = input->count; i < SV_SHOP_ROWS; ++i)
        STEP(!memcmp(&out.rows[i], &zero, sizeof zero));
    return 0;
}

static int catalogs(SvOwner owner)
{
    const uint32_t capacities[3] = {8, 10, 2};
    const uint32_t kinds[3] = {SV_CATALOG_VEHICLES, SV_CATALOG_ITEMS, SV_CATALOG_CLOTHES};
    SvShopCatalogWrite input = {sizeof input, 0, {{0}}};
    uint32_t building, i;
    for (building = 0; building < 3; ++building) {
        input.count = 0;
        STEP(replace_check(owner, building, kinds[building], capacities[building], &input) == 0);
        input.count = capacities[building];
        for (i = 0; i < input.count; ++i) {
            input.rows[i] = (SvShopRow){(int32_t)i, i ? SV_SHOP_PRICE_MAX : 0, 0};
            if (building == 0)
                input.rows[i].color = (int32_t)(i % 6);
        }
        if (building == 0)
            input.rows[7].type = 13;
        if (building == 1)
            input.rows[9].type = 26;
        STEP(replace_check(owner, building, kinds[building], capacities[building], &input) == 0);
        ++input.count;
        STEP(replace_failure(owner, building, &input) == 0);
    }
    input.count = 2;
    input.rows[0] = (SvShopRow){9, 777, 5};
    input.rows[1] = (SvShopRow){14, 0, 0};
    STEP(replace_failure(owner, 0, &input) == 0);
    input.rows[1] = (SvShopRow){-1, 0, 0};
    STEP(replace_failure(owner, 0, &input) == 0);
    input.rows[1] = (SvShopRow){0, -1, 0};
    STEP(replace_failure(owner, 0, &input) == 0);
    input.rows[1] = (SvShopRow){0, SV_SHOP_PRICE_MAX + 1, 0};
    STEP(replace_failure(owner, 0, &input) == 0);
    input.rows[1] = (SvShopRow){0, 0, -1};
    STEP(replace_failure(owner, 0, &input) == 0);
    input.rows[1].color = 6;
    STEP(replace_failure(owner, 0, &input) == 0);
    input.rows[0] = (SvShopRow){12, 0, 0};
    input.rows[1] = (SvShopRow){27, 0, 0};
    STEP(replace_failure(owner, 1, &input) == 0);
    input.rows[1] = (SvShopRow){-1, 0, 0};
    STEP(replace_failure(owner, 1, &input) == 0);
    input.rows[1] = (SvShopRow){26, 0, 1};
    STEP(replace_failure(owner, 1, &input) == 0);
    input.rows[0] = (SvShopRow){0, 0, 0};
    input.rows[1] = (SvShopRow){0, 0, 0};
    STEP(replace_failure(owner, 2, &input) == 0);
    input.rows[1] = (SvShopRow){1, 0, 1};
    STEP(replace_failure(owner, 2, &input) == 0);
    return 0;
}

static int idle(SvOwner owner)
{
    SvPhoneState before = {sizeof before}, after = {sizeof after}, peer = {sizeof peer};
    STEP(shop->phone(owner, phone, &before) == SV_OK);
    if (before.peer)
        STEP(shop->phone(owner, before.peer, &peer) == SV_OK);
    STEP(shop->phone_idle(owner, phone) == SV_OK);
    STEP(shop->phone(owner, phone, &after) == SV_OK);
    STEP(after.phone == phone && after.human == before.human && !after.peer);
    STEP(after.state == SV_PHONE_IDLE && !after.number && !after.info && !after.ring_ticks);
    STEP(after.cooldown == before.cooldown);
    if (before.peer) {
        STEP(shop->phone(owner, before.peer, &after) == SV_OK);
        STEP(after.state == SV_PHONE_IDLE && !after.peer && !after.number && !after.info && !after.ring_ticks);
        STEP(after.cooldown == peer.cooldown);
    }
    return 0;
}

static int command_checks(SvOwner owner, const char *arguments)
{
    SvShopCatalogWrite input = {sizeof input, 1, {{9, 456, 5}}};
    SvShopCatalog catalog = {sizeof catalog};
    SvPhoneState state = {sizeof state};
    STEP(refresh(owner) == 0);
    if (!strcmp(arguments, "idle"))
        return idle(owner);
    if (!strcmp(arguments, "badphone")) {
        STEP(phone_failure(owner, phone, SV_STATE) == 0);
        STEP(shop->phone_idle(owner, phone) == SV_STATE);
    } else if (!strcmp(arguments, "guard")) {
        STEP(shop->phone(owner, phone, &state) == SV_OK);
        STEP(shop->catalog(owner, 0, &catalog) == SV_OK);
        STEP(shop->phone_idle(owner, phone) == SV_STATE);
        STEP(shop->catalog_replace(owner, 0, &input) == SV_STATE);
    } else if (!strcmp(arguments, "gated")) {
        STEP(shop->catalog_replace(owner, 0, &input) == SV_STATE);
    } else if (!strcmp(arguments, "badcatalog")) {
        STEP(catalog_failure(owner, 0, SV_STATE) == 0);
    } else if (!strcmp(arguments, "badcity")) {
        STEP(catalog_failure(owner, 0, SV_STATE) == 0);
        STEP(shop->catalog_replace(owner, 0, &input) == SV_STATE);
    } else if (!strcmp(arguments, "replace")) {
        STEP(replace_check(owner, 0, SV_CATALOG_VEHICLES, 8, &input) == 0);
    } else if (!strcmp(arguments, "last")) {
        input.rows[0] = (SvShopRow){13, SV_SHOP_PRICE_MAX, 5};
        STEP(replace_check(owner, SV_SHOP_BUILDINGS - 1, SV_CATALOG_VEHICLES, 8, &input) == 0);
    } else if (!strcmp(arguments, "threshold")) {
        const void *table = NULL;
        const SvGameEngine *engine;
        SvEngineValue value = {sizeof value, SV_ENGINE_INT, {0}};
        value.value.integer = 9;
        STEP(saved_host->get_api(owner, SV_API_ENGINE, SV_GAME_ENGINE_VERSION, &table) == SV_OK);
        engine = table;
        STEP(engine->set(owner, "human.death_start", SV_ENGINE_GLOBAL, &value) == SV_OK);
    } else if (!strcmp(arguments, "stale")) {
        STEP(phone_failure(owner, first_phone, SV_NOT_FOUND) == 0);
        STEP(shop->phone_idle(owner, first_phone) == SV_NOT_FOUND);
    } else {
        return __LINE__;
    }
    return 0;
}

static SvResult SV_CALL command(SvOwner owner, uint64_t actor, const char *arguments)
{
    int line = command_checks(owner, arguments);
    char message[96];
    (void)actor;
    if (line) {
        snprintf(message, sizeof message, "Shop command %s failed at line %d.", arguments, line);
        saved_host->log(owner, message);
        return SV_FAILED;
    }
    return write_api->set_godmode(owner, player, 1);
}

int shop_fixture(const SvHost *host, SvOwner owner)
{
    const void *table = NULL;
    SvPhoneState state = {sizeof state};
    saved_host = host;
    STEP(host->get_api(owner, SV_API_SHOP, SV_GAME_SHOP_VERSION, &table) == SV_OK);
    shop = table;
    STEP(shop && shop->size >= sizeof *shop && shop->version == SV_GAME_SHOP_VERSION);
    STEP(host->get_api(owner, SV_API_GAME_READ, SV_GAME_READ_VERSION, &table) == SV_OK);
    read_api = table;
    STEP(host->get_api(owner, SV_API_GAME_WRITE, SV_GAME_WRITE_VERSION, &table) == SV_OK);
    write_api = table;
    STEP(refresh(owner) == 0);
    first_phone = phone;
    STEP(shop->phone(owner, phone, &state) == SV_OK);
    STEP(state.phone == phone && state.human == human && !state.peer && state.state == SV_PHONE_DIAL);
    STEP(state.number == 12 && state.info == 12 && state.cooldown == 17 && state.ring_ticks == 4);
    STEP(invalid_inputs(owner) == 0);
    STEP(catalogs(owner) == 0);
    STEP(idle(owner) == 0);
    STEP(host->command(owner, "shop.check", SV_COMMAND_LOCAL, command) == SV_OK);
    return 0;
}
