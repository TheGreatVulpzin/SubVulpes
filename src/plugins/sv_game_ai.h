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

/* The filter of street_list. kinds is a mix of SV_STREET_ALONG_X and SV_STREET_ALONG_Z (the direction of the road).
 * human_gap and object_gap are the least distance from a candidate to any human and to any object, from 0 to 64 units.
 * A gap of 0 turns that check off. street_near uses all kinds, a human gap of 4 and an object gap of 16. */
#define SV_AI_STREET_LIST_MAX 8u
enum { SV_STREET_ALONG_X = 1, SV_STREET_ALONG_Z = 2, SV_STREET_ANY = 3 };
typedef struct {
    uint32_t size, kinds;
    float human_gap, object_gap;
} SvAiStreetFilter;

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

/* What a driver does at the end of an open route (street_route_open). */
enum { SV_ROUTE_STOP = 1, SV_ROUTE_RELEASE = 2 };

/* The target of aim_at: a point, or an entity (a human, or a vehicle or other object). */
enum { SV_AIM_POINT = 1, SV_AIM_ENTITY = 2 };
typedef struct {
    uint32_t size, kind;
    SvVector point;
    SvEntity entity;
} SvAiAimTarget;

/* The parts of a bot input that override_input_partial can replace. */
enum { SV_AI_PART_AXES = 1, SV_AI_PART_BUTTONS = 2, SV_AI_PART_VIEW = 4, SV_AI_PART_ALL = 7 };
#define SV_AI_BUTTONS_ALL 0x7ffffu

/* The parts of a chase. chase_mode switches all four together. chase_effects switches any mix. */
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
    SvResult (SV_CALL *chase_effects)(SvOwner owner, SvEntity bot, uint32_t parts);
    /* Like override_input, but only for the parts in the mask SV_AI_PART_: the axes, the buttons and the view (yaw and pitch).
     * For the buttons, button_mask says which bits to replace, and the other bits stay with the default AI. The default AI
     * runs first, and the plugin values go over its result. All parts with all button bits are the same as override_input,
     * which skips the default AI. The aim policy of aim_mode keeps the last word on button bit 2. Every other rule is the rule
     * of override_input. A passenger in seat 1 to 3 accepts the buttons and the view, and no axes. SV_INVALID for parts 0, a bit
     * outside SV_AI_PART_ALL or a button bit outside SV_AI_BUTTONS_ALL. */
    SvResult (SV_CALL *override_input_partial)(SvOwner owner, SvEntity bot, const SvAiInput *input, uint32_t parts,
        uint32_t button_mask, uint32_t ticks);
    /* Like street_near, but gives up to capacity candidates (1 to SV_AI_STREET_LIST_MAX), the nearest first, at most one for
     * each lane. The filter may be NULL for the defaults of street_near. Each out entry needs its size set. *count gets the
     * number of candidates. SV_NOT_FOUND when there is none. Every other bound is the bound of street_near. SV_INVALID for
     * kinds 0 or a bit outside SV_STREET_ANY, a gap outside 0 to 64, or a short out entry. */
    SvResult (SV_CALL *street_list)(SvOwner owner, SvVector position, float radius, const SvAiStreetFilter *filter,
        SvAiStreetPosition *out, uint32_t capacity, uint32_t *count);
    /* Turns the view of a bot toward a point or an entity on each update, for 1 to SV_AI_TICKS_MAX updates, after the default AI
     * has run. The bot may be on foot or a passenger in seat 1 to 3. The aim is exact: it uses the angles that the AI itself uses
     * for a threat, from the head of the bot. For a human the aim point is the torso, or the head when perception is 1. The aim
     * ends when the entity is gone. release ends it too. It does not fire. A plugin fires with override_input_partial.
     * SV_STATE for a driver, a bot with no body or a bot in a vehicle seat that is not a passenger seat. SV_NOT_FOUND for an
     * entity that is gone. SV_INVALID for a bad kind, a point outside the map, or ticks outside 1 to SV_AI_TICKS_MAX. */
    SvResult (SV_CALL *aim_at)(SvOwner owner, SvEntity bot, const SvAiAimTarget *target, uint32_t ticks);
    /* Like street_route, but the route is open: it has 2 to SV_AI_WAYPOINTS_MAX points, each joined to the next, and the last
     * does not join the first. At the end of the last road the driver does end_action. SV_ROUTE_STOP brakes and holds until
     * the plugin gives a new route or calls release. SV_ROUTE_RELEASE gives the car back to the default AI. Every other rule
     * is the rule of street_route. SV_INVALID for an end action that is not SV_ROUTE_STOP or SV_ROUTE_RELEASE. */
    SvResult (SV_CALL *street_route_open)(SvOwner owner, SvEntity bot, const SvAiStreetPoint *points, uint32_t count,
        uint32_t end_action);
    /* Sets the base target speed of a driver, in the unit of the game (the distance in one tick). The default is 0.33, and a
     * chase car uses 0.5 or 0.67. The game scales the value by the turn of the road. Any finite number from 0 up is valid, with
     * no upper bound, and a high value is not tested. -1 ends the setting. The brake before a crossing and the cars ahead still
     * lower the speed, and the stuck logic does not change. The bot must drive a car (SV_STATE otherwise). SV_INVALID for a
     * value that is not a finite number or is below 0 (other than -1). The setting is part of the bot control lease: release,
     * plugin stop, slot reuse and world reset clear it. The server drives the car, so no client effect is expected. */
    SvResult (SV_CALL *driver_speed)(SvOwner owner, SvEntity bot, float speed);
    /* Scales the aim noise of a bot on foot. The game aims at a noisy estimate of the enemy. The noise shrinks as the bot sees
     * the enemy for longer, and it grows with the distance. scale multiplies that noise. 1 is the game rule, 0 is a perfect
     * estimate, and 4 is four times as noisy. Any finite number from 0 up is valid, with no upper bound. -1 ends the
     * setting. The reaction time and the shot chance do not change. The setting is part of the bot control lease. SV_DENIED
     * for a human, SV_STATE for a bot with no body, SV_INVALID for a value that is not a finite number or is below 0 (other
     * than -1). */
    SvResult (SV_CALL *accuracy)(SvOwner owner, SvEntity bot, float scale);
    /* Sets how far a bot on foot sees an enemy. The game uses 128. range is any finite number from 0 up, with no upper bound.
     * -1 ends the setting. The bot still needs a clear line to the enemy and the enemy must be in front of it. The rules of
     * accuracy apply. */
    SvResult (SV_CALL *sight_range)(SvOwner owner, SvEntity bot, float range);
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
float sv_ai_speed_base(int32_t player, float k, float base);
float sv_ai_spread(int32_t player);
float sv_ai_sight(int32_t player);

#endif
