#include "systems_host.h"

/* Test adapter for the shared N16 queue. It never calls a plugin. */
static SvSettingEvent notices[3];
static uint32_t notice_count;
static int account_player, saved_account;

static int post(const SvSettingEvent *event)
{
    if (notice_count == 3)
        return 0;
    CHECK(event->base.size == sizeof *event && event->base.kind == 1024u);
    CHECK(event->owner == 1 && event->type == SV_SETTING_INT);
    notices[notice_count++] = *event;
    return 1;
}

void settings_prepare(void)
{
    SvSettingInfo info = {sizeof info};
    SvSettingValue value = {sizeof value, SV_SETTING_INT, 1};
    CHECK(sv_game_settings.describe(0, 0, &info) == SV_STATE);
    CHECK(sv_game_settings.get(0, "player_limit", &value) == SV_STATE);
    CHECK(sv_game_settings.set(0, "player_limit", &value) == SV_STATE);
    CHECK(sv_settings_native(SV_SETTING_COUNT, 77) == 77);
    CHECK(sv_settings_native(0x40000000u, 77) == 77);
    CHECK(sv_settings_native(SV_SETTING_NEW_ACCOUNT_MONEY, 1000) == 1000);
    CHECK(!sv_settings_install(0, post));
    CHECK(!sv_settings_install(SV_EVENT_STOP, post));
    CHECK(!sv_settings_install(6, post));
    CHECK(!sv_settings_install(1024u, NULL));
    CHECK(sv_settings_install(1024u, post));
    account_player = createplayer();
    CHECK(account_player >= 0);
    strcpy(sr_players[account_player].name, "Settings account");
    sr_players[account_player].account_id[0] = 321;
    sr_players[account_player].account_id[1] = 654;
    saved_account = createaccount(account_player);
    CHECK(saved_account >= 0 && sr_accounts[saved_account].money == 1000);
    /* Keep this account detached from the native per-tick balance synchronization. */
    CHECK(sr_players[account_player].account == -1);
    sr_game.gametype = 3;
}

void settings_verify(void)
{
    int account;
    CHECK(notice_count == 3);
    CHECK(!strcmp(notices[0].name, "player_limit") && notices[0].before == 128 && notices[0].after == 1);
    CHECK(!strcmp(notices[1].name, "new_account_money") && notices[1].before == 1000 && notices[1].after == 2000);
    CHECK(!strcmp(notices[2].name, "bot_count") && notices[2].before == 24 && notices[2].after == 0);
    CHECK(sr_option.maxnumofplayers == 1 && sr_serverinfo.max_players == 1);
    CHECK(sr_players[world_player].active && sr_players[world_player].money == 12345);
    CHECK(sr_accounts[saved_account].money == 1000);
    CHECK(sv_settings_native(SV_SETTING_NEW_ACCOUNT_MONEY, 1000) == 2000);
    CHECK(sv_settings_native(SV_SETTING_BOT_COUNT, 24) == 0);
    CHECK(sv_settings_native(SV_SETTING_BOT_COUNT, 32) == 0);
    /* Existing account construction must not change its balance. */
    account = createaccount(account_player);
    CHECK(account == saved_account && sr_accounts[account].money == 1000);
    notice_count = 0;
    sr_players[world_player].godmode = 0;
    CHECK(sv_plugins_command(0, "settings.boundaries"));
    CHECK(sr_players[world_player].godmode == 1);
    CHECK(sr_option.maxnumofplayers == 128 && sr_serverinfo.max_players == 128);
    CHECK(sv_settings_native(SV_SETTING_NEW_ACCOUNT_MONEY, 1000) == INT32_MAX);
    CHECK(sv_settings_native(SV_SETTING_BOT_COUNT, 32) == 256);
    CHECK(notice_count == 3);
    puts("SETTINGS_PASS");
}
