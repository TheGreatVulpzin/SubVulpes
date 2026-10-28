#include "systems_host.h"
#include "sv_game_shop.h"

enum { PASS, DENY, BAD_REPLY, IDLE, NUMBER, STATE, HAND, HEALTH, TTL, COOLDOWN,
    ITEM_GENERATION, HUMAN_GENERATION, HARMLESS };
static int phones[3], hand, write_allowed = 1, decision_action;
static uint32_t callback_count, expected_kind;
static SvEntity expected_phone, expected_human;
static int32_t expected_number;
static Item initial_phones[3];
static InventorySlot initial_inventory[8];

static void field(size_t offset, uint32_t building, int32_t value)
{
    memcpy((uint8_t *)&sr_data_city + offset + building * 0x9dcu, &value, sizeof value);
}

static void rows(uint32_t building, const SvShopRow *value, uint32_t count)
{
    memcpy((uint8_t *)&sr_data_city + offsetof(City, shop_rows) + building * 0x9dcu,
        value, count * sizeof *value);
}

static int write_context(SvOwner owner)
{
    return write_allowed && sv_plugins_context(owner);
}

static void command(const char *arguments)
{
    char text[80];
    snprintf(text, sizeof text, "shop.check %s", arguments);
    sr_players[world_player].godmode = 0;
    CHECK(sv_plugins_command(0, text));
    CHECK(sr_players[world_player].godmode == 1);
    CHECK(!sv_plugins_failed());
}

static void reset_phones(void)
{
    int i;
    for (i = 0; i < 3; ++i)
        sr_items[phones[i]] = initial_phones[i];
    memcpy(sr_humans[world_human].inventory, initial_inventory, sizeof initial_inventory);
    sr_humans[world_human].health = 100;
}

static void dial(void)
{
    reset_phones();
    sr_items[phones[0]].call_state = SV_PHONE_DIAL;
    sr_items[phones[0]].phone_number = 1234;
    sr_items[phones[0]].info = 1234;
    sr_items[phones[0]].cooldown = 0;
    sr_items[phones[0]].ring_ticks = 0;
}

static void graph(int state, int incoming)
{
    Item *a, *b;
    reset_phones();
    a = &sr_items[phones[0]];
    b = &sr_items[phones[1]];
    a->call_state = b->call_state = state;
    a->phone_number = 4321;
    b->phone_number = 5678;
    a->info = 7002;
    b->info = 7001;
    a->ring_ticks = 32;
    b->ring_ticks = 48;
    a->cooldown = 17;
    b->cooldown = 29;
    a->linked_item = incoming ? -1 : phones[1];
    b->linked_item = incoming || state == SV_PHONE_TALK ? phones[0] : -1;
}

static void bad_phone(void)
{
    Item before[4];
    int i;
    for (i = 0; i < 3; ++i)
        before[i] = sr_items[phones[i]];
    before[3] = sr_items[world_item];
    command("badphone");
    for (i = 0; i < 3; ++i)
        CHECK(!memcmp(&before[i], &sr_items[phones[i]], sizeof(Item)));
    CHECK(!memcmp(&before[3], &sr_items[world_item], sizeof(Item)));
}

static void idle_fields(int item, int cooldown)
{
    const Item *p = &sr_items[item];
    CHECK(p->call_state == SV_PHONE_IDLE && !p->phone_number && !p->info && !p->ring_ticks);
    CHECK(p->linked_item == -1 && p->cooldown == cooldown);
}

static void idle_event(int item, int line)
{
    uint32_t i;
    for (i = 0; i < sr_numofevents; ++i)
        if (sr_events[i].type == 6 && sr_events[i].param == item && sr_events[i].attach_type == SV_PHONE_IDLE &&
            sr_events[i].attach_id == 0 && sr_events[i].range_mode == line)
            return;
    CHECK(0);
}

