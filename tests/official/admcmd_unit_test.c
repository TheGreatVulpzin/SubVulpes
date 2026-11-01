#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include "../../plugins/admcmd/admcmd.c"
#include <stdlib.h>

#define CHECK(x) do { if (!(x)) { fprintf(stderr, "%d: %s\n", __LINE__, #x); exit(1); } } while (0)

static char reply[2048];
static int permitted, admin_flag = 1, bot_flag, writes, null_service;
static float test_mass = 2;
static SvResult borrow_result = SV_OK;
static SvPermissions service;
static SvPlugins plugins;

static SvResult SV_CALL tell(SvOwner owner, uint32_t kind, SvEntity player, const char *text)
{
    (void)owner; (void)kind; (void)player;
    CHECK(strlen(reply) + strlen(text) < sizeof reply);
    strcat(reply, text); return SV_OK;
}
static SvResult SV_CALL log_text(SvOwner owner, const char *text)
{
    return tell(owner, 0, 0, text);
}
static int SV_CALL can(SvEntity player, const char *node)
{
    (void)player; (void)node; return permitted;
}
static SvResult SV_CALL group_of(SvEntity player, SvPermissionGroup *out)
{
    (void)player; out->level = 10; strcpy(out->name, "member"); out->prefix[0] = 0; return SV_OK;
}
static SvResult SV_CALL groups(SvPermissionGroup *out, uint32_t capacity, uint32_t stride, uint32_t *count)
{
    (void)out; (void)capacity; (void)stride; (void)count; return SV_OK;
}
static SvResult SV_CALL assign(SvEntity actor, SvEntity player, const char *group)
{
    (void)actor; (void)player; (void)group; return SV_OK;
}
static SvResult SV_CALL borrow(SvOwner owner, const char *name, uint32_t version, const void **out)
{
    CHECK(owner == 1 && !strcmp(name, SV_PERMISSIONS_API) && version == 1);
    *out = null_service ? NULL : &service; return borrow_result;
}
static SvResult SV_CALL read_player(SvOwner owner, SvEntity player, SvPlayerState *out)
{
    (void)owner; out->entity = player; out->human = 11; out->is_admin = admin_flag; out->is_bot = bot_flag;
    return SV_OK;
}
static SvResult SV_CALL read_human(SvOwner owner, SvEntity human, SvHumanState *out)
{
    (void)owner; out->entity = human; out->health = 100; return SV_OK;
}
static SvResult SV_CALL read_bone(SvOwner owner, SvEntity human, uint32_t slot, SvBoneState *out)
{
    (void)owner; (void)human; (void)slot; out->mass = test_mass; return SV_OK;
}
static SvResult SV_CALL find_property(SvOwner owner, const char *name, uint32_t *out)
{
    (void)owner; (void)name; *out = 1; return SV_OK;
}
static SvResult SV_CALL set_property(SvOwner owner, uint32_t id, SvEntity entity, uint32_t slot, SvValue value)
{
    (void)owner; (void)id; (void)entity; (void)slot; (void)value; ++writes; return SV_OK;
}

