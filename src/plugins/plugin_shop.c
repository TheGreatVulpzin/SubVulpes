#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include "plugin_host.h"
#include "plugin_actions.h"
#include "plugin_entities.h"
#include "sv_game_shop.h"
#include "plugin_game_internal.h"
#include "items.h"
#include "events.h"
#include "sr_runtime.h"
#include <string.h>

/* The shared property registry owns the phone scalar rows. */
#ifdef SV_API_GAME_PROPS
#include "plugin_props.h"
#endif

enum { BUILDING_STRIDE = 0x9dc };
SR_STATIC_ASSERT(sizeof(SvShopRow) == 12, "Shop row representation");
SR_STATIC_ASSERT(offsetof(City, shop_rows) + (SV_SHOP_BUILDINGS - 1) * BUILDING_STRIDE +
    SV_SHOP_ROWS * sizeof(SvShopRow) <= sizeof(City), "Shop building backing storage");

static SvShopDecide decide_event;
static SvShopWriteContext can_write;
static uint32_t digit_event, call_event, features;
static DWORD server_thread;

static SvResult context(SvOwner owner, int write)
{
    if (!sv_plugins_context(owner) || !sv_game_is_ready())
        return SV_STATE;
    if (write && (!can_write || !can_write(owner)))
        return SV_STATE;
    return SV_OK;
}

static int basic_phone(int32_t index)
{
    const Item *item;
    if ((uint32_t)index >= SR_MAX_ITEM)
        return 0;
    item = &sr_items[index];
    return item->active && item->type == ITEM_CELL_PHONE &&
        item->call_state >= SV_PHONE_IDLE && item->call_state <= SV_PHONE_TALK &&
        item->phone_number >= 0 && item->phone_number <= 9999 && item->info >= 0 && item->info <= 65535 &&
        item->phone_line >= 0 && item->phone_line <= 3 && item->cooldown >= 0 && item->cooldown <= 180 &&
        item->ring_ticks >= 0 && item->ring_ticks <= 768;
}

static int incoming(int32_t item, int32_t *caller)
{
    int32_t i;
    *caller = -1;
    for (i = 0; i < SR_MAX_ITEM; ++i)
        if (sr_items[i].active && sr_items[i].linked_item == item) {
            if (i == item || *caller != -1 || !basic_phone(i))
                return 0;
            *caller = i;
        }
    return 1;
}

/* Ringing has one outgoing link. Talking has two reciprocal links. */
static SvResult graph(int32_t item, int32_t *peer)
{
    int32_t caller, other_caller, linked;
    const Item *phone = &sr_items[item];
    if (!basic_phone(item) || !incoming(item, &caller))
        return SV_STATE;
    linked = phone->linked_item;
    *peer = -1;
    if (phone->call_state <= SV_PHONE_DIAL)
        return linked == -1 && caller == -1 ? SV_OK : SV_STATE;
    if (linked != -1) {
        if (linked == item || !basic_phone(linked) || !incoming(linked, &other_caller) || other_caller != item)
            return SV_STATE;
        if (phone->call_state == SV_PHONE_TALK) {
            if (sr_items[linked].call_state != SV_PHONE_TALK || sr_items[linked].linked_item != item || caller != linked)
                return SV_STATE;
        } else if (sr_items[linked].call_state != SV_PHONE_RINGING || sr_items[linked].linked_item != -1 || caller != -1) {
            return SV_STATE;
        }
        *peer = linked;
        return SV_OK;
    }
    if (phone->call_state != SV_PHONE_RINGING || caller == -1 || sr_items[caller].call_state != SV_PHONE_RINGING ||
        !incoming(caller, &other_caller) || other_caller != -1)
        return SV_STATE;
    *peer = caller;
    return SV_OK;
}