static void phone_graphs(void)
{
    uint32_t i;
    const size_t offsets[] = {offsetof(Item, call_state), offsetof(Item, phone_number), offsetof(Item, phone_number),
        offsetof(Item, info), offsetof(Item, info), offsetof(Item, phone_line), offsetof(Item, phone_line),
        offsetof(Item, cooldown), offsetof(Item, cooldown), offsetof(Item, ring_ticks), offsetof(Item, ring_ticks)};
    const int32_t bad[] = {4, -1, 10000, -1, 65536, -1, 4, -1, 181, -1, 769};
    for (i = 0; i < 3; ++i) {
        graph(i == 2 ? SV_PHONE_TALK : SV_PHONE_RINGING, i == 1);
        command("idle");
        idle_fields(phones[0], 17);
        idle_fields(phones[1], 29);
        idle_event(phones[0], 0);
        idle_event(phones[1], 0);
    }
    dial();
    sr_items[phones[0]].phone_number = 9999;
    sr_items[phones[0]].info = 65535;
    sr_items[phones[0]].phone_line = 3;
    sr_items[phones[0]].cooldown = 180;
    sr_items[phones[0]].ring_ticks = 768;
    command("idle");
    idle_fields(phones[0], 180);
    CHECK(sr_items[phones[0]].phone_line == 3);
    CHECK(sr_items[phones[0]].phone_id == 7001);
    idle_event(phones[0], 3);
    for (i = 0; i < sizeof bad / sizeof bad[0]; ++i) {
        dial();
        memcpy((uint8_t *)&sr_items[phones[0]] + offsets[i], &bad[i], sizeof bad[i]);
        bad_phone();
    }
    graph(SV_PHONE_RINGING, 0);
    sr_items[phones[0]].linked_item = SR_MAX_ITEM;
    bad_phone();
    graph(SV_PHONE_RINGING, 0);
    sr_items[phones[0]].linked_item = (int32_t)(0x90000000u / sizeof(Item));
    bad_phone();
    graph(SV_PHONE_RINGING, 0);
    sr_items[phones[0]].linked_item = world_item;
    bad_phone();
    graph(SV_PHONE_TALK, 0);
    sr_items[phones[1]].linked_item = -1;
    bad_phone();
    graph(SV_PHONE_RINGING, 1);
    sr_items[phones[2]].call_state = SV_PHONE_RINGING;
    sr_items[phones[2]].linked_item = phones[0];
    bad_phone();
    dial();
    sr_items[world_item].linked_item = phones[0];
    bad_phone();
    sr_items[world_item].linked_item = -1;
    graph(SV_PHONE_TALK, 0);
    sr_items[phones[1]].parent_human = SR_MAX_HUMAN;
    bad_phone();
    dial();
    sr_items[phones[0]].parent_human = SR_MAX_HUMAN;
    bad_phone();
    dial();
    sr_items[phones[0]].parent_human = (int32_t)(0x90000000u / sizeof(Human));
    bad_phone();
    dial();
    sr_items[phones[0]].parent_human = -1;
    sr_items[phones[0]].parent_item = (int32_t)(0x90000000u / sizeof(Item));
    bad_phone();
    dial();
    sr_items[phones[0]].parent_slot = 8;
    bad_phone();
    dial();
    sr_items[phones[0]].parent_slot = -1;
    bad_phone();
    dial();
    sr_items[phones[0]].parent_item = phones[0];
    bad_phone();
    dial();
    sr_humans[world_human].inventory[hand].count = 0;
    bad_phone();
    dial();
    sr_humans[world_human].inventory[hand].count = 9;
    bad_phone();
    dial();
    sr_humans[world_human].inventory[3].items[0] = phones[0];
    sr_humans[world_human].inventory[3].count = 1;
    bad_phone();
    dial();
}

static SvResult decide(const SvEvent *base)
{
    const SvPhoneEvent *event;
    CHECK(base && base->size >= sizeof(SvPhoneEvent));
    event = (const SvPhoneEvent *)base;
    CHECK(base->kind == expected_kind && base->tick == (uint32_t)sr_game.ticks);
    CHECK(event->phone == expected_phone && event->human == expected_human && event->number == expected_number);
    ++callback_count;
    switch (decision_action) {
    case DENY: return SV_DENIED;
    case BAD_REPLY: return SV_INVALID;
    case IDLE: command("idle"); break;
    case NUMBER: ++sr_items[phones[0]].phone_number; break;
    case STATE: sr_items[phones[0]].call_state = SV_PHONE_IDLE; break;
    case HAND: CHECK(linkitem((uint32_t)phones[0], UINT32_MAX, world_human, hand ^ 1)); break;
    case HEALTH: sr_humans[world_human].health = sv_engine_integer(SV_ENGINE_DEATH_START, 0); break;
    case TTL: sr_items[phones[0]].time_to_live = 0; break;
    case COOLDOWN: sr_items[phones[0]].cooldown = 1; break;
    case ITEM_GENERATION:
        sv_entity_deleted(SV_ENTITY_ITEM, (uint32_t)phones[0]);
        sv_entity_created(SV_ENTITY_ITEM, (uint32_t)phones[0]);
        break;
    case HUMAN_GENERATION:
        sv_entity_deleted(SV_ENTITY_HUMAN, (uint32_t)world_human);
        sv_entity_created(SV_ENTITY_HUMAN, (uint32_t)world_human);
        break;
    case HARMLESS:
        sr_humans[world_human].health = 99;
        sr_items[phones[0]].time_to_live = 99;
        break;
    default: break;
    }
    return SV_OK;
}

