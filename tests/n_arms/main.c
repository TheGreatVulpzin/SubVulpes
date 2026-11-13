#include "systems_host.h"
#include "plugin_props.h"
#include "humans.h"

#define g_bondopt sr_data_bondopt

static void command(const char *text)
{
    sr_players[world_player].godmode = 0;
    CHECK(sv_plugins_command(0, text));
    CHECK(sr_players[world_player].godmode == 1);
}

/* The muscle direction that the arm code gives the shoulder of side 0 from the same body state. */
static vec3 shoulder_turn(void)
{
    static Human saved;
    Human *hu = &sr_humans[world_human];
    vec3 dir;
    saved = *hu;
    human_calcarmtorque(world_human);
    dir = g_bondopt[hu->bones[3].bond].dir;
    *hu = saved;
    return dir;
}

static int same(vec3 a, vec3 b)
{
    return a.x == b.x && a.y == b.y && a.z == b.z;
}

int main(int argc, char **argv)
{
    float t[3] = {9, 9, 9};
    vec3 normal_turn, guard_turn;
    world_start();
    sv_systems_install();
    sv_game_ready();
    CHECK(sr_humans[world_human].bones[3].bond >= 0);
    CHECK(!sv_props_arm_target(world_human, 0, t) && t[0] == 9);
    CHECK(!sv_props_arm_target(-1, 0, t) && !sv_props_arm_target(SR_MAX_HUMAN, 0, t) &&
          !sv_props_arm_target(world_human, 2, t));
    normal_turn = shoulder_turn();
    CHECK(same(normal_turn, shoulder_turn()));
    command("narms check");

    /* Side 0 on: the arm code gets the target of the plugin, side 1 keeps the target of the game. */
    command("narms on");
    CHECK(sv_props_arm_target(world_human, 0, t) && t[0] == 0.15f && t[1] == 0.1f && t[2] == -0.3f);
    CHECK(!sv_props_arm_target(world_human, 1, t));
    guard_turn = shoulder_turn();
    CHECK(!same(guard_turn, normal_turn));
    command("narms both");
    CHECK(sv_props_arm_target(world_human, 1, t) && t[0] == -0.2f && t[2] == -1.0f);
    command("narms off");
    CHECK(!sv_props_arm_target(world_human, 0, t));
    CHECK(same(shoulder_turn(), normal_turn));
    command("narms on");

    if (argc > 1 && !strcmp(argv[1], "--reuse")) {
        /* A new human in the slot starts with no pose. */
        sv_entity_deleted(SV_ENTITY_HUMAN, (uint32_t)world_human);
        sv_entity_created(SV_ENTITY_HUMAN, (uint32_t)world_human);
        CHECK(!sv_props_arm_target(world_human, 0, t));
        sv_plugins_stop();
        puts("N_ARMS_REUSE_PASS");
        return 0;
    }
    if (argc > 1 && !strcmp(argv[1], "--reset")) {
        sv_game_reset();
        CHECK(!sv_props_arm_target(world_human, 0, t));
        sv_plugins_stop();
        puts("N_ARMS_RESET_PASS");
        return 0;
    }
    /* A disabled plugin gives the arms back to the game while the world goes on. */
    CHECK(sv_plugins_disable("test.narms") == SV_OK);
    CHECK(!sv_props_arm_target(world_human, 0, t));
    CHECK(same(shoulder_turn(), normal_turn));
    sv_plugins_stop();
    puts("N_ARMS_PASS");
    return 0;
}