int main(void)
{
    SvHost host = { sizeof host, SV_PLUGIN_ABI };
    SvGameRead read = { sizeof read, 1 };
    SvGameWrite write = { sizeof write, 1 };
    SvGameProps props = { sizeof props, 1 };
    const SvPermissions *found;
    SvPermissionGroup group = { sizeof group };
    SvPlayerState player = { sizeof player };
    char long_line[257], many_words[200];
    unsigned i;
    SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX);
    host.log = log_text; read.player = read_player; read.human = read_human; read.bone = read_bone;
    write.message_to = tell; props.find = find_property; props.set = set_property;
    api.host = &host; api.owner = 1; api.read = &read; api.write = &write; api.props = &props;
    api.plugins = &plugins;
    plugins.size = sizeof plugins; plugins.borrow = borrow;
    service = (SvPermissions){ sizeof service, 1, can, group_of, groups, assign };
    CHECK(of_permissions(&api, &found) == SV_OK && found == &service);
    plugins.size = offsetof(SvPlugins, borrow);
    CHECK(of_permissions(&api, &found) == SV_INCOMPATIBLE && !found);
    plugins.size = sizeof plugins; plugins.borrow = NULL;
    CHECK(of_permissions(&api, &found) == SV_INCOMPATIBLE && !found);
    plugins.borrow = borrow; service.size = sizeof service - 1;
    CHECK(of_permissions(&api, &found) == SV_INCOMPATIBLE && !found);
    service.size = sizeof service; service.version = 2;
    CHECK(of_permissions(&api, &found) == SV_INCOMPATIBLE && !found);
    service.version = 1; service.can = NULL;
    CHECK(of_permissions(&api, &found) == SV_INCOMPATIBLE && !found);
    service.can = can;
    service.group_of = NULL; CHECK(of_permissions(&api, &found) == SV_INCOMPATIBLE && !found);
    service.group_of = group_of; service.groups = NULL;
    CHECK(of_permissions(&api, &found) == SV_INCOMPATIBLE && !found);
    service.groups = groups; service.assign = NULL;
    CHECK(of_permissions(&api, &found) == SV_INCOMPATIBLE && !found);
    service.assign = assign; null_service = 1;
    CHECK(of_permissions(&api, &found) == SV_INCOMPATIBLE && !found);
    null_service = 0;
    CHECK(!of_can(&api, 1, "admcmd.heal"));
    permitted = 1; CHECK(of_can(&api, 1, "admcmd.heal"));
    borrow_result = SV_INCOMPATIBLE; CHECK(!of_can(&api, 1, "admcmd.heal"));
    borrow_result = SV_NOT_FOUND; CHECK(of_can(&api, 1, "admcmd.heal"));
    bot_flag = 1; CHECK(!of_can(&api, 1, "admcmd.heal"));
    bot_flag = 0; admin_flag = 0; CHECK(!of_can(&api, 1, "admcmd.heal"));
    CHECK(of_group(&api, 1, &group) == SV_OK && group.level == 0 && !strcmp(group.name, "guest"));
    admin_flag = 1;
    CHECK(of_group(&api, 1, &group) == SV_OK && group.level == 90 && !strcmp(group.name, "admin"));
    CHECK(of_group(&api, 0, &group) == SV_OK && group.level == 100);
    borrow_result = SV_OK;
    CHECK(of_group(&api, 1, &group) == SV_OK && group.level == 10);
    permitted = 0;
    CHECK(command("heal", 0, 1, 1, "") == SV_OK && strstr(reply, "You do not have permission: admcmd.heal") && !writes);
    permitted = 1; reply[0] = 0;
    memset(long_line, 'x', sizeof long_line - 1); long_line[sizeof long_line - 1] = 0;
    CHECK(command("help", 0, 0, 1, long_line) == SV_OK && strstr(reply, "Command text is too long."));
    reply[0] = 0;
    for (i = 0; i < sizeof many_words - 1; ++i) many_words[i] = i % 2 ? ' ' : 'x';
    many_words[sizeof many_words - 1] = 0;
    CHECK(command("help", 0, 0, 1, many_words) == SV_OK && strstr(reply, "use /help for the command form."));
    reply[0] = 0;
    CHECK(command("heal", 0, 1, 1, "self extra") == SV_OK && strstr(reply, "heal failed: invalid input. Use /help.") && !writes);
    player.entity = 1; player.human = 11;
    test_mass = 0.25f; CHECK(heavy(&player) == SV_INVALID && !writes);
    test_mass = 501; CHECK(heavy(&player) == SV_INVALID && !writes);
    test_mass = 2; CHECK(heavy(&player) == SV_OK && writes == 15);
    CHECK(heavy(&player) == SV_CONFLICT && writes == 15);
    for (i = 0; i < 256; ++i) effects[i].human = 1000 + i;
    CHECK(heavy(&player) == SV_LIMIT && writes == 15);
    puts("Admin command contracts passed.");
    return 0;
}
