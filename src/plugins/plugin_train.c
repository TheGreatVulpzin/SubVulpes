/* The speed of a train (N49). The locomotive pulls toward a target speed of 0.4. A plugin may set another target between 0 and 1. A target
 * of 0 brakes the train anywhere. The door face of the locomotive follows the real speed, so a train that stops opens its door. */
#include "plugin_host.h"
#include "plugin_entities.h"
#include "plugin_train.h"
#include "sv_game_write.h"
#include "plugin_game_internal.h"
#include "sr_runtime.h"
#include "sr_server.h"
#include "objects.h"
#include "object_types.h"
#include "vectors.h"
#include <math.h>
#include <string.h>

#define SPEED_MAX 8

typedef struct {
    SvOwner owner;
    SvEntity locomotive;
    uint64_t reset;
    float throttle;
} Speed;

static Speed speeds[SPEED_MAX];

static int finite_number(float value)
{
    return value == value && value - value == 0.0f;
}

static int speed_live(const Speed *speed)
{
    return speed->owner && speed->reset == sv_game_reset_sequence() &&
        sv_entity_resolve(SV_ENTITY_OBJECT, speed->locomotive) >= 0;
}

SvResult SV_CALL sv_train_speed(SvOwner owner, SvEntity locomotive, float throttle)
{
    int32_t index;
    int i, found = -1, free_slot = -1;
    if (!sv_plugins_write_context(owner) || !sv_game_is_ready())
        return SV_STATE;
    index = sv_entity_resolve(SV_ENTITY_OBJECT, locomotive);
    if (index < 0)
        return SV_NOT_FOUND;
    if (!finite_number(throttle) || (throttle != -1.0f && throttle < 0.0f))
        return SV_INVALID;
    if (sr_objects[index].type != 12)
        return SV_DENIED;
    for (i = 0; i < SPEED_MAX; ++i) {
        if (speeds[i].owner && !speed_live(&speeds[i]))
            memset(&speeds[i], 0, sizeof speeds[i]);
        if (!speeds[i].owner) {
            if (free_slot < 0)
                free_slot = i;
        } else if (speeds[i].locomotive == locomotive) {
            found = i;
        }
    }
    if (found >= 0) {
        if (speeds[found].owner != owner)
            return SV_CONFLICT;
        if (throttle == -1.0f)
            memset(&speeds[found], 0, sizeof speeds[found]);
        else
            speeds[found].throttle = throttle;
        return SV_OK;
    }
    if (throttle == -1.0f)
        return SV_OK;
    if (free_slot < 0)
        return SV_LIMIT;
    speeds[free_slot] = (Speed){owner, locomotive, sv_game_reset_sequence(), throttle};
    return SV_OK;
}

/* The game sends every locomotive between two station pieces of the track: 74 and 28. */
#define STATION_A 74
#define STATION_B 28
#define TRAIN_MAX 8
#define SLOTS_MIN 3

/* The turn of a locomotive. The locomotive drives toward -rot[2], so the best turn is the one that puts -rot[2] along the wanted way. */
static void face_toward(vec3 rot[3], float dx, float dz)
{
    static const float add[4] = { 0.0f, 3.1415927f, 0.0f, 3.1415927f };
    float base = (float)atan2((double)dx, (double)dz);
    float best = -2.0f;
    int i;
    vec3 trial[3];
    float length = sqrtf(dx * dx + dz * dz);
    if (length < 1.0f) {
        resetorientation(rot);
        return;
    }
    for (i = 0; i < 4; ++i) {
        float angle = (i < 2 ? base : -base) + add[i];
        float dot;
        resetorientation(trial);
        rotateorientation(trial, &sr_data_yaxis, angle);
        dot = -(trial[2].x * dx + trial[2].z * dz) / length;
        if (dot > best) {
            best = dot;
            rot[0] = trial[0];
            rot[1] = trial[1];
            rot[2] = trial[2];
        }
    }
}

SvResult SV_CALL sv_train_create(SvOwner owner, uint32_t piece, uint32_t station, SvEntity *locomotive)
{
    int i, free_slots = 0, trains = 0, made, to;
    int need_particles, need_bonds;
    vec3 rot[3], pos;
    if (!locomotive)
        return SV_INVALID;
    *locomotive = 0;
    if (!sv_plugins_write_context(owner) || !sv_game_is_ready())
        return SV_STATE;
    to = station ? STATION_B : STATION_A;
    if (station > 1 || sr_data_numoftracks <= STATION_A || piece >= (uint32_t)sr_data_numoftracks)
        return SV_INVALID;
    for (i = 0; i < SR_MAX_OBJECT; ++i) {
        if (!sr_objects[i].active)
            ++free_slots;
        else if (sr_objects[i].type == 12)
            ++trains;
    }
    if (trains >= TRAIN_MAX)
        return SV_LIMIT;
    /* The game makes the locomotive and the two bogies one after the other, so all the room must exist before it starts. */
    need_particles = sr_objecttypes[12].num_particles + 2 * sr_objecttypes[13].num_particles + 8;
    need_bonds = sr_objecttypes[12].num_bonds + 2 * sr_objecttypes[13].num_bonds + 40;
    if (free_slots < SLOTS_MIN || need_particles > SR_MAX_PARTICLE - sr_numofparticles || need_bonds > SR_MAX_BOND - sr_numofbonds)
        return SV_LIMIT;
    pos = sr_data_track[piece].pos;
    pos.y = (float)((double)pos.y + 1.625);
    face_toward(rot, sr_data_track[to].pos.x - pos.x, sr_data_track[to].pos.z - pos.z);
    made = createvehicle(0, &pos, rot);
    if (made < 0)
        return SV_FAILED;
    sr_objects[made].track_target = to;
    *locomotive = sv_entity_at(SV_ENTITY_OBJECT, (uint32_t)made);
    return SV_OK;
}

SvResult SV_CALL sv_train_remove(SvOwner owner, SvEntity locomotive)
{
    int32_t index;
    int i;
    if (!sv_plugins_write_context(owner) || !sv_game_is_ready())
        return SV_STATE;
    index = sv_entity_resolve(SV_ENTITY_OBJECT, locomotive);
    if (index < 0)
        return SV_NOT_FOUND;
    if (sr_objects[index].type != 12)
        return SV_DENIED;
    /* The locomotive and its two bogies go in the same tick, so no bond points to a particle that is gone. */
    for (i = 0; i < SR_MAX_OBJECT; ++i)
        if (sr_objects[i].active && sr_objects[i].type == 13 && sr_objects[i].linked_object == index)
            sr_objects[i].time_to_live = 0;
    sr_objects[index].time_to_live = 0;
    return SV_OK;
}

float sv_train_throttle(int obj, float thr)
{
    int i;
    for (i = 0; i < SPEED_MAX; ++i)
        if (speeds[i].owner && speed_live(&speeds[i]) && sv_entity_resolve(SV_ENTITY_OBJECT, speeds[i].locomotive) == obj)
            return speeds[i].throttle;
    return thr;
}

void sv_train_release(SvOwner owner)
{
    int i;
    for (i = 0; i < SPEED_MAX; ++i)
        if (speeds[i].owner == owner)
            memset(&speeds[i], 0, sizeof speeds[i]);
}
