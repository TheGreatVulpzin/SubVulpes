#include "systems_host.h"
#include "sv_game_ai.h"
#include "city.h"
#include "ai_car.h"
#include "sv_game_write.h"
#include <math.h>

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

/* The view of a bot must be the view of the threat aim in player_ai.c for a point. */
static void expect_aim(int p, double x, double y, double z)
{
    vec3 head = sr_humans[sr_players[p].human_id].bones[6].pos;
    double dx = x - head.x, dy = y - head.y - 0.5, dz = z - head.z;
    double yaw = atan2(dx, -dz), pitch = atan2(-dy, sqrt(dx * dx + dz * dz));
    CHECK(fabs(sr_players[p].yaw - yaw) < 0.001 && fabs(sr_players[p].input_pitch - pitch) < 0.001);
}

static void command(const char *text)
{
    sr_players[world_player].godmode = 0;
    CHECK(sv_plugins_command(0, text));
    CHECK(sr_players[world_player].godmode == 1);
}

int main(int argc, char **argv)
{
    int bot, driver, scenario, dead, dead_human, released, h;
    int receiver, donor, old_body, donor_body, rider, rider_body;
    float normal_throttle;
    world_start();
    bot = make_bot("Aim");
    driver = make_bot("Driver");
    scenario = make_bot("Scenario");
    dead = make_bot("Dead");
    dead_human = sr_players[dead].human_id;
    released = make_bot("Release");
    /* A real player with an account and a bot to take the body of. Both bodies are free of accounts. */
    receiver = make_bot("Receiver");
    donor = make_bot("Donor");
    sr_players[receiver].is_bot = 0;
    sr_data_accounts.count = 1;
    sr_players[receiver].account = 0;
    sr_players[receiver].team = sr_players[donor].team = 0;
    old_body = sr_players[receiver].human_id;
    donor_body = sr_players[donor].human_id;
    sr_humans[old_body].account = sr_humans[donor_body].account = -1;
    sr_humans[old_body].team = sr_humans[donor_body].team = 0;
    rider = make_bot("Rider");
    rider_body = sr_players[rider].human_id;
    sr_humans[dead_human].health = -5;
    sr_humans[dead_human].despawn_timer = 3333;
    sr_humans[sr_players[released].human_id].health = -5;
    city_addintersection(0, 0, 0);
    city_addintersection(40, 0, 0);
    addcitystreet(0, 1, 1, 1, "Straight");
    sr_objects[world_object].health = 100;
    sr_objects[world_object].num_seats = 2;
    sr_objects[world_object].driver = driver;
    h = sr_players[driver].human_id;
    sr_humans[h].vehicle_id = world_object;
    sr_humans[h].vehicle_seat = 0;
    sr_humans[rider_body].vehicle_id = world_object;
    sr_humans[rider_body].vehicle_seat = 1;
    sr_players[scenario].ai_chase = 1;
    sv_systems_install();
    sv_game_ready();
    if (argc > 1 && !strcmp(argv[1], "--missing-capability")) {
        command("nbots missing");
        sv_plugins_stop();
        puts("N_BOTS_CAP_PASS");
        return 0;
    }
    command("nbots on");
    sv_ai_after_player(bot);
    playersimulation();
    CHECK(sr_players[bot].move_mode == 2);
    CHECK(sr_humans[sr_players[bot].human_id].move_mode == 2);
    CHECK(sr_players[driver].ai_chase == 1);
    CHECK(sr_humans[dead_human].despawn_timer == 100);
    h = sr_players[released].human_id;
    humansimulation();
    CHECK(sr_players[released].human_id == -1 && sr_humans[h].player_id == -1);
    command("nbots possess");
    CHECK(sr_players[receiver].human_id == donor_body && sr_players[donor].human_id == -1);
    CHECK(sr_humans[donor_body].player_id == receiver && sr_humans[old_body].player_id == -1);
    command("nbots unpossess");
    CHECK(sr_players[receiver].human_id == old_body && sr_players[donor].human_id == donor_body);
    CHECK(sr_humans[old_body].player_id == receiver && sr_humans[donor_body].player_id == donor);
    h = sr_players[driver].human_id;
    command("nbots off");
    sv_ai_after_player(bot);
    playersimulation();
    CHECK(sr_players[bot].move_mode == 1);
    CHECK(sr_humans[sr_players[bot].human_id].move_mode == 1);
    CHECK(sr_players[driver].ai_chase == 0);
    /* A partial override lets the default AI run, then puts the chosen parts over its result for the given updates. */
    command("nbots parts-input");
    CHECK(sv_ai_before_player(bot) == 0);
    sr_players[bot].input_buttons = 1u | 8u;
    sr_players[bot].input_axes[1] = 0.0f;
    sr_players[bot].yaw = 0.25f;
    sv_ai_after_player(bot);
    CHECK((sr_players[bot].input_buttons & 4u) && (sr_players[bot].input_buttons & 9u) == 9u);
    CHECK(sr_players[bot].input_axes[1] == 0.5f && sr_players[bot].yaw == 0.25f);
    sr_players[bot].input_buttons = 0;
    sv_ai_after_player(bot);
    CHECK(sr_players[bot].input_buttons == 4u);
    sr_players[bot].input_buttons = 0;
    sr_players[bot].input_axes[1] = 0.0f;
    sv_ai_after_player(bot);
    CHECK(sr_players[bot].input_buttons == 0 && sr_players[bot].input_axes[1] == 0.0f);
    /* aim_at: the exact view for a point, for an entity and for a passenger. It ends after the given updates. */
    command("nbots aim-at");
    sv_ai_after_player(bot);
    expect_aim(bot, 10, 20, 30);
    sr_players[bot].yaw = 2.0f;
    sv_ai_after_player(bot);
    expect_aim(bot, 10, 20, 30);
    sr_players[bot].yaw = 2.0f;
    sv_ai_after_player(bot);
    CHECK(sr_players[bot].yaw == 2.0f);
    command("nbots aim-entity");
    sv_ai_after_player(bot);
    {
        vec3 torso = sr_humans[dead_human].bones[2].pos;
        expect_aim(bot, torso.x, torso.y, torso.z);
    }
    command("nbots aim-rider");
    sr_players[rider].input_buttons = 0;
    sv_ai_after_player(rider);
    expect_aim(rider, 10, 20, 30);
    CHECK(sr_players[rider].input_buttons & 1u);
    /* The input lock: the held values go over what the client sent, until the lock ends. */
    sr_players[receiver].yaw = 1.0f;
    sr_players[receiver].input_pitch = 0.5f;
    command("nbots lock");
    sr_players[receiver].yaw = 2.0f;
    sr_players[receiver].input_pitch = 0.25f;
    sr_players[receiver].input_axes[0] = sr_players[receiver].input_axes[3] = 1.0f;
    sr_players[receiver].input_buttons = 5;
    sv_input_lock_apply();
    CHECK(sr_players[receiver].input_axes[0] == 0 && sr_players[receiver].input_axes[3] == 0 && sr_players[receiver].input_buttons == 0);
    CHECK(sr_players[receiver].yaw == 1.0f && sr_players[receiver].input_pitch == 0.5f);
    command("nbots unlock");
    sr_players[receiver].yaw = 2.0f;
    sr_players[receiver].input_axes[0] = 1.0f;
    sr_players[receiver].input_buttons = 5;
    sv_input_lock_apply();
    CHECK(sr_players[receiver].input_axes[0] == 1.0f && sr_players[receiver].input_buttons == 5 && sr_players[receiver].yaw == 2.0f);
    /* All parts with all buttons skip the default AI, as override_input does. */
    command("nbots parts-full");
    CHECK(sv_ai_before_player(bot) == 1 && sr_players[bot].input_buttons == 8u);
    sr_objects[world_object].pos = (vec3){80, 5.5f, 2};
    command("nbots street-occupied");
    command("nbots street-blocked");
    command("nbots street-list");
    sr_objects[world_object].pos = (vec3){10, 20, 30};
    g_city.intersections[1].pos.y = 8;
    g_city.intersections[1].y = 1;
    command("nbots street-slope");
    g_city.intersections[1].pos.y = 4;
    g_city.intersections[1].y = 0;
    {
        Player *p = &sr_players[driver];
        Object *o = &sr_objects[world_object];
        p->ai_waypoint = 0;
        p->wait_isec = -1;
        p->waypoints[0].road = p->waypoints[1].road = 0;
        p->waypoints[0].dir = p->waypoints[0].lane = 0;
        p->waypoints[1].turn = 0;
        o->pos = (vec3){80, 4, 2};
        o->rot[0] = (vec3){0, 0, 1};
        o->rot[1] = (vec3){0, 1, 0};
        o->rot[2] = (vec3){-1, 0, 0};
        o->vel = (vec3){0.1f, 0, 0};
        ai_playercar(driver, world_object);
        normal_throttle = p->input_axes[3];
    }
    command("nbots on");
    ai_playercar(driver, world_object);
    CHECK(sr_players[driver].input_axes[3] > normal_throttle);
    /* The parts: lights alone keep the normal speed. Speed with lane change drives faster. All four are the value 1. */
    command("nbots parts-lights");
    CHECK(sr_players[driver].ai_chase == AI_CHASE_LIGHTS);
    command("nbots parts-speed");
    CHECK(sr_players[driver].ai_chase == (AI_CHASE_SPEED | AI_CHASE_LANE));
    command("nbots parts-all");
    CHECK(sr_players[driver].ai_chase == 1);
    command("nbots on");
    sv_ai_after_player(bot);
    sr_humans[h].vehicle_id = 0x40000000;
    command("nbots bad-seat");
    sr_humans[h].vehicle_id = world_object;
    g_city.num_roads = 513;
    command("nbots bad-graph");
    g_city.num_roads = 1;
    g_city.roads[0].ends[0] = 0x40000000;
    command("nbots bad-graph");
    g_city.roads[0].ends[0] = 0;
    /* Give the scenario bot the same valid driver seat for this check. */
    sr_humans[h].vehicle_id = -1;
    h = sr_players[scenario].human_id;
    sr_humans[h].vehicle_id = world_object;
    sr_humans[h].vehicle_seat = 0;
    sr_objects[world_object].driver = scenario;
    command("nbots scenario");
    if (argc > 1 && !strcmp(argv[1], "--reuse")) {
        sv_entity_deleted(SV_ENTITY_PLAYER, (uint32_t)bot);
        sv_entity_created(SV_ENTITY_PLAYER, (uint32_t)bot);
        sv_entity_deleted(SV_ENTITY_PLAYER, (uint32_t)driver);
        sv_entity_created(SV_ENTITY_PLAYER, (uint32_t)driver);
        h = dead_human;
        sv_entity_deleted(SV_ENTITY_HUMAN, (uint32_t)h);
        sv_entity_created(SV_ENTITY_HUMAN, (uint32_t)h);
        sr_humans[h].despawn_timer = 777;
        sv_plugins_stop();
        CHECK(sr_players[bot].move_mode == 2);
        CHECK(sr_players[driver].ai_chase == 1);
        CHECK(sr_humans[h].despawn_timer == 777);
        puts("N_BOTS_REUSE_PASS");
        return 0;
    }
    if (argc > 1 && !strcmp(argv[1], "--reset")) {
        sv_game_reset();
        h = dead_human;
        sr_humans[h].despawn_timer = 777;
        sv_plugins_stop();
        CHECK(sr_players[bot].move_mode == 2);
        CHECK(sr_players[driver].ai_chase == 1);
        CHECK(sr_humans[h].despawn_timer == 777);
        puts("N_BOTS_RESET_PASS");
        return 0;
    }
    command("nbots possess");
    CHECK(sr_players[receiver].human_id == donor_body);
    command("nbots lock");
    sv_plugins_stop();
    sr_players[receiver].input_buttons = 5;
    sv_input_lock_apply();
    CHECK(sr_players[receiver].input_buttons == 5);
    CHECK(sr_players[receiver].human_id == old_body && sr_players[donor].human_id == donor_body);
    CHECK(sr_humans[old_body].player_id == receiver && sr_humans[donor_body].player_id == donor);
    CHECK(sr_players[bot].move_mode == 1);
    CHECK((sr_players[bot].input_buttons & 2u) == 0);
    CHECK(sr_players[driver].ai_chase == 0);
    CHECK(sr_players[scenario].ai_chase == 1);
    CHECK(sr_humans[dead_human].despawn_timer == 3333);
    puts("N_BOTS_PASS");
    return 0;
}
