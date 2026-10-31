#include "../common/official.h"
#include "groups.h"
#include <stdio.h>
#include <string.h>

static Official api;
static PmGroups groups;
static int loaded;
static uint32_t owners;
typedef struct {
    SvEntity player, account, requester;
    char group[SV_PERMISSION_NAME_SIZE], pending[SV_PERMISSION_NAME_SIZE];
    int temporary, bootstrap;
} Session;
static Session sessions[256];
typedef struct {
    uint32_t format, count;
    char names[SV_PERMISSION_GROUP_MAX][SV_PERMISSION_NAME_SIZE];
} GroupIndex;

static int group_setting(const char *key, const char *fallback)
{
    char name[64];
    int index;
    if (api.config->get_text(api.owner, key, name, sizeof name) == SV_OK && pm_name(name) &&
        (index = pm_find(&groups, name)) >= 0) return index;
    return pm_find(&groups, fallback);
}

static int account_group(SvEntity account)
{
    char name[SV_PERMISSION_NAME_SIZE];
    if (account && of_get(&api, SV_STORAGE_ACCOUNT, account, "group", name, sizeof name) == SV_OK && pm_name(name)) {
        int index = pm_find(&groups, name);
        if (index >= 0) return index;
    }
    return group_setting("default_group", "guest");
}

static Session *cached(SvEntity player)
{
    uint32_t i;
    if (!player) return NULL;
    for (i = 0; i < 256; ++i) if (sessions[i].player == player) return &sessions[i];
    return NULL;
}

static int session_group(const Session *session)
{
    int index = session ? pm_find(&groups, session->group) : -1;
    return index >= 0 ? index : pm_find(&groups, "guest");
}

static int SV_CALL can(SvEntity player, const char *node)
{
    Session *session = cached(player);
    return loaded && session && pm_can(&groups, session_group(session), node);
}

static SvResult SV_CALL group_of(SvEntity player, SvPermissionGroup *out)
{
    Session *session = cached(player);
    const PmGroup *group;
    SvPermissionGroup value = { sizeof value };
    if (!loaded || !session) return SV_NOT_FOUND;
    if (!out || out->size < sizeof *out) return SV_INVALID;
    group = &groups.group[session_group(session)];
    value.level = group->level;
    strcpy(value.name, group->name); strcpy(value.prefix, group->prefix);
    *out = value;
    return SV_OK;
}

static SvResult SV_CALL list_groups(SvPermissionGroup *out, uint32_t capacity, uint32_t item_size, uint32_t *count)
{
    uint32_t i;
    if (!loaded) return SV_STATE;
    if (!out || !count || !capacity || capacity > SV_PERMISSION_GROUP_MAX || item_size < sizeof *out || item_size > 4096)
        return SV_INVALID;
    if (capacity < groups.count) return SV_LIMIT;
    for (i = 0; i < groups.count; ++i) {
        SvPermissionGroup value = { sizeof value };
        value.level = groups.group[i].level;
        strcpy(value.name, groups.group[i].name); strcpy(value.prefix, groups.group[i].prefix);
        memcpy((char *)out + i * item_size, &value, sizeof value);
    }
    *count = groups.count;
    return SV_OK;
}

static int permitted(SvEntity actor, const char *node)
{
    if (!actor || can(actor, node)) return 1;
    of_tell(&api, actor, "You do not have permission: %s", node);
    return 0;
}

static int level(SvEntity player)
{
    return player ? groups.group[session_group(cached(player))].level : 101;
}

static int is_owner(SvEntity actor)
{
    return !actor || (cached(actor) && !strcmp(cached(actor)->group, "owner"));
}

static SvResult save_index(const PmGroups *state)
{
    GroupIndex index = { 1, 0, { { 0 } } };
    uint32_t i;
    index.count = state->count;
    for (i = 0; i < state->count; ++i) strcpy(index.names[i], state->group[i].name);
    return of_put(&api, SV_STORAGE_SERVER, 0, "groups", &index, sizeof index);
}

static SvResult save_group(const PmGroup *group)
{
    char key[SV_STORAGE_KEY_SIZE];
    snprintf(key, sizeof key, "g.%s", group->name);
    return of_put(&api, SV_STORAGE_SERVER, 0, key, group, sizeof *group);
}

