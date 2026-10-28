#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include "systems_plugin.h"
#include "sv_game_engine.h"
#include "sv_game_props.h"

static const SvGameEngine *engine;
static const SvGameProps *write_api;
static const SvHost *saved_host;
static SvEntity player;
static const char *names[SV_ENGINE_PARAM_COUNT] = {
    "object.engine_power", "physics.ragdoll_orientation_reset", "human.death_start"
};
static const uint32_t targets[SV_ENGINE_PARAM_COUNT] = {9, SV_ENGINE_GLOBAL, SV_ENGINE_GLOBAL};
static const uint32_t types[SV_ENGINE_PARAM_COUNT] = {SV_ENGINE_FLOAT, SV_ENGINE_BOOL, SV_ENGINE_INT};

static SvEngineValue real_value(float value)
{
    SvEngineValue result = {sizeof result, SV_ENGINE_FLOAT, {0}};
    result.value.real = value;
    return result;
}

static SvEngineValue integer_value(uint32_t type, int32_t value)
{
    SvEngineValue result = {sizeof result, type, {0}};
    result.value.integer = value;
    return result;
}

static int get_failure(SvOwner owner, const char *name, uint32_t target, SvResult expected)
{
    SvEngineValue out, before;
    memset(&out, 0xa5, sizeof out);
    out.size = sizeof out;
    before = out;
    STEP(engine->get(owner, name, target, &out) == expected);
    STEP(!memcmp(&out, &before, sizeof out));
    return 0;
}

static int describe_failure(SvOwner owner, uint32_t index, uint32_t target, SvResult expected)
{
    SvEngineInfo out, before;
    memset(&out, 0xa5, sizeof out);
    out.size = sizeof out;
    before = out;
    STEP(engine->describe(owner, index, target, &out) == expected);
    STEP(!memcmp(&out, &before, sizeof out));
    return 0;
}

static int metadata(SvOwner owner)
{
    SvEngineInfo out = {sizeof out};
    uint32_t i, model;
    for (i = 0; i < SV_ENGINE_PARAM_COUNT; ++i) {
        STEP(engine->describe(owner, i, targets[i], &out) == SV_OK);
        STEP(out.size == sizeof out && out.index == i && !strcmp(out.name, names[i]));
        STEP(out.scope == (uint32_t)(i == SV_ENGINE_POWER ? SV_ENGINE_PER_TYPE : SV_ENGINE_GLOBAL_SCOPE));
        STEP(out.minimum.size == sizeof(SvEngineValue) && out.maximum.size == sizeof(SvEngineValue));
        STEP(out.default_value.size == sizeof(SvEngineValue));
        STEP(out.minimum.type == types[i] && out.maximum.type == types[i] && out.default_value.type == types[i]);
        if (i == SV_ENGINE_POWER) {
            STEP(out.minimum.value.real == 0 && out.maximum.value.real == 16);
            STEP(out.default_value.value.real == 1);
        } else {
            STEP(out.minimum.value.integer == 0);
            STEP(out.maximum.value.integer == (i == SV_ENGINE_RAGDOLL_RESET ? 1 : 99));
            STEP(out.default_value.value.integer == (i == SV_ENGINE_RAGDOLL_RESET ? 1 : 0));
        }
    }
    for (model = 0; model < SV_ENGINE_OBJECT_TYPES; ++model)
        STEP(engine->describe(owner, SV_ENGINE_POWER, model, &out) == SV_OK);
    STEP(engine->describe(owner, SV_ENGINE_POWER, 2, &out) == SV_OK);
    STEP(out.default_value.value.real == 2);
    return 0;
}

static int values(SvOwner owner, float power, int32_t reset, int32_t death)
{
    SvEngineValue out = {sizeof out};
    STEP(engine->get(owner, names[0], 9, &out) == SV_OK);
    STEP(out.size == sizeof out && out.type == SV_ENGINE_FLOAT && out.value.real == power);
    STEP(engine->get(owner, names[0], 2, &out) == SV_OK && out.value.real == 2);
    STEP(engine->get(owner, names[1], SV_ENGINE_GLOBAL, &out) == SV_OK);
    STEP(out.type == SV_ENGINE_BOOL && out.value.integer == reset);
    STEP(engine->get(owner, names[2], SV_ENGINE_GLOBAL, &out) == SV_OK);
    STEP(out.type == SV_ENGINE_INT && out.value.integer == death);
    STEP(metadata(owner) == 0);
    return 0;
}

static DWORD WINAPI wrong_thread(void *data)
{
    SvOwner owner = *(SvOwner *)data;
    STEP(describe_failure(owner, SV_ENGINE_POWER, 9, SV_STATE) == 0);
    STEP(get_failure(owner, NULL, 9, SV_STATE) == 0);
    STEP(engine->set(owner, NULL, 9, NULL) == SV_STATE);
    return 0;
}

