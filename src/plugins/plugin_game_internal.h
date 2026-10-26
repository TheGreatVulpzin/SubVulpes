#ifndef SV_PLUGIN_GAME_INTERNAL_H
#define SV_PLUGIN_GAME_INTERNAL_H

#include "sv_game_read.h"

int sv_game_is_ready(void);
uint64_t sv_game_reset_sequence(void);
extern const SvGameRead sv_game_read;

#endif