static SvResult load_groups(void)
{
    GroupIndex index;
    SvWorldState world = { sizeof world };
    PmGroups state = { 0 };
    SvResult result;
    uint32_t i, cursor = 0;
    if (loaded) return SV_OK;
    if (api.read->world(api.owner, &world) != SV_OK) return SV_STATE;
    result = of_get(&api, SV_STORAGE_SERVER, 0, "groups", &index, sizeof index);
    if (result == SV_NOT_FOUND) {
        pm_defaults(&state);
        for (i = 0; i < state.count; ++i)
            if ((result = save_group(&state.group[i])) != SV_OK) return result;
        if ((result = save_index(&state)) != SV_OK) return result;
    } else {
        if (result != SV_OK || index.format != 1 || index.count < 5 || index.count > SV_PERMISSION_GROUP_MAX)
            return SV_INVALID;
        state.count = index.count;
        for (i = 0; i < state.count; ++i) {
            char key[SV_STORAGE_KEY_SIZE];
            if (!pm_name(index.names[i])) return SV_INVALID;
            snprintf(key, sizeof key, "g.%s", index.names[i]);
            result = of_get(&api, SV_STORAGE_SERVER, 0, key, &state.group[i], sizeof state.group[i]);
            if (result != SV_OK || !pm_name(state.group[i].name) || strcmp(state.group[i].name, index.names[i])) return SV_INVALID;
        }
        if (!pm_valid(&state)) return SV_INVALID;
    }
    groups = state;
    owners = 0;
    /* Account rows, rather than a saved counter, decide whether recovery is available. */
    while (cursor < 65536) {
        SvEntity page[256];
        uint32_t count, next;
        if (api.read->entities(api.owner, SV_ENTITY_ACCOUNT, cursor, page, 256, &count, &next) != SV_OK) return SV_STATE;
        for (i = 0; i < count; ++i)
            if (!strcmp(groups.group[account_group(page[i])].name, "owner")) ++owners;
        if (next <= cursor) break;
        cursor = next;
    }
    loaded = 1;
    return SV_OK;
}

static Session *remember(SvEntity player)
{
    SvPlayerState state = { sizeof state };
    Session *session = cached(player);
    uint32_t i;
    if (api.read->player(api.owner, player, &state) != SV_OK) return NULL;
    if (!session) {
        for (i = 0; i < 256 && sessions[i].player; ++i) {}
        if (i == 256) return NULL;
        session = &sessions[i];
        memset(session, 0, sizeof *session);
        session->player = player;
    }
    if (session->account != state.account) {
        session->account = state.account;
        session->temporary = session->bootstrap = 0;
    }
    if (!session->temporary)
        strcpy(session->group, groups.group[account_group(state.account)].name);
    return session;
}

static SvResult sync_admin(Session *session)
{
    SvPlayerState state = { sizeof state };
    int admin;
    if (api.read->player(api.owner, session->player, &state) != SV_OK || state.is_bot) return SV_OK;
    admin = can(session->player, "core.admin");
    return state.is_admin == admin ? SV_OK : of_set(&api, "player.admin", session->player, admin);
}

static int may_assign(SvEntity actor, Session *target, int group)
{
    Session *caller = cached(actor);
    int recovery = caller && caller->bootstrap && owners == 0;
    if (!target || !target->account || group < 0 || (uint32_t)group >= groups.count) return 0;
    if (actor && !recovery && !can(actor, "permissions.assign")) return 0;
    if (actor && !recovery && (level(actor) < level(target->player) || groups.group[group].level > level(actor))) return 0;
    if ((!strcmp(target->group, "owner") || !strcmp(groups.group[group].name, "owner")) && !is_owner(actor) && !recovery) return 0;
    if (!strcmp(target->group, "owner") && strcmp(groups.group[group].name, "owner") && owners <= 1) return 0;
    return 1;
}

static SvResult set_group(SvEntity actor, Session *target, int group)
{
    int old;
    SvResult result;
    SvPlayerState state = { sizeof state };
    if (!may_assign(actor, target, group)) return SV_DENIED;
    if (api.read->player(api.owner, target->player, &state) != SV_OK || state.is_bot || state.account != target->account)
        return SV_NOT_FOUND;
    old = account_group(target->account);
    result = of_put(&api, SV_STORAGE_ACCOUNT, target->account, "group", groups.group[group].name, SV_PERMISSION_NAME_SIZE);
    if (result != SV_OK) return result;
    if (!strcmp(groups.group[old].name, "owner") && strcmp(groups.group[group].name, "owner") && owners) --owners;
    if (strcmp(groups.group[old].name, "owner") && !strcmp(groups.group[group].name, "owner")) ++owners;
    target->temporary = 0;
    strcpy(target->group, groups.group[group].name);
    if (owners) {
        uint32_t i;
        for (i = 0; i < 256; ++i) sessions[i].bootstrap = 0;
    }
    return sync_admin(target);
}

