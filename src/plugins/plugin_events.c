/* The queue of game events for plugins. See docs/plugin-events.md.
 *
 * The core posts a record when an event happens, in the middle of the simulation if it must. The host takes the records
 * out at a safe point and gives them to the plugins. The queue holds 1,024 records. A full queue drops the new record
 * and counts it. The next delivery starts with a lost record that has the count. */
#include "plugin_host.h"
#include <string.h>

enum { QUEUE_MAX = 1024 };

static SvGameEvent queue[QUEUE_MAX];
static uint32_t first, used, lost, lost_tick;

/* One kind at a time: the kinds that the core posts. */
static int postable(uint32_t kind)
{
    return kind == SV_EVENT_SPAWN || kind == SV_EVENT_DEATH || kind == SV_EVENT_LEAVE || kind == SV_EVENT_ROUND ||
        kind == SV_EVENT_VEHICLE || kind == SV_EVENT_SHOT || kind == SV_EVENT_OBJECT_GONE || kind == SV_EVENT_ITEM;
}

void sv_events_post(uint32_t kind, uint32_t tick, SvEntity player, SvEntity human, SvEntity actor)
{
    sv_events_post_ex(kind, tick, player, human, actor, 0, 0, 0, 0);
}

void sv_events_post_ex(uint32_t kind, uint32_t tick, SvEntity player, SvEntity human, SvEntity actor, SvEntity item,
    SvEntity object, int32_t value, int32_t extra)
{
    SvGameEvent *record;
    if (!postable(kind) || !sv_plugins_wants(kind))
        return;
    if (used >= QUEUE_MAX) {
        ++lost;
        lost_tick = tick;
        return;
    }
    record = &queue[(first + used) % QUEUE_MAX];
    memset(record, 0, sizeof *record);
    record->base.size = sizeof *record;
    record->base.kind = kind;
    record->base.tick = tick;
    record->player = player;
    record->human = human;
    record->actor = actor;
    record->item = item;
    record->object = object;
    record->value = value;
    record->extra = extra;
    ++used;
}

int sv_events_post_record(const SvEvent *record)
{
    SvGameEvent *slot;
    if (!record || record->size < sizeof *record || record->size > sizeof *slot || record->kind != SV_EVENT_SETTING)
        return 0;
    if (!sv_plugins_wants(record->kind))
        return 1;
    if (used >= QUEUE_MAX) {
        ++lost;
        lost_tick = record->tick;
        return 0;
    }
    slot = &queue[(first + used) % QUEUE_MAX];
    memset(slot, 0, sizeof *slot);
    memcpy(slot, record, record->size);
    ++used;
    return 1;
}

uint32_t sv_events_pending(void)
{
    return used + (lost ? 1u : 0u);
}

int sv_events_next(SvGameEvent *out)
{
    if (lost) {
        memset(out, 0, sizeof *out);
        out->base.size = sizeof *out;
        out->base.kind = SV_EVENT_LOST;
        out->base.tick = lost_tick;
        out->lost = lost;
        lost = 0;
        return 1;
    }
    if (!used)
        return 0;
    *out = queue[first];
    first = (first + 1) % QUEUE_MAX;
    --used;
    return 1;
}

void sv_events_clear(void)
{
    first = used = lost = lost_tick = 0;
}
