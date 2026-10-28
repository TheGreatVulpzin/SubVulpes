/* Mission slots use the game's existing item and mission update paths. */
#include "plugin_host.h"
#include "plugin_entities.h"
#include "sv_game_mission.h"
#include "plugin_game_internal.h"
#include "logic.h"
#include "economy.h"
#include "sr_server.h"
#include <string.h>

typedef struct { SvOwner owner; SvMissionRef reference; } Binding;
static Binding bindings[SV_MISSION_TEAMS][SV_MISSION_SLOTS];
static int installed;
/* Event 12 stores team * 16 and slot * 2 in the same low byte. */
enum { CLIENT_MISSION_SLOTS = 8 };

static SvResult context(SvOwner owner)
{
    return sv_plugins_context(owner) && sv_game_is_ready() ? SV_OK : SV_STATE;
}

static SvMissionRef reference(uint32_t team, uint32_t slot)
{
    Mission *m = &team_missions((int)team)[slot];
    SvMissionRef r = {sizeof r, team, slot, m->scenario, 0, 0};
    r.reset = sv_game_reset_sequence();
    r.item = sv_entity_at(SV_ENTITY_ITEM, m->item);
    return r;
}

static SvResult SV_CALL read_slots(SvOwner owner, uint32_t team, SvMissionSlots *out)
{
    SvMissionSlots copy = {sizeof copy};
    uint32_t j;
    if (context(owner) != SV_OK)
        return SV_STATE;
    if (team >= SV_MISSION_TEAMS || !out || out->size < sizeof *out)
        return SV_INVALID;
    copy.team = team;
    copy.item_size = sizeof(SvMissionState);
    for (j = 0; j < SV_MISSION_SLOTS; ++j) {
        Mission *m = &team_missions((int)team)[j];
        SvMissionState *s = &copy.slots[j];
        s->active = m->active;
        s->kind = m->kind;
        s->partner = m->partner;
        s->reward = m->cash;
        s->expire = m->expire;
        s->site = m->site;
        s->stake = m->stake;
        if (m->active) {
            s->reference = reference(team, j);
            s->item = s->reference.item;
        }
    }
    *out = copy;
    return SV_OK;
}

static int scenario_free(int id)
{
    int team, slot;
    for (team = 0; team < 5; ++team)
        for (slot = 0; slot < 16; ++slot)
            if (team_missions(team)[slot].active && team_missions(team)[slot].scenario == id)
                return 0;
    return 1;
}

static SvResult SV_CALL start(SvOwner owner, uint32_t team, const SvMissionStart *input, SvMissionRef *out)
{
    int32_t item;
    int slot, t, j;
    Mission copy = {0};
    if (context(owner) != SV_OK)
        return SV_STATE;
    if (team >= SV_MISSION_TEAMS || !input || input->size < sizeof *input || !out ||
        input->kind >= SV_MISSION_KINDS || input->partner < 0 || input->partner >= 5 ||
        input->reward < 0 || input->reward > SV_MISSION_REWARD_MAX ||
        input->expire <= g_game_world.clock || input->expire > SV_MISSION_EXPIRE_MAX ||
        input->site < 0 || input->site > 32766)
        return SV_INVALID;
    if ((input->kind == 0 && input->site >= g_city.num_buildings) ||
        (input->kind == 1 && input->site != 255) || (input->kind >= 2 && input->site != 0))
        return SV_INVALID;
    item = sv_entity_resolve(SV_ENTITY_ITEM, input->item);
    if (item < 0)
        return SV_NOT_FOUND;
    if (sr_items[item].type < 17 || sr_items[item].type > 22)
        return SV_INVALID;
    if (sr_items[item].time_to_live <= 0)
        return SV_STATE;
    slot = scenario_getnextmission((int)team);
    if (slot >= CLIENT_MISSION_SLOTS)
        return SV_LIMIT;
    if (g_scenario.next_id < 0 || g_scenario.next_id == INT32_MAX)
        return SV_LIMIT;
    if (!scenario_free(g_scenario.next_id))
        return SV_STATE;
    for (t = 0; t < 5; ++t)
        for (j = 0; j < 16; ++j)
            if (team_missions(t)[j].active && team_missions(t)[j].item == item)
                return SV_CONFLICT;
    if (!installed)
        return SV_STATE;
    copy.active = 1;
    copy.scenario = g_scenario.next_id;
    copy.kind = (int)input->kind;
    copy.item = item;
    copy.item_type = sr_items[item].type;
    copy.partner = input->partner;
    copy.cash = input->reward;
    copy.expire = input->expire;
    copy.site = input->site;
    /* Plugin missions do not create native stakes or price-growth rules. */
    team_missions((int)team)[slot] = copy;
    ++g_scenario.next_id;
    sr_items[item].time_to_live = 65535;
    *out = reference(team, (uint32_t)slot);
    bindings[team][slot].owner = owner;
    bindings[team][slot].reference = *out;
    sr_createevent_updatemission((int)team, slot);
    return SV_OK;
}