static void ask(uint32_t kind, int action, SvResult expected)
{
    uint32_t before = callback_count;
    Item old_phone = sr_items[phones[0]], old_peer = sr_items[phones[1]];
    expected_phone = sv_entity_at(SV_ENTITY_ITEM, phones[0]);
    expected_human = sv_entity_at(SV_ENTITY_HUMAN, world_human);
    expected_number = sr_items[phones[0]].phone_number;
    expected_kind = kind == SV_PHONE_DIGIT_DECISION ? 256u : 512u;
    decision_action = action;
    CHECK(sv_shop_phone_decision(kind, expected_phone, expected_human) == expected);
    CHECK(callback_count == before + 1);
    if (action == PASS || action == DENY || action == BAD_REPLY) {
        CHECK(!memcmp(&old_phone, &sr_items[phones[0]], sizeof old_phone));
        CHECK(!memcmp(&old_peer, &sr_items[phones[1]], sizeof old_peer));
    }
}

static void reject(uint32_t kind, SvEntity phone, SvEntity human, SvResult expected)
{
    uint32_t before = callback_count;
    CHECK(sv_shop_phone_decision(kind, phone, human) == expected);
    CHECK(callback_count == before);
}

static DWORD WINAPI wrong_thread(void *unused)
{
    (void)unused;
    CHECK(sv_shop_phone_decision(SV_PHONE_CALL_DECISION, expected_phone, expected_human) == SV_STATE);
    CHECK(sv_shop_phone_retire(expected_phone) == SV_STATE);
    return 0;
}

