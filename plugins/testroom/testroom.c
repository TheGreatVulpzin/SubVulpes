/* The test room. An administrator types /test start in the chat. The plugin runs one test after another. It says what it
 * does and what the administrator should see, then waits for /test yes or /test no. It uses only the public plugin API, so
 * a failed test points at the API or at the game, and not at a private shortcut. The results go to the server log. */
#include <math.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "sv_game_read.h"
#include "sv_game_write.h"
#include "sv_game_props.h"
#include "sv_game_events.h"
#include "sv_game_ai.h"
#include "sv_game_engine.h"
#include "sv_game_shop.h"
#include "sv_game_storage.h"

#define VERSION "0.1.8"
#define LINE_MAX_CHARS 60
#define MAX_TESTS 40

static const SvHost *host;
static SvOwner self;
static const SvGameRead *game;
static const SvGameWrite *change;
static const SvGameProps *props;
static const SvGameAi *ai;
static const SvGameEngine *engine;
static const SvGameStorage *storage;

enum { TODO, PASS, FAIL, SKIP };
/* What a test changed on the player, so the next test or /test stop can undo it. */
enum { T_HEALTH = 1, T_BLEED = 2, T_STUN = 4, T_DEALT = 8, T_LOOKS = 16, T_TYPES = 32, T_DEATH = 64, T_PHONE = 128, T_AI = 256 };
#define EVENTS_BASE (SV_EVENT_CHAT | SV_EVENT_ROUND | SV_EVENT_VEHICLE | SV_EVENT_SHOT | SV_EVENT_OBJECT_GONE | SV_EVENT_ITEM)

typedef struct Test {
    const char *name;
    const char *what;
    const char *expect;
    SvResult (*run)(void);
    SvResult (*go)(void);
} Test;

static int test_count;
static SvEntity tester;
static int current;
static unsigned char result[MAX_TESTS + 1];
static unsigned touched;
static int32_t saved_looks[SV_LOOK_COUNT];
static SvEntity made_objects[6], made_players[4], made_items[8];
static int made_object_count, made_player_count, made_item_count;
static uint32_t sound_next;
static SvEntity last_magazine, last_car, last_npc, last_npc_human, last_gun;
static float saved_speed, saved_bullet_mass;
static unsigned name_step;
static int chat_prefix_on, notices_on, phone_claim_on;

/* ------------------------------------------------------------ text */

static void say(const char *text)
{
    char line[LINE_MAX_CHARS + 4];
    size_t length = strlen(text);
    while (length) {
        size_t take = length > LINE_MAX_CHARS ? LINE_MAX_CHARS : length, i;
        if (take < length)
            for (i = take; i > LINE_MAX_CHARS / 2; --i)
                if (text[i] == ' ') {
                    take = i;
                    break;
                }
        memcpy(line, text, take);
        line[take] = 0;
        change->message_to(self, SV_MESSAGE_SERVER, tester, line);
        text += take;
        length -= take;
        while (*text == ' ') {
            ++text;
            --length;
        }
    }
}

static void sayf(const char *format, ...)
{
    char text[200];
    va_list args;
    va_start(args, format);
    vsnprintf(text, sizeof text, format, args);
    va_end(args);
    say(text);
}

static void logf_(const char *format, ...)
{
    char text[200];
    va_list args;
    va_start(args, format);
    vsnprintf(text, sizeof text, format, args);
    va_end(args);
    host->log(self, text);
}

/* ------------------------------------------------------------ small helpers */

static SvResult body(SvPlayerState *player, SvHumanState *human)
{
    SvResult r = game->player(self, tester, player);
    if (r != SV_OK)
        return r;
    if (!player->human)
        return SV_STATE;
    return game->human(self, player->human, human);
}

static uint32_t prop_id(const char *name)
{
    static struct { char name[SV_PROP_NAME_MAX]; uint32_t id; } cache[16];
    static int used;
    uint32_t id = 0;
    int i;
    for (i = 0; i < used; ++i)
        if (!strcmp(cache[i].name, name))
            return cache[i].id;
    if (props->find(self, name, &id) != SV_OK || used >= 16)
        return 0;
    snprintf(cache[used].name, sizeof cache[used].name, "%s", name);
    cache[used++].id = id;
    return id;
}

static SvResult set_int(const char *name, SvEntity entity, int32_t value)
{
    SvValue v;
    v.i = value;
    return props->set(self, prop_id(name), entity, 0, v);
}

static SvResult set_float(const char *name, SvEntity entity, float value)
{
    SvValue v;
    v.f = value;
    return props->set(self, prop_id(name), entity, 0, v);
}

static SvVector offset(SvVector from, float dx, float dy, float dz)
{
    from.x += dx;
    from.y += dy;
    from.z += dz;
    return from;
}

static SvVector beside(const SvHumanState *human, float dx, float dy, float dz)
{
    return offset(human->position, dx, dy, dz);
}

static void forget(void)
{
    made_object_count = made_player_count = made_item_count = 0;
}

/* Undo what the last test changed and remove what it made. Errors do not matter here: the thing may be gone already. */
static SvResult set_engine_int(const char *name, int32_t value)
{
    SvEngineValue v = { sizeof v, SV_ENGINE_INT };
    v.value.integer = value;
    return engine ? engine->set(self, name, SV_ENGINE_GLOBAL, &v) : SV_INCOMPATIBLE;
}

