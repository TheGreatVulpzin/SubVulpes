#include "plugin_host.h"
#include "sv_game_engine.h"
#include "plugin_game_internal.h"
#include "sr_runtime.h"
#include <string.h>

typedef struct {
    const char *name;
    uint32_t scope, type;
    float minimum, maximum;
    int32_t initial;
    size_t type_offset, object_offset;
    double factor;
} Row;

static const Row rows[SV_ENGINE_PARAM_COUNT] = {
    {"object.engine_power", SV_ENGINE_PER_TYPE, SV_ENGINE_FLOAT, 0, 16, 0,
     offsetof(ObjectType, engine_power), offsetof(Object, engine_power), 213.0},
    {"physics.ragdoll_orientation_reset", SV_ENGINE_GLOBAL_SCOPE, SV_ENGINE_BOOL, 0, 1, 1, 0, 0, 0},
    {"human.death_start", SV_ENGINE_GLOBAL_SCOPE, SV_ENGINE_INT, 0, 99, 0, 0, 0, 0}
};
static SvEngineValue defaults[SV_ENGINE_PARAM_COUNT][SV_ENGINE_OBJECT_TYPES];
static SvEngineValue globals[SV_ENGINE_PARAM_COUNT];
static SvOwner writers[SV_ENGINE_PARAM_COUNT][SV_ENGINE_OBJECT_TYPES];
static uint32_t installed;

static SvResult context(SvOwner owner)
{
    return sv_plugins_context(owner) && sv_game_is_ready() ? SV_OK : SV_STATE;
}

static int lookup(const char *name)
{
    uint32_t i;
    if (!name || !*name || strnlen(name, SV_ENGINE_NAME_SIZE) >= SV_ENGINE_NAME_SIZE)
        return -2;
    for (i = 0; i < SV_ENGINE_PARAM_COUNT; ++i)
        if (!strcmp(name, rows[i].name))
            return (int)i;
    return -1;
}

static int target_valid(uint32_t index, uint32_t object_type)
{
    return rows[index].scope == SV_ENGINE_PER_TYPE ? object_type < SV_ENGINE_OBJECT_TYPES : object_type == SV_ENGINE_GLOBAL;
}

static SvEngineValue current(uint32_t index, uint32_t object_type)
{
    SvEngineValue value = {sizeof value, rows[index].type, {0}};
    if (rows[index].scope == SV_ENGINE_PER_TYPE)
        memcpy(&value.value.real, (const char *)&sr_data_objecttype[object_type] + rows[index].type_offset, sizeof(float));
    else if (installed)
        value = globals[index];
    else
        value.value.integer = rows[index].initial;
    return value;
}

static void apply(uint32_t index, uint32_t object_type, const SvEngineValue *value)
{
    uint32_t i;
    if (rows[index].scope == SV_ENGINE_PER_TYPE) {
        float copied = (float)((double)value->value.real * rows[index].factor);
        memcpy((char *)&sr_data_objecttype[object_type] + rows[index].type_offset, &value->value.real, sizeof(float));
        for (i = 0; i < SR_MAX_OBJECT; ++i)
            if (sr_objects[i].active && sr_objects[i].type == (int32_t)object_type)
                memcpy((char *)&sr_objects[i] + rows[index].object_offset, &copied, sizeof copied);
    } else {
        globals[index] = *value;
    }
}

int sv_engine_install(uint32_t features)
{
    uint32_t i, type;
    if (installed || !(features & SV_ENGINE_OWNER_CLEANUP) ||
        (features & ~(SV_ENGINE_OWNER_CLEANUP | SV_ENGINE_GLOBAL_CONSUMERS)))
        return 0;
    /* Check the entire snapshot before changing its defaults or install state. */
    for (i = 0; i < SV_ENGINE_PARAM_COUNT; ++i)
        if (rows[i].scope == SV_ENGINE_PER_TYPE)
            for (type = 0; type < SV_ENGINE_OBJECT_TYPES; ++type) {
                SvEngineValue value = current(i, type);
                if (!(value.value.real >= rows[i].minimum && value.value.real <= rows[i].maximum))
                    return 0;
            }
    for (i = 0; i < SV_ENGINE_PARAM_COUNT; ++i) {
        uint32_t count = rows[i].scope == SV_ENGINE_PER_TYPE ? SV_ENGINE_OBJECT_TYPES : 1;
        for (type = 0; type < count; ++type)
            defaults[i][type] = current(i, type);
        globals[i] = defaults[i][0];
    }
    installed = features;
    return 1;
}