static SvResult SV_CALL assign(SvEntity actor, SvEntity player, const char *name)
{
    Session *target = cached(player);
    int group = pm_name(name) ? pm_find(&groups, name) : -1;
    if (!loaded || !target) return SV_NOT_FOUND;
    if (!may_assign(actor, target, group)) return SV_DENIED;
    if (target->pending[0]) return SV_CONFLICT;
    strcpy(target->pending, name);
    target->requester = actor;
    return SV_OK;
}

static const SvPermissions service = { sizeof service, SV_PERMISSIONS_VERSION, can, group_of, list_groups, assign };

static SvResult refresh(void)
{
    SvEntity page[256];
    uint32_t count, next, i;
    if (api.read->entities(api.owner, SV_ENTITY_PLAYER, 0, page, 256, &count, &next) != SV_OK) return SV_STATE;
    for (i = 0; i < 256; ++i) {
        SvPlayerState state = { sizeof state };
        if (sessions[i].player && api.read->player(api.owner, sessions[i].player, &state) != SV_OK)
            memset(&sessions[i], 0, sizeof sessions[i]);
    }
    for (i = 0; i < count; ++i) {
        Session *session = remember(page[i]);
        SvResult result;
        if (!session) return SV_LIMIT;
        if (session->pending[0]) {
            result = set_group(session->requester, session, pm_find(&groups, session->pending));
            of_tell(&api, session->requester, result == SV_OK ? "Group assignment saved." : "Group assignment failed: %d.", result);
            session->pending[0] = 0;
        }
        result = sync_admin(session);
        if (result != SV_OK) return result;
    }
    return SV_OK;
}

static SvResult SV_CALL event(SvOwner owner, const SvEvent *base)
{
    SvResult result;
    Session *session;
    (void)owner;
    result = load_groups();
    if (result == SV_STATE) return SV_OK;
    if (result != SV_OK) { api.host->log(api.owner, "Cannot load permission groups. Check plugin storage and disk space."); return SV_FAILED; }
    if (base->kind == SV_EVENT_TICK || base->kind == SV_EVENT_CONFIG) return refresh();
    if (base->kind == SV_EVENT_LEAVE && base->size >= sizeof(SvGameEvent)) {
        session = cached(((const SvGameEvent *)base)->player);
        if (session) memset(session, 0, sizeof *session);
    } else if (base->kind == SV_EVENT_JOIN && base->size >= sizeof(SvGameEvent)) {
        session = remember(((const SvGameEvent *)base)->player);
        if (session) return sync_admin(session);
    } else if (base->kind == SV_EVENT_CORE_COMMAND && base->size >= sizeof(SvCoreCommandEvent)) {
        SvCoreCommandEvent *command = (SvCoreCommandEvent *)base;
        char node[40];
        remember(command->player);
        if (of_int(&api, "gate_core_commands", 1) && command->builtin[0]) {
            snprintf(node, sizeof node, "core.%s", command->builtin);
            command->decision = can(command->player, node) ? SV_COMMAND_ALLOW : SV_COMMAND_DENY;
            if (command->decision == SV_COMMAND_DENY) of_tell(&api, command->player, "You do not have permission: %s", node);
        }
    } else if (base->kind == SV_EVENT_ADMIN_LOGIN && base->size >= sizeof(SvAdminLoginEvent)) {
        if (!of_int(&api, "password_login", 1)) {
            of_tell(&api, ((const SvAdminLoginEvent *)base)->player, "Password login is off.");
            return SV_DENIED;
        }
    } else if (base->kind == SV_EVENT_ADMIN_SUCCESS && base->size >= sizeof(SvAdminLoginEvent)) {
        SvEntity player = ((const SvAdminLoginEvent *)base)->player;
        int group = group_setting("login_group", "admin");
        session = remember(player);
        if (session) {
            int old = account_group(session->account);
            if (!strcmp(session->group, "owner")) group = pm_find(&groups, "owner");
            if (of_int(&api, "save_login_group", 0) && session->account) {
                result = of_put(&api, SV_STORAGE_ACCOUNT, session->account, "group", groups.group[group].name, SV_PERMISSION_NAME_SIZE);
                if (result != SV_OK) return result;
                if (strcmp(groups.group[old].name, "owner") && !strcmp(groups.group[group].name, "owner")) ++owners;
            }
            session->temporary = !of_int(&api, "save_login_group", 0) || !session->account;
            session->bootstrap = owners == 0;
            strcpy(session->group, groups.group[group].name);
            return sync_admin(session);
        }
    }
    return SV_OK;
}