static SvResult SV_CALL on_event(SvOwner owner, const SvEvent *event);

static void cleanup(void)
{
    SvPlayerState player = { sizeof player };
    SvHumanState human = { sizeof human };
    int i;
    if ((touched & T_AI) && ai)
        ai->release(self, last_npc);
    if (touched & T_DEATH)
        set_engine_int("human.death_start", 0);
    if (touched & T_PHONE)
        host->subscribe(self, EVENTS_BASE, on_event);
    phone_claim_on = 0;
    for (i = 0; i < made_object_count; ++i)
        change->object_remove(self, made_objects[i]);
    for (i = 0; i < made_player_count; ++i)
        change->npc_remove(self, made_players[i]);
    /* The game removes an item in its next lifetime pass. Detach it first, so a hand is free for the next test now. */
    for (i = 0; i < made_item_count; ++i) {
        change->item_detach(self, made_items[i]);
        change->item_remove(self, made_items[i]);
    }
    forget();
    chat_prefix_on = 0;
    notices_on = 0;
    if (touched & T_TYPES) {
        SvValue v;
        v.f = saved_speed;
        props->set(self, prop_id("item_type.bullet_speed"), 0, 4, v);
        v.f = saved_bullet_mass;
        props->set(self, prop_id("bullet_type.mass"), 0, 3, v);
    }
    if (tester && touched && body(&player, &human) == SV_OK) {
        if (touched & T_HEALTH)
            set_int("human.health", player.human, 100);
        if (touched & T_BLEED)
            set_int("human.bleeding", player.human, 0);
        if (touched & T_STUN)
            set_int("human.stun", player.human, 0);
        if (touched & T_DEALT)
            set_float("human.damage_dealt_scale", player.human, 1.0f);
        if ((touched & T_LOOKS) && change->set_looks(self, tester, saved_looks) != SV_OK)
            logf_("TESTROOM could not put the looks back");
    }
    touched = 0;
}

static void remember(SvEntity *list, int *count, int limit, SvEntity entity)
{
    if (entity && *count < limit)
        list[(*count)++] = entity;
}

/* ------------------------------------------------------------ the tests */

/* One person cannot tell who else sees a line. Say how many players are here, so the answer is honest. */
static void players_note(void)
{
    SvStatus status = { sizeof status };
    if (host->status(self, &status) == SV_OK && status.players < 2)
        say("You are the only player here, so this test cannot show who else sees the line. Join a second client to check.");
}

static SvResult t_chat_all(void)
{
    SvResult r = change->message(self, SV_MESSAGE_SERVER, "TEST: every player sees this line.");
    players_note();
    return r;
}

static SvResult t_chat_private(void)
{
    SvResult r = change->message_to(self, SV_MESSAGE_SERVER, tester, "TEST: only you see this line.");
    players_note();
    return r;
}

static SvResult t_chat_admin(void)
{
    return change->message(self, SV_MESSAGE_ADMIN, "TEST: only administrators see this line.");
}

static SvResult t_sound_go(void)
{
    SvPlayerState player = { sizeof player };
    SvHumanState human = { sizeof human };
    SvResult r = body(&player, &human);
    uint32_t index = sound_next;
    if (r != SV_OK)
        return r;
    sound_next = (sound_next + 1) % 47;
    r = change->sound(self, index, human.position, 1.0f, 1.0f);
    sayf("Sound %u played. Type /test go for the next one.", index);
    return r;
}

static SvResult t_sound(void)
{
    sound_next = 0;
    return t_sound_go();
}

/* Moves you next to a newspaper that lies free in the world (item type 26). The plugin cannot make one. */
static SvResult to_newspaper(void)
{
    SvPlayerState player = { sizeof player };
    SvHumanState human = { sizeof human };
    SvResult r = body(&player, &human);
    uint32_t cursor = 0, page_number;
    if (r != SV_OK)
        return r;
    for (page_number = 0; page_number < 8; ++page_number) {
        SvEntity page[SV_ENTITY_PAGE_MAX];
        uint32_t count = 0, next = 0, i;
        r = game->entities(self, SV_ENTITY_ITEM, cursor, page, SV_ENTITY_PAGE_MAX, &count, &next);
        if (r != SV_OK)
            return r;
        for (i = 0; i < count; ++i) {
            SvItemState item = { sizeof item };
            if (game->item(self, page[i], &item) == SV_OK && item.type == 26 && !item.parent_human && !item.parent_item) {
                r = change->teleport(self, player.human, offset(item.position, 2.0f, 0.5f, 0.0f));
                say(r == SV_OK ? "You stand next to a newspaper. Pick it up and read it." : "Could not move you next to a newspaper.");
                return r == SV_STATE ? SV_OK : r;
            }
        }
        if (next <= cursor)
            break;
        cursor = next;
    }
    say("No free newspaper lies in the world now. Skip this test, or find one.");
    return SV_OK;
}

static SvResult t_head_replace(void)
{
    SvResult r = change->headline(self, 0, "TEST ROOM: this is headline 0, replaced by a plugin.");
    if (r != SV_OK)
        return r;
    return to_newspaper();
}

static SvResult t_head_add(void)
{
    SvWorldState world = { sizeof world };
    char text[80];
    SvResult r = game->world(self, &world);
    if (r != SV_OK)
        return r;
    snprintf(text, sizeof text, "TEST ROOM: this is a new headline, number %u.", world.headline_count);
    r = change->headline(self, world.headline_count, text);
    sayf("The newspaper now has %u lines.", world.headline_count + 1);
    return r;
}

