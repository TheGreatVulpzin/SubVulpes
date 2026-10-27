/* The damage scales of the plugin API. See plugin_damage.h. */
#include "plugin_damage.h"
#include "sr_runtime.h"
#include <math.h>

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

int sv_damage_human(int victim, int shooter_player, int amount)
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
    return scaled(amount, scale);
}

int sv_damage_object(int object, int amount)
{
    ensure();
    return object >= 0 && object < SR_MAX_OBJECT ? scaled(amount, object_taken[object]) : amount;
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
