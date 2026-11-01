#include "permissions.h"
#include <string.h>

SvResult of_permissions(Official *ctx, const SvPermissions **out)
{
    const void *table = NULL;
    const SvPermissions *service;
    SvResult result;
    *out = NULL;
    if (ctx->plugins->size < offsetof(SvPlugins, borrow) + sizeof ctx->plugins->borrow || !ctx->plugins->borrow)
        return SV_INCOMPATIBLE;
    result = ctx->plugins->borrow(ctx->owner, SV_PERMISSIONS_API, SV_PERMISSIONS_VERSION, &table);
    if (result != SV_OK) return result;
    service = (const SvPermissions *)table;
    if (!service || service->size < sizeof *service || service->version != SV_PERMISSIONS_VERSION ||
        !service->can || !service->group_of || !service->groups || !service->assign)
        return SV_INCOMPATIBLE;
    *out = service;
    return SV_OK;
}

int of_can(Official *ctx, SvEntity actor, const char *node)
{
    const SvPermissions *service;
    SvPlayerState player = { sizeof player };
    SvResult result;
    if (!actor) return 1;
    result = of_permissions(ctx, &service);
    if (result == SV_OK) return service->can(actor, node);
    if (result != SV_NOT_FOUND) return 0;
    return ctx->read->player(ctx->owner, actor, &player) == SV_OK && player.is_admin && !player.is_bot;
}

SvResult of_group(Official *ctx, SvEntity player, SvPermissionGroup *out)
{
    const SvPermissions *service;
    SvPlayerState state = { sizeof state };
    SvResult result;
    if (!player) {
        out->level = 100; strcpy(out->name, "owner"); out->prefix[0] = 0;
        return SV_OK;
    }
    result = of_permissions(ctx, &service);
    if (result == SV_OK) return service->group_of(player, out);
    if (result != SV_NOT_FOUND) return result;
    result = ctx->read->player(ctx->owner, player, &state);
    if (result == SV_OK) {
        out->level = state.is_admin ? 90 : 0;
        strcpy(out->name, state.is_admin ? "admin" : "guest"); out->prefix[0] = 0;
    }
    return result;
}
