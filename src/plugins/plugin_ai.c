/* Bot control uses references so a reused slot never inherits a lease. */
#include "plugin_host.h"
#include "plugin_entities.h"
#include "sv_game_ai.h"
#include "plugin_game_internal.h"
#include "logic.h"
#include "sr_os.h"
#include "plugin_lists.h"
#include <string.h>

typedef struct {
    SvOwner owner;
    SvEntity bot, target;
    uint64_t reset;
    uint32_t ticks;
    SvAiInput input;
    int32_t perception, aim, chance;
    uint32_t road_count, road_next;
    SvAiStreetPoint roads[SV_AI_WAYPOINTS_MAX];
    int seated;
} Control;

static Control controls[SR_MAX_PLAYER];
static int installed;

static int finite_value(float value)
{
    return value == value && value - value == 0.0f;
}

static int position_valid(SvVector p)
{
    return finite_value(p.x) && finite_value(p.y) && finite_value(p.z) &&
        p.x >= -32768 && p.x <= 32768 && p.y >= -32768 && p.y <= 32768 &&
        p.z >= -32768 && p.z <= 32768;
}

static SvResult resolve(SvOwner owner, SvEntity bot, int32_t *index)
{
    if (!sv_plugins_context(owner) || !sv_game_is_ready())
        return SV_STATE;
    *index = sv_entity_resolve(SV_ENTITY_PLAYER, bot);
    if (*index < 0)
        return SV_NOT_FOUND;
    if (!sr_players[*index].is_bot)
        return SV_DENIED;
    return SV_OK;
}

static int live_control(int32_t index)
{
    Control *c = &controls[index];
    return c->owner && c->reset == sv_game_reset_sequence() &&
        sv_entity_resolve(SV_ENTITY_PLAYER, c->bot) == index;
}

static SvResult writable(int32_t index)
{
    int human = sr_players[index].human_id;
    if ((uint32_t)human >= SR_MAX_HUMAN || !sr_humans[human].active ||
        sr_humans[human].player_id != index)
        return SV_STATE;
    /* Foot points have no road topology. Never send them to ai_playercar. */
    if (sr_humans[human].vehicle_id != -1)
        return SV_CONFLICT;
    return SV_OK;
}

static Control *claim(SvOwner owner, SvEntity bot, int32_t index)
{
    Control *c = &controls[index];
    if (!live_control(index) || c->owner != owner) {
        memset(c, 0, sizeof *c);
        c->owner = owner;
        c->bot = bot;
        c->reset = sv_game_reset_sequence();
        c->perception = c->aim = c->chance = -1;
    }
    return c;
}

SV_LIST_ITEM_CHECK(threat, SvAiThreat, 32);

static SvResult SV_CALL read_threats(SvOwner owner, SvEntity bot, SvAiThreats *out)
{
    int32_t index;
    uint32_t j, stride;
    SvAiThreats copy = {0};
    Player *p;
    SvResult result = resolve(owner, bot, &index);
    if (result != SV_OK)
        return result;
    stride = out ? sv_list_stride(out->size, (uint32_t)offsetof(SvAiThreats, threats), SV_AI_THREATS_MAX, 32, sizeof(SvAiThreat)) : 0;
    if (!stride)
        return SV_INVALID;
    p = &sr_players[index];
    if (p->num_threats < 0 || p->num_threats > 16)
        return SV_FAILED;
    copy.size = sizeof copy;
    copy.count = (uint32_t)p->num_threats;
    for (j = 0; j < copy.count; ++j) {
        Threat *t = &p->threats[j];
        copy.threats[j].human = sv_entity_at(SV_ENTITY_HUMAN, t->human);
        memcpy(&copy.threats[j].position, &t->pos, sizeof(SvVector));
        copy.threats[j].level = t->level;
        copy.threats[j].timer = t->timer;
    }
    out->size = (uint32_t)offsetof(SvAiThreats, threats) + SV_AI_THREATS_MAX * stride;
    out->count = copy.count;
    out->item_size = stride;
    out->reserved = 0;
    sv_list_put(out->threats, copy.threats, copy.count, SV_AI_THREATS_MAX, sizeof(SvAiThreat), stride);
    return SV_OK;
}