static SvResult holder(int32_t item, SvEntity *human)
{
    const Item *phone = &sr_items[item];
    int32_t parent = phone->parent_human, slot = phone->parent_slot, i, found = 0;
    *human = 0;
    if (parent == -1) {
        int32_t container = phone->parent_item;
        if (container == -1)
            return SV_OK;
        if ((uint32_t)container >= SR_MAX_ITEM || container == item || !sr_items[container].active ||
            slot < 0 || slot >= 8 || sr_items[container].num_children < 0 || sr_items[container].num_children > 8 ||
            slot >= sr_items[container].num_children || sr_items[container].children[slot] != item)
            return SV_STATE;
        for (i = 0; i < sr_items[container].num_children; ++i)
            if (sr_items[container].children[i] == item)
                ++found;
        return found == 1 ? SV_OK : SV_STATE;
    }
    if (phone->parent_item != -1 || (uint32_t)parent >= SR_MAX_HUMAN || !sr_humans[parent].active ||
        (uint32_t)slot >= 8)
        return SV_STATE;
    for (i = 0; i < 8; ++i) {
        const InventorySlot *inventory = &sr_humans[parent].inventory[i];
        int32_t j;
        if (inventory->count < 0 || inventory->count > 8)
            return SV_STATE;
        for (j = 0; j < inventory->count; ++j)
            if (inventory->items[j] == item) {
                if (i != slot)
                    return SV_STATE;
                ++found;
            }
    }
    if (found != 1)
        return SV_STATE;
    *human = sv_entity_at(SV_ENTITY_HUMAN, parent);
    return *human ? SV_OK : SV_STATE;
}

static SvResult resolve_phone(SvEntity reference, int32_t *item, int32_t *peer, SvEntity *human)
{
    SvResult result;
    SvEntity peer_human;
    *item = sv_entity_resolve(SV_ENTITY_ITEM, reference);
    if (*item < 0)
        return SV_NOT_FOUND;
    if (sr_items[*item].type != ITEM_CELL_PHONE)
        return SV_INVALID;
    result = graph(*item, peer);
    if (result != SV_OK)
        return result;
    result = holder(*item, human);
    if (result != SV_OK || *peer == -1)
        return result;
    if (!sv_entity_at(SV_ENTITY_ITEM, *peer))
        return SV_STATE;
    return holder(*peer, &peer_human);
}

static void idle_one(int32_t item)
{
    sr_items[item].call_state = SV_PHONE_IDLE;
    sr_items[item].phone_number = 0;
    sr_items[item].linked_item = -1;
    sr_items[item].info = 0;
    sr_items[item].ring_ticks = 0;
    createevent_updateiteminfo((uint32_t)item);
}

static void idle_apply(int32_t item, int32_t peer)
{
    if (peer != -1)
        idle_one(peer);
    idle_one(item);
}

static SvResult SV_CALL phone(SvOwner owner, SvEntity reference, SvPhoneState *out)
{
    int32_t item, peer;
    SvEntity human;
    SvPhoneState value;
    SvResult result = context(owner, 0);
    if (result != SV_OK)
        return result;
    if (!out || out->size < sizeof *out)
        return SV_INVALID;
    result = resolve_phone(reference, &item, &peer, &human);
    if (result != SV_OK)
        return result;
    memset(&value, 0, sizeof value);
    value.size = sizeof value;
    value.phone = reference;
    value.human = human;
    value.peer = peer == -1 ? 0 : sv_entity_at(SV_ENTITY_ITEM, peer);
    value.state = sr_items[item].call_state;
    value.number = sr_items[item].phone_number;
    value.info = sr_items[item].info;
    value.cooldown = sr_items[item].cooldown;
    value.ring_ticks = sr_items[item].ring_ticks;
    *out = value;
    return SV_OK;
}

static SvResult SV_CALL phone_idle(SvOwner owner, SvEntity reference)
{
    int32_t item, peer;
    SvEntity human;
    SvResult result = context(owner, 1);
    if (result != SV_OK)
        return result;
    result = resolve_phone(reference, &item, &peer, &human);
    if (result == SV_OK)
        idle_apply(item, peer);
    return result;
}

/* The phones that wait for a decision of the plugins. */
static SvEntity digit_waiting[SR_MAX_ITEM], call_phone[SR_MAX_ITEM];
static int32_t call_number[SR_MAX_ITEM];
static uint32_t digit_count;

