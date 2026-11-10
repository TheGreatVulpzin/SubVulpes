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

/* The host sets item_size to the size of one SvAiThreat. A caller steps through the list with it. */
typedef struct {
    uint32_t size, count, item_size, reserved;
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
    uint32_t size;
    SvAiStreetPoint point;
    SvVector position, direction;
    uint32_t next_node;
} SvAiStreetPosition;

/* A road of the street map. count is the number of roads. ends are the intersections at the two ends; a street point
 * with direction 0 runs from ends[0] to ends[1]. kind is 0 for a road along x and 1 along z. lanes are the lanes on
 * the two sides. direction is the unit vector from ends[0] to ends[1] and side the unit vector across the road. */
typedef struct {
    uint32_t size, count;
    int32_t ends[2], kind, lanes[2];
    SvVector direction, side;
} SvAiRoad;

/* An intersection of the street map. count is the number of intersections. roads are the roads at the four sides, -1
 * for none. extent is how far each road reaches into the intersection. light is the signal for the cars that arrive
 * on each side: 0 green, 1 yellow, 2 red. A car stops at 1 and 2. */
typedef struct {
    uint32_t size, count;
    SvVector position;
    int32_t roads[4], extent[4], light[4];
} SvAiIntersection;

/* The parts of a chase. chase_mode switches all four together. chase_parts switches any mix. */
enum { SV_CHASE_SPEED = 1, SV_CHASE_LATERAL = 2, SV_CHASE_LANE = 4, SV_CHASE_LIGHTS = 8, SV_CHASE_ALL = 15 };

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
    SvResult (SV_CALL *chase_mode)(SvOwner owner, SvEntity bot, int32_t enabled);
    SvResult (SV_CALL *street_near)(SvOwner owner, SvVector position, float radius, SvAiStreetPosition *out);
    SvResult (SV_CALL *possess)(SvOwner owner, SvEntity player, SvEntity bot);
    SvResult (SV_CALL *road)(SvOwner owner, uint32_t index, SvAiRoad *out);
    SvResult (SV_CALL *intersection)(SvOwner owner, uint32_t index, SvAiIntersection *out);
    SvResult (SV_CALL *aim_snap)(SvOwner owner, SvEntity bot, int32_t enabled);
    /* Ends a possession that the same plugin made with possess. The player gets the old body back when it is still alive
     * and has no owner, and the bot gets its body back. The same checks as the stop of the plugin apply. SV_NOT_FOUND
     * when the player has no possession, SV_DENIED when another plugin made it, SV_STATE outside a safe callback. */
    SvResult (SV_CALL *unpossess)(SvOwner owner, SvEntity player);
    /* Switches the parts of a chase for a car driver: a mix of SV_CHASE_ bits, or 0 for none. SV_CHASE_ALL is the same
     * as chase_mode 1. Every other rule is the rule of chase_mode. SV_INVALID for a bit outside SV_CHASE_ALL. */
    SvResult (SV_CALL *chase_parts)(SvOwner owner, SvEntity bot, uint32_t parts);
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
int sv_ai_snap(int32_t player);

#endif