static SvResult t_teleport(void)
{
    SvPlayerState player = { sizeof player };
    SvHumanState human = { sizeof human };
    SvResult r = body(&player, &human);
    if (r != SV_OK)
        return r;
    return change->teleport(self, player.human, beside(&human, 25.0f, 0.0f, 0.0f));
}

static SvResult t_health(void)
{
    SvPlayerState player = { sizeof player };
    SvHumanState human = { sizeof human };
    SvResult r = body(&player, &human);
    if (r != SV_OK)
        return r;
    touched |= T_HEALTH;
    return set_int("human.health", player.human, 50);
}

static SvResult t_bleed(void)
{
    SvPlayerState player = { sizeof player };
    SvHumanState human = { sizeof human };
    SvResult r = body(&player, &human);
    if (r != SV_OK)
        return r;
    touched |= T_BLEED;
    return set_int("human.bleeding", player.human, 1);
}

static SvResult t_stun(void)
{
    SvPlayerState player = { sizeof player };
    SvHumanState human = { sizeof human };
    SvResult r = body(&player, &human);
    if (r != SV_OK)
        return r;
    touched |= T_STUN;
    return set_int("human.stun", player.human, 300);
}

static SvResult t_push(void)
{
    SvPlayerState player = { sizeof player };
    SvHumanState human = { sizeof human };
    SvValue push;
    SvResult r = body(&player, &human);
    uint32_t bone;
    if (r != SV_OK)
        return r;
    push.v.x = 0.0f;
    push.v.y = 0.3f;
    push.v.z = 0.0f;
    for (bone = 0; bone < 15 && r == SV_OK; ++bone)
        r = props->add(self, prop_id("human.bone_velocity"), player.human, bone, push);
    return r;
}

static SvResult t_item_hand(void)
{
    SvPlayerState player = { sizeof player };
    SvHumanState human = { sizeof human };
    SvEntity item = 0;
    SvResult r = body(&player, &human);
    if (r != SV_OK)
        return r;
    r = change->item_create(self, 12, human.position, &item);
    if (r != SV_OK)
        return r;
    remember(made_items, &made_item_count, 8, item);
    return change->item_equip(self, item, player.human, 0);
}

static SvResult t_magnum(void)
{
    SvPlayerState player = { sizeof player };
    SvHumanState human = { sizeof human };
    SvEntity gun = 0, magazine = 0;
    SvResult r = body(&player, &human);
    if (r != SV_OK)
        return r;
    r = change->item_create(self, 4, human.position, &gun);
    if (r != SV_OK)
        return r;
    remember(made_items, &made_item_count, 8, gun);
    r = change->item_create(self, 5, human.position, &magazine);
    if (r != SV_OK)
        return r;
    remember(made_items, &made_item_count, 8, magazine);
    last_magazine = magazine;
    last_gun = gun;
    r = change->item_attach(self, gun, magazine);
    if (r != SV_OK)
        return r;
    return change->item_equip(self, gun, player.human, 0);
}

static SvResult t_car(void)
{
    SvPlayerState player = { sizeof player };
    SvHumanState human = { sizeof human };
    SvEntity car = 0;
    SvResult r = body(&player, &human);
    if (r != SV_OK)
        return r;
    r = change->object_create(self, 0, beside(&human, 8.0f, 1.0f, 0.0f), 0.0f, 1, &car);
    remember(made_objects, &made_object_count, 6, car);
    last_car = car;
    return r;
}

static SvResult t_npc(void)
{
    SvPlayerState player = { sizeof player };
    SvHumanState human = { sizeof human };
    SvEntity npc = 0, npc_human = 0;
    SvResult r = body(&player, &human);
    if (r != SV_OK)
        return r;
    r = change->npc_create(self, beside(&human, 4.0f, 0.5f, 0.0f), 0.0f, "Guard", &npc, &npc_human);
    remember(made_players, &made_player_count, 4, npc);
    last_npc = npc;
    last_npc_human = npc_human;
    return r;
}

static SvResult t_exit_go(void)
{
    SvPlayerState player = { sizeof player };
    SvHumanState human = { sizeof human };
    SvResult r = body(&player, &human);
    if (r != SV_OK)
        return r;
    r = change->vehicle_exit(self, player.human, 1);
    if (r == SV_STATE)
        say("You do not sit in a vehicle. Get in the car, then type /test go.");
    return r == SV_STATE ? SV_OK : r;
}

static SvResult t_exit(void)
{
    SvResult r = t_car();
    if (r == SV_OK)
        say("Get in the car (use key), then type /test go.");
    return r;
}

static SvResult t_aim_go(void)
{
    SvPlayerState player = { sizeof player };
    SvHumanState human = { sizeof human };
    SvLineHit hit = { sizeof hit };
    static const char *const kinds[] = { "nothing", "the level", "a vehicle", "a human" };
    SvResult r = body(&player, &human);
    if (r != SV_OK)
        return r;
    r = game->aim(self, player.human, 300.0f, SV_LINE_ALL, &hit);
    if (r != SV_OK)
        return r;
    sayf("Your aim hits %s, %.1f units away. Look elsewhere, type /test go.", kinds[hit.kind < 4 ? hit.kind : 0],
        (double)hit.distance);
    return SV_OK;
}

