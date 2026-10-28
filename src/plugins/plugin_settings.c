#include "plugin_host.h"
#include "sv_game_settings.h"
#include "plugin_game_internal.h"
#include "server.h"
#include <string.h>

typedef struct { const char *name; int32_t minimum, maximum, initial; } Row;
static const Row rows[SV_SETTING_COUNT] = {
    {"player_limit", 1, 128, 16},
    {"new_account_money", 0, INT32_MAX, 1000},
    {"bot_count", 0, 256, 24}
};
static int32_t values[SV_SETTING_COUNT];
static unsigned char changed[SV_SETTING_COUNT];
static SvSettingsPost post_change;
static uint32_t change_kind;

static SvResult context(SvOwner owner)
{
    return sv_plugins_context(owner) && sv_game_is_ready() ? SV_OK : SV_STATE;
}

static int lookup(const char *name)
{
    uint32_t i;
    if (!name || !*name || strnlen(name, SV_SETTING_NAME_SIZE) >= SV_SETTING_NAME_SIZE)
        return -2;
    for (i = 0; i < SV_SETTING_COUNT; ++i)
        if (!strcmp(name, rows[i].name))
            return (int)i;
    return -1;
}

static int32_t initial(uint32_t index)
{
    return index == SV_SETTING_BOT_COUNT && sr_game.gametype == 4 ? 32 : rows[index].initial;
}

int32_t sv_settings_native(uint32_t index, int32_t fallback)
{
    if (index >= SV_SETTING_COUNT || !changed[index])
        return fallback;
    return values[index];
}

static int32_t current(uint32_t index)
{
    if (index == SV_SETTING_PLAYER_LIMIT)
        return sr_serverinfo.max_players;
    return sv_settings_native(index, initial(index));
}

int sv_settings_install(uint32_t event_kind, SvSettingsPost post)
{
    if (!post || event_kind <= SV_EVENT_STOP || (event_kind & (event_kind - 1)))
        return 0;
    post_change = post;
    change_kind = event_kind;
    return 1;
}

static SvResult SV_CALL describe(SvOwner owner, uint32_t index, SvSettingInfo *out)
{
    SvSettingInfo value;
    if (context(owner) != SV_OK)
        return SV_STATE;
    if (index >= SV_SETTING_COUNT || !out || out->size < sizeof *out)
        return SV_INVALID;
    memset(&value, 0, sizeof value);
    value.size = sizeof value;
    value.index = index;
    value.type = SV_SETTING_INT;
    strcpy(value.name, rows[index].name);
    value.minimum = rows[index].minimum;
    value.maximum = rows[index].maximum;
    value.default_value = initial(index);
    *out = value;
    return SV_OK;
}

static SvResult SV_CALL get(SvOwner owner, const char *name, SvSettingValue *out)
{
    int index;
    SvSettingValue value;
    if (context(owner) != SV_OK)
        return SV_STATE;
    if (!out || out->size < sizeof *out)
        return SV_INVALID;
    index = lookup(name);
    if (index < 0)
        return index == -2 ? SV_INVALID : SV_NOT_FOUND;
    value = (SvSettingValue){sizeof value, SV_SETTING_INT, current((uint32_t)index)};
    *out = value;
    return SV_OK;
}

static SvResult SV_CALL set(SvOwner owner, const char *name, const SvSettingValue *input)
{
    int index;
    SvSettingEvent event;
    if (context(owner) != SV_OK)
        return SV_STATE;
    if (!input || input->size < sizeof *input || input->type != SV_SETTING_INT)
        return SV_INVALID;
    index = lookup(name);
    if (index < 0)
        return index == -2 ? SV_INVALID : SV_NOT_FOUND;
    if (input->value < rows[index].minimum || input->value > rows[index].maximum)
        return SV_INVALID;
    if (!post_change)
        return SV_STATE;
    if (input->value == current((uint32_t)index)) {
        values[index] = input->value;
        changed[index] = 1;
        if (index == SV_SETTING_PLAYER_LIMIT)
            sr_option.maxnumofplayers = input->value;
        return SV_OK;
    }
    memset(&event, 0, sizeof event);
    event.base = (SvEvent){sizeof event, change_kind, (uint32_t)sr_game.ticks};
    strcpy(event.name, rows[index].name);
    event.owner = owner;
    event.type = SV_SETTING_INT;
    event.before = current((uint32_t)index);
    event.after = input->value;
    if (!post_change(&event))
        return SV_LIMIT;
    values[index] = input->value;
    changed[index] = 1;
    if (index == SV_SETTING_PLAYER_LIMIT) {
        sr_option.maxnumofplayers = input->value;
        sr_serverinfo.max_players = input->value;
    }
    return SV_OK;
}

const SvGameSettings sv_game_settings = {
    sizeof(SvGameSettings), SV_GAME_SETTINGS_VERSION, describe, get, set
};
