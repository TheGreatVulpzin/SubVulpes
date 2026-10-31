#ifndef SV_OFFICIAL_H
#define SV_OFFICIAL_H

#include "sv_game_read.h"
#include "sv_game_write.h"
#include "sv_game_props.h"
#include "sv_game_storage.h"
#include "sv_game_events.h"
#include "sv_config.h"
#include "sv_plugins_api.h"

#define OF_CAP(id) (1u << ((id) - 1u))
typedef struct {
    const SvHost *host;
    SvOwner owner;
    const SvGameRead *read;
    const SvGameWrite *write;
    const SvGameProps *props;
    const SvGameStorage *store;
    const SvConfig *config;
    const SvPlugins *plugins;
} Official;

SvResult of_init(Official *ctx, const SvHost *host, SvOwner owner, uint32_t caps);
void of_tell(Official *ctx, SvEntity actor, const char *format, ...);
int of_token(char **rest, char *word, size_t capacity);
int of_number(const char *text, int32_t low, int32_t high, int32_t *out);
SvResult of_slot(Official *ctx, uint32_t kind, uint32_t slot, SvEntity *out);
SvResult of_target(Official *ctx, SvEntity actor, const char *name, SvPlayerState *out);
SvResult of_set(Official *ctx, const char *name, SvEntity entity, int32_t number);
int32_t of_int(Official *ctx, const char *key, int32_t fallback);
SvResult of_get(Official *ctx, uint32_t scope, SvEntity subject, const char *name, void *out, uint32_t size);
SvResult of_put(Official *ctx, uint32_t scope, SvEntity subject, const char *name, const void *data, uint32_t size);
SvResult of_erase(Official *ctx, uint32_t scope, SvEntity subject, const char *name);

#endif