static int invalid_inputs(SvOwner owner)
{
    SvEngineValue input = real_value(4), out, before;
    SvEngineInfo info, old_info;
    HANDLE thread;
    DWORD code;
    uint32_t i;
    const uint32_t float_bits[] = {0x7fc00000u, 0x7f800000u, 0xff800000u};
    const char *bad_names[] = {NULL, ""};
    char long_name[SV_ENGINE_NAME_SIZE + 1];
    thread = CreateThread(NULL, 0, wrong_thread, &owner, 0, NULL);
    STEP(thread != NULL);
    STEP(WaitForSingleObject(thread, 5000) == WAIT_OBJECT_0);
    STEP(GetExitCodeThread(thread, &code));
    CloseHandle(thread);
    STEP(code == 0);
    STEP(describe_failure(owner + 1, 0, 9, SV_STATE) == 0);
    STEP(get_failure(owner + 1, NULL, 9, SV_STATE) == 0);
    STEP(engine->set(owner + 1, NULL, 9, NULL) == SV_STATE);
    STEP(describe_failure(owner, SV_ENGINE_PARAM_COUNT, 9, SV_INVALID) == 0);
    STEP(describe_failure(owner, 0x40000000u, 9, SV_INVALID) == 0);
    STEP(describe_failure(owner, SV_ENGINE_POWER, SV_ENGINE_OBJECT_TYPES, SV_INVALID) == 0);
    STEP(describe_failure(owner, SV_ENGINE_POWER, 0x40000000u, SV_INVALID) == 0);
    STEP(describe_failure(owner, SV_ENGINE_POWER, SV_ENGINE_GLOBAL, SV_INVALID) == 0);
    STEP(describe_failure(owner, SV_ENGINE_RAGDOLL_RESET, 9, SV_INVALID) == 0);
    STEP(describe_failure(owner, SV_ENGINE_DEATH_START, 9, SV_INVALID) == 0);
    STEP(engine->describe(owner, SV_ENGINE_POWER, 9, NULL) == SV_INVALID);
    memset(&info, 0xa5, sizeof info);
    info.size = sizeof info - 1;
    old_info = info;
    STEP(engine->describe(owner, SV_ENGINE_POWER, 9, &info) == SV_INVALID);
    STEP(!memcmp(&info, &old_info, sizeof info));
    STEP(engine->get(owner, names[0], 9, NULL) == SV_INVALID);
    memset(&out, 0xa5, sizeof out);
    out.size = sizeof out - 1;
    before = out;
    STEP(engine->get(owner, names[0], 9, &out) == SV_INVALID);
    STEP(!memcmp(&out, &before, sizeof out));
    STEP(engine->set(owner, names[0], 9, NULL) == SV_INVALID);
    input.size = sizeof input - 1;
    STEP(engine->set(owner, names[0], 9, &input) == SV_INVALID);
    input.size = sizeof input;
    for (i = 0; i < sizeof bad_names / sizeof bad_names[0]; ++i) {
        STEP(get_failure(owner, bad_names[i], 9, SV_INVALID) == 0);
        STEP(engine->set(owner, bad_names[i], 9, &input) == SV_INVALID);
    }
    memset(long_name, 'x', sizeof long_name);
    long_name[sizeof long_name - 1] = 0;
    STEP(get_failure(owner, long_name, 9, SV_INVALID) == 0);
    STEP(engine->set(owner, long_name, 9, &input) == SV_INVALID);
    long_name[SV_ENGINE_NAME_SIZE - 1] = 0;
    STEP(get_failure(owner, long_name, 9, SV_NOT_FOUND) == 0);
    STEP(engine->set(owner, long_name, 9, &input) == SV_NOT_FOUND);
    STEP(get_failure(owner, "unknown", 9, SV_NOT_FOUND) == 0);
    STEP(engine->set(owner, "unknown", 9, &input) == SV_NOT_FOUND);
    STEP(get_failure(owner, names[0], SV_ENGINE_OBJECT_TYPES, SV_INVALID) == 0);
    STEP(engine->set(owner, names[0], SV_ENGINE_OBJECT_TYPES, &input) == SV_INVALID);
    STEP(get_failure(owner, names[0], SV_ENGINE_GLOBAL, SV_INVALID) == 0);
    STEP(engine->set(owner, names[0], SV_ENGINE_GLOBAL, &input) == SV_INVALID);
    for (i = 0; i < SV_ENGINE_PARAM_COUNT; ++i) {
        input = integer_value(types[i] == SV_ENGINE_INT ? SV_ENGINE_BOOL : SV_ENGINE_INT, 0);
        STEP(engine->set(owner, names[i], targets[i], &input) == SV_INVALID);
        if (i != SV_ENGINE_POWER) {
            input = integer_value(types[i], 0);
            STEP(get_failure(owner, names[i], 9, SV_INVALID) == 0);
            STEP(engine->set(owner, names[i], 9, &input) == SV_INVALID);
            input.value.integer = -1;
            STEP(engine->set(owner, names[i], SV_ENGINE_GLOBAL, &input) == SV_INVALID);
            input.value.integer = i == SV_ENGINE_RAGDOLL_RESET ? 2 : 100;
            STEP(engine->set(owner, names[i], SV_ENGINE_GLOBAL, &input) == SV_INVALID);
        }
    }
    input = real_value(3.402823466e38f);
    STEP(engine->set(owner, names[0], 9, &input) == SV_INVALID);
    input.value.real = -0.01f;
    STEP(engine->set(owner, names[0], 9, &input) == SV_INVALID);
    input.value.real = 16.01f;
    STEP(engine->set(owner, names[0], 9, &input) == SV_INVALID);
    for (i = 0; i < sizeof float_bits / sizeof float_bits[0]; ++i) {
        memcpy(&input.value.real, &float_bits[i], sizeof(float));
        STEP(engine->set(owner, names[0], 9, &input) == SV_INVALID);
    }
    STEP(values(owner, 1, 1, 0) == 0);
    return 0;
}