static void decisions(void)
{
    SvEntity p, h, other = 0;
    HANDLE thread;
    DWORD exit_code;
    int i, other_health;
    uint32_t before;
    const int mutations[] = {IDLE, NUMBER, STATE, HAND, HEALTH, TTL, COOLDOWN, ITEM_GENERATION, HUMAN_GENERATION};
    dial();
    ask(SV_PHONE_DIGIT_DECISION, PASS, SV_OK);
    ask(SV_PHONE_CALL_DECISION, PASS, SV_OK);
    ask(SV_PHONE_CALL_DECISION, DENY, SV_DENIED);
    ask(SV_PHONE_CALL_DECISION, BAD_REPLY, SV_STATE);
    thread = CreateThread(NULL, 0, wrong_thread, NULL, 0, NULL);
    CHECK(thread != NULL);
    before = callback_count;
    CHECK(WaitForSingleObject(thread, 5000) == WAIT_OBJECT_0);
    CHECK(GetExitCodeThread(thread, &exit_code) && exit_code == 0);
    CHECK(CloseHandle(thread));
    CHECK(callback_count == before);
    p = sv_entity_at(SV_ENTITY_ITEM, phones[0]);
    h = sv_entity_at(SV_ENTITY_HUMAN, world_human);
    reject(0, p, h, SV_INVALID);
    reject(3, p, h, SV_INVALID);
    reject(SV_PHONE_CALL_DECISION, 0, h, SV_NOT_FOUND);
    reject(SV_PHONE_CALL_DECISION, h, h, SV_NOT_FOUND);
    reject(SV_PHONE_CALL_DECISION, sv_entity_at(SV_ENTITY_ITEM, world_item), h, SV_INVALID);
    reject(SV_PHONE_CALL_DECISION, p, 0, SV_NOT_FOUND);
    for (i = 0; i < SR_MAX_HUMAN; ++i)
        if (i != world_human && sr_humans[i].active && sr_humans[i].inventory[hand].count == 0) {
            other = sv_entity_at(SV_ENTITY_HUMAN, i);
            break;
        }
    CHECK(other != 0);
    other_health = sr_humans[i].health;
    sr_humans[i].health = 100;
    CHECK(linkitem((uint32_t)phones[1], UINT32_MAX, i, hand));
    reject(SV_PHONE_CALL_DECISION, p, other, SV_STATE);
    CHECK(linkitem((uint32_t)phones[1], UINT32_MAX, -1, 0));
    sr_humans[i].health = other_health;
    sr_items[phones[0]].phone_number = 999;
    reject(SV_PHONE_CALL_DECISION, p, h, SV_STATE);
    ask(SV_PHONE_DIGIT_DECISION, PASS, SV_OK);
    sr_items[phones[0]].phone_number = 0;
    ask(SV_PHONE_DIGIT_DECISION, PASS, SV_OK);
    sr_items[phones[0]].phone_number = 1000;
    ask(SV_PHONE_CALL_DECISION, PASS, SV_OK);
    sr_items[phones[0]].phone_number = 9999;
    ask(SV_PHONE_CALL_DECISION, PASS, SV_OK);
    sr_items[phones[0]].phone_number = -1;
    reject(SV_PHONE_DIGIT_DECISION, p, h, SV_STATE);
    sr_items[phones[0]].phone_number = 10000;
    reject(SV_PHONE_DIGIT_DECISION, p, h, SV_STATE);
    dial();
    sr_items[phones[0]].cooldown = 1;
    reject(SV_PHONE_CALL_DECISION, p, h, SV_STATE);
    dial();
    sr_items[phones[0]].time_to_live = 0;
    reject(SV_PHONE_CALL_DECISION, p, h, SV_STATE);
    dial();
    sr_items[phones[0]].call_state = SV_PHONE_IDLE;
    reject(SV_PHONE_CALL_DECISION, p, h, SV_STATE);
    dial();
    sr_humans[world_human].health = sv_engine_integer(SV_ENGINE_DEATH_START, 0);
    reject(SV_PHONE_CALL_DECISION, p, h, SV_STATE);
    dial();
    CHECK(linkitem((uint32_t)phones[0], UINT32_MAX, -1, 0));
    reject(SV_PHONE_CALL_DECISION, p, h, SV_STATE);
    dial();
    CHECK(linkitem((uint32_t)phones[0], UINT32_MAX, world_human, 3));
    reject(SV_PHONE_CALL_DECISION, p, h, SV_STATE);
    dial();
    sr_humans[world_human].inventory[hand].items[1] = world_item;
    sr_humans[world_human].inventory[hand].count = 2;
    reject(SV_PHONE_CALL_DECISION, p, h, SV_STATE);
    dial();
    sr_items[phones[0]].parent_human = SR_MAX_HUMAN;
    reject(SV_PHONE_CALL_DECISION, p, h, SV_STATE);
    for (i = 0; i < (int)(sizeof mutations / sizeof mutations[0]); ++i) {
        dial();
        ask(SV_PHONE_CALL_DECISION, mutations[i], SV_DENIED);
    }
    dial();
    ask(SV_PHONE_CALL_DECISION, HARMLESS, SV_OK);
    command("stale");
    command("threshold");
    CHECK(sv_engine_integer(SV_ENGINE_DEATH_START, 0) == 9);
    dial();
    p = sv_entity_at(SV_ENTITY_ITEM, phones[0]);
    h = sv_entity_at(SV_ENTITY_HUMAN, world_human);
    sr_humans[world_human].health = 9;
    reject(SV_PHONE_CALL_DECISION, p, h, SV_STATE);
    sr_humans[world_human].health = 10;
    ask(SV_PHONE_CALL_DECISION, PASS, SV_OK);
    sv_engine_release_owner(1);
    CHECK(sv_engine_integer(SV_ENGINE_DEATH_START, 0) == 0);
    graph(SV_PHONE_TALK, 0);
    sr_items[phones[0]].time_to_live = 0;
    CHECK(sv_shop_phone_retire(sv_entity_at(SV_ENTITY_ITEM, phones[0])) == SV_OK);
    idle_fields(phones[0], 17);
    idle_fields(phones[1], 29);
    CHECK(sv_shop_phone_retire(0) == SV_NOT_FOUND);
    CHECK(sv_shop_phone_retire(sv_entity_at(SV_ENTITY_ITEM, world_item)) == SV_INVALID);
    dial();
}