void sv_engine_release_owner(SvOwner owner)
{
    uint32_t i, type;
    if (!owner || !installed)
        return;
    for (i = 0; i < SV_ENGINE_PARAM_COUNT; ++i) {
        uint32_t count = rows[i].scope == SV_ENGINE_PER_TYPE ? SV_ENGINE_OBJECT_TYPES : 1;
        for (type = 0; type < count; ++type)
            if (writers[i][type] == owner) {
                apply(i, type, &defaults[i][type]);
                writers[i][type] = 0;
            }
    }
}

int32_t sv_engine_integer(uint32_t index, int32_t fallback)
{
    if (!(installed & SV_ENGINE_GLOBAL_CONSUMERS) || index >= SV_ENGINE_PARAM_COUNT ||
        rows[index].scope != SV_ENGINE_GLOBAL_SCOPE || rows[index].type == SV_ENGINE_FLOAT)
        return fallback;
    return globals[index].value.integer;
}

static SvResult SV_CALL describe(SvOwner owner, uint32_t index, uint32_t object_type, SvEngineInfo *out)
{
    SvEngineInfo info;
    if (context(owner) != SV_OK)
        return SV_STATE;
    if (index >= SV_ENGINE_PARAM_COUNT || !target_valid(index, object_type) || !out || out->size < sizeof *out)
        return SV_INVALID;
    memset(&info, 0, sizeof info);
    info.size = sizeof info;
    info.index = index;
    info.scope = rows[index].scope;
    strcpy(info.name, rows[index].name);
    info.minimum = info.maximum = (SvEngineValue){sizeof(SvEngineValue), rows[index].type, {0}};
    if (rows[index].type == SV_ENGINE_FLOAT) {
        info.minimum.value.real = rows[index].minimum;
        info.maximum.value.real = rows[index].maximum;
    } else {
        info.minimum.value.integer = (int32_t)rows[index].minimum;
        info.maximum.value.integer = (int32_t)rows[index].maximum;
    }
    info.default_value = installed ? defaults[index][rows[index].scope == SV_ENGINE_PER_TYPE ? object_type : 0] :
        current(index, object_type);
    *out = info;
    return SV_OK;
}

static SvResult SV_CALL get(SvOwner owner, const char *name, uint32_t object_type, SvEngineValue *out)
{
    int index;
    if (context(owner) != SV_OK)
        return SV_STATE;
    if (!out || out->size < sizeof *out)
        return SV_INVALID;
    index = lookup(name);
    if (index < 0)
        return index == -2 ? SV_INVALID : SV_NOT_FOUND;
    if (!target_valid((uint32_t)index, object_type))
        return SV_INVALID;
    *out = current((uint32_t)index, object_type);
    return SV_OK;
}

static SvResult SV_CALL set(SvOwner owner, const char *name, uint32_t object_type, const SvEngineValue *input)
{
    int index;
    uint32_t slot;
    SvEngineValue value;
    if (context(owner) != SV_OK)
        return SV_STATE;
    if (!input || input->size < sizeof *input)
        return SV_INVALID;
    index = lookup(name);
    if (index < 0)
        return index == -2 ? SV_INVALID : SV_NOT_FOUND;
    if (!target_valid((uint32_t)index, object_type) || input->type != rows[index].type)
        return SV_INVALID;
    value = *input;
    value.size = sizeof value;
    if (value.type == SV_ENGINE_FLOAT) {
        if (!(value.value.real >= rows[index].minimum && value.value.real <= rows[index].maximum))
            return SV_INVALID;
    } else if (value.value.integer < (int32_t)rows[index].minimum || value.value.integer > (int32_t)rows[index].maximum) {
        return SV_INVALID;
    }
    if (!installed || (rows[index].scope == SV_ENGINE_GLOBAL_SCOPE && !(installed & SV_ENGINE_GLOBAL_CONSUMERS)))
        return SV_STATE;
    slot = rows[index].scope == SV_ENGINE_PER_TYPE ? object_type : 0;
    apply((uint32_t)index, object_type, &value);
    writers[index][slot] = owner;
    return SV_OK;
}

const SvGameEngine sv_game_engine = {
    sizeof(SvGameEngine), SV_GAME_ENGINE_VERSION, describe, get, set
};