static SvResult t_aim(void)
{
    SvResult r = t_npc();
    if (r != SV_OK)
        return r;
    say("A bot stands 4 units beside you. Aim at it, at the ground and at the sky, and type /test go each time.");
    return t_aim_go();
}

/* The five look settings and their highest values, as the game makes them. */
static const int32_t look_max[SV_LOOK_COUNT] = { 2, 7, 3, 1, 1 };
static int32_t look_now[SV_LOOK_COUNT];
static unsigned look_step;

static SvResult t_looks_go(void)
{
    unsigned which = look_step++ % SV_LOOK_COUNT;
    SvResult r;
    look_now[which] = (look_now[which] + 1) % (look_max[which] + 1);
    r = change->set_looks(self, tester, look_now);
    sayf("Look %c is now %d (0 to %d). Type /test go for the next look.", (char)('a' + which), (int)look_now[which],
        (int)look_max[which]);
    return r;
}

static SvResult t_looks(void)
{
    SvPlayerState player = { sizeof player };
    SvHumanState human = { sizeof human };
    SvResult r = body(&player, &human);
    if (r != SV_OK)
        return r;
    memcpy(saved_looks, player.looks, sizeof saved_looks);
    touched |= T_LOOKS;
    memset(look_now, 0, sizeof look_now);
    look_step = 0;
    r = change->set_looks(self, tester, look_now);
    sayf("Your five looks were %d %d %d %d %d. Now all are 0.", (int)saved_looks[0], (int)saved_looks[1],
        (int)saved_looks[2], (int)saved_looks[3], (int)saved_looks[4]);
    say("Type /test go to raise look a, then b, c, d and e, one at a time. Watch what changes.");
    return r;
}

/* The car moves 10 units for each go. */
static SvResult t_car_move_go(void)
{
    SvObjectState car = { sizeof car };
    SvResult r = game->object(self, last_car, &car);
    if (r != SV_OK)
        return r;
    r = change->teleport(self, last_car, offset(car.position, 10.0f, 0.0f, 0.0f));
    say("The car moved 10 units. Type /test go to move it again.");
    return r;
}

static SvResult t_car_move(void)
{
    SvResult r = t_car();
    if (r == SV_OK)
        say("Type /test go to move the car 10 units.");
    return r;
}

/* You sit in the driver seat of a car beside you. */
static SvResult t_seat_go(void)
{
    SvPlayerState player = { sizeof player };
    SvHumanState human = { sizeof human };
    SvResult r = body(&player, &human);
    if (r != SV_OK)
        return r;
    r = change->vehicle_enter(self, player.human, last_car, 0);
    if (r == SV_STATE)
        say("You already sit in a vehicle. Get out first, then type /test go.");
    if (r == SV_CONFLICT)
        say("The seat is taken.");
    return r == SV_STATE || r == SV_CONFLICT ? SV_OK : r;
}

static SvResult t_seat(void)
{
    SvResult r = t_car();
    if (r == SV_OK)
        say("Type /test go to sit in the car.");
    return r;
}

/* A Magnum with one round in the magazine. */
static SvResult t_ammo(void)
{
    SvValue v;
    SvResult r = t_magnum();
    if (r != SV_OK)
        return r;
    v.i = 1;
    return props->set(self, prop_id("item.ammo"), last_magazine, 0, v);
}

/* The bot gets a new name for each go. */
static SvResult t_rename_go(void)
{
    static const char *const names[] = { "Boss", "Cop", "Mr Smith", "A very long name of thirty-one", "" };
    const char *name = names[name_step++ % 5];
    SvResult r = change->set_name(self, last_npc, name);
    sayf("The bot name is now '%s'. Type /test go for the next name.", name);
    return r;
}

static SvResult t_rename(void)
{
    SvResult r = t_npc();
    name_step = 0;
    if (r == SV_OK)
        say("The bot beside you is named Guard. Look at its name, then type /test go.");
    return r;
}

/* The bot talks above its head. */
static SvResult t_speak_go(void)
{
    SvResult r = change->speak(self, last_npc_human, "Hello, I am a bot.", 1);
    say("The bot spoke. Type /test go to hear it again.");
    return r;
}

static SvResult t_speak(void)
{
    SvResult r = t_npc();
    if (r == SV_OK)
        say("Look at the bot, then type /test go.");
    return r;
}

/* A tracer and a spark. The shot is only a picture: no bullet flies and nothing takes damage. */
static SvResult t_tracer_go(void)
{
    SvPlayerState player = { sizeof player };
    SvHumanState human = { sizeof human };
    SvBoneState head = { sizeof head };
    SvLineHit hit = { sizeof hit };
    SvVector from, speed;
    SvResult r = body(&player, &human);
    if (r != SV_OK)
        return r;
    r = game->bone(self, player.human, 2, &head);
    if (r != SV_OK)
        return r;
    /* The head faces the negative third row of its rotation. */
    speed.x = -head.rotation[2].x * 14.0f;
    speed.y = -head.rotation[2].y * 14.0f;
    speed.z = -head.rotation[2].z * 14.0f;
    from = offset(head.position, speed.x * 0.05f, speed.y * 0.05f, speed.z * 0.05f);
    r = change->bullet(self, 3, from, speed, last_gun);
    if (r != SV_OK)
        return r;
    if (game->aim(self, player.human, 300.0f, SV_LINE_ALL, &hit) == SV_OK && hit.kind != SV_HIT_NONE) {
        static const uint32_t effect[] = { 0, 0, 2, 1 };
        r = change->bullet_hit(self, effect[hit.kind], hit.position, hit.normal);
    }
    say("A tracer flew where you look. Type /test go to fire another.");
    return r;
}

