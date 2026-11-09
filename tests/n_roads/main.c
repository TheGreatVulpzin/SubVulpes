#include "systems_host.h"
#include "sv_game_ai.h"
#include "city.h"
#include "logic.h"
#include "items.h"

static int make_bot(const char *name)
{
    float pos[3] = {200, 20, 200};
    float rot[9] = {1, 0, 0, 0, 1, 0, 0, 0, 1};
    int p = createplayer(), h;
    CHECK(p >= 0);
    strcpy(sr_players[p].name, name);
    sr_players[p].is_bot = 1;
    sr_players[p].move_mode = 1;
    h = createhuman(pos, rot, p);
    CHECK(h >= 0);
    sr_players[p].human_id = h;
    return p;
}

static void command(const char *text)
{
    sr_players[world_player].godmode = 0;
    CHECK(sv_plugins_command(0, text));
    CHECK(sr_players[world_player].godmode == 1);
}

/* The yaw that the foot AI aims at for the target: from bone 6 of the bot to the head of the target. */
static float target_yaw(int bot)
{
    const Human *self = &sr_humans[sr_players[bot].human_id];
    const Human *other = &sr_humans[world_human];
    double dx = (double)other->bones[2].pos.x - self->bones[6].pos.x;
    double dz = (double)other->bones[2].pos.z - self->bones[6].pos.z;
    return (float)x87_atan2(dx, -dz);
}

/* One AI update from yaw zero. Returns the new yaw. */
static float update_from_zero(int bot)
{
    sr_players[bot].yaw = 0;
    sr_players[bot].input_pitch = 0;
    playerai(bot);
    return sr_players[bot].yaw;
}

/* Holds the trigger of the gun for "ticks" weapon updates. */
static void hold(int gun, int ticks)
{
    while (ticks-- > 0) {
        sr_items[gun].flags |= 1;
        itemweaponsimulation(gun);
    }
}

int main(int argc, char **argv)
{
    static const vec3 position = {10.0f, 21.0f, 30.0f};
    static const vec3 rotation[3] = {{1, 0, 0}, {0, 1, 0}, {0, 0, 1}};
    int bot, gun;
    float aim, yaw;
    world_start();
    bot = make_bot("Aim");
    city_addintersection(0, 0, 0);
    city_addintersection(40, 0, 0);
    addcitystreet(0, 1, 1, 1, "Straight");
    g_city.intersections[1].red[0] = 0;
    g_city.intersections[1].red[1] = 1;
    g_city.intersections[1].red[2] = 2;
    g_city.intersections[1].red[3] = 0;
    sr_itemtypes[10].inv_inertia = (vec3){1, 1, 1};
    gun = (int)createitem(10, &position, NULL, rotation);
    CHECK(gun >= 0);
    sr_items[gun].parent_human = world_human;
    sr_game.state = 2;
    sv_systems_install();
    sv_game_ready();
    if (argc > 1 && !strcmp(argv[1], "--missing-capability")) {
        command("nroads missing");
        sv_plugins_stop();
        puts("N_ROADS_CAP_PASS");
        return 0;
    }
    command("nroads check");

    /* A held 9 mm fires once. The count goes on while the trigger stays down. */
    sr_itemtypes[10].fire_delay = 0;
    sr_items[gun].ammo = 5;
    sr_items[gun].trigger_ticks = 0;
    hold(gun, 8);
    CHECK(sr_items[gun].ammo == 4 && sr_items[gun].trigger_ticks == 8);
    /* A write of 0 lets the held trigger fire again two updates later. */
    command("nroads trigger-zero");
    CHECK(sr_items[gun].trigger_ticks == 0);
    hold(gun, 1);
    CHECK(sr_items[gun].ammo == 4);
    hold(gun, 1);
    CHECK(sr_items[gun].ammo == 3);
    /* A released trigger goes back to 0 on the next update, so the write leaves no state. */
    sr_items[gun].flags &= ~1;
    itemweaponsimulation(gun);
    CHECK(sr_items[gun].trigger_ticks == 0);

    /* Aim snap: the bot turns to the target in one update. */
    command("nroads snap-on");
    CHECK(sv_ai_snap(bot) == 1 && sv_ai_snap(world_player) == 0);
    CHECK(sv_ai_snap(-1) == 0 && sv_ai_snap(SR_MAX_PLAYER) == 0);
    aim = target_yaw(bot);
    CHECK(fabs(aim) > 0.5);
    yaw = update_from_zero(bot);
    CHECK(fabs((double)yaw - aim) < 1e-4);
    command("nroads snap-off");
    CHECK(sv_ai_snap(bot) == 0);
    yaw = update_from_zero(bot);
    CHECK(fabs((double)yaw - aim / 16.0) < 1e-3);
    command("nroads snap-on");

    g_city.num_roads = 513;
    command("nroads bad-graph");
    g_city.num_roads = 1;
    g_city.num_intersections = -1;
    command("nroads bad-graph");
    g_city.num_intersections = 2;

    if (argc > 1 && !strcmp(argv[1], "--reuse")) {
        sv_entity_deleted(SV_ENTITY_PLAYER, (uint32_t)bot);
        sv_entity_created(SV_ENTITY_PLAYER, (uint32_t)bot);
        CHECK(sv_ai_snap(bot) == 0);
        sv_plugins_stop();
        puts("N_ROADS_REUSE_PASS");
        return 0;
    }
    if (argc > 1 && !strcmp(argv[1], "--reset")) {
        sv_game_reset();
        CHECK(sv_ai_snap(bot) == 0);
        sv_plugins_stop();
        puts("N_ROADS_RESET_PASS");
        return 0;
    }
    sv_plugins_stop();
    CHECK(sv_ai_snap(bot) == 0);
    puts("N_ROADS_PASS");
    return 0;
}
