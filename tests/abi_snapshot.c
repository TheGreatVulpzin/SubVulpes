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
#include "sv_server.h"

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
    _Static_assert(SV_ACCOUNT_SAVE_DISK == 0 && SV_ACCOUNT_SAVE_DISCONNECT == 1, "Keep the account save reason codes");
    _Static_assert(offsetof(SvGameRead, connection) == 60, "Preserve the read prefix");
    _Static_assert(offsetof(SvGameWrite, kicked_clear) == 116, "Preserve the write prefix");
    _Static_assert(sizeof(SvLineHit) == 56, "Preserve line hit ABI");
    RECORD(SvStatus);
    RECORD(SvEvent);
    RECORD(SvTimerEvent);
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
    RECORD(SvConnectionState);
    RECORD(SvGameRead);
    RECORD(SvGameWrite);
    RECORD(SvValue);
    RECORD(SvPropInfo);
    RECORD(SvGameProps);
    RECORD(SvGameEvent);
    RECORD(SvAccountSaveEvent);
    RECORD(SvChatEvent);
    RECORD(SvCoreCommandEvent);
    RECORD(SvAdminLoginEvent);
    RECORD(SvDamageEvent);
    RECORD(SvAiThreat);
    RECORD(SvAiThreats);
    RECORD(SvAiInput);
    RECORD(SvAiStreetPoint);
    RECORD(SvAiStreetPosition);
    RECORD(SvAiStreetFilter);
    RECORD(SvAiAimTarget);
    RECORD(SvAiRoad);
    RECORD(SvAiIntersection);
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
    TABLE(SvHost, log, random, 8);
    TABLE(SvPlugin, enable, disable, 2);
    TABLE(SvConfig, get_int, get_text, 3);
    TABLE(SvPlugins, refuse, borrow, 4);
    TABLE(SvGameRead, entities, account_number, 21);
    TABLE(SvGameWrite, message, train_speed, 33);
    TABLE(SvGameProps, find, add, 5);
    TABLE(SvGameAi, threats, driver_speed, 24);
    TABLE(SvGameEconomy, company, add_stocks, 4);
    TABLE(SvGameEngine, describe, set, 3);
    TABLE(SvGameMission, slots, cancel, 3);
    TABLE(SvGameSettings, describe, set, 3);
    TABLE(SvGameShop, phone, catalog_replace, 4);
    TABLE(SvGameStorage, get, erase, 3);
    printf("SV_EVENT_ACCOUNT_SAVE kind=%u bit=%llu\n", SV_EVENT_ACCOUNT_SAVE, (unsigned long long)SV_EVENT_EXT_BIT(0));
    printf("SvAccountSaveEvent reason=%zu\n", offsetof(SvAccountSaveEvent, reason));
    printf("SvConnectionState flags=%zu address_id=%zu network_id=%zu\n", offsetof(SvConnectionState, flags), offsetof(SvConnectionState, address_id), offsetof(SvConnectionState, network_id));
    printf("SV_EVENT_TIMER kind=%u bit=%llu\n", SV_EVENT_TIMER, (unsigned long long)SV_EVENT_EXT_BIT(1));
    printf("Core hooks kinds=%u,%u,%u decisions=%u,%u,%u\n", SV_EVENT_CORE_COMMAND,
        SV_EVENT_ADMIN_LOGIN, SV_EVENT_ADMIN_SUCCESS, SV_COMMAND_DEFAULT, SV_COMMAND_ALLOW, SV_COMMAND_DENY);
    printf("SvCoreCommandEvent name=%zu arguments=%zu builtin=%zu decision=%zu\n",
        offsetof(SvCoreCommandEvent, name), offsetof(SvCoreCommandEvent, arguments),
        offsetof(SvCoreCommandEvent, builtin), offsetof(SvCoreCommandEvent, decision));
    RECORD(SvBuildingState);
    RECORD(SvAreaState);
    RECORD(SvBlockState);
    RECORD(SvSurfaceHit);
    TABLE(SvServerControl, request, request, 1);
    TABLE(SvConsole, execute, execute, 1);
    return 0;
}