static int ready(SvEntity actor)
{
    SvResult result = load_groups();
    if (result == SV_OK) {
        if (actor) remember(actor);
        return 1;
    }
    of_tell(&api, actor, "Permission storage is not ready: %d.", result);
    return 0;
}

static void whoami(SvEntity actor)
{
    static const char *nodes[] = { "core.admin", "core.kick", "admcmd.heal", "bans.ban", "permissions.assign", "chatadd.mute" };
    uint32_t i;
    int group = session_group(cached(actor));
    of_tell(&api, actor, "Group: %s (level %d).", groups.group[group].name, groups.group[group].level);
    for (i = 0; i < sizeof nodes / sizeof nodes[0]; ++i)
        if (can(actor, nodes[i])) of_tell(&api, actor, "%s", nodes[i]);
}

static SvResult SV_CALL whoami_command(SvOwner owner, uint64_t actor, const char *arguments)
{
    (void)owner;
    if (ready(actor) && !*arguments) whoami(actor);
    return SV_OK;
}

static SvResult SV_CALL perm_command(SvOwner owner, uint64_t actor, const char *arguments)
{
    char line[256], operation[64], target_name[64], value[64], *rest = line;
    SvPlayerState target = { sizeof target };
    (void)owner;
    if (!ready(actor)) return SV_OK;
    if (strnlen(arguments, sizeof line) >= sizeof line) return SV_INVALID;
    strcpy(line, arguments);
    if (of_token(&rest, operation, sizeof operation) != 1) { of_tell(&api, actor, "Use /perm set, check or who."); return SV_OK; }
    if (!strcmp(operation, "who") && !*rest) {
        SvEntity page[256];
        uint32_t count, next, i;
        if (!permitted(actor, "permissions.view")) return SV_OK;
        if (api.read->entities(api.owner, SV_ENTITY_PLAYER, 0, page, 256, &count, &next) != SV_OK) return SV_STATE;
        for (i = 0; i < count; ++i) {
            if (api.read->player(api.owner, page[i], &target) != SV_OK) continue;
            remember(page[i]);
            of_tell(&api, actor, "%s: %s", target.name, groups.group[session_group(cached(page[i]))].name);
        }
        return SV_OK;
    }
    if (of_token(&rest, target_name, sizeof target_name) != 1 || of_token(&rest, value, sizeof value) != 1 || *rest) {
        of_tell(&api, actor, "Use /perm set PLAYER GROUP or /perm check PLAYER NODE."); return SV_OK;
    }
    if (of_target(&api, actor, target_name, &target) != SV_OK) { of_tell(&api, actor, "Player not found."); return SV_OK; }
    remember(target.entity);
    if (!strcmp(operation, "check")) {
        if (permitted(actor, "permissions.view") && pm_node(value, 0))
            of_tell(&api, actor, "%s has %s: %s", target.name, value, can(target.entity, value) ? "yes" : "no");
    } else if (!strcmp(operation, "set")) {
        int group = pm_name(value) ? pm_find(&groups, value) : -1;
        SvResult result = set_group(actor, cached(target.entity), group);
        of_tell(&api, actor, result == SV_OK ? "Group assignment saved." : "Group assignment refused: %d.", result);
    } else of_tell(&api, actor, "Unknown permission command.");
    return SV_OK;
}

static int editable(SvEntity actor, const PmGroup *group)
{
    if (!permitted(actor, "permissions.group")) return 0;
    if ((!strcmp(group->name, "owner") && !is_owner(actor)) || group->level > level(actor)) {
        of_tell(&api, actor, "The target group has a higher level or is protected."); return 0;
    }
    return 1;
}

