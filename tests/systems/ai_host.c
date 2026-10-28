#include "systems_host.h"
#include "city.h"
#include "ai_car.h"
#include "sr_os.h"

static int bot, policy, driver, shooter, car, bad_body;
#define COMMAND(text) do { sr_players[policy].godmode = 0; CHECK(sv_plugins_command(0, text)); CHECK(sr_players[policy].godmode == 1); } while (0)

static int make_bot(const char *name, int body)
{
    float pos[3] = {100, 20, 100};
    float rot[9] = {1, 0, 0, 0, 1, 0, 0, 0, 1};
    int p = createplayer();
    CHECK(p >= 0);
    strcpy(sr_players[p].name, name);
    sr_players[p].is_bot = 1;
    sr_players[p].team = 6;
    if (body) {
        int h = createhuman(pos, rot, p);
        CHECK(h >= 0);
        sr_players[p].human_id = h;
        sr_humans[h].pos = (vec3){100, 20, 100};
    }
    return p;
}

void ai_prepare(void)
{
    int malformed, vehicle;
    SvAiThreats out = {sizeof out};
    SvAiInput input = {sizeof input};
    SvEntity ref = sv_entity_at(SV_ENTITY_PLAYER, world_player);
    CHECK(sv_game_ai.threats(0, ref, &out) == SV_STATE);
    CHECK(sv_game_ai.destination(0, ref, (SvVector){0}) == SV_STATE);
    CHECK(sv_game_ai.waypoints(0, ref, NULL, 0) == SV_STATE);
    CHECK(sv_game_ai.target(0, ref, 0) == SV_STATE);
    CHECK(sv_game_ai.override_input(0, ref, &input, 1) == SV_STATE);
    CHECK(sv_game_ai.release(0, ref) == SV_STATE);
    bot = make_bot("AI bot", 1);
    sr_players[bot].num_threats = 1;
    sr_players[bot].threats[0].human = world_human;
    sr_players[bot].threats[0].level = 9;
    malformed = make_bot("AI malformed", 0);
    sr_players[malformed].num_threats = 17;
    vehicle = make_bot("AI vehicle", 1);
    sr_humans[sr_players[vehicle].human_id].vehicle_id = world_object;
    (void)make_bot("AI empty", 0);
    bad_body = make_bot("AI bad body", 1);
    sr_players[bad_body].human_id += 0x40000000;
    policy = make_bot("AI policy", 1);
    driver = make_bot("AI driver", 1);
    shooter = make_bot("AI shooter", 1);
    {
        const vec3 pos = {8, 4, 2};
        const vec3 rot[3] = {{1, 0, 0}, {0, 1, 0}, {0, 0, 1}};
        int gun, mag;
        sr_data_objecttype[0].num_seats = 5;
        car = createobject(0, &pos, NULL, rot, 0);
        CHECK(car >= 0);
        sr_objects[car].driver = driver;
        sr_objects[car].health = 100;
        sr_humans[sr_players[driver].human_id].vehicle_id = car;
        sr_humans[sr_players[driver].human_id].vehicle_seat = 0;
        sr_humans[sr_players[shooter].human_id].vehicle_id = car;
        sr_humans[sr_players[shooter].human_id].vehicle_seat = 1;
        setupitemtypes();
        gun = (int)createitem(0, &pos, NULL, rot);
        mag = (int)createitem(1, &pos, NULL, rot);
        CHECK(gun >= 0 && mag >= 0);
        CHECK(linkitem((uint32_t)gun, UINT32_MAX, sr_players[shooter].human_id, 0));
        CHECK(linkitemtoitem(gun, mag));
        sr_items[gun].ammo = 30;
        sr_items[mag].ammo = 30;
    }
    city_addintersection(0, 0, 0);
    city_addintersection(10, 0, 0);
    city_addintersection(10, 0, 10);
    city_addintersection(0, 0, 10);
    addcitystreet(0, 1, 1, 1, "Test south");
    addcitystreet(1, 2, 1, 1, "Test east");
    addcitystreet(3, 2, 1, 1, "Test north");
    addcitystreet(0, 3, 1, 1, "Test west");
    /* The fixture installs the proposed core seams. Production does not yet install them. */
    sv_ai_install();
}