static SvResult t_tracer(void)
{
    SvResult r = t_magnum();
    if (r == SV_OK)
        say("Type /test go to fire a tracer along your aim. It does no damage.");
    return r;
}

/* The Magnum shoots twice as fast and its bullets weigh three times as much, until the next test. */
static SvResult t_strong(void)
{
    SvValue speed = { 0 }, mass = { 0 };
    SvResult r = t_npc();
    if (r != SV_OK)
        return r;
    r = t_magnum();
    if (r != SV_OK)
        return r;
    r = props->get(self, prop_id("item_type.bullet_speed"), 0, 4, &speed);
    if (r == SV_OK)
        r = props->get(self, prop_id("bullet_type.mass"), 0, 3, &mass);
    if (r != SV_OK)
        return r;
    saved_speed = speed.f;
    saved_bullet_mass = mass.f;
    touched |= T_TYPES;
    speed.f *= 2.0f;
    mass.f *= 3.0f;
    r = props->set(self, prop_id("item_type.bullet_speed"), 0, 4, speed);
    if (r == SV_OK)
        r = props->set(self, prop_id("bullet_type.mass"), 0, 3, mass);
    sayf("Magnum speed %.2f -> %.2f, bullet mass %.4f -> %.4f.", (double)saved_speed, (double)speed.f,
        (double)saved_bullet_mass, (double)mass.f);
    return r;
}

/* A bomb at the bot: humans within 12 units lose 30 health and fly up. */
static SvResult t_bomb_go(void)
{
    SvHumanState bot = { sizeof bot };
    SvEntity found[32];
    float distance[32];
    SvValue hurt, push;
    uint32_t count = 0, i, bone, hurt_count = 0;
    SvResult r = game->human(self, last_npc_human, &bot);
    if (r != SV_OK)
        return r;
    r = game->within(self, SV_ENTITY_HUMAN, bot.position, 12.0f, found, distance, 32, &count);
    if (r != SV_OK && r != SV_LIMIT)
        return r;
    hurt.i = -30;
    push.v.x = 0.0f;
    push.v.y = 0.4f;
    push.v.z = 0.0f;
    for (i = 0; i < count; ++i) {
        if (props->add(self, prop_id("human.health"), found[i], 0, hurt) != SV_OK)
            continue;
        ++hurt_count;
        for (bone = 0; bone < 15; ++bone)
            props->add(self, prop_id("human.bone_velocity"), found[i], bone, push);
    }
    change->sound(self, 21, bot.position, 2.0f, 0.5f);
    change->bullet_hit(self, 0, bot.position, (SvVector){ 0.0f, 1.0f, 0.0f });
    sayf("Boom. %u of %u humans near the bot were hurt. Type /test go for another one.", hurt_count, count);
    return SV_OK;
}

static SvResult t_bomb(void)
{
    SvResult r = t_npc();
    if (r == SV_OK)
        say("Type /test go to blow up the bot, and anyone within 12 units.");
    return r;
}

/* The chat decision: while this is on, the text that the tester types gets a prefix. */
static SvResult SV_CALL on_event(SvOwner owner, const SvEvent *event)
{
    (void)owner;
    if (notices_on && event->size == sizeof(SvGameEvent)) {
        const SvGameEvent *record = (const SvGameEvent *)event;
        if (event->kind == SV_EVENT_ROUND)
            sayf("[round] state %d, round number %d.", (int)record->value, (int)record->extra);
        else if (record->player == tester && event->kind == SV_EVENT_VEHICLE)
            sayf("[vehicle] you %s, seat %d.", record->extra ? "got in" : "got out", (int)record->value);
        else if (record->player == tester && event->kind == SV_EVENT_SHOT)
            sayf("[shot] item type %d, %d rounds left.", (int)record->value, (int)record->extra);
        else if (record->player == tester && event->kind == SV_EVENT_ITEM)
            sayf("[item] %s slot %d.", record->extra ? "into" : "out of", (int)record->value);
        else if (event->kind == SV_EVENT_OBJECT_GONE)
            sayf("[vehicle gone] type %d.", (int)record->value);
    }
    if (event->kind == SV_EVENT_PHONE_CALL && event->size == sizeof(SvPhoneEvent) && phone_claim_on) {
        const SvPhoneEvent *call = (const SvPhoneEvent *)event;
        if (call->number == 1234) {
            sayf("[phone] The plugin took your call to %d. The phone does not ring.", (int)call->number);
            return SV_DENIED;
        }
    }
    if (event->kind == SV_EVENT_CHAT && event->size == sizeof(SvChatEvent) && chat_prefix_on) {
        SvChatEvent *chat = (SvChatEvent *)event;
        if (chat->player == tester) {
            char text[64];
            snprintf(text, sizeof text, "[TEST] %.55s", chat->text);
            memcpy(chat->replacement, text, sizeof chat->replacement);
        }
    }
    return SV_OK;
}

static SvResult t_chat_prefix_go(void)
{
    chat_prefix_on = !chat_prefix_on;
    sayf("The chat prefix is now %s. Type a text in the chat, then /test go to switch it.", chat_prefix_on ? "on" : "off");
    return SV_OK;
}

