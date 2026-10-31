#ifndef SV_PERMISSION_GROUPS_H
#define SV_PERMISSION_GROUPS_H

#include "sv_permissions.h"

#define PM_NODE_MAX 20u
typedef struct {
    uint32_t format, node_count;
    int32_t level;
    char name[SV_PERMISSION_NAME_SIZE], parent[SV_PERMISSION_NAME_SIZE], prefix[32];
    char nodes[PM_NODE_MAX][SV_PERMISSION_NODE_SIZE];
} PmGroup;
typedef struct {
    uint32_t count;
    PmGroup group[SV_PERMISSION_GROUP_MAX];
} PmGroups;

int pm_name(const char *name);
int pm_node(const char *node, int negative);
int pm_find(const PmGroups *groups, const char *name);
int pm_default(const char *name);
int pm_can(const PmGroups *groups, int group, const char *node);
int pm_valid(const PmGroups *groups);
void pm_defaults(PmGroups *groups);

#endif