static SvResult SV_CALL set_waypoints(SvOwner owner, SvEntity bot, const SvVector *points, uint32_t count)
{
    int32_t index;
    uint32_t j;
    SvResult result = resolve(owner, bot, &index);
    if (result != SV_OK)
        return result;
    if (count > SV_AI_WAYPOINTS_MAX || (count && !points))
        return SV_INVALID;
    for (j = 0; j < count; ++j)
        if (!position_valid(points[j]))
            return SV_INVALID;
    result = writable(index);
    if (result != SV_OK)
        return result;
    for (j = 0; j < count; ++j) {
        memset(&sr_players[index].waypoints[j], 0, sizeof(AiWaypoint));
        memcpy(&sr_players[index].waypoints[j].pos, &points[j], sizeof(SvVector));
    }
    sr_players[index].num_path = (int)count;
    sr_players[index].ai_waypoint = 0;
    if (live_control(index)) {
        controls[index].road_count = 0;
        controls[index].seated = 0;
    }
    return SV_OK;
}

static SvResult SV_CALL set_destination(SvOwner owner, SvEntity bot, SvVector position)
{
    return set_waypoints(owner, bot, &position, 1);
}

static SvResult SV_CALL set_target(SvOwner owner, SvEntity bot, SvEntity human)
{
    int32_t index, target;
    SvResult result = resolve(owner, bot, &index);
    if (result != SV_OK)
        return result;
    target = sv_entity_resolve(SV_ENTITY_HUMAN, human);
    if (target < 0)
        return SV_NOT_FOUND;
    if (target == sr_players[index].human_id || sr_humans[target].health <= 0)
        return SV_INVALID;
    result = writable(index);
    if (result != SV_OK)
        return result;
    if (!installed)
        return SV_STATE;
    {
        Control *c = claim(owner, bot, index);
        c->target = human;
        c->seated = 0;
        c->road_count = 0;
    }
    return SV_OK;
}

static SvResult SV_CALL override_input(SvOwner owner, SvEntity bot, const SvAiInput *input, uint32_t ticks)
{
    int32_t index;
    int j;
    Control *c;
    SvResult result = resolve(owner, bot, &index);
    if (result != SV_OK)
        return result;
    if (!input || input->size < sizeof *input || !ticks || ticks > SV_AI_TICKS_MAX ||
        input->buttons > 0x7ffffu || !finite_value(input->yaw) ||
        input->yaw < -3.141593f || input->yaw > 3.141593f ||
        !finite_value(input->pitch) || input->pitch < -1.570797f || input->pitch > 1.570797f)
        return SV_INVALID;
    for (j = 0; j < 4; ++j)
        if (!finite_value(input->axes[j]) || input->axes[j] < -1 || input->axes[j] > 1)
            return SV_INVALID;
    result = writable(index);
    if (result != SV_OK)
        return result;
    if (!installed)
        return SV_STATE;
    c = claim(owner, bot, index);
    c->input = *input;
    c->ticks = ticks;
    return SV_OK;
}

static SvResult SV_CALL release(SvOwner owner, SvEntity bot)
{
    int32_t index;
    SvResult result = resolve(owner, bot, &index);
    if (result != SV_OK)
        return result;
    memset(&controls[index], 0, sizeof controls[index]);
    return SV_OK;
}

static SvResult body_state(int32_t index)
{
    int human = sr_players[index].human_id;
    if ((uint32_t)human >= SR_MAX_HUMAN || !sr_humans[human].active ||
        sr_humans[human].player_id != index)
        return SV_STATE;
    return SV_OK;
}