void shop_prepare(void)
{
    SvPhoneState out = {sizeof out};
    SvShopCatalog catalog = {sizeof catalog};
    SvShopCatalogWrite input = {sizeof input, 0, {{0}}};
    const vec3 position = {20, 20, 20};
    const vec3 rotation[3] = {{1, 0, 0}, {0, 1, 0}, {0, 0, 1}};
    const SvShopRow initial_rows[3] = {{9, 100, 2}, {12, 200, 0}, {0, 5000, 0}};
    int i;
    CHECK(sv_game_shop.phone(1, 0, &out) == SV_STATE);
    CHECK(sv_game_shop.phone_idle(1, 0) == SV_STATE);
    CHECK(sv_game_shop.catalog(1, 0, &catalog) == SV_STATE);
    CHECK(sv_game_shop.catalog_replace(1, 0, &input) == SV_STATE);
    CHECK(sv_shop_phone_decision(1, 0, 0) == SV_STATE);
    CHECK(sv_shop_phone_retire(0) == SV_STATE);
    CHECK(sv_shop_props_install() == SV_STATE);
    CHECK(!sv_shop_install(NULL, write_context, 256, 512, 3));
    CHECK(!sv_shop_install(decide, NULL, 256, 512, 3));
    CHECK(!sv_shop_install(decide, write_context, 1, 512, 3));
    CHECK(!sv_shop_install(decide, write_context, 256, 256, 3));
    CHECK(!sv_shop_install(decide, write_context, 256, 512, 4));
    CHECK(sv_shop_install(decide, write_context, 256, 512, 3));
    sr_itemtypes[23].inv_inertia = (vec3){1, 1, 1};
    for (i = 0; i < 3; ++i) {
        phones[i] = (int)createitem(23, &position, NULL, rotation);
        CHECK(phones[i] >= 0);
        sr_items[phones[i]].phone_id = 7001 + i;
    }
    for (hand = 0; hand < 2; ++hand)
        if (sr_humans[world_human].inventory[hand].count == 0)
            break;
    CHECK(hand < 2);
    CHECK(linkitem((uint32_t)phones[0], UINT32_MAX, world_human, hand));
    sr_items[phones[0]].call_state = SV_PHONE_DIAL;
    sr_items[phones[0]].phone_number = sr_items[phones[0]].info = 12;
    sr_items[phones[0]].cooldown = 17;
    sr_items[phones[0]].ring_ticks = 4;
    for (i = 0; i < 3; ++i)
        initial_phones[i] = sr_items[phones[i]];
    memcpy(initial_inventory, sr_humans[world_human].inventory, sizeof initial_inventory);
    sr_data_city.num_buildings = 4;
    for (i = 0; i < 4; ++i) {
        field(offsetof(City, building_type), (uint32_t)i, i == 0 ? 2 : i == 1 ? 7 : i == 2 ? 4 : 5);
        field(offsetof(City, shop_count), (uint32_t)i, i < 3 ? 1 : 0);
        if (i < 3)
            rows((uint32_t)i, &initial_rows[i], 1);
    }
}

void shop_verify(void)
{
    uint32_t before[4] = {0}, after[4] = {0};
    int money, i;
    SvShopRow invalid = {14, 0, 0};
    phone_graphs();
    write_allowed = 0;
    command("guard");
    write_allowed = 1;
    CHECK(sv_shop_install(decide, write_context, 256, 512, 0));
    command("gated");
    reject(SV_PHONE_CALL_DECISION, sv_entity_at(SV_ENTITY_ITEM, phones[0]),
        sv_entity_at(SV_ENTITY_HUMAN, world_human), SV_STATE);
    CHECK(sv_shop_install(decide, write_context, 256, 512, 3));
    for (i = 0; i < SR_MAX_PLAYER; ++i) before[0] += sr_players[i].active != 0;
    for (i = 0; i < SR_MAX_HUMAN; ++i) before[1] += sr_humans[i].active != 0;
    for (i = 0; i < SR_MAX_ITEM; ++i) before[2] += sr_items[i].active != 0;
    for (i = 0; i < SR_MAX_OBJECT; ++i) before[3] += sr_objects[i].active != 0;
    money = sr_players[world_player].money;
    command("replace");
    for (i = 0; i < SR_MAX_PLAYER; ++i) after[0] += sr_players[i].active != 0;
    for (i = 0; i < SR_MAX_HUMAN; ++i) after[1] += sr_humans[i].active != 0;
    for (i = 0; i < SR_MAX_ITEM; ++i) after[2] += sr_items[i].active != 0;
    for (i = 0; i < SR_MAX_OBJECT; ++i) after[3] += sr_objects[i].active != 0;
    CHECK(!memcmp(before, after, sizeof before) && sr_players[world_player].money == money);
    field(offsetof(City, shop_count), 0, -1);
    command("badcatalog");
    command("replace");
    field(offsetof(City, shop_count), 0, 9);
    command("badcatalog");
    command("replace");
    rows(0, &invalid, 1);
    command("badcatalog");
    command("replace");
    sr_data_city.num_buildings = -1;
    command("badcity");
    sr_data_city.num_buildings = SV_SHOP_BUILDINGS + 1;
    command("badcity");
    sr_data_city.num_buildings = SV_SHOP_BUILDINGS;
    field(offsetof(City, building_type), SV_SHOP_BUILDINGS - 1, 2);
    field(offsetof(City, shop_count), SV_SHOP_BUILDINGS - 1, 0);
    command("last");
    sr_data_city.num_buildings = 4;
    decisions();
    puts("SHOP_PASS");
}
