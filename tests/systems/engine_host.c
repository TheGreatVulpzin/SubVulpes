#include "systems_host.h"
#include "sv_game_engine.h"
#include <math.h>

static int old_truck, other_object;
static float untouched_power;

static int make_object(int type)
{
    const vec3 position = {1000, 20, 1000};
    const vec3 rotation[3] = {{1, 0, 0}, {0, 1, 0}, {0, 0, 1}};
    int object = createobject(type, &position, NULL, rotation, 0);
    CHECK(object >= 0);
    return object;
}

static void command(const char *arguments)
{
    char text[80];
    snprintf(text, sizeof text, "engine.check %s", arguments);
    sr_players[world_player].godmode = 0;
    CHECK(sv_plugins_command(0, text));
    CHECK(sr_players[world_player].godmode == 1);
    CHECK(!sv_plugins_failed());
}

static void engine_force(int new_truck)
{
    int active[SR_MAX_OBJECT], i;
    Object *boosted = &sr_objects[old_truck], *baseline = &sr_objects[new_truck];
    /* Isolate one native engine pass from the other system fixtures. */
    for (i = 0; i < SR_MAX_OBJECT; ++i) {
        active[i] = sr_objects[i].active;
        sr_objects[i].active = i == old_truck || i == new_truck;
    }
    CHECK(boosted->gear_ratio == 0 && baseline->gear_ratio == 0);
    CHECK(boosted->num_wheels == 0 && baseline->num_wheels == 0);
    boosted->throttle = baseline->throttle = 1;
    baseline->engine_power = 213;
    objectenginesimulation2();
    CHECK(baseline->engine_rpm > 0);
    CHECK(fabsf(boosted->engine_rpm - 4 * baseline->engine_rpm) < 0.000001f);
    baseline->engine_power = 852;
    boosted->engine_rpm = baseline->engine_rpm = 0;
    boosted->throttle = baseline->throttle = 0;
    for (i = 0; i < SR_MAX_OBJECT; ++i)
        sr_objects[i].active = active[i];
}

void engine_prepare(void)
{
    SvEngineInfo info = {sizeof info};
    SvEngineValue input = {sizeof input, SV_ENGINE_FLOAT, {0}};
    uint32_t nan_bits = 0x7fc00000u;
    float saved;
    CHECK(sv_game_engine.describe(1, SV_ENGINE_POWER, 9, &info) == SV_STATE);
    CHECK(sv_game_engine.get(1, "object.engine_power", 9, &input) == SV_STATE);
    CHECK(sv_game_engine.set(1, "object.engine_power", 9, &input) == SV_STATE);
    CHECK(sv_engine_integer(SV_ENGINE_RAGDOLL_RESET, 77) == 77);
    CHECK(sv_engine_integer(SV_ENGINE_DEATH_START, 77) == 77);
    CHECK(sv_engine_integer(0x04000000u, 77) == 77);
    CHECK(!sv_engine_install(0));
    CHECK(!sv_engine_install(SV_ENGINE_GLOBAL_CONSUMERS));
    CHECK(!sv_engine_install(SV_ENGINE_OWNER_CLEANUP | 4u));
    untouched_power = sr_data_objecttype[0].engine_power;
    memset(&sr_data_objecttype[9], 0, sizeof sr_data_objecttype[9]);
    memset(&sr_data_objecttype[2], 0, sizeof sr_data_objecttype[2]);
    sr_data_objecttype[9].state = sr_data_objecttype[2].state = 1;
    sr_data_objecttype[9].engine_power = 1;
    sr_data_objecttype[2].engine_power = 2;
    old_truck = make_object(9);
    other_object = make_object(2);
    CHECK(sr_objects[old_truck].engine_power == 213);
    CHECK(sr_objects[other_object].engine_power == 426);
    saved = sr_data_objecttype[13].engine_power;
    memcpy(&sr_data_objecttype[13].engine_power, &nan_bits, sizeof nan_bits);
    CHECK(!sv_engine_install(SV_ENGINE_OWNER_CLEANUP | SV_ENGINE_GLOBAL_CONSUMERS));
    CHECK(sv_engine_integer(SV_ENGINE_RAGDOLL_RESET, 77) == 77);
    sr_data_objecttype[13].engine_power = 17;
    CHECK(!sv_engine_install(SV_ENGINE_OWNER_CLEANUP | SV_ENGINE_GLOBAL_CONSUMERS));
    CHECK(sv_engine_integer(SV_ENGINE_DEATH_START, 77) == 77);
    sr_data_objecttype[13].engine_power = saved;
    CHECK(sv_engine_install(SV_ENGINE_OWNER_CLEANUP | SV_ENGINE_GLOBAL_CONSUMERS));
    CHECK(!sv_engine_install(SV_ENGINE_OWNER_CLEANUP | SV_ENGINE_GLOBAL_CONSUMERS));
    CHECK(sv_engine_integer(SV_ENGINE_RAGDOLL_RESET, 77) == 1);
    CHECK(sv_engine_integer(SV_ENGINE_DEATH_START, 77) == 0);
    CHECK(sv_engine_integer(SV_ENGINE_PARAM_COUNT, 77) == 77);
    CHECK(sv_engine_integer(0x04000000u, 77) == 77);
    CHECK(sv_engine_integer(SV_ENGINE_POWER, 77) == 77);
    sr_objects[other_object].engine_power = 987;
    sv_engine_release_owner(0);
    CHECK(sr_objects[other_object].engine_power == 987);
    sr_objects[other_object].engine_power = 426;
}

void engine_verify(void)
{
    int new_truck;
    CHECK(sr_data_objecttype[9].engine_power == 4);
    CHECK(sr_objects[old_truck].engine_power == 852);
    CHECK(sr_data_objecttype[2].engine_power == 2 && sr_objects[other_object].engine_power == 426);
    CHECK(sr_data_objecttype[0].engine_power == untouched_power);
    CHECK(sv_engine_integer(SV_ENGINE_RAGDOLL_RESET, 77) == 0);
    CHECK(sv_engine_integer(SV_ENGINE_DEATH_START, 77) == 9);
    new_truck = make_object(9);
    CHECK(sr_objects[new_truck].engine_power == 852);
    engine_force(new_truck);
    sv_engine_release_owner(2);
    CHECK(sr_data_objecttype[9].engine_power == 4);
    CHECK(sr_objects[old_truck].engine_power == 852 && sr_objects[new_truck].engine_power == 852);
    command("changed");
    sv_engine_release_owner(1);
    CHECK(sr_data_objecttype[9].engine_power == 1);
    CHECK(sr_objects[old_truck].engine_power == 213 && sr_objects[new_truck].engine_power == 213);
    CHECK(sr_data_objecttype[2].engine_power == 2 && sr_objects[other_object].engine_power == 426);
    CHECK(sv_engine_integer(SV_ENGINE_RAGDOLL_RESET, 77) == 1);
    CHECK(sv_engine_integer(SV_ENGINE_DEATH_START, 77) == 0);
    command("default");
    puts("ENGINE_PASS");
}
