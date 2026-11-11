/* The speed of a train (N49). The locomotive pulls toward a target speed of 0.4. A plugin may set another target between 0 and 1. A target
 * of 0 brakes the train anywhere. The door face of the locomotive follows the real speed, so a train that stops opens its door. */
#include "plugin_host.h"
#include "plugin_entities.h"
#include "plugin_train.h"
#include "sv_game_write.h"
#include "plugin_game_internal.h"
#include "sr_runtime.h"
#include "sr_server.h"
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
    if (!finite_number(throttle) || (throttle != -1.0f && (throttle < 0.0f || throttle > 1.0f)))
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