SvResult sv_shop_phone_retire(SvEntity reference)
{
    int32_t item, peer;
    SvEntity human;
    SvResult result;
    if (!can_write || !sv_game_is_ready() || GetCurrentThreadId() != server_thread)
        return SV_STATE;
    result = resolve_phone(reference, &item, &peer, &human);
    if (result == SV_OK) {
        if (digit_waiting[item]) {
            digit_waiting[item] = 0;
            --digit_count;
        }
        call_phone[item] = 0;
        sv_action_cancel(call_event, item);
        idle_apply(item, peer);
    }
    return result;
}

static void *building_field(size_t offset, uint32_t building)
{
    return (uint8_t *)&sr_data_city + offset + building * BUILDING_STRIDE;
}

static SvResult catalog_location(uint32_t building, uint32_t *kind, uint32_t *capacity)
{
    int32_t native_kind;
    if (building >= SV_SHOP_BUILDINGS)
        return SV_INVALID;
    if (sr_data_city.num_buildings < 0 || sr_data_city.num_buildings > (int32_t)SV_SHOP_BUILDINGS)
        return SV_STATE;
    if (building >= (uint32_t)sr_data_city.num_buildings)
        return SV_NOT_FOUND;
    memcpy(&native_kind, building_field(offsetof(City, building_type), building), sizeof native_kind);
    switch (native_kind) {
    case 2: *kind = SV_CATALOG_VEHICLES; *capacity = 8; return SV_OK;
    case 7: *kind = SV_CATALOG_ITEMS; *capacity = 10; return SV_OK;
    case 4: *kind = SV_CATALOG_CLOTHES; *capacity = 2; return SV_OK;
    default: return SV_STATE;
    }
}

static int row_valid(uint32_t kind, uint32_t index, const SvShopRow *row)
{
    if (row->price < 0 || row->price > SV_SHOP_PRICE_MAX || row->type < 0 || row->color < 0)
        return 0;
    if (kind == SV_CATALOG_VEHICLES)
        return row->type < 14 && row->color <= 5;
    if (kind == SV_CATALOG_ITEMS)
        return row->type < NUM_ITEM_TYPES && row->color == 0;
    return row->type == (int32_t)index && row->color == 0;
}

static SvResult SV_CALL catalog(SvOwner owner, uint32_t building, SvShopCatalog *out)
{
    SvShopCatalog value;
    int32_t count;
    uint32_t i;
    SvResult result = context(owner, 0);
    if (result != SV_OK)
        return result;
    if (!out || out->size < sizeof *out)
        return SV_INVALID;
    memset(&value, 0, sizeof value);
    result = catalog_location(building, &value.kind, &value.capacity);
    if (result != SV_OK)
        return result;
    memcpy(&count, building_field(offsetof(City, shop_count), building), sizeof count);
    if (count < 0 || (uint32_t)count > value.capacity)
        return SV_STATE;
    value.size = sizeof value;
    value.row_size = sizeof(SvShopRow);
    value.count = (uint32_t)count;
    for (i = 0; i < value.count; ++i) {
        memcpy(&value.rows[i], (const uint8_t *)building_field(offsetof(City, shop_rows), building) + i * sizeof(SvShopRow),
            sizeof(SvShopRow));
        if (!row_valid(value.kind, i, &value.rows[i]))
            return SV_STATE;
    }
    *out = value;
    return SV_OK;
}

static SvResult SV_CALL catalog_replace(SvOwner owner, uint32_t building, const SvShopCatalogWrite *input)
{
    SvShopRow candidate[SV_SHOP_ROWS] = {{0}};
    uint32_t kind, capacity, i;
    int32_t count;
    SvResult result = context(owner, 1);
    if (result != SV_OK)
        return result;
    if (!input || input->size < sizeof *input)
        return SV_INVALID;
    result = catalog_location(building, &kind, &capacity);
    if (result != SV_OK)
        return result;
    if (input->count > capacity)
        return SV_INVALID;
    for (i = 0; i < input->count; ++i) {
        if (!row_valid(kind, i, &input->rows[i]))
            return SV_INVALID;
        candidate[i] = input->rows[i];
    }
    if (!(features & SV_SHOP_SAFE_PURCHASES))
        return SV_STATE;
    count = (int32_t)input->count;
    memcpy(building_field(offsetof(City, shop_rows), building), candidate, capacity * sizeof(SvShopRow));
    memcpy(building_field(offsetof(City, shop_count), building), &count, sizeof count);
    return SV_OK;
}

