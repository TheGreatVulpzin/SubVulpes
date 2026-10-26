#ifndef SV_PLUGIN_HOST_H
#define SV_PLUGIN_HOST_H

#include "sv_plugin.h"

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
int sv_game_actor_admin(uint64_t actor);
void sv_log(const char *fmt, ...);

#endif