static SvResult SV_CALL group_command(SvOwner owner, uint64_t actor, const char *arguments)
{
    char line[256], operation[64], name[64], value[64], *rest = line;
    PmGroups candidate;
    PmGroup *group;
    int index;
    SvResult result = SV_OK;
    uint32_t i;
    (void)owner;
    if (!ready(actor)) return SV_OK;
    if (strnlen(arguments, sizeof line) >= sizeof line) return SV_INVALID;
    strcpy(line, arguments);
    if (of_token(&rest, operation, sizeof operation) != 1) { of_tell(&api, actor, "Use /group list, show, create, delete, allow, deny, remove, prefix or level."); return SV_OK; }
    if (!strcmp(operation, "list") && !*rest) {
        if (permitted(actor, "permissions.view"))
            for (i = 0; i < groups.count; ++i) of_tell(&api, actor, "%s level=%d", groups.group[i].name, groups.group[i].level);
        return SV_OK;
    }
    if (of_token(&rest, name, sizeof name) != 1 || !pm_name(name)) { of_tell(&api, actor, "Invalid group name."); return SV_OK; }
    index = pm_find(&groups, name);
    if (!strcmp(operation, "show") && index >= 0 && !*rest) {
        if (!permitted(actor, "permissions.view")) return SV_OK;
        group = &groups.group[index];
        of_tell(&api, actor, "Group %s level=%d parent=%s prefix=%s", name, group->level, group->parent, group->prefix);
        for (i = 0; i < group->node_count; ++i) of_tell(&api, actor, "%s", group->nodes[i]);
        return SV_OK;
    }
    candidate = groups;
    if (!strcmp(operation, "create")) {
        int parent;
        if (!permitted(actor, "permissions.group")) return SV_OK;
        if (index >= 0 || candidate.count >= SV_PERMISSION_GROUP_MAX || of_token(&rest, value, sizeof value) < 0 || *rest) {
            of_tell(&api, actor, "Group exists, the group limit is full, or the parent is invalid."); return SV_OK;
        }
        parent = value[0] ? pm_find(&groups, value) : pm_find(&groups, "guest");
        if (parent < 0 || groups.group[parent].level > level(actor)) { of_tell(&api, actor, "Parent not found or above your level."); return SV_OK; }
        group = &candidate.group[candidate.count++];
        memset(group, 0, sizeof *group);
        group->format = 1; group->level = groups.group[parent].level;
        strcpy(group->name, name); strcpy(group->parent, groups.group[parent].name);
        result = save_group(group);
        if (result == SV_OK) result = save_index(&candidate);
        if (result == SV_OK) groups = candidate;
    } else {
        if (index < 0) { of_tell(&api, actor, "Group not found."); return SV_OK; }
        group = &candidate.group[index];
        if (!editable(actor, group)) return SV_OK;
        if (!strcmp(operation, "delete") && !*rest) {
            char key[SV_STORAGE_KEY_SIZE];
            if (pm_default(name)) { of_tell(&api, actor, "A default group cannot be deleted."); return SV_OK; }
            for (i = 0; i < groups.count; ++i)
                if (!strcmp(groups.group[i].parent, name)) { of_tell(&api, actor, "A child group still uses this parent."); return SV_OK; }
            memmove(group, group + 1, (candidate.count - (uint32_t)index - 1) * sizeof *group);
            --candidate.count;
            result = save_index(&candidate);
            if (result == SV_OK) {
                groups = candidate;
                snprintf(key, sizeof key, "g.%s", name);
                of_erase(&api, SV_STORAGE_SERVER, 0, key);
            }
        } else if (!strcmp(operation, "prefix")) {
            if (strnlen(rest, sizeof group->prefix) >= sizeof group->prefix) { of_tell(&api, actor, "A prefix holds at most 31 bytes."); return SV_OK; }
            strcpy(group->prefix, rest);
            if (!pm_valid(&candidate)) { of_tell(&api, actor, "A prefix must use printable ASCII."); return SV_OK; }
            result = save_group(group);
            if (result == SV_OK) groups = candidate;
        } else {
            if (of_token(&rest, value, sizeof value) != 1 || *rest) { of_tell(&api, actor, "Invalid group value."); return SV_OK; }
            if (!strcmp(operation, "level")) {
                int32_t number;
                if (!of_number(value, 0, 100, &number) || number > level(actor) ||
                    (!strcmp(name, "owner") && number != 100) ||
                    (actor && cached(actor) && !strcmp(cached(actor)->group, name) && number < group->level)) {
                    of_tell(&api, actor, "Level must be 0 to 100 and within your authority."); return SV_OK;
                }
                group->level = number;
            } else if (!strcmp(operation, "allow") || !strcmp(operation, "deny") || !strcmp(operation, "remove")) {
                uint32_t kept = 0;
                if (!pm_node(value, 0)) { of_tell(&api, actor, "Invalid permission node."); return SV_OK; }
                if (!strcmp(operation, "allow") && actor && !can(actor, value)) { of_tell(&api, actor, "You cannot grant a node you do not hold."); return SV_OK; }
                for (i = 0; i < group->node_count; ++i) {
                    const char *node = group->nodes[i][0] == '-' ? group->nodes[i] + 1 : group->nodes[i];
                    if (strcmp(node, value)) {
                        if (kept != i) strcpy(group->nodes[kept], group->nodes[i]);
                        ++kept;
                    }
                }
                group->node_count = kept;
                if (strcmp(operation, "remove")) {
                    if (kept >= PM_NODE_MAX || (operation[0] == 'd' && strlen(value) + 1 >= SV_PERMISSION_NODE_SIZE)) {
                        of_tell(&api, actor, "The node list or node text is full."); return SV_OK;
                    }
                    snprintf(group->nodes[kept], SV_PERMISSION_NODE_SIZE, "%s%s", operation[0] == 'd' ? "-" : "", value);
                    ++group->node_count;
                }
            } else { of_tell(&api, actor, "Unknown group command."); return SV_OK; }
            if (!pm_valid(&candidate)) { of_tell(&api, actor, "Invalid group data."); return SV_OK; }
            result = save_group(group);
            if (result == SV_OK) groups = candidate;
        }
    }
    if (result == SV_OK) {
        refresh();
        of_tell(&api, actor, "Group saved.");
    } else of_tell(&api, actor, "Group save failed: %d.", result);
    return SV_OK;
}