int sv_shop_install(SvShopDecide decide, SvShopWriteContext write_context, uint32_t digit_kind,
    uint32_t call_kind, uint32_t installed_features)
{
    if (!decide || !write_context || digit_kind != 256u || call_kind != 512u ||
        (installed_features & ~(SV_SHOP_SAFE_DECISIONS | SV_SHOP_SAFE_PURCHASES)))
        return 0;
#ifdef SV_API_GAME_PROPS
    if (sv_shop_props_install() != SV_OK)
        return 0;
#endif
    decide_event = decide;
    can_write = write_context;
    digit_event = digit_kind;
    call_event = call_kind;
    features = installed_features;
    server_thread = GetCurrentThreadId();
    return 1;
}

SvResult sv_shop_phone_decision(uint32_t decision, SvEntity reference, SvEntity human)
{
    SvPhoneEvent event;
    SvEntity holder_ref;
    int32_t item, peer, body, number, hand, after_item, after_peer;
    SvResult result;
    if (!decide_event || !(features & SV_SHOP_SAFE_DECISIONS) || !sv_game_is_ready() ||
        GetCurrentThreadId() != server_thread)
        return SV_STATE;
    if (decision != SV_PHONE_DIGIT_DECISION && decision != SV_PHONE_CALL_DECISION)
        return SV_INVALID;
    body = sv_entity_resolve(SV_ENTITY_HUMAN, human);
    if (body < 0)
        return SV_NOT_FOUND;
    result = resolve_phone(reference, &item, &peer, &holder_ref);
    if (result != SV_OK)
        return result;
    number = sr_items[item].phone_number;
    hand = sr_items[item].parent_slot;
    if (holder_ref != human || sr_items[item].parent_slot > 1 || sr_humans[body].inventory[sr_items[item].parent_slot].count != 1 ||
        sr_humans[body].health <= sv_engine_integer(SV_ENGINE_DEATH_START, 0) ||
        sr_items[item].call_state != SV_PHONE_DIAL || sr_items[item].cooldown != 0 || sr_items[item].time_to_live <= 0 ||
        (decision == SV_PHONE_CALL_DECISION && number < 1000))
        return SV_STATE;
    memset(&event, 0, sizeof event);
    event.base = (SvEvent){sizeof event, decision == SV_PHONE_DIGIT_DECISION ? digit_event : call_event, (uint32_t)sr_game.ticks};
    event.phone = reference;
    event.number = number;
    event.human = human;
    result = decide_event(&event.base);
    if (result == SV_DENIED)
        return result;
    if (result != SV_OK)
        return SV_STATE;
    if (sv_entity_resolve(SV_ENTITY_HUMAN, human) != body ||
        resolve_phone(reference, &after_item, &after_peer, &holder_ref) != SV_OK || after_item != item || holder_ref != human ||
        sr_items[item].parent_slot != hand || sr_humans[body].inventory[sr_items[item].parent_slot].count != 1 ||
        sr_humans[body].health <= sv_engine_integer(SV_ENGINE_DEATH_START, 0) ||
        sr_items[item].call_state != SV_PHONE_DIAL || sr_items[item].cooldown != 0 || sr_items[item].time_to_live <= 0 ||
        sr_items[item].phone_number != number)
        return SV_DENIED;
    return SV_OK;
}

static int decisions_open(uint32_t kind)
{
    return decide_event && (features & SV_SHOP_SAFE_DECISIONS) && sv_plugins_wants(kind);
}