static SvResult SV_CALL cancel(SvOwner owner, const SvMissionRef *r)
{
    Mission *m;
    int t, j;
    int32_t item;
    if (context(owner) != SV_OK)
        return SV_STATE;
    if (!r || r->size < sizeof *r || r->team >= SV_MISSION_TEAMS || r->slot >= SV_MISSION_SLOTS)
        return SV_INVALID;
    m = &team_missions((int)r->team)[r->slot];
    item = sv_entity_resolve(SV_ENTITY_ITEM, r->item);
    if (!m->active || r->reset != sv_game_reset_sequence() || m->scenario != r->scenario ||
        item < 0 || item != m->item)
        return SV_NOT_FOUND;
    /* Native stake cancellation changes prices. Keep that path closed until it has a general finance guard. */
    for (t = 0; t < 5; ++t)
        for (j = 0; j < 16; ++j)
            if (team_missions(t)[j].active && team_missions(t)[j].scenario == m->scenario &&
                (j >= CLIENT_MISSION_SLOTS || team_missions(t)[j].stake != 0))
                return SV_STATE;
    scenario_setmissioninactive(m->scenario);
    return SV_OK;
}

void sv_mission_install(void)
{
    memset(bindings, 0, sizeof bindings);
    installed = 1;
}

/* An API-owned mission has zero growth and stake. Check its current reward just before native completion. */
int sv_mission_before_complete(int32_t team, int32_t slot)
{
    Binding *b;
    Mission *m;
    int t, j;
    if ((uint32_t)team >= SV_MISSION_TEAMS || (uint32_t)slot >= SV_MISSION_SLOTS)
        return 0;
    b = &bindings[team][slot];
    m = &team_missions(team)[slot];
    if (!b->owner || b->reference.reset != sv_game_reset_sequence() || b->reference.scenario != m->scenario)
        return 1;
    if (!m->active || sv_entity_resolve(SV_ENTITY_ITEM, b->reference.item) != m->item)
        return 0;
    for (t = 0; t < 5; ++t) {
        float price = g_market.stocks[t].price;
        int64_t reward = 0;
        int p;
        if (!(price >= 10 && price <= 500))
            return 0;
        for (j = 0; j < 16; ++j) {
            Mission *linked = &team_missions(t)[j];
            if (!linked->active || linked->scenario != m->scenario)
                continue;
            if (j >= CLIENT_MISSION_SLOTS || linked->stake || linked->own_growth != 0 || linked->other_growth != 0 ||
                linked->cash < 0 || linked->cash > SV_MISSION_REWARD_MAX)
                return 0;
            if (t == team)
                reward += linked->cash;
        }
        if (t == team)
            for (p = 0; p < SR_MAX_PLAYER; ++p)
                if (sr_players[p].active && sr_players[p].team == team) {
                    int64_t sum = (int64_t)sr_players[p].money + reward;
                    if (sum < INT32_MIN || sum > INT32_MAX)
                        return 0;
                }
    }
    return 1;
}

static void retire(Binding *b, Mission *m)
{
    if (m->active && m->scenario == b->reference.scenario) {
        if (sv_entity_resolve(SV_ENTITY_ITEM, b->reference.item) != m->item)
            m->item = -1;
        scenario_setmissioninactive(m->scenario);
    }
    memset(b, 0, sizeof *b);
}

void sv_mission_before_tick(void)
{
    int t, j;
    for (t = 0; t < 5; ++t)
        for (j = 0; j < 16; ++j) {
            Binding *b = &bindings[t][j];
            Mission *m = &team_missions(t)[j];
            if (!b->owner)
                continue;
            if (b->reference.reset != sv_game_reset_sequence() || !m->active || m->scenario != b->reference.scenario)
                memset(b, 0, sizeof *b);
            else if (m->expire < g_game_world.clock ||
                sv_entity_resolve(SV_ENTITY_ITEM, b->reference.item) != m->item)
                retire(b, m);
        }
}

void sv_mission_release_owner(SvOwner owner)
{
    int t, j;
    if (!owner)
        return;
    for (t = 0; t < 5; ++t)
        for (j = 0; j < 16; ++j)
            if (bindings[t][j].owner == owner) {
                if (bindings[t][j].reference.reset != sv_game_reset_sequence())
                    memset(&bindings[t][j], 0, sizeof bindings[t][j]);
                else
                    retire(&bindings[t][j], &team_missions(t)[j]);
            }
}

const SvGameMission sv_game_mission = {
    sizeof(SvGameMission), SV_GAME_MISSION_VERSION, read_slots, start, cancel
};