static SvResult t_chat_prefix(void)
{
    chat_prefix_on = 0;
    say("Type /test go to switch on a prefix for your chat texts.");
    return SV_OK;
}

static SvResult t_notices_go(void)
{
    notices_on = !notices_on;
    sayf("Event lines are now %s. Type /test go to switch them.", notices_on ? "on" : "off");
    return SV_OK;
}

static SvResult t_notices(void)
{
    SvResult r = t_car();
    if (r == SV_OK)
        r = t_magnum();
    notices_on = 0;
    if (r == SV_OK)
        say("Type /test go. Then get in the car and out, fire the Magnum, and drop and pick up items.");
    return r;
}

static SvResult t_dealt(void)
{
    SvResult r = t_npc();
    if (r != SV_OK)
        return r;
    r = t_magnum();
    if (r != SV_OK)
        return r;
    {
        SvPlayerState player = { sizeof player };
        SvHumanState human = { sizeof human };
        r = body(&player, &human);
        if (r != SV_OK)
            return r;
        touched |= T_DEALT;
        return set_float("human.damage_dealt_scale", player.human, 0.0f);
    }
}

static SvResult t_collapse(void)
{
    SvPlayerState player = { sizeof player };
    SvHumanState human = { sizeof human };
    SvResult r = body(&player, &human);
    if (r != SV_OK)
        return r;
    touched |= T_DEATH | T_HEALTH;
    r = set_engine_int("human.death_start", 60);
    return r == SV_OK ? set_int("human.health", player.human, 50) : r;
}

static SvResult t_ai_walk(void)
{
    SvPlayerState player = { sizeof player };
    SvHumanState human = { sizeof human };
    SvResult r = ai ? t_npc() : SV_INCOMPATIBLE;
    if (r == SV_OK)
        r = body(&player, &human);
    if (r != SV_OK)
        return r;
    touched |= T_AI;
    return ai->destination(self, last_npc, beside(&human, 25.0f, 0.0f, 0.0f));
}

static SvResult t_ai_walk_go(void)
{
    SvAiInput input = { sizeof input };
    input.yaw = 1.5f;
    if (!ai)
        return SV_INCOMPATIBLE;
    say("The bot should stop and face the side for a few seconds.");
    return ai->override_input(self, last_npc, &input, 300);
}

static SvResult t_phone(void)
{
    SvResult r = host->subscribe(self, EVENTS_BASE | SV_EVENT_PHONE_CALL, on_event);
    if (r != SV_OK)
        return r;
    touched |= T_PHONE;
    phone_claim_on = 1;
    say("Pick up a phone and dial 1234. Then dial another 4 digit number.");
    return SV_OK;
}

static SvResult t_storage(void)
{
    SvStorageKey key = { sizeof key, SV_STORAGE_SERVER };
    SvStorageValue value = { sizeof value };
    uint32_t runs = 0;
    SvResult r;
    if (!storage)
        return SV_INCOMPATIBLE;
    snprintf(key.name, sizeof key.name, "runs");
    r = storage->get(self, &key, &value);
    if (r == SV_OK && value.length == sizeof runs)
        memcpy(&runs, value.bytes, sizeof runs);
    else if (r != SV_OK && r != SV_NOT_FOUND)
        return r;
    ++runs;
    value.length = sizeof runs;
    memcpy(value.bytes, &runs, sizeof runs);
    r = storage->put(self, &key, &value);
    if (r == SV_OK)
        sayf("This test ran %u times on this server. Stop the server, start it, run this test again.", runs);
    return r;
}