static SvResult deliver_call(int32_t item)
{
    int32_t resolved, peer;
    SvEntity reference = call_phone[item], human;
    SvResult result;
    if (!reference || resolve_phone(reference, &resolved, &peer, &human) != SV_OK || resolved != item ||
        sr_items[item].phone_number != call_number[item])
        return SV_NOT_FOUND;
    result = sv_shop_phone_decision(SV_PHONE_CALL_DECISION, reference, human);
    if (result == SV_DENIED)
        (void)sv_shop_phone_retire(reference);
    return result;
}

void sv_shop_phone_digit_added(int32_t item)
{
    if ((uint32_t)item < SR_MAX_ITEM && decisions_open(digit_event)) {
        if (!digit_waiting[item])
            ++digit_count;
        digit_waiting[item] = sv_entity_at(SV_ENTITY_ITEM, item);
        if (!digit_waiting[item])
            --digit_count;
        /* A changed number cannot reuse an earlier call result. */
        sv_action_cancel(call_event, item);
        call_phone[item] = 0;
    }
}

int sv_shop_phone_call_wait(int32_t item)
{
    SvEntity reference;
    int result;
    if ((uint32_t)item >= SR_MAX_ITEM)
        return 0;
    if (!decisions_open(call_event)) {
        sv_action_cancel(call_event, item);
        call_phone[item] = 0;
        return 0;
    }
    reference = sv_entity_at(SV_ENTITY_ITEM, item);
    if (call_phone[item] != reference || call_number[item] != sr_items[item].phone_number)
        sv_action_cancel(call_event, item);
    call_phone[item] = reference;
    call_number[item] = sr_items[item].phone_number;
    result = sv_action_gate(call_event, item, deliver_call);
    if (result != SV_ACTION_HOLD)
        call_phone[item] = 0;
    return result != SV_ACTION_PROCEED;
}

/* Digit notices run before queued call decisions. A claimed digit cancels the call too. */
void sv_shop_poll(void)
{
    int32_t i, item, peer;
    if (!digit_count)
        return;
    for (i = 0; i < (int32_t)SR_MAX_ITEM; ++i) {
        SvEntity reference = digit_waiting[i], human;
        if (!reference)
            continue;
        digit_waiting[i] = 0;
        --digit_count;
        if (resolve_phone(reference, &item, &peer, &human) == SV_OK &&
            sv_shop_phone_decision(SV_PHONE_DIGIT_DECISION, reference, human) == SV_DENIED)
            (void)sv_shop_phone_retire(reference);
    }
}

#ifdef SV_API_GAME_PROPS
static void *phone_state_field(int32_t index, uint32_t slot)
{
    (void)slot;
    return &sr_items[index].call_state;
}

static void *phone_number_field(int32_t index, uint32_t slot)
{
    (void)slot;
    return &sr_items[index].phone_number;
}

static SvResult idle_check(int32_t index, SvValue value)
{
    int32_t item, peer;
    SvEntity human;
    if (value.i != SV_PHONE_IDLE || sr_items[index].type != ITEM_CELL_PHONE)
        return SV_INVALID;
    return resolve_phone(sv_entity_at(SV_ENTITY_ITEM, index), &item, &peer, &human);
}

/* The property check validated links before the shared table wrote state zero. */
static void idle_after(int32_t index)
{
    int32_t peer = sr_items[index].linked_item;
    if (peer == -1)
        (void)incoming(index, &peer);
    idle_apply(index, peer);
}

static const Prop phone_props[] = {
    {"item.phone_state", SV_ENTITY_ITEM, SV_PROP_INT, 1, SV_PROP_GET | SV_PROP_SET, 0, 3,
     phone_state_field, idle_check, idle_after, NULL, 0},
    {"item.phone_number", SV_ENTITY_ITEM, SV_PROP_INT, 1, SV_PROP_GET, 0, 9999,
     phone_number_field, NULL, NULL, NULL, 0}
};
#endif

SvResult sv_shop_props_install(void)
{
#ifdef SV_API_GAME_PROPS
    return sv_props_add_group(phone_props, sizeof phone_props / sizeof phone_props[0]);
#else
    return SV_STATE;
#endif
}

const SvGameShop sv_game_shop = {
    sizeof(SvGameShop), SV_GAME_SHOP_VERSION, phone, phone_idle, catalog, catalog_replace
};