static SvResult set_policy(SvOwner owner, SvEntity bot, int32_t value, int field)
{
    int32_t index;
    Control *c;
    SvResult result = resolve(owner, bot, &index);
    if (result != SV_OK)
        return result;
    if ((field == 0 && value != 1 && value != 2) ||
        (field == 1 && value != 0 && value != 1) || (field == 2 && (value < 0 || value > 100)))
        return SV_INVALID;
    if (!installed || body_state(index) != SV_OK)
        return SV_STATE;
    c = claim(owner, bot, index);
    if (field == 0) c->perception = value;
    if (field == 1) c->aim = value;
    if (field == 2) c->chance = value;
    return SV_OK;
}

static SvResult SV_CALL set_perception(SvOwner owner, SvEntity bot, uint32_t part)
{
    return set_policy(owner, bot, part <= 2 ? (int32_t)part : -1, 0);
}

static SvResult SV_CALL set_aim(SvOwner owner, SvEntity bot, int32_t enabled)
{
    return set_policy(owner, bot, enabled, 1);
}

static SvResult SV_CALL set_chance(SvOwner owner, SvEntity bot, uint32_t percent)
{
    return set_policy(owner, bot, percent <= 100 ? (int32_t)percent : -1, 2);
}

/* Check every index that ai_playercar uses before a controlled driver runs. */
static int road_point_valid(const SvAiStreetPoint *point)
{
    const CityRoad *r;
    int end, j;
    if (g_city.num_roads < 1 || g_city.num_roads > 512 ||
        g_city.num_intersections < 1 || g_city.num_intersections > 1024 ||
        point->road >= (uint32_t)g_city.num_roads ||
        (point->direction != 0 && point->direction != 1) || point->lane < 0 || point->lane > 1)
        return 0;
    r = &g_city.roads[point->road];
    if (r->kind != 0 && r->kind != 1)
        return 0;
    for (end = 0; end < 2; ++end) {
        int found = 0;
        if ((uint32_t)r->ends[end] >= (uint32_t)g_city.num_intersections)
            return 0;
        for (j = 0; j < 4; ++j)
            if (g_city.intersections[r->ends[end]].roads[j] == (int32_t)point->road)
                found = 1;
        if (!found)
            return 0;
    }
    return r->ends[0] != r->ends[1];
}

static int road_pair_valid(const SvAiStreetPoint *a, const SvAiStreetPoint *b)
{
    return road_point_valid(a) && road_point_valid(b) && a->road != b->road &&
        g_city.roads[a->road].ends[a->direction ^ 1] == g_city.roads[b->road].ends[b->direction];
}

static int seat_state(int32_t index, int driver)
{
    Human *h;
    Object *o;
    if (body_state(index) != SV_OK)
        return 0;
    h = &sr_humans[sr_players[index].human_id];
    if ((uint32_t)h->vehicle_id >= SR_MAX_OBJECT)
        return 0;
    o = &sr_objects[h->vehicle_id];
    if (!o->active || o->health <= 0 || o->type < 0 || o->type > 9 ||
        o->num_seats < 1 || o->num_seats > 7 ||
        h->vehicle_seat < 0 || h->vehicle_seat >= o->num_seats)
        return 0;
    if (driver)
        return h->vehicle_seat == 0 && o->driver == index;
    return h->vehicle_seat >= 1 && h->vehicle_seat <= 3;
}

static void road_copy(AiWaypoint *out, const SvAiStreetPoint *point, uint32_t previous)
{
    const CityIntersection *c = &g_city.intersections[g_city.roads[point->road].ends[point->direction]];
    int j, from = 0, to = 0;
    memset(out, 0, sizeof *out);
    out->road = (int32_t)point->road;
    out->dir = point->direction;
    out->lane = point->lane;
    for (j = 0; j < 4; ++j) {
        if (c->roads[j] == (int32_t)previous) from = j;
        if (c->roads[j] == (int32_t)point->road) to = j;
    }
    if (to == ((from + 1) & 3)) out->turn = -1;
    if (to == ((from - 1) & 3)) out->turn = 1;
}