static const Test tests[] = {
    { "chat_all", "Sends one message to every client.", "The line 'TEST: every player sees this line.' appears in the chat.", t_chat_all, NULL },
    { "chat_private", "Sends a message to you only.", "You see the line. Other players would not.", t_chat_private, NULL },
    { "chat_admin", "Sends a message of the admin kind.", "You see the line, because you are an administrator.", t_chat_admin, NULL },
    { "sound", "Plays a game sound at your place. Type /test go for the next sound.", "You hear a sound for each go. Different numbers sound different.", t_sound, t_sound_go },
    { "headline_replace", "Replaces news line 0 and moves you next to a newspaper.", "Pick up the newspaper and read it. Its first line is the test text.", t_head_replace, NULL },
    { "headline_add", "Adds a new news line at the end.", "In the newspaper the last line is the test text. Say no if the client shows no new line.", t_head_add, NULL },
    { "teleport", "Moves your body 25 units to the side.", "You jump sideways about 25 units and keep standing.", t_teleport, NULL },
    { "health", "Sets your health to 50.", "Your health bar drops to half. The next test sets it back to 100.", t_health, NULL },
    { "bleeding", "Makes your body bleed.", "You bleed (blood) until the next test.", t_bleed, NULL },
    { "stun", "Stuns your body for 5 seconds.", "Your body staggers and weakens for about 5 seconds.", t_stun, NULL },
    { "push", "Gives every bone of your body an upward speed.", "You are thrown into the air.", t_push, NULL },
    { "item_in_hand", "Makes a bandage and puts it in your hand.", "You hold a bandage.", t_item_hand, NULL },
    { "magnum", "Makes a Magnum with a magazine and puts it in your hand.", "You hold a loaded Magnum. Shoot it.", t_magnum, NULL },
    { "car", "Makes a car 8 units beside you.", "A car (color 1) stands beside you.", t_car, NULL },
    { "npc", "Makes a bot named Guard 4 units beside you.", "A bot stands beside you. It does not walk.", t_npc, NULL },
    { "vehicle_exit", "Makes a car. You get in. Then /test go throws you out of it.", "You leave the car at once, without flying away.", t_exit, t_exit_go },
    { "aim", "Reads what your aim hits. Type /test go after you look somewhere else.", "The answer matches what you look at: ground, a car or a human.", t_aim, t_aim_go },
    { "looks", "Sets your five look settings to 0, then raises one for each /test go.", "Each go changes one part of your look. Say yes if every part changes something. The next test puts your looks back.", t_looks, t_looks_go },
    { "car_teleport", "Makes a car beside you. Type /test go to move it 10 units each time.", "The car jumps 10 units at once, without flying or breaking.", t_car_move, t_car_move_go },
    { "vehicle_enter", "Makes a car beside you. Type /test go to sit in its driver seat.", "You sit in the driver seat at once. Try to drive.", t_seat, t_seat_go },
    { "one_round", "Makes a Magnum with one round in the magazine and puts it in your hand.", "You fire one shot, and the next trigger pull does nothing.", t_ammo, NULL },
    { "npc_name", "Makes a bot named Guard. Type /test go to give it a new name.", "The name above the bot changes at once for each go. The last name is empty.", t_rename, t_rename_go },
    { "npc_speak", "Makes a bot. Type /test go to make it talk.", "A speech bubble with the text shows above the bot.", t_speak, t_speak_go },
    { "tracer", "Makes a loaded Magnum. Type /test go to fire a picture of a bullet along your aim.", "A tracer flies where you look, with a spark where it lands. Nothing takes damage.", t_tracer, t_tracer_go },
    { "strong_magnum", "Makes a bot and a Magnum, and makes the Magnum bullets fast and heavy.", "A few shots kill the bot. The next test puts the Magnum back.", t_strong, NULL },
    { "bomb", "Makes a bot. Type /test go to blow up everything within 12 units of it.", "The bot, and you if you stand near, lose 30 health and fly up. A sound and a spark play.", t_bomb, t_bomb_go },
    { "chat_prefix", "Switches on a prefix for your chat texts with /test go. A plugin rewrites each text before it goes out.", "Your next chat text appears as '[TEST] ' and then your text. After a second go it is plain again.", t_chat_prefix, t_chat_prefix_go },
    { "notice_events", "Makes a car and a loaded Magnum. Type /test go to see a chat line for every game event of yours.", "Lines appear for a seat in the car, each shot, each item that goes into or out of a slot, and a change of the round.", t_notices, t_notices_go },
    { "damage_dealt_zero", "Makes a bot and a Magnum for you, and sets your damage to 0.", "Shoot the bot. It takes no damage.", t_dealt, NULL },
    { "collapse_threshold", "Sets the death start to 60 and your health to 50.", "Your body falls limp at once, although the health bar shows half. The next test puts both back.", t_collapse, NULL },
    { "ai_walk", "Makes a bot and sends it 25 units away. Type /test go to make it stop and turn.", "The bot walks to a point beside you. After a go it stops and faces the side for a few seconds.", t_ai_walk, t_ai_walk_go },
    { "phone_claim", "Makes the plugin take a call to the number 1234 on any phone.", "Dial 1234: a chat line says the plugin took the call and the phone does not ring. Other numbers dial as before.", t_phone, NULL },
    { "storage_counter", "Stores a counter on the server and says its value.", "The number grows by 1 each run. After a server restart it keeps growing.", t_storage, NULL },
};

/* ------------------------------------------------------------ flow */

static const char *const result_name[] = { "todo", "PASS", "FAIL", "SKIP" };

static void report(void)
{
    int counts[4] = { 0, 0, 0, 0 }, i;
    char line[LINE_MAX_CHARS + 4];
    for (i = 1; i <= test_count; ++i)
        ++counts[result[i]];
    sayf("Test room: %d pass, %d fail, %d skipped, %d not run.", counts[PASS], counts[FAIL], counts[SKIP], counts[TODO]);
    line[0] = 0;
    for (i = 1; i <= test_count; ++i)
        if (result[i] == FAIL) {
            char item[32];
            snprintf(item, sizeof item, "%d %s  ", i, tests[i - 1].name);
            if (strlen(line) + strlen(item) > LINE_MAX_CHARS) {
                say(line);
                line[0] = 0;
            }
            strcat(line, item);
        }
    if (line[0]) {
        say("Failed:");
        say(line);
    }
    logf_("TESTROOM report: %d pass, %d fail, %d skipped, %d not run", counts[PASS], counts[FAIL], counts[SKIP], counts[TODO]);
}

static void start_test(int number);

static void mark(int outcome, SvResult code)
{
    if (!current)
        return;
    result[current] = (unsigned char)outcome;
    logf_("TESTROOM %d %s %s code=%d", current, tests[current - 1].name, result_name[outcome], (int)code);
}

static void next_test(void)
{
    if (current < test_count)
        start_test(current + 1);
    else {
        cleanup();
        current = 0;
        say("All tests done.");
        report();
    }
}

static void start_test(int number)
{
    const Test *test = &tests[number - 1];
    SvResult r;
    cleanup();
    current = number;
    sayf("[%d/%d] %s", number, test_count, test->name);
    sayf("Does: %s", test->what);
    sayf("Expect: %s", test->expect);
    r = test->run();
    if (r != SV_OK) {
        sayf("The API answered error %d, so this test fails.", (int)r);
        mark(FAIL, r);
        next_test();
        return;
    }
    say(test->go ? "Then /test yes, /test no or /test skip." : "Type /test yes, /test no or /test skip.");
}

