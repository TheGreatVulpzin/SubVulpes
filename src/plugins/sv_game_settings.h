#ifndef SV_GAME_SETTINGS_H
#define SV_GAME_SETTINGS_H

#include "sv_game.h"

#define SV_API_SETTINGS 6u
#define SV_GAME_SETTINGS_VERSION 1u
#define SV_SETTING_NAME_SIZE 32u

enum { SV_SETTING_INT = 1u };
enum { SV_SETTING_PLAYER_LIMIT, SV_SETTING_NEW_ACCOUNT_MONEY, SV_SETTING_BOT_COUNT, SV_SETTING_COUNT };

#pragma pack(push, 8)
typedef struct {
    uint32_t size, type;
    int32_t value;
} SvSettingValue;

typedef struct {
    uint32_t size, index, type;
    char name[SV_SETTING_NAME_SIZE];
    int32_t minimum, maximum, default_value;
} SvSettingInfo;

typedef struct {
    SvEvent base;
    char name[SV_SETTING_NAME_SIZE];
    SvOwner owner;
    uint32_t type;
    int32_t before, after;
} SvSettingEvent;

typedef struct {
    uint32_t size, version;
    SvResult (SV_CALL *describe)(SvOwner owner, uint32_t index, SvSettingInfo *out);
    SvResult (SV_CALL *get)(SvOwner owner, const char *name, SvSettingValue *out);
    SvResult (SV_CALL *set)(SvOwner owner, const char *name, const SvSettingValue *input);
} SvGameSettings;
#pragma pack(pop)

/* The shared N16 queue copies the record. It returns zero when its queue is full. */
typedef int (*SvSettingsPost)(const SvSettingEvent *event);
int sv_settings_install(uint32_t event_kind, SvSettingsPost post);
/* Replace native constants with this getter. An unset row keeps its call-site default. */
int32_t sv_settings_native(uint32_t index, int32_t fallback);

#endif
