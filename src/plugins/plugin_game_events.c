/* The core side of the plugin events: it turns a game index into a reference and posts it. See plugin_events.c. */
#include "plugin_game_events.h"
#include "plugin_host.h"
#include "plugin_entities.h"
#include "plugin_damage.h"
#include "plugin_game_internal.h"
#include "sr_runtime.h"
#include <string.h>

/* The player whose bullet hit the human last, or -1. It lives as long as the human. */
static int32_t last_hit[SR_MAX_HUMAN];

static uint32_t now(void)
{
    return (uint32_t)sr_game.ticks;
}

void sv_game_event_spawn(int player, int human)
{
    if (human < 0 || human >= SR_MAX_HUMAN)
        return;
    last_hit[human] = -1;
    sv_damage_reset_human(human);
    if (player < 0 || player >= SR_MAX_PLAYER)
        return;
    sv_events_post(SV_EVENT_SPAWN, now(), sv_entity_at(SV_ENTITY_PLAYER, player), sv_entity_at(SV_ENTITY_HUMAN, human), 0);
}

void sv_game_event_death(int human)
{
    SvEntity killer = 0;
    int owner, player;
    if (human < 0 || human >= SR_MAX_HUMAN || !sv_plugins_wants(SV_EVENT_DEATH))
        return;
    player = sr_humans[human].player_id;
    owner = last_hit[human];
    if (owner >= 0 && owner < SR_MAX_PLAYER && sr_players[owner].active) {
        int body = sr_players[owner].human_id;
        if (body >= 0 && body < SR_MAX_HUMAN && body != human && sr_humans[body].active)
            killer = sv_entity_at(SV_ENTITY_HUMAN, body);
    }
    sv_events_post(SV_EVENT_DEATH, now(), player >= 0 && player < SR_MAX_PLAYER ? sv_entity_at(SV_ENTITY_PLAYER, player) : 0,
        sv_entity_at(SV_ENTITY_HUMAN, human), killer);
}

void sv_game_event_leave(int player)
{
    if (player < 0 || player >= SR_MAX_PLAYER)
        return;
    sv_events_post(SV_EVENT_LEAVE, now(), sv_entity_at(SV_ENTITY_PLAYER, player), 0, 0);
}

void sv_game_event_hit(int human, int owner)
{
    if (human < 0 || human >= SR_MAX_HUMAN)
        return;
    last_hit[human] = owner;
}

static SvEntity player_of(int human)
{
    return human >= 0 && human < SR_MAX_HUMAN ? sv_entity_at(SV_ENTITY_PLAYER, sr_humans[human].player_id) : 0;
}

void sv_game_event_vehicle(int human, int object, int seat, int entered)
{
    if (!sv_plugins_wants(SV_EVENT_VEHICLE) || human < 0 || human >= SR_MAX_HUMAN || object < 0 || object >= SR_MAX_OBJECT)
        return;
    sv_events_post_ex(SV_EVENT_VEHICLE, now(), player_of(human), sv_entity_at(SV_ENTITY_HUMAN, human), 0, 0,
        sv_entity_at(SV_ENTITY_OBJECT, object), seat, entered);
}

/* The rounds left in a gun: the round in the chamber plus the rounds of the magazines in it. */
int sv_game_shot_rounds(int item)
{
    int rounds, i, children;
    if (item < 0 || item >= SR_MAX_ITEM)
        return 0;
    rounds = sr_items[item].ammo;
    children = sr_items[item].num_children;
    if (children > 8)
        children = 8;
    for (i = 0; i < children; ++i) {
        int child = sr_items[item].children[i];
        if (child >= 0 && child < SR_MAX_ITEM && sr_items[child].ammo > 0)
            rounds += sr_items[child].ammo;
    }
    return rounds;
}

void sv_game_event_shot(int item, int human)
{
    if (!sv_plugins_wants(SV_EVENT_SHOT) || item < 0 || item >= SR_MAX_ITEM || human < 0 || human >= SR_MAX_HUMAN)
        return;
    sv_events_post_ex(SV_EVENT_SHOT, now(), player_of(human), sv_entity_at(SV_ENTITY_HUMAN, human), 0,
        sv_entity_at(SV_ENTITY_ITEM, item), 0, sr_items[item].type, sv_game_shot_rounds(item));
}

