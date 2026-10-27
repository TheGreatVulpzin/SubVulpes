/* The damage scales of the plugin API. See plugin_damage.h. */
#include "plugin_damage.h"
#include "plugin_entities.h"
#include "plugin_host.h"
#include "sr_runtime.h"
#include <math.h>
#include <string.h>

static float human_taken[SR_MAX_HUMAN], human_dealt[SR_MAX_HUMAN], object_taken[SR_MAX_OBJECT];
static int ready;

/* The tables start at 1. A static table starts at 0, so the first use fills them. */
static void ensure(void)
{
    int i;
    if (ready)
        return;
    for (i = 0; i < SR_MAX_HUMAN; ++i)
        human_taken[i] = human_dealt[i] = 1.0f;
    for (i = 0; i < SR_MAX_OBJECT; ++i)
        object_taken[i] = 1.0f;
    ready = 1;
}

static int scaled(int amount, float scale)
{
    double value;
    if (scale == 1.0f || amount <= 0)
        return amount;
    value = floor((double)amount * scale + 0.5);
    return value > 100000.0 ? 100000 : (int)value;
}

/* Asks the plugins that decide damage inside the simulation. Most games have none, and then this is one check. */
static int decide(unsigned source, int shooter_player, int human, int object, int amount)
{
    SvDamageEvent event;
    if (amount <= 0 || !sv_plugins_wants(SV_EVENT_DAMAGE))
        return amount;
    memset(&event, 0, sizeof event);
    event.base.size = sizeof event;
    event.base.kind = SV_EVENT_DAMAGE;
    event.base.tick = (uint32_t)sr_game.ticks;
    event.source = source;
    event.amount = event.new_amount = amount;
    if (human >= 0 && human < SR_MAX_HUMAN)
        event.human = sv_entity_at(SV_ENTITY_HUMAN, human);
    if (object >= 0 && object < SR_MAX_OBJECT)
        event.object = sv_entity_at(SV_ENTITY_OBJECT, object);
    if (shooter_player >= 0 && shooter_player < SR_MAX_PLAYER) {
        int body = sr_players[shooter_player].human_id;
        if (body >= 0 && body < SR_MAX_HUMAN && sr_humans[body].active)
            event.attacker = sv_entity_at(SV_ENTITY_HUMAN, body);
    }
    return sv_plugins_decide_damage(&event);
}

int sv_damage_human(int victim, int shooter_player, int amount, unsigned source)
{
    float scale = 1.0f;
    ensure();
    if (victim >= 0 && victim < SR_MAX_HUMAN)
        scale *= human_taken[victim];
    if (shooter_player >= 0 && shooter_player < SR_MAX_PLAYER) {
        int body = sr_players[shooter_player].human_id;
        if (body >= 0 && body < SR_MAX_HUMAN)
            scale *= human_dealt[body];
    }
    return decide(source, shooter_player, victim, -1, scaled(amount, scale));
}

int sv_damage_object(int object, int amount)
{
    ensure();
    if (object < 0 || object >= SR_MAX_OBJECT)
        return amount;
    return decide(SV_DAMAGE_OBJECT, -1, -1, object, scaled(amount, object_taken[object]));
}

float sv_damage_scale_object(int object)
{
    ensure();
    return object >= 0 && object < SR_MAX_OBJECT ? object_taken[object] : 1.0f;
}

void sv_damage_reset_human(int human)
{
    ensure();
    if (human >= 0 && human < SR_MAX_HUMAN)
        human_taken[human] = human_dealt[human] = 1.0f;
}

void sv_damage_reset_object(int object)
{
    ensure();
    if (object >= 0 && object < SR_MAX_OBJECT)
        object_taken[object] = 1.0f;
}

float *sv_damage_field_human_taken(int human)
{
    ensure();
    return &human_taken[human];
}

float *sv_damage_field_human_dealt(int human)
{
    ensure();
    return &human_dealt[human];
}

float *sv_damage_field_object_taken(int object)
{
    ensure();
    return &object_taken[object];
}
