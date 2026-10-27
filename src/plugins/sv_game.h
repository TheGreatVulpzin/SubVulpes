#ifndef SV_GAME_H
#define SV_GAME_H

#include "sv_plugin.h"

/* A news line (a headline) has at most 124 characters. The buffer for one holds the characters and a zero. */
#define SV_HEADLINE_TEXT_MAX 124u
#define SV_HEADLINE_BUFFER 125u
#define SV_HEADLINE_COUNT_MAX 1024u

typedef uint64_t SvEntity;
typedef struct { float x, y, z; } SvVector;
enum {
    SV_ENTITY_PLAYER = 1, SV_ENTITY_HUMAN, SV_ENTITY_ITEM,
    SV_ENTITY_OBJECT, SV_ENTITY_ACCOUNT
};

#endif
