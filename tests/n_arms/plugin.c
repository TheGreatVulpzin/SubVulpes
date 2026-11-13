#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include "systems_plugin.h"

static const SvHost *host;
static const SvGameProps *props;
static const SvGameRead *game;
static SvEntity normal, body;
static uint32_t pose, target;

static int errors(SvOwner owner)
{
    SvValue value = {0};
    SvPropInfo info = {sizeof info};
    STEP(props->find(owner, "human.arm_pose", &pose) == SV_OK);
    STEP(props->find(owner, "human.arm_target", &target) == SV_OK);
    STEP(props->info(owner, target, &info) == SV_OK && info.type == SV_PROP_VECTOR && info.count == 2);
    value.i = -1;
    STEP(props->set(owner, pose, body, 0, value) == SV_INVALID);
    value.i = 4;
    STEP(props->set(owner, pose, body, 0, value) == SV_INVALID);
    value.i = 1;
    STEP(props->set(owner, pose, body, 1, value) == SV_INVALID);
    STEP(props->add(owner, pose, body, 0, value) == SV_DENIED);
    STEP(props->set(owner, pose, normal, 0, value) == SV_NOT_FOUND);
    STEP(props->set(owner, pose, body ^ ((uint64_t)1 << 32), 0, value) == SV_NOT_FOUND);
    value.v = (SvVector){0, 0, -1.6f};
    STEP(props->set(owner, target, body, 0, value) == SV_INVALID);
    value.v = (SvVector){0, 0, 0};
    value.i = 0x7f800000; /* x is infinite */
    STEP(props->set(owner, target, body, 0, value) == SV_INVALID);
    value.v = (SvVector){0.15f, 0.1f, -0.3f};
    STEP(props->set(owner, target, body, 2, value) == SV_INVALID);
    STEP(props->add(owner, target, body, 0, value) == SV_DENIED);
    STEP(props->set(0, target, body, 0, value) == SV_STATE);
    /* No pose yet: a read gives 0. */
    STEP(props->get(owner, pose, body, 0, &value) == SV_OK && value.i == 0);
    return 0;
}

static DWORD WINAPI wrong_thread(void *argument)
{
    SvOwner owner = *(SvOwner *)argument;
    SvValue value = {0};
    value.i = 1;
    STEP(props->set(owner, pose, body, 0, value) == SV_STATE);
    return 0;
}

static int setup(SvOwner owner)
{
    const void *table;
    SvEntity list[256];
    uint32_t count, next, i;
    HANDLE thread;
    DWORD code;
    STEP(host->get_api(owner, SV_API_GAME_PROPS, 1, &table) == SV_OK);
    props = table;
    STEP(host->get_api(owner, SV_API_GAME_READ, 1, &table) == SV_OK);
    game = table;
    STEP(game->entities(owner, SV_ENTITY_PLAYER, 0, list, 256, &count, &next) == SV_OK);
    for (i = 0; i < count; ++i) {
        SvPlayerState p = {sizeof p};
        STEP(game->player(owner, list[i], &p) == SV_OK);
        if (!strcmp(p.name, "Systems test")) {
            normal = list[i];
            body = p.human;
        }
    }
    STEP(normal && body);
    {
        int line = errors(owner);
        if (line) return line;
    }
    thread = CreateThread(NULL, 0, wrong_thread, &owner, 0, NULL);
    STEP(thread != NULL);
    STEP(WaitForSingleObject(thread, 5000) == WAIT_OBJECT_0);
    STEP(GetExitCodeThread(thread, &code));
    CloseHandle(thread);
    STEP(code == 0);
    return 0;
}

static int run(SvOwner owner, const char *text)
{
    SvValue value = {0};
    if (!normal) {
        int line = setup(owner);
        if (line) return line;
    }
    if (!strcmp(text, "on")) {
        /* The guard of the old punch plugin on side 0. */
        value.v = (SvVector){0.15f, 0.1f, -0.3f};
        STEP(props->set(owner, target, body, 0, value) == SV_OK);
        value.v = (SvVector){-0.2f, 0.2f, -1.0f};
        STEP(props->set(owner, target, body, 1, value) == SV_OK);
        value.i = 1;
        STEP(props->set(owner, pose, body, 0, value) == SV_OK);
        STEP(props->get(owner, pose, body, 0, &value) == SV_OK && value.i == 1);
        STEP(props->get(owner, target, body, 1, &value) == SV_OK && value.v.x == -0.2f && value.v.z == -1.0f);
    } else if (!strcmp(text, "both")) {
        value.i = 3;
        STEP(props->set(owner, pose, body, 0, value) == SV_OK);
    } else if (!strcmp(text, "off")) {
        value.i = 0;
        STEP(props->set(owner, pose, body, 0, value) == SV_OK);
    } else if (!strcmp(text, "read-zero")) {
        STEP(props->get(owner, pose, body, 0, &value) == SV_OK && value.i == 0);
        STEP(props->get(owner, target, body, 0, &value) == SV_OK && value.v.x == 0 && value.v.z == 0);
    }
    return 0;
}

static SvResult SV_CALL command(SvOwner owner, uint64_t actor, const char *text)
{
    int line = run(owner, text);
    char message[96];
    (void)actor;
    if (line) {
        snprintf(message, sizeof message, "N arms test failed at line %d.", line);
        host->log(owner, message);
        return SV_FAILED;
    }
    return systems_godmode(props, owner, normal, 1);
}

static SvResult SV_CALL enable(const SvHost *api, SvOwner owner)
{
    host = api;
    return host->command(owner, "narms", SV_COMMAND_LOCAL, command);
}

SV_EXPORT SvResult SV_CALL sv_plugin_entry(uint32_t abi, SvPlugin *plugin)
{
    if (abi != SV_PLUGIN_ABI || !plugin || plugin->size < sizeof *plugin)
        return SV_INCOMPATIBLE;
    memset(plugin, 0, sizeof *plugin);
    plugin->size = sizeof *plugin;
    plugin->abi = abi;
    strcpy(plugin->id, "test.narms");
    strcpy(plugin->version, "0.1.0");
    plugin->flags = SV_PLUGIN_LIVE;
    plugin->enable = enable;
    return SV_OK;
}
