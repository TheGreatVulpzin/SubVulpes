#ifndef SV_GAME_H
#define SV_GAME_H

#include "sv_plugin.h"

typedef uint64_t SvEntity;
typedef struct { float x, y, z; } SvVector;
enum {
    SV_ENTITY_PLAYER = 1, SV_ENTITY_HUMAN, SV_ENTITY_ITEM,
    SV_ENTITY_OBJECT, SV_ENTITY_ACCOUNT
};

#endif
