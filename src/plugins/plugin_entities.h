#ifndef SV_PLUGIN_ENTITIES_H
#define SV_PLUGIN_ENTITIES_H

#include "sv_game.h"

int sv_entities_start(void);
void sv_entities_clear(uint32_t kind);
void sv_entities_stop(void);
void sv_entity_created(uint32_t kind, uint32_t index);
void sv_entity_deleted(uint32_t kind, uint32_t index);
uint32_t sv_entity_capacity(uint32_t kind);
SvEntity sv_entity_at(uint32_t kind, int32_t index);
int32_t sv_entity_resolve(uint32_t kind, SvEntity entity);

#endif