static SvResult SV_CALL set_street_route(SvOwner owner, SvEntity bot, const SvAiStreetPoint *points, uint32_t count)
{
    int32_t index;
    uint32_t j;
    Control *c;
    SvResult result = resolve(owner, bot, &index);
    if (result != SV_OK)
        return result;
    if (!points || count < 2 || count > SV_AI_WAYPOINTS_MAX)
        return SV_INVALID;
    for (j = 0; j < count; ++j)
        if (!road_pair_valid(&points[j], &points[(j + 1) % count]))
            return SV_INVALID;
    if (!installed || !seat_state(index, 1))
        return SV_STATE;
    c = claim(owner, bot, index);
    memcpy(c->roads, points, count * sizeof *points);
    c->road_count = count;
    c->road_next = 2 % count;
    c->target = 0;
    c->ticks = 0;
    road_copy(&sr_players[index].waypoints[0], &points[0], points[count - 1].road);
    road_copy(&sr_players[index].waypoints[1], &points[1], points[0].road);
    sr_players[index].num_path = 2;
    sr_players[index].ai_waypoint = 0;
    sr_players[index].wait_isec = -1;
    return SV_OK;
}

static int armed_passenger(int32_t index)
{
    Human *h;
    Item *gun;
    int item, mag;
    if (!seat_state(index, 0))
        return 0;
    h = &sr_humans[sr_players[index].human_id];
    if (h->inventory[0].count != 1)
        return 0;
    item = h->inventory[0].items[0];
    if ((uint32_t)item >= SR_MAX_ITEM || !sr_items[item].active)
        return 0;
    gun = &sr_items[item];
    if ((uint32_t)gun->type >= SV_ITEM_TYPE_COUNT || !sr_itemtypes[gun->type].is_gun || gun->num_children != 1)
        return 0;
    mag = gun->children[0];
    return (uint32_t)mag < SR_MAX_ITEM && sr_items[mag].active && sr_items[mag].parent_item == item;
}

static SvResult SV_CALL set_seated_target(SvOwner owner, SvEntity bot, SvEntity player)
{
    int32_t index, other;
    int human;
    Control *c;
    SvResult result = resolve(owner, bot, &index);
    if (result != SV_OK)
        return result;
    other = sv_entity_resolve(SV_ENTITY_PLAYER, player);
    if (other < 0)
        return SV_NOT_FOUND;
    if (other == index)
        return SV_INVALID;
    if (!installed || body_state(other) != SV_OK || !armed_passenger(index))
        return SV_STATE;
    human = sr_players[other].human_id;
    if (sr_humans[human].health <= 0)
        return SV_STATE;
    c = claim(owner, bot, index);
    c->target = sv_entity_at(SV_ENTITY_HUMAN, human);
    c->seated = 1;
    c->road_count = 0;
    return SV_OK;
}

void sv_ai_install(void)
{
    memset(controls, 0, sizeof controls);
    installed = 1;
}

static int route_state(int32_t index)
{
    Player *p = &sr_players[index];
    SvAiStreetPoint a, b;
    if (!seat_state(index, 1) || p->ai_waypoint != 0 || p->num_path != 2 ||
        (p->wait_isec != -1 && (uint32_t)p->wait_isec >= (uint32_t)g_city.num_intersections))
        return 0;
    a = (SvAiStreetPoint){(uint32_t)p->waypoints[0].road, p->waypoints[0].dir, p->waypoints[0].lane};
    b = (SvAiStreetPoint){(uint32_t)p->waypoints[1].road, p->waypoints[1].dir, p->waypoints[1].lane};
    return road_pair_valid(&a, &b);
}

