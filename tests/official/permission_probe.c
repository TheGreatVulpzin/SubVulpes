#include "../../plugins/common/official.h"
#include "../../plugins/permissions/sv_permissions.h"
#include <stdio.h>
#include <string.h>

static Official api;
static const SvPermissions *permissions;
#define CHECK(x) do { if (!(x)) { api.host->log(api.owner, "PERMISSION_PROBE_FAILED " #x); return SV_FAILED; } } while (0)

static SvResult SV_CALL command(SvOwner owner, uint64_t actor, const char *arguments)
{
    char line[256], operation[64], player[64], value[64], text[200], *rest = line;
    SvPlayerState state = { sizeof state };
    SvPermissionGroup group = { sizeof group };
    (void)actor;
    if (strlen(arguments) >= sizeof line) return SV_INVALID;
    strcpy(line, arguments);
    CHECK(of_token(&rest, operation, sizeof operation) == 1);
    if (!strcmp(operation, "npc")) {
        SvEntity player_ref = 0, human_ref = 0;
        CHECK(api.write->npc_create(owner, (SvVector){ 30, 20, 30 }, 0, "Bot", &player_ref, &human_ref) == SV_OK);
        strcpy(text, "PERMISSION_BOT_CREATED");
    } else if (!strcmp(operation, "list")) {
        struct { SvPermissionGroup group; unsigned sentinel; } entries[24];
        uint32_t count = 999;
        memset(entries, 0xcc, sizeof entries);
        CHECK(permissions->groups(NULL, 24, sizeof entries[0], &count) == SV_INVALID && count == 999);
        CHECK(permissions->groups(&entries[0].group, 0, sizeof entries[0], &count) == SV_INVALID);
        CHECK(permissions->groups(&entries[0].group, 25, sizeof entries[0], &count) == SV_INVALID);
        CHECK(permissions->groups(&entries[0].group, 24, 1, &count) == SV_INVALID);
        CHECK(permissions->groups(&entries[0].group, 24, 4097, &count) == SV_INVALID);
        CHECK(permissions->groups(&entries[0].group, 1, sizeof entries[0], &count) == SV_LIMIT && count == 999);
        CHECK(permissions->groups(&entries[0].group, 24, sizeof entries[0], &count) == SV_OK && count >= 5 && count <= 24);
        CHECK(entries[0].sentinel == 0xccccccccu && !strcmp(entries[0].group.name, "guest"));
        snprintf(text, sizeof text, "PERMISSION_GROUPS %u", count);
    } else {
        CHECK(of_token(&rest, player, sizeof player) == 1);
        CHECK(of_target(&api, 0, player, &state) == SV_OK);
        if (!strcmp(operation, "group")) {
            CHECK(permissions->group_of(state.entity, NULL) == SV_INVALID);
            group.size = 1;
            CHECK(permissions->group_of(state.entity, &group) == SV_INVALID);
            group.size = sizeof group;
            CHECK(permissions->group_of(state.entity, &group) == SV_OK);
            snprintf(text, sizeof text, "PERMISSION_GROUP %s %s %d prefix=%s", player, group.name, group.level, group.prefix);
        } else if (!strcmp(operation, "state")) {
            snprintf(text, sizeof text, "PERMISSION_STATE %s money=%d admin=%d", player, state.money, state.is_admin);
        } else if (!strcmp(operation, "assign")) {
            char requested[64];
            SvPlayerState target = { sizeof target };
            SvResult result;
            CHECK(of_token(&rest, value, sizeof value) == 1 && of_token(&rest, requested, sizeof requested) == 1 && !*rest);
            CHECK(of_target(&api, 0, value, &target) == SV_OK);
            result = permissions->assign(state.entity, target.entity, requested);
            snprintf(text, sizeof text, "PERMISSION_ASSIGN %s %s %s %d", player, value, requested, result);
        } else if (!strcmp(operation, "can")) {
            CHECK(of_token(&rest, value, sizeof value) == 1 && !*rest);
            snprintf(text, sizeof text, "PERMISSION_CAN %s %s %d", player, value, permissions->can(state.entity, value));
        } else return SV_INVALID;
    }
    return api.host->log(owner, text);
}

static SvResult SV_CALL enable(const SvHost *host, SvOwner owner)
{
    const void *table;
    CHECK(of_init(&api, host, owner, OF_CAP(SV_API_GAME_READ) | OF_CAP(SV_API_GAME_WRITE) | OF_CAP(SV_API_PLUGINS)) == SV_OK);
    CHECK(api.plugins->find(owner, SV_PERMISSIONS_API, SV_PERMISSIONS_VERSION, &table) == SV_OK);
    permissions = (const SvPermissions *)table;
    CHECK(permissions->size >= sizeof *permissions && permissions->version == SV_PERMISSIONS_VERSION);
    return host->command(owner, "permprobe", SV_COMMAND_LOCAL, command);
}

SV_EXPORT SvResult SV_CALL sv_plugin_entry(uint32_t abi, SvPlugin *plugin)
{
    if (abi != SV_PLUGIN_ABI || !plugin || plugin->size < sizeof *plugin) return SV_INCOMPATIBLE;
    memset(plugin, 0, sizeof *plugin);
    plugin->size = sizeof *plugin; plugin->abi = SV_PLUGIN_ABI;
    strcpy(plugin->id, "test.permissions"); strcpy(plugin->version, "0.1.0");
    plugin->flags = SV_PLUGIN_LIVE; plugin->enable = enable;
    return SV_OK;
}
