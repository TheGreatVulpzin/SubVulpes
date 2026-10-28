#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include "systems_plugin.h"
#include "sv_game_settings.h"
#include "sv_game_write.h"

static const SvGameSettings *api;
static const SvHost *saved_host;
typedef struct { SvOwner owner; } Args;

static DWORD WINAPI wrong_thread(void *data)
{
    Args *a = data;
    SvSettingInfo info = {sizeof info};
    SvSettingValue value = {sizeof value, SV_SETTING_INT, 10};
    STEP(api->describe(a->owner, 0, &info) == SV_STATE);
    STEP(api->get(a->owner, "player_limit", &value) == SV_STATE);
    STEP(api->set(a->owner, "player_limit", &value) == SV_STATE);
    return 0;
}

static SvResult SV_CALL boundaries(SvOwner owner, uint64_t actor, const char *arguments)
{
    const void *table;
    const SvGameWrite *write;
    const SvGameRead *read;
    SvSettingValue value = {sizeof value, SV_SETTING_INT, 128};
    SvEntity entities[256];
    SvPlayerState player = {sizeof player};
    uint32_t count, next, i;
    (void)actor;
    (void)arguments;
    STEP(api->set(owner, "player_limit", &value) == SV_OK);
    value.value = INT32_MAX;
    STEP(api->set(owner, "new_account_money", &value) == SV_OK);
    value.value = 256;
    STEP(api->set(owner, "bot_count", &value) == SV_OK);
    STEP(saved_host->get_api(owner, SV_API_GAME_READ, SV_GAME_READ_VERSION, &table) == SV_OK);
    read = table;
    STEP(saved_host->get_api(owner, SV_API_GAME_WRITE, SV_GAME_WRITE_VERSION, &table) == SV_OK);
    write = table;
    STEP(read->entities(owner, SV_ENTITY_PLAYER, 0, entities, 256, &count, &next) == SV_OK);
    for (i = 0; i < count; ++i) {
        STEP(read->player(owner, entities[i], &player) == SV_OK);
        if (!strcmp(player.name, "Systems test"))
            return write->set_godmode(owner, entities[i], 1);
    }
    return SV_FAILED;
}

int settings_fixture(const SvHost *host, SvOwner owner)
{
    const void *table = NULL;
    SvSettingInfo info = {sizeof info}, saved_info;
    SvSettingValue out = {sizeof out}, saved_out;
    SvSettingValue value = {sizeof value, SV_SETTING_INT, 1};
    const char *names[] = {"player_limit", "new_account_money", "bot_count"};
    int32_t minimum[] = {1, 0, 0}, maximum[] = {128, INT32_MAX, 256};
    uint32_t i;
    char long_name[SV_SETTING_NAME_SIZE + 1];
    Args args = {owner};
    HANDLE thread;
    DWORD code;
    STEP(host->get_api(owner, SV_API_SETTINGS, SV_GAME_SETTINGS_VERSION, &table) == SV_OK);
    api = table;
    saved_host = host;
    STEP(api && api->size >= sizeof *api && api->version == SV_GAME_SETTINGS_VERSION);
    thread = CreateThread(NULL, 0, wrong_thread, &args, 0, NULL);
    STEP(thread != NULL);
    STEP(WaitForSingleObject(thread, 5000) == WAIT_OBJECT_0);
    STEP(GetExitCodeThread(thread, &code));
    CloseHandle(thread);
    STEP(code == 0);
    STEP(api->describe(0, 0, &info) == SV_STATE);
    STEP(api->get(owner + 1, "player_limit", &out) == SV_STATE);
    STEP(api->set(owner + 1, "player_limit", &value) == SV_STATE);
    for (i = 0; i < SV_SETTING_COUNT; ++i) {
        STEP(api->describe(owner, i, &info) == SV_OK);
        STEP(info.index == i && info.type == SV_SETTING_INT && !strcmp(info.name, names[i]));
        STEP(info.minimum == minimum[i] && info.maximum == maximum[i]);
        STEP(api->get(owner, info.name, &out) == SV_OK);
        STEP(out.type == SV_SETTING_INT);
        value.value = minimum[i] - 1;
        STEP(api->set(owner, info.name, &value) == SV_INVALID);
        if (maximum[i] != INT32_MAX) {
            value.value = maximum[i] + 1;
            STEP(api->set(owner, info.name, &value) == SV_INVALID);
        }
    }
    saved_info = info;
    STEP(api->describe(owner, SV_SETTING_COUNT, &info) == SV_INVALID);
    STEP(!memcmp(&info, &saved_info, sizeof info));
    STEP(api->describe(owner, 0, NULL) == SV_INVALID);
    info.size = 0;
    saved_info = info;
    STEP(api->describe(owner, 0, &info) == SV_INVALID);
    STEP(!memcmp(&info, &saved_info, sizeof info));
    STEP(api->get(owner, NULL, &out) == SV_INVALID);
    STEP(api->get(owner, "", &out) == SV_INVALID);
    memset(long_name, 'x', sizeof long_name);
    long_name[sizeof long_name - 1] = 0;
    STEP(api->get(owner, long_name, &out) == SV_INVALID);
    STEP(api->set(owner, long_name, &value) == SV_INVALID);
    saved_out = out;
    STEP(api->get(owner, "unknown", &out) == SV_NOT_FOUND);
    STEP(!memcmp(&out, &saved_out, sizeof out));
    STEP(api->get(owner, "player_limit", NULL) == SV_INVALID);
    out.size = 0;
    saved_out = out;
    STEP(api->get(owner, "player_limit", &out) == SV_INVALID);
    STEP(!memcmp(&out, &saved_out, sizeof out));
    out.size = sizeof out;
    STEP(api->set(owner, "player_limit", NULL) == SV_INVALID);
    value.value = 1;
    value.size = 0;
    STEP(api->set(owner, "player_limit", &value) == SV_INVALID);
    value.size = sizeof value;
    value.type = 2;
    STEP(api->set(owner, "player_limit", &value) == SV_INVALID);
    value.type = SV_SETTING_INT;
    STEP(api->set(owner, NULL, &value) == SV_INVALID);
    STEP(api->set(owner, "unknown", &value) == SV_NOT_FOUND);
    STEP(api->get(owner, "player_limit", &out) == SV_OK && out.value == 128);
    STEP(api->get(owner, "new_account_money", &out) == SV_OK && out.value == 1000);
    STEP(api->get(owner, "bot_count", &out) == SV_OK && out.value == 24);
    value.value = 1;
    STEP(api->set(owner, "player_limit", &value) == SV_OK);
    value.value = 2000;
    STEP(api->set(owner, "new_account_money", &value) == SV_OK);
    value.value = 0;
    STEP(api->set(owner, "bot_count", &value) == SV_OK);
    STEP(api->set(owner, "bot_count", &value) == SV_OK);
    value.value = 256;
    STEP(api->set(owner, "bot_count", &value) == SV_LIMIT);
    STEP(api->get(owner, "bot_count", &out) == SV_OK && out.value == 0);
    STEP(host->command(owner, "settings.boundaries", SV_COMMAND_LOCAL, boundaries) == SV_OK);
    return 0;
}