static SvResult SV_CALL enable(const SvHost *host, SvOwner owner)
{
    SvResult result = of_init(&api, host, owner, OF_CAP(SV_API_GAME_READ) | OF_CAP(SV_API_GAME_WRITE) |
        OF_CAP(SV_API_GAME_PROPS) | OF_CAP(SV_API_STORAGE) | OF_CAP(SV_API_CONFIG) | OF_CAP(SV_API_PLUGINS));
    if (result != SV_OK) return result;
    pm_defaults(&groups);
    memset(sessions, 0, sizeof sessions); loaded = 0; owners = 0;
    result = api.plugins->provide(owner, SV_PERMISSIONS_API, SV_PERMISSIONS_VERSION, &service);
    if (result == SV_OK) result = host->command(owner, "perm", SV_COMMAND_PUBLIC, perm_command);
    if (result == SV_OK) result = host->command(owner, "pm", SV_COMMAND_PUBLIC, perm_command);
    if (result == SV_OK) result = host->command(owner, "group", SV_COMMAND_PUBLIC, group_command);
    if (result == SV_OK) result = host->command(owner, "grp", SV_COMMAND_PUBLIC, group_command);
    if (result == SV_OK) result = host->command(owner, "whoami", SV_COMMAND_PUBLIC, whoami_command);
    if (result == SV_OK) result = host->subscribe(owner, SV_EVENT_TICK | SV_EVENT_CONFIG | SV_EVENT_JOIN | SV_EVENT_LEAVE |
        SV_EVENT_EXT_BIT(2) | SV_EVENT_EXT_BIT(3) | SV_EVENT_EXT_BIT(4), event);
    return result;
}

static void SV_CALL disable(SvOwner owner)
{
    (void)owner;
    memset(sessions, 0, sizeof sessions); loaded = 0;
}

SV_EXPORT SvResult SV_CALL sv_plugin_entry(uint32_t abi, SvPlugin *plugin)
{
    if (abi != SV_PLUGIN_ABI || !plugin || plugin->size < sizeof *plugin) return SV_INCOMPATIBLE;
    memset(plugin, 0, sizeof *plugin);
    plugin->size = sizeof *plugin; plugin->abi = SV_PLUGIN_ABI;
    strcpy(plugin->id, SV_OFFICIAL_ID); strcpy(plugin->version, SV_OFFICIAL_VERSION);
    plugin->flags = SV_PLUGIN_LIVE; plugin->enable = enable; plugin->disable = disable;
    return SV_OK;
}