static int is_number(const char *text)
{
    if (!*text)
        return 0;
    while (*text)
        if (*text < '0' || *text > '9')
            return 0;
        else
            ++text;
    return 1;
}

static void list(void)
{
    char line[LINE_MAX_CHARS + 4];
    int i;
    line[0] = 0;
    for (i = 1; i <= test_count; ++i) {
        char item[40];
        snprintf(item, sizeof item, "%d %s %c  ", i, tests[i - 1].name, "-+xs"[result[i]]);
        if (strlen(line) + strlen(item) > LINE_MAX_CHARS) {
            say(line);
            line[0] = 0;
        }
        strcat(line, item);
    }
    if (line[0])
        say(line);
    say("(- not run, + pass, x fail, s skipped)");
}

static SvResult SV_CALL test_command(SvOwner owner, uint64_t actor, const char *arguments)
{
    char word[32];
    size_t length = 0;
    (void)owner;
    while (*arguments == ' ')
        ++arguments;
    while (arguments[length] && arguments[length] != ' ' && length < sizeof word - 1) {
        word[length] = arguments[length];
        ++length;
    }
    word[length] = 0;
    if (!actor)
        return host->log(self, "The test room runs from the chat of an administrator.");
    if (current && tester && actor != tester && strcmp(word, "start")) {
        change->message_to(self, SV_MESSAGE_SERVER, actor, "Another administrator runs the test room.");
        return SV_OK;
    }
    if (!strcmp(word, "start")) {
        cleanup();
        tester = actor;
        memset(result, 0, sizeof result);
        say("Test room started. Answer each test with /test yes, no or skip.");
        say("Other commands: /test go, /test <number>, list, report, stop.");
        start_test(1);
        return SV_OK;
    }
    tester = actor;
    if (!strcmp(word, "yes") || !strcmp(word, "no") || !strcmp(word, "skip")) {
        if (!current) {
            say("No test runs. Type /test start.");
            return SV_OK;
        }
        mark(word[0] == 'y' ? PASS : word[0] == 'n' ? FAIL : SKIP, SV_OK);
        next_test();
    } else if (!strcmp(word, "go")) {
        if (!current || !tests[current - 1].go) {
            say("This test has nothing to repeat.");
            return SV_OK;
        }
        {
            SvResult r = tests[current - 1].go();
            if (r != SV_OK) {
                sayf("The API answered error %d, so this test fails.", (int)r);
                mark(FAIL, r);
                next_test();
            }
        }
    } else if (is_number(word)) {
        int number = atoi(word);
        if (number < 1 || number > test_count)
            sayf("Test numbers go from 1 to %d.", test_count);
        else
            start_test(number);
    } else if (!strcmp(word, "list")) {
        list();
    } else if (!strcmp(word, "report")) {
        report();
    } else if (!strcmp(word, "stop")) {
        cleanup();
        current = 0;
        say("Test room stopped.");
        report();
    } else {
        say("Commands: /test start, yes, no, skip, go, <number>, list, report, stop.");
    }
    return SV_OK;
}

static SvResult SV_CALL enable(const SvHost *h, SvOwner owner)
{
    SvResult r;
    if (h->abi != SV_PLUGIN_ABI || h->size < SV_HOST_V1_SIZE)
        return SV_INCOMPATIBLE;
    host = h;
    self = owner;
    test_count = (int)(sizeof tests / sizeof tests[0]);
    if (test_count > MAX_TESTS)
        return SV_FAILED;
    r = h->get_api(owner, SV_API_GAME_READ, SV_GAME_READ_VERSION, (const void **)&game);
    if (r == SV_OK)
        r = h->get_api(owner, SV_API_GAME_WRITE, SV_GAME_WRITE_VERSION, (const void **)&change);
    if (r == SV_OK)
        r = h->get_api(owner, SV_API_GAME_PROPS, SV_GAME_PROPS_VERSION, (const void **)&props);
    if (r != SV_OK)
        return r;
    h->get_api(owner, SV_API_AI, SV_GAME_AI_VERSION, (const void **)&ai);
    h->get_api(owner, SV_API_ENGINE, SV_GAME_ENGINE_VERSION, (const void **)&engine);
    h->get_api(owner, SV_API_STORAGE, SV_GAME_STORAGE_VERSION, (const void **)&storage);
    r = h->subscribe(owner, EVENTS_BASE, on_event);
    if (r != SV_OK)
        return r;
    return h->command(owner, "test", SV_COMMAND_ADMIN, test_command);
}

SV_EXPORT SvResult SV_CALL sv_plugin_entry(uint32_t abi, SvPlugin *plugin)
{
    if (abi != SV_PLUGIN_ABI || !plugin || plugin->size < sizeof *plugin)
        return SV_INCOMPATIBLE;
    memset(plugin, 0, sizeof *plugin);
    plugin->size = sizeof *plugin;
    plugin->abi = SV_PLUGIN_ABI;
    strcpy(plugin->id, "sv.testroom");
    strcpy(plugin->version, VERSION);
    plugin->flags = SV_PLUGIN_LIVE;
    plugin->enable = enable;
    return SV_OK;
}
