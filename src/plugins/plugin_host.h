#ifndef SV_PLUGIN_HOST_H
#define SV_PLUGIN_HOST_H

#include "sv_plugin.h"
#include "sv_game_events.h"

#define SV_PLUGIN_MAX 32
#define SV_PLUGIN_COMMAND_MAX 128
#define SV_PLUGIN_DEP_MAX 8

typedef enum {
    SV_MODULE_DISABLED, SV_MODULE_ACTIVE, SV_MODULE_FAILED, SV_MODULE_RESTART
} SvModuleState;

int sv_plugins_start(void);
void sv_plugins_stop(void);
void sv_plugins_tick(void);
int sv_plugins_failed(void);
SvResult sv_plugins_enable(const char *id);
SvResult sv_plugins_disable(const char *id);
void sv_plugins_list(void);
int sv_plugins_cli(int argc, char **argv);
void sv_plugins_native_bypass(int enabled);
int sv_plugins_command(uint64_t actor, const char *text);
void sv_control_start(void);
void sv_control_poll(void);
void sv_control_stop(void);

void sv_game_status(SvStatus *status);
int sv_game_start(void);
void sv_game_stop(void);
void sv_game_ready(void);
void sv_game_reset(void);
SvResult sv_game_api(uint32_t capability, uint32_t version, const void **api);
int sv_plugins_context(SvOwner owner);
/* 1 when an active plugin asked for this kind of event and the call is on the server thread. */
int sv_plugins_wants(uint32_t kind);
/* The event queue (plugin_events.c). The core posts, and the host takes the records out at a safe point. */
void sv_events_post(uint32_t kind, uint32_t tick, SvEntity player, SvEntity human, SvEntity actor);
uint32_t sv_events_pending(void);
int sv_events_next(SvGameEvent *out);
void sv_events_clear(void);
/* Gives a decision event to the active plugins that asked for its kind, in load order, with the full API. The first
 * plugin that answers SV_DENIED ends the call and the result is SV_DENIED. Any other answer than SV_OK or SV_DENIED is a
 * failure, and the plugin is disabled. The result is SV_OK when no plugin answers SV_DENIED, and also when the call is not
 * on the server thread, the host is not running or the call comes from inside a plugin callback. */
SvResult sv_plugins_decide(const SvEvent *event);
int sv_game_actor_admin(uint64_t actor);
void sv_log(const char *fmt, ...);

#endif