static int set_value(SvOwner owner, uint32_t index, uint32_t target, const SvEngineValue *input)
{
    SvEngineValue out = {sizeof out};
    STEP(engine->set(owner, names[index], target, input) == SV_OK);
    STEP(engine->get(owner, names[index], target, &out) == SV_OK);
    STEP(out.size == sizeof out && out.type == input->type);
    STEP(!memcmp(&out.value, &input->value, sizeof out.value));
    return 0;
}

static int write_values(SvOwner owner)
{
    SvEngineValue input = real_value(0);
    SvEngineValue last_model = {sizeof last_model};
    STEP(engine->get(owner, names[0], SV_ENGINE_OBJECT_TYPES - 1, &last_model) == SV_OK);
    STEP(set_value(owner, SV_ENGINE_POWER, 9, &input) == 0);
    input.value.real = 16;
    STEP(set_value(owner, SV_ENGINE_POWER, 9, &input) == 0);
    STEP(set_value(owner, SV_ENGINE_POWER, SV_ENGINE_OBJECT_TYPES - 1, &input) == 0);
    STEP(set_value(owner, SV_ENGINE_POWER, SV_ENGINE_OBJECT_TYPES - 1, &last_model) == 0);
    input.value.real = 4;
    STEP(set_value(owner, SV_ENGINE_POWER, 9, &input) == 0);
    input = integer_value(SV_ENGINE_BOOL, 1);
    STEP(set_value(owner, SV_ENGINE_RAGDOLL_RESET, SV_ENGINE_GLOBAL, &input) == 0);
    input.value.integer = 0;
    STEP(set_value(owner, SV_ENGINE_RAGDOLL_RESET, SV_ENGINE_GLOBAL, &input) == 0);
    input = integer_value(SV_ENGINE_INT, 0);
    STEP(set_value(owner, SV_ENGINE_DEATH_START, SV_ENGINE_GLOBAL, &input) == 0);
    input.value.integer = 99;
    STEP(set_value(owner, SV_ENGINE_DEATH_START, SV_ENGINE_GLOBAL, &input) == 0);
    input.value.integer = 9;
    STEP(set_value(owner, SV_ENGINE_DEATH_START, SV_ENGINE_GLOBAL, &input) == 0);
    STEP(values(owner, 4, 0, 9) == 0);
    return 0;
}

static SvResult SV_CALL command(SvOwner owner, uint64_t actor, const char *arguments)
{
    int line;
    char message[96];
    (void)actor;
    if (!strcmp(arguments, "changed"))
        line = values(owner, 4, 0, 9);
    else if (!strcmp(arguments, "default"))
        line = values(owner, 1, 1, 0);
    else
        return SV_INVALID;
    if (line) {
        snprintf(message, sizeof message, "Engine command %s failed at line %d.", arguments, line);
        saved_host->log(owner, message);
        return SV_FAILED;
    }
    return systems_godmode(write_api, owner, player, 1);
}

int engine_fixture(const SvHost *host, SvOwner owner)
{
    const void *table = NULL;
    const SvGameRead *read_api;
    SvEntity entities[SV_ENTITY_PAGE_MAX];
    SvPlayerState state = {sizeof state};
    uint32_t count, next, i;
    saved_host = host;
    STEP(host->get_api(owner, SV_API_ENGINE, SV_GAME_ENGINE_VERSION, &table) == SV_OK);
    engine = table;
    STEP(engine && engine->size >= sizeof *engine && engine->version == SV_GAME_ENGINE_VERSION);
    STEP(host->get_api(owner, SV_API_GAME_READ, SV_GAME_READ_VERSION, &table) == SV_OK);
    read_api = table;
    STEP(host->get_api(owner, SV_API_GAME_PROPS, SV_GAME_PROPS_VERSION, &table) == SV_OK);
    write_api = table;
    STEP(read_api->entities(owner, SV_ENTITY_PLAYER, 0, entities, SV_ENTITY_PAGE_MAX, &count, &next) == SV_OK);
    for (i = 0; i < count; ++i) {
        STEP(read_api->player(owner, entities[i], &state) == SV_OK);
        if (!strcmp(state.name, "Systems test"))
            player = entities[i];
    }
    STEP(player != 0);
    STEP(metadata(owner) == 0);
    STEP(invalid_inputs(owner) == 0);
    STEP(write_values(owner) == 0);
    STEP(host->command(owner, "engine.check", SV_COMMAND_LOCAL, command) == SV_OK);
    return 0;
}
