/* ABI version 1, Windows x86. Keep this list explicit for review. */
#include <stdio.h>
#include "sv_plugin.h"
#include "sv_config.h"
#include "sv_plugins_api.h"
#include "sv_game.h"
#include "sv_game_read.h"
#include "sv_game_write.h"
#include "sv_game_props.h"
#include "sv_game_events.h"
#include "sv_game_ai.h"
#include "sv_game_economy.h"
#include "sv_game_engine.h"
#include "sv_game_mission.h"
#include "sv_game_settings.h"
#include "sv_game_shop.h"
#include "sv_game_storage.h"

#define RECORD(type) printf(#type " size=%zu\n", sizeof(type))
#define TABLE(type, first, last, count) \
    printf(#type " functions=%zu prefix=%zu first=" #first ":%zu last=" #last ":%zu\n", \
        (sizeof(type) - offsetof(type, first)) / sizeof(void (SV_CALL *)(void)), \
        offsetof(type, size), offsetof(type, first), offsetof(type, last)); \
    _Static_assert((offsetof(type, last) - offsetof(type, first)) / \
        sizeof(void (SV_CALL *)(void)) + 1 == count, "Update the function count")

int main(void)
{
    _Static_assert(sizeof(void *) == 4, "Build the ABI snapshot for x86");
    RECORD(SvStatus);
    RECORD(SvEvent);
    RECORD(SvHost);
    RECORD(SvPlugin);
    RECORD(SvConfig);
    RECORD(SvConfigEvent);
    RECORD(SvPlugins);
    RECORD(SvVector);
    RECORD(SvPlayerState);
    RECORD(SvHumanState);
    RECORD(SvItemState);
    RECORD(SvObjectState);
    RECORD(SvAccountState);
    RECORD(SvBoneState);
    RECORD(SvInventoryState);
    RECORD(SvWorldState);
    RECORD(SvLineHit);
    RECORD(SvGameRead);
    RECORD(SvGameWrite);
    RECORD(SvValue);
    RECORD(SvPropInfo);
    RECORD(SvGameProps);
    RECORD(SvGameEvent);
    RECORD(SvChatEvent);
    RECORD(SvDamageEvent);
    RECORD(SvAiThreat);
    RECORD(SvAiThreats);
    RECORD(SvAiInput);
    RECORD(SvAiStreetPoint);
    RECORD(SvGameAi);
    RECORD(SvCompany);
    RECORD(SvGameEconomy);
    RECORD(SvEngineValue);
    RECORD(SvEngineInfo);
    RECORD(SvGameEngine);
    RECORD(SvMissionRef);
    RECORD(SvMissionState);
    RECORD(SvMissionSlots);
    RECORD(SvMissionStart);
    RECORD(SvGameMission);
    RECORD(SvSettingValue);
    RECORD(SvSettingInfo);
    RECORD(SvSettingEvent);
    RECORD(SvGameSettings);
    RECORD(SvPhoneEvent);
    RECORD(SvPhoneState);
    RECORD(SvShopRow);
    RECORD(SvShopCatalog);
    RECORD(SvShopCatalogWrite);
    RECORD(SvGameShop);
    RECORD(SvStorageKey);
    RECORD(SvStorageValue);
    RECORD(SvGameStorage);
    TABLE(SvHost, log, get_api, 5);
    TABLE(SvPlugin, enable, disable, 2);
    TABLE(SvConfig, get_int, get_text, 3);
    TABLE(SvPlugins, refuse, subscribe64, 4);
    TABLE(SvGameRead, entities, within, 13);
    TABLE(SvGameWrite, message, disown, 24);
    TABLE(SvGameProps, find, add, 5);
    TABLE(SvGameAi, threats, seated_target, 11);
    TABLE(SvGameEconomy, company, add_stocks, 4);
    TABLE(SvGameEngine, describe, set, 3);
    TABLE(SvGameMission, slots, cancel, 3);
    TABLE(SvGameSettings, describe, set, 3);
    TABLE(SvGameShop, phone, catalog_replace, 4);
    TABLE(SvGameStorage, get, erase, 3);
    return 0;
}
