/* The input lock of a real player (N71). The client sends its input in each packet. After the server has read the packets,
 * a lock puts the held values over what the client sent, so the body and the view stay still. */
#include "plugin_host.h"
#include "plugin_entities.h"
#include "sv_game_write.h"
#include "plugin_game_internal.h"
#include "player.h"
#include "server.h"
#include "sr_runtime.h"
#include "sr_server.h"
#include <string.h>

typedef struct {
    SvOwner owner;
    SvEntity player;
    uint64_t reset;
    uint32_t parts;
    float yaw, pitch, arm_axis, arm_pitch;
} Lock;

static Lock locks[SR_MAX_PLAYER];

static int lock_live(const Lock *lock, int32_t index)
{
    return lock->owner && lock->reset == sv_game_reset_sequence() &&
        sv_entity_resolve(SV_ENTITY_PLAYER, lock->player) == index;
}

SvResult SV_CALL sv_input_lock_set(SvOwner owner, SvEntity player, uint32_t parts)
{
    int32_t index;
    Lock *lock;
    Player *p;
    if (!sv_plugins_write_context(owner) || !sv_game_is_ready())
        return SV_STATE;
    index = sv_entity_resolve(SV_ENTITY_PLAYER, player);
    if (index < 0)
        return SV_NOT_FOUND;
    if (parts > SV_INPUT_LOCK_ALL)
        return SV_INVALID;
    p = &sr_players[index];
    if (p->is_bot)
        return SV_DENIED;
    lock = &locks[index];
    if (lock_live(lock, index) && lock->owner != owner)
        return SV_CONFLICT;
    if (!parts) {
        if (lock_live(lock, index))
            memset(lock, 0, sizeof *lock);
        return SV_OK;
    }
    /* The view is held at the angles that the player has when the lock starts. */
    if (!lock_live(lock, index) || !(lock->parts & SV_INPUT_LOCK_VIEW)) {
        lock->yaw = p->yaw;
        lock->pitch = p->input_pitch;
        lock->arm_axis = p->input_extra[0];
        lock->arm_pitch = p->input_extra[1];
    }
    lock->owner = owner;
    lock->player = player;
    lock->reset = sv_game_reset_sequence();
    lock->parts = parts;
    return SV_OK;
}

void sv_input_lock_apply(void)
{
    int32_t i;
    for (i = 0; i < SR_MAX_PLAYER; ++i) {
        Lock *lock = &locks[i];
        Player *p = &sr_players[i];
        if (!lock->owner)
            continue;
        if (!lock_live(lock, i) || !p->active) {
            memset(lock, 0, sizeof *lock);
            continue;
        }
        if (lock->parts & SV_INPUT_LOCK_BODY) {
            memset(p->input_axes, 0, sizeof p->input_axes);
            p->input_buttons = 0;
        }
        if (lock->parts & SV_INPUT_LOCK_VIEW) {
            p->yaw = lock->yaw;
            p->input_pitch = lock->pitch;
            p->input_extra[0] = lock->arm_axis;
            p->input_extra[1] = lock->arm_pitch;
        }
    }
}

void sv_input_lock_release(SvOwner owner)
{
    int32_t i;
    for (i = 0; i < SR_MAX_PLAYER; ++i)
        if (locks[i].owner == owner)
            memset(&locks[i], 0, sizeof locks[i]);
}
