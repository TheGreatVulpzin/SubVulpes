#include "plugin_actions.h"
#include "plugin_host.h"
#include <string.h>

enum { ACTION_MAX = 256, ACTION_PENDING = 1, ACTION_RUNNING, ACTION_ALLOWED, ACTION_DENIED };
typedef struct {
    uint32_t kind;
    int32_t key;
    SvActionDeliver deliver;
    int state;
} Action;
static Action actions[ACTION_MAX];
static uint32_t used;
static int polling;

static void erase(Action *action)
{
    memset(action, 0, sizeof *action);
    --used;
}

void sv_action_cancel(uint32_t kind, int32_t key)
{
    uint32_t i;
    if (!used)
        return;
    for (i = 0; i < ACTION_MAX; ++i)
        if (actions[i].state && actions[i].kind == kind && actions[i].key == key) {
            erase(&actions[i]);
            return;
        }
}

void sv_actions_clear(void)
{
    memset(actions, 0, sizeof actions);
    used = 0;
}

int sv_action_gate(uint32_t kind, int32_t key, SvActionDeliver deliver)
{
    uint32_t i;
    Action *slot = NULL;
    if (!sv_plugins_wants(kind) || !deliver) {
        sv_action_cancel(kind, key);
        return SV_ACTION_PROCEED;
    }
    for (i = 0; i < ACTION_MAX; ++i) {
        Action *action = &actions[i];
        if (!action->state) {
            if (!slot)
                slot = action;
        } else if (action->kind == kind && action->key == key) {
            int state = action->state;
            if (state == ACTION_PENDING || state == ACTION_RUNNING)
                return SV_ACTION_HOLD;
            erase(action);
            return state == ACTION_DENIED ? SV_ACTION_DENIED : SV_ACTION_PROCEED;
        }
    }
    if (!slot)
        return SV_ACTION_PROCEED;
    *slot = (Action){kind, key, deliver, ACTION_PENDING};
    ++used;
    return SV_ACTION_HOLD;
}

void sv_actions_poll(void)
{
    uint32_t i;
    if (!used || polling)
        return;
    polling = 1;
    /* New actions from a delivery wait for the next poll. */
    for (i = 0; i < ACTION_MAX; ++i)
        if (actions[i].state == ACTION_PENDING)
            actions[i].state = ACTION_RUNNING;
    for (i = 0; i < ACTION_MAX; ++i) {
        Action *action = &actions[i];
        SvResult result;
        if (action->state != ACTION_RUNNING)
            continue;
        result = action->deliver(action->key);
        /* A delivery can cancel its own action when its resource ends. */
        if (action->state == ACTION_RUNNING)
            action->state = result == SV_DENIED ? ACTION_DENIED : ACTION_ALLOWED;
    }
    polling = 0;
}
