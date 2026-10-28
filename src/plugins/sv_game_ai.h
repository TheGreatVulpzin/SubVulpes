#ifndef SV_GAME_AI_H
#define SV_GAME_AI_H

/* Bot perception, foot routes and temporary input control. */
#include "sv_game.h"

#define SV_API_AI 3u
#define SV_GAME_AI_VERSION 1u
#define SV_AI_THREATS_MAX 16u
#define SV_AI_WAYPOINTS_MAX 64u
#define SV_AI_TICKS_MAX 3600u

#pragma pack(push, 8)
typedef struct {
    SvEntity human;
    SvVector position;
    float level;
    int32_t timer;
} SvAiThreat;

typedef struct {
    uint32_t size, count;
    SvAiThreat threats[SV_AI_THREATS_MAX];
} SvAiThreats;

typedef struct {
    uint32_t size, buttons;
    float axes[4];
    float yaw, pitch;
} SvAiInput;

typedef struct {
    uint32_t road;
    int32_t direction, lane;
} SvAiStreetPoint;

typedef struct {
    uint32_t size, version;
    SvResult (SV_CALL *threats)(SvOwner owner, SvEntity bot, SvAiThreats *out);
    SvResult (SV_CALL *destination)(SvOwner owner, SvEntity bot, SvVector position);
    SvResult (SV_CALL *waypoints)(SvOwner owner, SvEntity bot, const SvVector *points, uint32_t count);
    SvResult (SV_CALL *target)(SvOwner owner, SvEntity bot, SvEntity human);
    SvResult (SV_CALL *override_input)(SvOwner owner, SvEntity bot, const SvAiInput *input, uint32_t ticks);
    SvResult (SV_CALL *release)(SvOwner owner, SvEntity bot);
    SvResult (SV_CALL *perception)(SvOwner owner, SvEntity bot, uint32_t body_part);
    SvResult (SV_CALL *aim_mode)(SvOwner owner, SvEntity bot, int32_t enabled);
    SvResult (SV_CALL *shot_chance)(SvOwner owner, SvEntity bot, uint32_t percent);
    SvResult (SV_CALL *street_route)(SvOwner owner, SvEntity bot, const SvAiStreetPoint *points, uint32_t count);
    SvResult (SV_CALL *seated_target)(SvOwner owner, SvEntity bot, SvEntity player);
} SvGameAi;
#pragma pack(pop)

/* Host seams. The core must install all seams before enabling control.
 * These functions are not DLL entry points. See docs/plugin-ai.md. */
void sv_ai_install(void);
int sv_ai_before_player(int32_t player);
void sv_ai_after_threats(int32_t player);
void sv_ai_release_owner(SvOwner owner);
int32_t sv_ai_perception_part(int32_t player);
void sv_ai_after_player(int32_t player);
int sv_ai_shot(int32_t player, int passenger);
int sv_ai_route_next(int32_t player);

#endif