void sv_game_event_object_gone(int object)
{
    if (!sv_plugins_wants(SV_EVENT_OBJECT_GONE) || object < 0 || object >= SR_MAX_OBJECT)
        return;
    sv_events_post_ex(SV_EVENT_OBJECT_GONE, now(), 0, 0, 0, 0, sv_entity_at(SV_ENTITY_OBJECT, object),
        sr_objects[object].type, sr_objects[object].color);
}

void sv_game_event_item(int item, int human, int slot, int in)
{
    if (!sv_plugins_wants(SV_EVENT_ITEM) || item < 0 || item >= SR_MAX_ITEM || human < 0 || human >= SR_MAX_HUMAN)
        return;
    sv_events_post_ex(SV_EVENT_ITEM, now(), player_of(human), sv_entity_at(SV_ENTITY_HUMAN, human), 0,
        sv_entity_at(SV_ENTITY_ITEM, item), 0, slot, in);
}

/* The state and the number of the round at the last look. The first look only records them. */
static int round_known, round_state, round_number;

void sv_game_event_poll(void)
{
    if (!sv_game_is_ready())
        return;
    if (round_known && (round_state != sr_game.state || round_number != sr_game.round_number)) {
        round_state = sr_game.state;
        round_number = sr_game.round_number;
        if (sv_plugins_wants(SV_EVENT_ROUND))
            sv_events_post_ex(SV_EVENT_ROUND, now(), 0, 0, 0, 0, 0, round_state, round_number);
    }
    if (!round_known) {
        round_known = 1;
        round_state = sr_game.state;
        round_number = sr_game.round_number;
    }
}

/* Forgets the last look, for a game that starts again. */
void sv_game_event_poll_reset(void)
{
    round_known = 0;
}

/* A replacement of the chat text: 1 to 63 printable ASCII bytes. */
static int replacement_ok(const char *text)
{
    size_t i, length = strnlen(text, 64);
    if (!length || length > 63)
        return 0;
    for (i = 0; i < length; ++i)
        if ((unsigned char)text[i] < 0x20 || (unsigned char)text[i] > 0x7e)
            return 0;
    return 1;
}

int sv_game_chat_denied(int player, char *text)
{
    SvChatEvent event;
    size_t length;
    if (player < 0 || player >= SR_MAX_PLAYER || !text || !sv_plugins_wants(SV_EVENT_CHAT))
        return 0;
    memset(&event, 0, sizeof event);
    event.player = sv_entity_at(SV_ENTITY_PLAYER, player);
    if (!event.player)
        return 0;
    event.base.size = sizeof event;
    event.base.kind = SV_EVENT_CHAT;
    event.base.tick = now();
    length = strnlen(text, sizeof event.text - 1);
    memcpy(event.text, text, length);
    memcpy(event.replacement, event.text, sizeof event.replacement);
    if (sv_plugins_decide(&event.base) == SV_DENIED)
        return 1;
    /* The plugin may have written a new text. The last byte of the record is forced to zero before the check. */
    event.replacement[sizeof event.replacement - 1] = 0;
    if (strcmp(event.replacement, event.text) && replacement_ok(event.replacement)) {
        length = strlen(event.replacement);
        memcpy(text, event.replacement, length);
        text[length] = 0;
    }
    return 0;
}

int sv_game_join_denied(int player)
{
    SvGameEvent event;
    if (player < 0 || player >= SR_MAX_PLAYER || !sv_plugins_wants(SV_EVENT_JOIN))
        return 0;
    memset(&event, 0, sizeof event);
    event.player = sv_entity_at(SV_ENTITY_PLAYER, player);
    event.account = sv_entity_at(SV_ENTITY_ACCOUNT, sr_players[player].account);
    if (!event.player)
        return 0;
    event.base.size = sizeof event;
    event.base.kind = SV_EVENT_JOIN;
    event.base.tick = now();
    return sv_plugins_decide(&event.base) == SV_DENIED;
}
