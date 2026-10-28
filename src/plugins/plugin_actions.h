#ifndef SV_PLUGIN_ACTIONS_H
#define SV_PLUGIN_ACTIONS_H

#include "sv_plugin.h"

/* Core only. Call the gate on the server thread, outside plugin callbacks.
 * The delivery function builds the event and calls sv_plugins_decide at the next poll.
 * The pair (kind, key) identifies one action. The next gate call consumes the result once.
 *
 * The delivery function checks key lifetime. A stale key returns any result except SV_DENIED.
 * A full table permits the action. The gate never calls a plugin.
 *
 * An action from a button edge must run inside its delivery when allowed.
 * The delivery must then cancel its entry. The button is gone next tick. */
typedef SvResult (*SvActionDeliver)(int32_t key);
enum { SV_ACTION_PROCEED = 1, SV_ACTION_HOLD = 0, SV_ACTION_DENIED = -1 };
int sv_action_gate(uint32_t event_kind, int32_t key, SvActionDeliver deliver);
void sv_actions_poll(void);
/* Cancel work when its core resource ends. Clear all work at start, stop and reset. */
void sv_action_cancel(uint32_t event_kind, int32_t key);
void sv_actions_clear(void);

#endif
