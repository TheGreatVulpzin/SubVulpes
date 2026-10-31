#include "groups.h"
#include <string.h>

static int letter(char c) { return c >= 'a' && c <= 'z'; }

int pm_name(const char *name)
{
    size_t i, length = name ? strnlen(name, SV_PERMISSION_NAME_SIZE) : 0;
    if (!length || length >= SV_PERMISSION_NAME_SIZE || !letter(name[0])) return 0;
    for (i = 1; i < length; ++i)
        if (!letter(name[i]) && !(name[i] >= '0' && name[i] <= '9') && name[i] != '_' && name[i] != '-') return 0;
    return 1;
}

int pm_node(const char *node, int negative)
{
    size_t i = 0, length = node ? strnlen(node, SV_PERMISSION_NODE_SIZE) : 0;
    int first = 1;
    if (!length || length >= SV_PERMISSION_NODE_SIZE) return 0;
    if (node[i] == '-') { if (!negative) return 0; ++i; }
    for (; i < length; ++i) {
        if (node[i] == '*' && i + 1 == length && first) return 1;
        if (first && !letter(node[i])) return 0;
        if (!letter(node[i]) && !(node[i] >= '0' && node[i] <= '9') && node[i] != '_' && node[i] != '.') return 0;
        first = node[i] == '.';
    }
    return !first;
}

int pm_find(const PmGroups *groups, const char *name)
{
    uint32_t i;
    for (i = 0; i < groups->count; ++i)
        if (!strcmp(groups->group[i].name, name)) return (int)i;
    return -1;
}

int pm_default(const char *name)
{
    return !strcmp(name, "guest") || !strcmp(name, "member") || !strcmp(name, "moderator") ||
        !strcmp(name, "admin") || !strcmp(name, "owner");
}

static int matches(const char *rule, const char *node)
{
    size_t length = strlen(rule);
    if (!strcmp(rule, "*")) return 1;
    if (length >= 2 && rule[length - 1] == '*' && rule[length - 2] == '.')
        return !strncmp(rule, node, length - 1) && node[length - 1] != 0;
    return !strcmp(rule, node);
}

int pm_can(const PmGroups *groups, int group, const char *node)
{
    uint32_t depth;
    if (!pm_node(node, 0)) return 0;
    for (depth = 0; depth < groups->count && group >= 0 && (uint32_t)group < groups->count; ++depth) {
        const PmGroup *g = &groups->group[group];
        uint32_t n;
        int allowed = 0;
        /* The owner retains the tools needed to repair its own permissions. */
        if (!strcmp(g->name, "owner") && (!strncmp(node, "permissions.", 12) || !strcmp(node, "core.admin"))) return 1;
        for (n = 0; n < g->node_count; ++n) {
            const char *rule = g->nodes[n];
            if (rule[0] == '-' && matches(rule + 1, node)) return 0;
            if (rule[0] != '-' && matches(rule, node)) allowed = 1;
        }
        if (allowed) return 1;
        group = g->parent[0] ? pm_find(groups, g->parent) : -1;
    }
    return 0;
}

int pm_valid(const PmGroups *groups)
{
    uint32_t i, j, depth;
    if (groups->count < 5 || groups->count > SV_PERMISSION_GROUP_MAX) return 0;
    for (i = 0; i < groups->count; ++i) {
        const PmGroup *g = &groups->group[i];
        if (g->format != 1 || !pm_name(g->name) || g->level < 0 || g->level > 100 ||
            g->node_count > PM_NODE_MAX || strnlen(g->prefix, sizeof g->prefix) >= sizeof g->prefix ||
            strnlen(g->parent, sizeof g->parent) >= sizeof g->parent) return 0;
        for (j = 0; j < i; ++j) if (!strcmp(g->name, groups->group[j].name)) return 0;
        for (j = 0; g->prefix[j]; ++j) if ((unsigned char)g->prefix[j] < 32 || (unsigned char)g->prefix[j] > 126) return 0;
        for (j = 0; j < g->node_count; ++j) if (!pm_node(g->nodes[j], 1)) return 0;
    }
    for (i = 0; i < groups->count; ++i) {
        int parent = (int)i;
        for (depth = 0; depth < groups->count; ++depth) {
            const char *name = groups->group[parent].parent;
            if (!*name) break;
            parent = pm_find(groups, name);
            if (parent < 0) return 0;
        }
        if (depth == groups->count) return 0;
    }
    return pm_find(groups, "guest") >= 0 && pm_find(groups, "member") >= 0 && pm_find(groups, "moderator") >= 0 &&
        pm_find(groups, "admin") >= 0 && pm_find(groups, "owner") >= 0 && groups->group[pm_find(groups, "owner")].level == 100;
}

static void add(PmGroups *groups, const char *name, int level, const char *parent, const char *prefix, const char *nodes)
{
    PmGroup *g = &groups->group[groups->count++];
    const char *p = nodes;
    g->format = 1;
    g->level = level;
    strcpy(g->name, name); strcpy(g->parent, parent); strcpy(g->prefix, prefix);
    while (*p) {
        const char *end = strchr(p, ',');
        size_t length = end ? (size_t)(end - p) : strlen(p);
        memcpy(g->nodes[g->node_count++], p, length);
        p += length + (end != NULL);
    }
}

void pm_defaults(PmGroups *groups)
{
    memset(groups, 0, sizeof *groups);
    add(groups, "guest", 0, "", "", "permissions.view,chatadd.msg,admcmd.help");
    add(groups, "member", 10, "guest", "[Member]", "");
    add(groups, "moderator", 50, "member", "[Mod]", "core.kick,core.list,core.message,bans.kick,bans.ban,bans.view,chatadd.mute,admcmd.players,admcmd.who,admcmd.where,admcmd.id");
    add(groups, "admin", 90, "moderator", "[Admin]", "core.*,admcmd.*,bans.*,chatadd.*,permissions.*,auditlogs.view");
    add(groups, "owner", 100, "admin", "[Owner]", "*");
}