void ai_verify(void)
{
    Player *p = &sr_players[bot];
    int h = sr_players[shooter].human_id;
    int j;
    int gun = sr_humans[h].inventory[0].items[0];
    int mag = sr_items[gun].children[0];
    int expected;
    sr_players[bad_body].human_id -= 0x40000000;
    CHECK(sv_ai_perception_part(policy) == 1);
    CHECK(sv_ai_perception_part(-1) == -1);
    os_srand(77);
    expected = os_rand();
    os_srand(77);
    for (j = 0; j < 128; ++j) CHECK(sv_ai_shot(policy, 0));
    CHECK(os_rand() == expected);
    sr_players[policy].input_buttons = 8;
    sv_ai_after_player(policy);
    CHECK(sr_players[policy].input_buttons == 10);
    COMMAND("ai.control policy-off");
    CHECK(sv_ai_perception_part(policy) == 2);
    for (j = 0; j < 128; ++j) CHECK(!sv_ai_shot(policy, 0));
    sv_ai_after_player(policy);
    CHECK(sr_players[policy].input_buttons == 8);
    COMMAND("ai.control policy-half");
    for (j = 0; j < 128; ++j) {
        int shot = sv_ai_shot(policy, 0);
        CHECK(shot == 0 || shot == 1);
    }
    CHECK(sr_players[driver].waypoints[0].road == 0 && sr_players[driver].waypoints[1].road == 1);
    CHECK(!sv_ai_before_player(driver));
    /* ai_playercar copies the next road into the current road at each crossing. */
    sr_players[driver].waypoints[0] = sr_players[driver].waypoints[1];
    CHECK(sv_ai_route_next(driver));
    CHECK(sr_players[driver].waypoints[1].road == 2 && sr_players[driver].waypoints[1].dir == 1);
    sr_players[driver].waypoints[0] = sr_players[driver].waypoints[1];
    CHECK(sv_ai_route_next(driver));
    CHECK(sr_players[driver].waypoints[1].road == 3);
    sr_players[driver].waypoints[0] = sr_players[driver].waypoints[1];
    CHECK(sv_ai_route_next(driver));
    CHECK(sr_players[driver].waypoints[1].road == 0);
    sr_players[driver].waypoints[0].road = 512;
    CHECK(sv_ai_before_player(driver));
    CHECK(sr_players[driver].input_buttons == 4);
    COMMAND("ai.control route");
    g_city.num_roads = 513;
    COMMAND("ai.control bad-route");
    g_city.num_roads = 4;
    g_city.num_intersections = 1025;
    COMMAND("ai.control bad-route");
    g_city.num_intersections = 4;
    g_city.roads[4] = g_city.roads[0];
    g_city.intersections[0].roads[3] = 4;
    g_city.intersections[1].roads[0] = 4;
    COMMAND("ai.control bad-road-index");
    g_city.intersections[0].roads[3] = -1;
    g_city.intersections[1].roads[0] = -1;
    g_city.roads[0].ends[0] = 0x40000000;
    g_city.roads[3].ends[0] = 0x40000000;
    COMMAND("ai.control bad-route");
    g_city.roads[0].ends[0] = 0;
    g_city.roads[3].ends[0] = 0;
    g_city.roads[0].ends[0] = 1024;
    COMMAND("ai.control bad-route");
    CHECK(sv_ai_before_player(driver));
    g_city.roads[0].ends[0] = 0;
    COMMAND("ai.control route");
    /* Run the existing driver with the validated current and next roads away from a crossing. */
    ai_playercar(driver, car);
    CHECK(sr_players[driver].input_axes[3] >= -1 && sr_players[driver].input_axes[3] <= 1);
    sv_ai_after_threats(shooter);
    CHECK(sr_players[shooter].num_threats == 1);
    CHECK(sr_players[shooter].threats[0].human == world_human);
    CHECK(sr_players[shooter].threats[0].pos.y == sr_humans[world_human].bones[1].pos.y);
    CHECK(sr_humans[h].vehicle_id == car && sr_humans[h].vehicle_seat == 1);
    sr_humans[h].vehicle_seat = 4;
    COMMAND("ai.control bad-seat");
    CHECK(sv_ai_before_player(shooter));
    sr_humans[h].vehicle_seat = 1;
    sr_humans[h].vehicle_id = car + 0x10000000;
    COMMAND("ai.control bad-seat");
    sr_humans[h].vehicle_id = car;
    sr_humans[h].inventory[0].items[0] = gun + 0x40000000;
    sr_items[mag].parent_item = gun + 0x40000000;
    COMMAND("ai.control bad-seat");
    sr_humans[h].inventory[0].items[0] = gun;
    sr_items[mag].parent_item = gun;
    sr_items[gun].type += 0x40000000;
    COMMAND("ai.control bad-seat");
    sr_items[gun].type -= 0x40000000;
    sr_items[gun].children[0] = mag + 0x40000000;
    COMMAND("ai.control bad-seat");
    sr_items[gun].children[0] = mag;
    CHECK(p->num_path == 64 && p->ai_waypoint == 0);
    CHECK(p->waypoints[63].pos.x == 63);
    sv_ai_after_threats(bot);
    CHECK(p->num_threats == 1 && p->ai_target == 0);
    CHECK(p->threats[0].human == world_human && p->threats[0].level == 16);
    CHECK(p->num_path == 1 && p->waypoints[0].pos.x == sr_humans[world_human].pos.x);
    CHECK(sv_ai_before_player(bot));
    CHECK(p->input_axes[3] == 1 && p->input_buttons == 8 && p->input_mode == 1);
    playersimulation();
    CHECK(sr_humans[p->human_id].input_axes[3] == 1);
    CHECK(sr_humans[p->human_id].input_buttons == 8);
    CHECK(sv_ai_before_player(bot));
    CHECK(!sv_ai_before_player(bot));
    p->ai_wait = 1;
    playerai(bot);
    CHECK(p->input_axes[3] == 0 && p->input_buttons == 0 && p->ai_wait == 0);
    COMMAND("ai.control release");
    p->num_threats = 0;
    sv_ai_after_threats(bot);
    CHECK(p->num_threats == 0 && !sv_ai_before_player(bot));
    COMMAND("ai.control renew");
    sv_ai_release_owner(1);
    p->num_threats = 0;
    sv_ai_after_threats(bot);
    CHECK(p->num_threats == 0);
    CHECK(!sv_ai_before_player(bot));
    COMMAND("ai.control renew");
    sv_game_reset();
    CHECK(!sv_ai_before_player(bot));
    sv_ai_after_threats(bot);
    CHECK(p->num_threats == 0);
    COMMAND("ai.control renew");
    sv_entity_deleted(SV_ENTITY_PLAYER, (uint32_t)bot);
    sv_entity_created(SV_ENTITY_PLAYER, (uint32_t)bot);
    CHECK(!sv_ai_before_player(bot));
    sv_ai_after_threats(bot);
    CHECK(p->num_threats == 0);
    CHECK(!sv_plugins_failed());
    CHECK(!sv_ai_before_player(-1) && !sv_ai_before_player(SR_MAX_PLAYER));
    puts("AI_PASS");
}
