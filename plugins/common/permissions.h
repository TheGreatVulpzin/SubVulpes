#ifndef SV_OFFICIAL_PERMISSIONS_H
#define SV_OFFICIAL_PERMISSIONS_H

#include "official.h"
#include "../permissions/sv_permissions.h"

SvResult of_permissions(Official *ctx, const SvPermissions **out);
int of_can(Official *ctx, SvEntity actor, const char *node);
SvResult of_group(Official *ctx, SvEntity player, SvPermissionGroup *out);

#endif
