#ifndef SV_PERMISSIONS_H
#define SV_PERMISSIONS_H

#include "sv_game.h"

#define SV_PERMISSIONS_API "sv.permissions"
#define SV_PERMISSIONS_VERSION 1u
#define SV_PERMISSION_GROUP_MAX 24u
#define SV_PERMISSION_NAME_SIZE 24u
#define SV_PERMISSION_NODE_SIZE 40u

#pragma pack(push, 8)
typedef struct {
    uint32_t size;
    int32_t level;
    char name[SV_PERMISSION_NAME_SIZE], prefix[32];
} SvPermissionGroup;

/* Reads use the last permission callback's snapshot and never call the host with another owner's identity.
 * Assign queues a checked change for the provider's next tick. The provider reports a failed save privately.
 * Call these functions only during a host callback. Player references remain opaque and session scoped. */
typedef struct {
    uint32_t size, version;
    int (SV_CALL *can)(SvEntity player, const char *node);
    SvResult (SV_CALL *group_of)(SvEntity player, SvPermissionGroup *out);
    SvResult (SV_CALL *groups)(SvPermissionGroup *out, uint32_t capacity, uint32_t item_size, uint32_t *count);
    SvResult (SV_CALL *assign)(SvEntity actor, SvEntity player, const char *group);
} SvPermissions;
#pragma pack(pop)

#endif
