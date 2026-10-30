#ifndef SV_PLUGIN_GAME_INTERNAL_H
#define SV_PLUGIN_GAME_INTERNAL_H

#include "sv_game_read.h"
#include "sv_game_write.h"
#include "sv_game_props.h"
#include "sv_game_ai.h"
#include "sv_game_economy.h"
#include "sv_game_mission.h"
#include "sv_game_settings.h"
#include "sv_game_storage.h"
#include "sv_game_engine.h"
#include "sv_game_shop.h"

int sv_game_is_ready(void);
int sv_network_start(void);
void sv_network_stop(void);
int sv_network_ids(const uint8_t ip[4], uint32_t *address, uint32_t *network);
uint64_t sv_game_reset_sequence(void);
extern const SvGameRead sv_game_read;
extern const SvGameWrite sv_game_write;
extern const SvGameProps sv_game_props;
extern const SvGameAi sv_game_ai;
extern const SvGameEconomy sv_game_economy;
extern const SvGameMission sv_game_mission;
extern const SvGameSettings sv_game_settings;
extern const SvGameStorage sv_game_storage;
extern const SvGameEngine sv_game_engine;
extern const SvGameShop sv_game_shop;

#endif