int sv_ai_before_player(int32_t index)
{
    Control *c;
    Player *p;
    if ((uint32_t)index >= SR_MAX_PLAYER || !live_control(index))
        return 0;
    c = &controls[index];
    p = &sr_players[index];
    if (c->target && sv_entity_resolve(SV_ENTITY_HUMAN, c->target) < 0) {
        c->target = 0;
        p->num_threats = 0;
    }
    if (c->road_count && !route_state(index)) {
        memset(p->input_axes, 0, sizeof p->input_axes);
        p->input_buttons = 4;
        return 1;
    }
    if (c->seated && c->target && !armed_passenger(index)) {
        memset(p->input_axes, 0, sizeof p->input_axes);
        p->input_buttons = 0;
        return 1;
    }
    if (!p->is_bot || !c->ticks || writable(index) != SV_OK)
        return 0;
    memcpy(p->input_axes, c->input.axes, sizeof p->input_axes);
    p->input_buttons = c->input.buttons;
    p->yaw = c->input.yaw;
    p->input_pitch = c->input.pitch;
    p->input_mode = 1;
    --c->ticks;
    return 1;
}

void sv_ai_after_threats(int32_t index)
{
    int32_t target;
    Player *p;
    Threat *t;
    if ((uint32_t)index >= SR_MAX_PLAYER || !live_control(index))
        return;
    target = sv_entity_resolve(SV_ENTITY_HUMAN, controls[index].target);
    p = &sr_players[index];
    if (target < 0 || sr_humans[target].health <= 0 ||
        (controls[index].seated ? !armed_passenger(index) : writable(index) != SV_OK))
        return;
    /* The selected target replaces perception. The next scan can rebuild it. */
    p->num_threats = 1;
    p->ai_target = 0;
    t = &p->threats[0];
    t->human = target;
    t->pos = sr_humans[target].bones[controls[index].perception == 1 ? 1 : 2].pos;
    t->level = 16;
    t->timer = 600;
    if (!controls[index].seated) {
        p->num_path = 1;
        p->ai_waypoint = 0;
        memset(&p->waypoints[0], 0, sizeof(AiWaypoint));
        p->waypoints[0].pos = sr_humans[target].pos;
    }
}

int32_t sv_ai_perception_part(int32_t index)
{
    return (uint32_t)index < SR_MAX_PLAYER && live_control(index) ? controls[index].perception : -1;
}

void sv_ai_after_player(int32_t index)
{
    if ((uint32_t)index >= SR_MAX_PLAYER || !live_control(index))
        return;
    if (controls[index].aim == 1) sr_players[index].input_buttons |= 2u;
    if (controls[index].aim == 0) sr_players[index].input_buttons &= ~2u;
}

int sv_ai_shot(int32_t index, int passenger)
{
    int chance = (uint32_t)index < SR_MAX_PLAYER && live_control(index) ? controls[index].chance : -1;
    if (chance < 0)
        return passenger || (os_rand() & 3) == 0;
    if (chance == 0 || chance == 100)
        return chance == 100;
    return os_rand() % 100 < chance;
}

int sv_ai_route_next(int32_t index)
{
    Control *c;
    AiWaypoint *current;
    SvAiStreetPoint previous;
    if ((uint32_t)index >= SR_MAX_PLAYER || !live_control(index) || !controls[index].road_count)
        return 0;
    c = &controls[index];
    current = &sr_players[index].waypoints[0];
    previous = (SvAiStreetPoint){(uint32_t)current->road, current->dir, current->lane};
    if (seat_state(index, 1) && road_pair_valid(&previous, &c->roads[c->road_next])) {
        road_copy(&sr_players[index].waypoints[1], &c->roads[c->road_next], previous.road);
        c->road_next = (c->road_next + 1) % c->road_count;
    } else {
        /* A road removed after validation ends the route before the engine reads it. */
        current->road = -1;
    }
    return 1;
}

void sv_ai_release_owner(SvOwner owner)
{
    int i;
    for (i = 0; i < SR_MAX_PLAYER; ++i)
        if (controls[i].owner == owner)
            memset(&controls[i], 0, sizeof controls[i]);
}

const SvGameAi sv_game_ai = {
    sizeof(SvGameAi), SV_GAME_AI_VERSION,
    read_threats, set_destination, set_waypoints, set_target, override_input, release,
    set_perception, set_aim, set_chance, set_street_route, set_seated_target
};
