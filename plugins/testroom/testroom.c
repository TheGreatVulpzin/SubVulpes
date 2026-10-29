/* The test room. An administrator types /test start in the chat. The plugin runs one test after another. It says what it
 * does and what the administrator should see, then waits for /test yes or /test no. It uses only the public plugin API, so
 * a failed test points at the API or at the game, and not at a private shortcut. The results go to the server log. */
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
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
#include "sv_game_economy.h"
#include "sv_game_mission.h"
#include "sv_game_settings.h"
#include "sv_config.h"
#include "sv_plugins_api.h"

#define VERSION "0.2.8"
#define LINE_MAX_CHARS 60
#define MAX_TESTS 64

static const SvHost *host;
static SvOwner self;
static const SvGameRead *game;
static const SvGameWrite *change;
static const SvGameProps *props;
static const SvGameAi *ai;
static const SvGameEngine *engine;
static const SvGameStorage *storage;
static const SvGameEconomy *economy;
static const SvGameMission *mission;
static const SvGameSettings *settings;
static const SvGameShop *shop;
static const SvConfig *config;
static const SvPlugins *plugins_api;

enum { TODO, PASS, FAIL, SKIP };
/* What a test changed on the player, so the next test or /test stop can undo it. */
enum { T_HEALTH = 1, T_BLEED = 2, T_STUN = 4, T_DEALT = 8, T_LOOKS = 16, T_TYPES = 32, T_DEATH = 64, T_PHONE = 128, T_AI = 256, T_MONEY = 512, T_ECON = 1024, T_MISSION = 2048, T_SHOP = 4096,
    T_SETTING = 8192 };
#define EVENTS_BASE (SV_EVENT_TICK | SV_EVENT_CHAT | SV_EVENT_ROUND | SV_EVENT_VEHICLE | SV_EVENT_SHOT | SV_EVENT_OBJECT_GONE | SV_EVENT_ITEM |\
    SV_EVENT_SETTING | SV_EVENT_CONFIG)

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
static SvEntity made_objects[6], made_players[4], made_items[24];
static int made_object_count, made_player_count, made_item_count;
static uint32_t sound_next;
static SvEntity last_magazine, last_car, last_npc, last_npc_human, last_gun;
static float saved_speed, saved_bullet_mass;
static unsigned name_step;
static int chat_prefix_on, notices_on, phone_claim_on, setting_notice_on;
static SvEntity money_account, stock_player, phone_item;
static int32_t money_added, saved_shares, stocks_added, saved_bots;
static float saved_price;
static SvMissionRef mission_ref;
static int mission_live;
static struct { uint32_t building; SvShopCatalog old; } saved_shop[32];
static int saved_shop_count, duel_stage;
static SvEntity duel_a, duel_b;
/* A timed show, driven by the tick event: a scale of sounds, a grid of moving items or a newspaper that changes. */
enum { ANIM_NONE, ANIM_SCALE, ANIM_GRID, ANIM_FRAMES, ANIM_TOUR, ANIM_OCTAVES };
#define GRID_ITEMS 16
static int anim_kind, grid_count;
static uint32_t anim_next, anim_step, anim_every, anim_limit;
static SvEntity grid_items[GRID_ITEMS];
static uint32_t grid_skipped;
static SvVector grid_origin;

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
        {
            char logged[LINE_MAX_CHARS + 8];
            snprintf(logged, sizeof logged, "SAY %s", line);
            host->log(self, logged);
        }
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

/* Puts an item in a hand of a human: the right hand first, then the left one. */
static SvResult equip_hand(SvEntity item, SvEntity human)
{
    SvResult r = change->item_equip(self, item, human, 0);
    return r == SV_CONFLICT ? change->item_equip(self, item, human, 1) : r;
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
static void animate(uint32_t tick);

/* Put back what the economy, mission, shop, setting and money tests changed. */
static void undo_services(void)
{
    int i;
    if ((touched & T_MONEY) && money_added)
        change->add_money(self, money_account, -money_added);
    if ((touched & T_ECON) && economy) {
        if (stocks_added)
            economy->add_stocks(self, stock_player, -stocks_added);
        economy->set_shares(self, 0, saved_shares);
        economy->set_price(self, 0, saved_price);
    }
    if ((touched & T_MISSION) && mission_live && mission)
        mission->cancel(self, &mission_ref);
    if ((touched & T_SHOP) && shop)
        for (i = 0; i < saved_shop_count; ++i) {
            SvShopCatalogWrite write = { sizeof write };
            write.count = saved_shop[i].old.count;
            memcpy(write.rows, saved_shop[i].old.rows, sizeof write.rows);
            shop->catalog_replace(self, saved_shop[i].building, &write);
        }
    if ((touched & T_SETTING) && settings) {
        SvSettingValue value = { sizeof value, SV_CORE_SETTING_INT };
        value.value = saved_bots;
        settings->set(self, "bot_count", &value);
    }
    money_added = stocks_added = saved_shop_count = mission_live = setting_notice_on = 0;
}

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
    anim_kind = ANIM_NONE;
    grid_count = 0;
    undo_services();
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
    return equip_hand(item, player.human);
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
    return equip_hand(gun, player.human);
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
    if (anim_kind && event->kind == SV_EVENT_TICK)
        animate(event->tick);
    if (tester && event->kind == SV_EVENT_SETTING && event->size == sizeof(SvSettingEvent) && setting_notice_on) {
        const SvSettingEvent *changed = (const SvSettingEvent *)event;
        sayf("[setting] %s went from %d to %d.", changed->name, (int)changed->before, (int)changed->after);
    }
    if (tester && event->kind == SV_EVENT_CONFIG && event->size == sizeof(SvConfigEvent)) {
        const SvConfigEvent *changed = (const SvConfigEvent *)event;
        sayf("[config] %s: '%.20s' to '%.20s'.", changed->key, changed->old_value, changed->new_value);
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
    input.yaw = 3.0f;
    if (!ai)
        return SV_INCOMPATIBLE;
    say("The bot should stop and turn about 90 degrees from its path for a few seconds.");
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

static SvResult t_storage_erase(void)
{
    SvStorageKey key = { sizeof key, SV_STORAGE_SERVER };
    SvResult r;
    if (!storage)
        return SV_INCOMPATIBLE;
    snprintf(key.name, sizeof key.name, "runs");
    r = storage->erase(self, &key);
    say(r == SV_OK ? "Erased. Run this test again: the count starts at 1." : "Nothing to erase. Run this test first.");
    return r == SV_NOT_FOUND ? SV_OK : r;
}

/* ------------------------------------------------------------ reads */

static SvResult t_read_account(void)
{
    SvPlayerState player = { sizeof player };
    SvAccountState account = { sizeof account };
    SvResult r = game->player(self, tester, &player);
    if (r == SV_OK)
        r = game->account(self, player.account, &account);
    if (r == SV_OK)
        sayf("Account %.20s: money %d, play time %d.", account.name, (int)account.money, (int)account.playtime_raw);
    return r;
}

static SvResult t_read_inventory(void)
{
    SvPlayerState player = { sizeof player };
    SvHumanState human = { sizeof human };
    char line[LINE_MAX_CHARS];
    uint32_t slot, used = 0;
    SvResult r = body(&player, &human);
    if (r != SV_OK)
        return r;
    for (slot = 0; slot < 7 && used + 12 < sizeof line; ++slot) {
        SvInventoryState inventory = { sizeof inventory };
        r = game->inventory(self, player.human, slot, &inventory);
        if (r != SV_OK)
            return r;
        used += (uint32_t)snprintf(line + used, sizeof line - used, "s%u:%u ", slot, inventory.count);
    }
    say("Items in each slot (0 and 1 are the hands):");
    say(line);
    return SV_OK;
}

static SvResult t_read_line(void)
{
    SvPlayerState player = { sizeof player };
    SvHumanState human = { sizeof human };
    SvLineHit hit = { sizeof hit };
    SvResult r = body(&player, &human);
    if (r != SV_OK)
        return r;
    r = game->line(self, offset(human.position, 0.0f, 6.0f, 0.0f), offset(human.position, 0.0f, -6.0f, 0.0f), SV_LINE_ALL,
        player.human, &hit);
    if (r == SV_OK)
        sayf("Line down through you: kind %u, hit height %.2f, you stand at %.2f.", hit.kind, hit.position.y, human.position.y);
    return r;
}

static SvResult t_read_headline(void)
{
    SvWorldState world = { sizeof world };
    char text[SV_HEADLINE_BUFFER];
    SvResult r = game->world(self, &world);
    if (r == SV_OK)
        r = game->headline(self, 0, text, sizeof text);
    if (r == SV_OK) {
        sayf("The world has %u news lines. Line 0:", world.headline_count);
        say(text);
    }
    return r;
}

static SvResult t_props_list(void)
{
    SvPropInfo info = { sizeof info };
    uint32_t id = 1;
    char last[SV_PROP_NAME_MAX] = "";
    while (id < 512 && props->info(self, id, &info) == SV_OK) {
        snprintf(last, sizeof last, "%s", info.name);
        ++id;
        info.size = sizeof info;
    }
    sayf("%u properties are listed. The last one is %s.", id - 1, last);
    return id > 1 ? SV_OK : SV_NOT_FOUND;
}

/* ------------------------------------------------------------ writes */

static SvResult t_add_money(void)
{
    SvPlayerState player = { sizeof player };
    SvResult r = game->player(self, tester, &player);
    if (r != SV_OK)
        return r;
    r = change->add_money(self, player.account, 100);
    if (r == SV_OK) {
        touched |= T_MONEY;
        money_account = player.account;
        money_added += 100;
        sayf("Your money was %d. It should be 100 more now. The next test takes it back.", (int)player.money);
    }
    return r;
}

static SvResult t_disown(void)
{
    SvPlayerState player = { sizeof player };
    SvHumanState human = { sizeof human };
    SvEntity item = 0;
    SvResult r = body(&player, &human);
    if (r != SV_OK)
        return r;
    r = change->item_create(self, 12, beside(&human, 3.0f, 0.5f, 0.0f), &item);
    if (r == SV_OK)
        r = change->disown(self, item);
    return r;
}

/* ------------------------------------------------------------ bots */

static SvResult make_bot(SvVector at, const char *name, SvEntity *bot, SvEntity *bot_human)
{
    SvResult r = change->npc_create(self, at, 0.0f, name, bot, bot_human);
    if (r == SV_OK)
        remember(made_players, &made_player_count, 4, *bot);
    return r;
}

static SvResult arm(SvEntity human, SvVector at)
{
    SvEntity gun = 0, magazine = 0;
    SvResult r = change->item_create(self, 4, at, &gun);
    if (r != SV_OK)
        return r;
    remember(made_items, &made_item_count, 8, gun);
    r = change->item_create(self, 5, at, &magazine);
    if (r != SV_OK)
        return r;
    remember(made_items, &made_item_count, 8, magazine);
    r = change->item_attach(self, gun, magazine);
    return r == SV_OK ? equip_hand(gun, human) : r;
}

static SvResult t_threats_go(void)
{
    SvAiThreats threats = { sizeof threats };
    uint32_t i;
    SvResult r = ai ? ai->threats(self, last_npc, &threats) : SV_INCOMPATIBLE;
    if (r != SV_OK)
        return r;
    sayf("The bot knows %u threats.", threats.count);
    for (i = 0; i < threats.count && i < 3; ++i)
        sayf("Threat %u: level %.1f, timer %d.", i, threats.threats[i].level, (int)threats.threats[i].timer);
    return SV_OK;
}

static SvResult t_threats(void)
{
    SvResult r = ai ? t_npc() : SV_INCOMPATIBLE;
    if (r == SV_OK)
        say("Walk in front of the bot and wave. Then type /test go to read what it sees.");
    return r;
}

static SvResult t_waypoints(void)
{
    SvPlayerState player = { sizeof player };
    SvHumanState human = { sizeof human };
    SvVector points[3];
    SvResult r = ai ? t_npc() : SV_INCOMPATIBLE;
    if (r == SV_OK)
        r = body(&player, &human);
    if (r != SV_OK)
        return r;
    points[0] = beside(&human, 15.0f, 0.0f, 0.0f);
    points[1] = beside(&human, 15.0f, 0.0f, 15.0f);
    points[2] = beside(&human, 0.0f, 0.0f, 15.0f);
    touched |= T_AI;
    return ai->waypoints(self, last_npc, points, 3);
}

static SvResult t_waypoints_go(void)
{
    return ai ? ai->waypoints(self, last_npc, NULL, 0) : SV_INCOMPATIBLE;
}

static SvResult t_target(void)
{
    SvPlayerState player = { sizeof player };
    SvHumanState human = { sizeof human };
    SvResult r = ai ? t_npc() : SV_INCOMPATIBLE;
    if (r == SV_OK)
        r = body(&player, &human);
    if (r != SV_OK)
        return r;
    touched |= T_AI;
    return ai->target(self, last_npc, player.human);
}

static SvResult duel_apply(void)
{
    static const uint32_t chance[3] = { 100, 0, 100 };
    static const uint32_t part[3] = { 2, 2, 1 };
    static const char *const text[3] = { "Stage 0: the first bot shoots the second bot, aiming at the head.",
        "Stage 1: the shot chance is 0. The first bot stops shooting.",
        "Stage 2: the chance is 100 again and the bot aims at the torso, without the aim bit." };
    SvResult r = ai->shot_chance(self, duel_a, chance[duel_stage]);
    if (r == SV_OK)
        r = ai->perception(self, duel_a, part[duel_stage]);
    if (r == SV_OK)
        r = ai->aim_mode(self, duel_a, duel_stage < 2);
    if (r == SV_OK)
        say(text[duel_stage]);
    return r;
}

static SvResult t_duel_go(void)
{
    duel_stage = (duel_stage + 1) % 3;
    return ai ? duel_apply() : SV_INCOMPATIBLE;
}

static SvResult t_duel(void)
{
    SvPlayerState player = { sizeof player };
    SvHumanState human = { sizeof human };
    SvEntity a_human = 0, b_human = 0;
    SvResult r = ai ? body(&player, &human) : SV_INCOMPATIBLE;
    if (r == SV_OK)
        r = make_bot(beside(&human, 6.0f, 0.5f, 0.0f), "Shooter", &duel_a, &a_human);
    if (r == SV_OK)
        r = make_bot(beside(&human, 6.0f, 0.5f, 14.0f), "Target", &duel_b, &b_human);
    if (r == SV_OK)
        r = arm(a_human, human.position);
    if (r != SV_OK)
        return r;
    last_npc = duel_a;
    touched |= T_AI;
    duel_stage = 0;
    r = ai->target(self, duel_a, b_human);
    return r == SV_OK ? duel_apply() : r;
}

static SvResult t_seated(void)
{
    SvPlayerState player = { sizeof player };
    SvHumanState human = { sizeof human };
    SvEntity car = 0, a_human = 0, b_human = 0, a_bot = 0, b_bot = 0;
    SvResult r = ai ? body(&player, &human) : SV_INCOMPATIBLE;
    if (r == SV_OK)
        r = change->object_create(self, 0, beside(&human, 8.0f, 1.0f, 0.0f), 0.0f, 1, &car);
    if (r == SV_OK) {
        remember(made_objects, &made_object_count, 6, car);
        r = make_bot(beside(&human, 8.0f, 1.0f, 3.0f), "Passenger", &a_bot, &a_human);
    }
    if (r == SV_OK)
        r = change->vehicle_enter(self, a_human, car, 1);
    if (r == SV_OK)
        r = arm(a_human, human.position);
    if (r == SV_OK)
        r = make_bot(beside(&human, -14.0f, 0.5f, 0.0f), "Target", &b_bot, &b_human);
    if (r != SV_OK)
        return r;
    last_npc = a_bot;
    touched |= T_AI;
    return ai->seated_target(self, a_bot, b_bot);
}

/* ------------------------------------------------------------ economy, missions, shops */

static SvResult t_economy_read(void)
{
    uint32_t company;
    if (!economy)
        return SV_INCOMPATIBLE;
    for (company = 0; company < SV_COMPANY_COUNT; ++company) {
        SvCompany info = { sizeof info };
        SvResult r = economy->company(self, company, &info);
        if (r != SV_OK)
            return r;
        sayf("Company %u: price %.2f, shares %d, your stake %d.", company, info.price, (int)info.shares, (int)info.stake);
    }
    return SV_OK;
}

static SvResult t_economy_change(void)
{
    SvCompany info = { sizeof info };
    SvResult r = economy ? economy->company(self, 0, &info) : SV_INCOMPATIBLE;
    if (r != SV_OK)
        return r;
    saved_price = info.price;
    saved_shares = info.shares;
    touched |= T_ECON;
    r = economy->set_price(self, 0, 123.0f);
    if (r == SV_OK)
        say("The price of company 0 is now 123. Look at the stock list. Then type /test go.");
    return r;
}

static SvResult t_economy_change_go(void)
{
    SvCompany info = { sizeof info };
    SvResult r = economy->set_shares(self, 0, saved_shares + 1000);
    if (r == SV_OK) {
        stock_player = tester;
        r = economy->add_stocks(self, tester, 10);
    }
    if (r == SV_OK) {
        stocks_added += 10;
        r = economy->company(self, 0, &info);
    }
    if (r == SV_OK)
        sayf("Company 0 now has %d shares and you have %d of them.", (int)info.shares, (int)info.stake);
    return r;
}

static SvResult t_mission_read(void)
{
    uint32_t team;
    if (!mission)
        return SV_INCOMPATIBLE;
    for (team = 0; team < SV_MISSION_TEAMS; ++team) {
        SvMissionSlots slots = { sizeof slots };
        uint32_t i, active = 0;
        SvResult r = mission->slots(self, team, &slots);
        if (r != SV_OK)
            return r;
        for (i = 0; i < SV_MISSION_SLOTS; ++i)
            active += slots.slots[i].active != 0;
        sayf("Team %u has %u active missions.", team, active);
    }
    return SV_OK;
}

static SvResult t_mission_start(void)
{
    SvPlayerState player = { sizeof player };
    SvHumanState human = { sizeof human };
    SvWorldState world = { sizeof world };
    SvMissionStart start = { sizeof start };
    SvEntity objective = 0;
    uint32_t team;
    SvResult r = mission ? body(&player, &human) : SV_INCOMPATIBLE;
    if (r == SV_OK)
        r = game->world(self, &world);
    if (r == SV_OK)
        r = change->item_create(self, 17, beside(&human, 3.0f, 0.5f, 0.0f), &objective);
    if (r != SV_OK)
        return r;
    remember(made_items, &made_item_count, 8, objective);
    start.kind = 2;
    start.reward = 500;
    start.expire = world.world_clock_raw + 36000;
    if (start.expire > SV_MISSION_EXPIRE_MAX)
        start.expire = SV_MISSION_EXPIRE_MAX;
    start.item = objective;
    team = player.team < 0 || player.team >= (int32_t)SV_MISSION_TEAMS ? 0u : (uint32_t)player.team;
    r = mission->start(self, team, &start, &mission_ref);
    if (r == SV_OK) {
        mission_live = 1;
        touched |= T_MISSION;
        say("A mission of kind 2 with reward 500 was added for your team. Look at the mission list. Then type /test go.");
    }
    return r;
}

static SvResult t_mission_cancel(void)
{
    SvResult r = mission_live ? mission->cancel(self, &mission_ref) : SV_STATE;
    if (r == SV_OK) {
        mission_live = 0;
        say("The mission was cancelled. It should leave the list.");
    }
    return r;
}

static SvResult t_shop_catalog(void)
{
    uint32_t building;
    if (!shop)
        return SV_INCOMPATIBLE;
    saved_shop_count = 0;
    touched |= T_SHOP;
    for (building = 0; building < SV_SHOP_BUILDINGS && saved_shop_count < 32; ++building) {
        SvShopCatalog now = { sizeof now };
        SvShopCatalogWrite write = { sizeof write };
        if (shop->catalog(self, building, &now) != SV_OK || !now.count)
            continue;
        write.count = now.count;
        memcpy(write.rows, now.rows, sizeof write.rows);
        write.rows[0].price = 1;
        saved_shop[saved_shop_count].building = building;
        saved_shop[saved_shop_count].old = now;
        if (shop->catalog_replace(self, building, &write) == SV_OK)
            ++saved_shop_count;
    }
    sayf("%d shops now sell their first row for 1.", saved_shop_count);
    return saved_shop_count ? SV_OK : SV_NOT_FOUND;
}

static SvResult t_phone_state(void)
{
    SvPlayerState player = { sizeof player };
    SvHumanState human = { sizeof human };
    SvEntity phone = 0;
    SvResult r = shop ? body(&player, &human) : SV_INCOMPATIBLE;
    if (r == SV_OK)
        r = change->item_create(self, 23, human.position, &phone);
    if (r == SV_OK) {
        remember(made_items, &made_item_count, 24, phone);
        phone_item = phone;
        r = equip_hand(phone, player.human);
    }
    if (r == SV_OK)
        say("You hold a cell phone. Dial a number, then type /test go to read the phone and end the call.");
    return r;
}

static SvResult t_phone_state_go(void)
{
    SvPhoneState state = { sizeof state };
    SvResult r = shop->phone(self, phone_item, &state);
    if (r != SV_OK)
        return r;
    sayf("Phone state %d (0 idle, 1 dial, 2 ringing, 3 talk), number %d.", (int)state.state, (int)state.number);
    r = shop->phone_idle(self, phone_item);
    say(r == SV_OK ? "The call ended. The phone is idle." : "The phone could not go idle.");
    return r;
}

/* ------------------------------------------------------------ settings, engine, config, plugins */

static SvResult t_setting_list(void)
{
    uint32_t i;
    if (!settings)
        return SV_INCOMPATIBLE;
    for (i = 0; i < SV_SETTING_COUNT; ++i) {
        SvSettingInfo info = { sizeof info };
        SvSettingValue value = { sizeof value };
        SvResult r = settings->describe(self, i, &info);
        if (r == SV_OK)
            r = settings->get(self, info.name, &value);
        if (r != SV_OK)
            return r;
        sayf("%s = %d (range %d to %d, default %d).", info.name, (int)value.value, (int)info.minimum, (int)info.maximum,
            (int)info.default_value);
    }
    return SV_OK;
}

static SvResult t_setting_bots(void)
{
    SvSettingValue value = { sizeof value };
    SvResult r = settings ? settings->get(self, "bot_count", &value) : SV_INCOMPATIBLE;
    if (r != SV_OK)
        return r;
    saved_bots = value.value;
    touched |= T_SETTING;
    setting_notice_on = 1;
    value.type = SV_CORE_SETTING_INT;
    value.value = 2;
    r = settings->set(self, "bot_count", &value);
    if (r == SV_OK)
        sayf("The bot count was %d. It is 2 now. Watch the bots in the city for a minute.", (int)saved_bots);
    return r;
}

static SvResult t_engine_list(void)
{
    uint32_t i;
    if (!engine)
        return SV_INCOMPATIBLE;
    for (i = 0; i < SV_ENGINE_PARAM_COUNT; ++i) {
        SvEngineInfo info = { sizeof info };
        SvEngineValue value = { sizeof value };
        SvResult r = engine->describe(self, i, 0, &info);
        if (r == SV_INVALID)
            r = engine->describe(self, i, SV_ENGINE_GLOBAL, &info);
        if (r == SV_OK)
            r = engine->get(self, info.name, info.scope == SV_ENGINE_GLOBAL_SCOPE ? SV_ENGINE_GLOBAL : 0, &value);
        if (r != SV_OK)
            return r;
        if (value.type == SV_ENGINE_FLOAT)
            sayf("%s = %.3f.", info.name, value.value.real);
        else
            sayf("%s = %d.", info.name, (int)value.value.integer);
    }
    return SV_OK;
}

static SvResult t_config_read(void)
{
    int32_t count = 0;
    float scale = 0.0f;
    char greeting[SV_CONFIG_TEXT_MAX + 1] = "";
    SvResult r = config ? config->get_int(self, "count", &count) : SV_INCOMPATIBLE;
    if (r == SV_OK)
        r = config->get_float(self, "scale", &scale);
    if (r == SV_OK)
        r = config->get_text(self, "greeting", greeting, sizeof greeting);
    if (r == SV_OK) {
        sayf("greeting '%s', count %d, scale %.2f.", greeting, (int)count, scale);
        say("Change one with the plugins config command. A chat line shows the change.");
    }
    return r;
}

static SvResult t_plugins_calls(void)
{
    const void *table = NULL;
    SvResult subscribe, refuse, find;
    if (!plugins_api)
        return SV_INCOMPATIBLE;
    subscribe = host->subscribe(self, EVENTS_BASE | SV_EVENT_EXT_BIT(0), on_event);
    host->subscribe(self, EVENTS_BASE, on_event);
    refuse = plugins_api->refuse(self, "test");
    find = plugins_api->find(self, "sv.testroom", 1, &table);
    sayf("subscribe gave %d (want %d), refuse %d (want %d), find %d (want %d).", (int)subscribe, (int)SV_OK, (int)refuse,
        (int)SV_STATE, (int)find, (int)SV_NOT_FOUND);
    return subscribe == SV_OK && refuse == SV_STATE && find == SV_NOT_FOUND ? SV_OK : SV_FAILED;
}

/* The kick test. The player gives a name: /test kick NAME. The kick puts the address of the player in the kick list of the
 * server, which blocks that address until the server restarts or an administrator types /clearkick. */
static SvResult t_kick_run(void)
{
    say("This test removes another player. Type /test kick NAME with the name of a friend who agreed.");
    say("The list of the server blocks the address of that player. Type /clearkick in the chat when you are done.");
    return SV_OK;
}

/* ------------------------------------------------------------ timed shows (for a music and picture plugin) */

/* One log line for each sound of the sound tests, with the local time to the millisecond. The client probe writes the same
 * time for each sound that it plays, so tools/audio/match_probe.py can match the two logs. */
static void log_sound(uint32_t index, float pitch)
{
    SYSTEMTIME now;
    char text[96];
    GetLocalTime(&now);
    snprintf(text, sizeof text, "TOURSOUND %02u:%02u:%02u.%03u index=%u pitch=%.4f", now.wHour, now.wMinute, now.wSecond,
        now.wMilliseconds, index, pitch);
    host->log(self, text);
}

static void animate(uint32_t tick)
{
    SvPlayerState player = { sizeof player };
    SvHumanState human = { sizeof human };
    SvResult r = SV_OK;
    if (tick < anim_next)
        return;
    anim_next = tick + anim_every;
    if (body(&player, &human) != SV_OK) {
        anim_kind = ANIM_NONE;
        return;
    }
    if (anim_kind == ANIM_SCALE) {
        float pitch = 0.5f * powf(2.0f, (float)anim_step / 12.0f);
        r = change->sound(self, 21, human.position, 1.0f, pitch);
        if (anim_step % 4 == 0)
            sayf("Note %u, pitch %.2f.", anim_step, pitch);
    } else if (anim_kind == ANIM_TOUR) {
        r = change->sound(self, anim_step, human.position, 1.0f, 1.0f);
        sayf("Sound %u.", anim_step);
        log_sound(anim_step, 1.0f);
    } else if (anim_kind == ANIM_OCTAVES) {
        float pitch = 0.125f * (float)(1u << (anim_step % 6));
        r = change->sound(self, 27, human.position, 1.0f, pitch);
        sayf("Pitch %.3f.", pitch);
        log_sound(27, pitch);
    } else if (anim_kind == ANIM_GRID) {
        int i;
        for (i = 0; i < grid_count && r == SV_OK; ++i) {
            float wave = sinf((float)anim_step * 0.2f + (float)(i / 4 + i % 4)) * 2.0f;
            SvVector place = offset(grid_origin, (float)(i / 4) * 2.0f, 1.5f + wave, (float)(i % 4) * 2.0f);
            SvResult moved = change->teleport(self, grid_items[i], place);
            /* An item that a human picked up cannot move. The show goes on with the others. */
            if (moved == SV_STATE)
                ++grid_skipped;
            else
                r = moved;
        }
    } else if (anim_kind == ANIM_FRAMES) {
        char text[SV_HEADLINE_TEXT_MAX + 1];
        uint32_t x;
        for (x = 0; x < SV_HEADLINE_TEXT_MAX; ++x)
            text[x] = ((x % 62) + anim_step) % 16 < 8 ? '#' : '.';
        text[SV_HEADLINE_TEXT_MAX] = 0;
        r = change->headline(self, 0, text);
    }
    ++anim_step;
    if (r != SV_OK) {
        sayf("The show stopped with error %d after %u steps.", (int)r, anim_step);
        anim_kind = ANIM_NONE;
    } else if (anim_step >= anim_limit) {
        sayf("The show ended after %u steps. %u moves were skipped.", anim_step, grid_skipped);
        anim_kind = ANIM_NONE;
    }
}

static void anim_start(int kind, uint32_t every, uint32_t limit)
{
    grid_skipped = 0;
    anim_kind = kind;
    anim_every = every;
    anim_limit = limit;
    anim_step = 0;
    anim_next = 0;
}

static SvResult t_tour(void)
{
    say("Type /test go. Each sound plays once at pitch 1, one every 1.5 seconds, from number 0 to 46.");
    say("Write down which numbers are tones (like a beep or a ring) and which are noises. The log has the numbers.");
    return SV_OK;
}

static SvResult t_tour_go(void)
{
    anim_start(ANIM_TOUR, 90, 47);
    return SV_OK;
}

static SvResult t_octaves(void)
{
    say("Type /test go. The phone ring (sound 27) plays 12 times: six pitches from 0.125 to 4, twice.");
    say("Each step doubles the pitch, so each step is one octave higher. Tell me where two steps sound the same.");
    return SV_OK;
}

static SvResult t_octaves_go(void)
{
    anim_start(ANIM_OCTAVES, 90, 12);
    return SV_OK;
}

static SvResult t_scale(void)
{
    say("Type /test go. A scale of 25 sounds plays, one every 15 ticks.");
    return SV_OK;
}

static SvResult t_scale_go(void)
{
    anim_start(ANIM_SCALE, 15, 25);
    return SV_OK;
}

static SvResult t_grid_go(void)
{
    anim_every = anim_every == 1 ? 4 : anim_every == 4 ? 12 : 1;
    sayf("The grid moves every %u ticks now.", anim_every);
    anim_start(ANIM_GRID, anim_every, 300);
    return SV_OK;
}

static SvResult t_grid(void)
{
    SvPlayerState player = { sizeof player };
    SvHumanState human = { sizeof human };
    SvResult r = body(&player, &human);
    int i;
    if (r != SV_OK)
        return r;
    grid_count = 0;
    grid_origin = beside(&human, 6.0f, 0.0f, -3.0f);
    for (i = 0; i < GRID_ITEMS; ++i) {
        SvEntity item = 0;
        r = change->item_create(self, 12, offset(grid_origin, (float)(i / 4) * 2.0f, 1.5f, (float)(i % 4) * 2.0f), &item);
        if (r != SV_OK)
            break;
        remember(made_items, &made_item_count, 24, item);
        grid_items[grid_count++] = item;
    }
    sayf("%d items made. They move in a wave. Type /test go to change the speed.", grid_count);
    if (!grid_count)
        return r;
    anim_start(ANIM_GRID, 1, 300);
    return SV_OK;
}

static SvResult t_frames_go(void)
{
    anim_every = anim_every == 60 ? 20 : anim_every == 20 ? 6 : 60;
    sayf("The newspaper changes every %u ticks now.", anim_every);
    anim_start(ANIM_FRAMES, anim_every, 60);
    return SV_OK;
}

static SvResult t_frames(void)
{
    SvResult r = to_newspaper();
    if (r != SV_OK)
        return r;
    say("Pick up the newspaper and read the first line. Type /test go to start the show.");
    anim_every = 60;
    return SV_OK;
}

/* A tag on a car (SV_STORAGE_ENTITY): it reads back, and after the car goes the same reference finds nothing. */
static int tag_step;

static SvResult t_tag_go(void)
{
    SvStorageKey key = { sizeof key, SV_STORAGE_ENTITY, last_car, "tag" };
    SvStorageValue value = { sizeof value };
    SvResult r;
    if (!storage)
        return SV_INCOMPATIBLE;
    if (tag_step == 0) {
        r = change->object_remove(self, last_car);
        if (r == SV_OK) {
            tag_step = 1;
            say("The car is gone. Wait 2 seconds, then type /test go to read the tag again.");
        }
        return r;
    }
    r = storage->get(self, &key, &value);
    sayf("The tag of the removed car gave %d (want %d).", (int)r, (int)SV_NOT_FOUND);
    return r == SV_NOT_FOUND ? SV_OK : SV_FAILED;
}

static SvResult t_tag(void)
{
    SvStorageKey key = { sizeof key, SV_STORAGE_ENTITY, 0, "tag" };
    SvStorageValue value = { sizeof value, 5, "hello" };
    SvStorageValue back = { sizeof back };
    SvResult r = storage ? t_car() : SV_INCOMPATIBLE;
    if (r != SV_OK)
        return r;
    tag_step = 0;
    key.subject = last_car;
    r = storage->put(self, &key, &value);
    if (r == SV_OK)
        r = storage->get(self, &key, &back);
    if (r == SV_OK && (back.length != 5 || memcmp(back.bytes, "hello", 5)))
        r = SV_FAILED;
    if (r == SV_OK)
        say("The car carries the tag 'hello'. Type /test go to remove the car, and again to read the tag.");
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
    { "ai_walk", "Makes a bot and sends it 25 units away. Type /test go to make it stop and turn.", "The bot walks to a point beside you. After a go it stops and turns about 90 degrees from its path for a few seconds.", t_ai_walk, t_ai_walk_go },
    { "phone_claim", "Makes the plugin take a call to the number 1234 on any phone.", "Dial 1234: a chat line says the plugin took the call and the phone does not ring. Other numbers dial as before.", t_phone, NULL },
    { "storage_counter", "Stores a counter on the server and says its value.", "The number grows by 1 each run. After a server restart it keeps growing. Type /test go to erase the counter.", t_storage, t_storage_erase },
    { "read_account", "Reads your account.", "The name is yours and the money matches your game.", t_read_account, NULL },
    { "read_inventory", "Reads the seven slots of your body.", "A slot with an item shows 1 or more. Hold an item in a hand first, then run this test again.", t_read_inventory, NULL },
    { "read_line", "Casts a line from 6 units above you to 6 units below.", "The kind is 1 (level) and the hit height is near your feet.", t_read_line, NULL },
    { "read_headline", "Reads news line 0 and the number of lines.", "The text is the first news line of the newspaper.", t_read_headline, NULL },
    { "props_list", "Counts the rows of the property table.", "The count is 34 or more.", t_props_list, NULL },
    { "add_money", "Adds 100 dollars to your account.", "Your money rises by 100 at once. The next test takes it back.", t_add_money, NULL },
    { "disown", "Makes an item beside you and gives it up, so it stays after the plugin stops.", "An item lies beside you. Type /test stop: the item stays on the ground.", t_disown, NULL },
    { "ai_threats", "Makes a bot. Type /test go to read what the bot sees.", "After you wave in front of the bot, the count is 1 or more.", t_threats, t_threats_go },
    { "ai_waypoints", "Makes a bot and gives it a route of three points. Type /test go to clear the route.", "The bot walks around a triangle. After a go it stops.", t_waypoints, t_waypoints_go },
    { "ai_target", "Makes a bot and makes it select you as its target.", "Say what the bot does. It should turn to you.", t_target, NULL },
    { "ai_duel", "Makes an armed bot and a target bot. Type /test go for the next stage.", "Stage 0: shots at the head. Stage 1: no shots. Stage 2: shots at the torso.", t_duel, t_duel_go },
    { "ai_seated", "Makes a car with an armed bot in seat 1, and a target bot 22 units away.", "The bot in the car shoots at the bot outside.", t_seated, NULL },
    { "economy_read", "Reads the five companies.", "The prices and shares look like the stock list of the game.", t_economy_read, NULL },
    { "economy_change", "Sets the price of company 0 to 123. Type /test go to add shares and stocks.", "The stock list shows 123 for company 0. After a go it shows 10 more of your stocks. The next test puts all back.", t_economy_change, t_economy_change_go },
    { "mission_read", "Counts the active missions of each team.", "The counts match the mission lists that you saw.", t_mission_read, NULL },
    { "mission_start", "Adds a mission with an objective item. Type /test go to cancel it.", "The mission shows in the list of your team. After a go it leaves.", t_mission_start, t_mission_cancel },
    { "shop_catalog", "Sets the price of the first row of every shop to 1.", "Open a shop that sells guns, cars or clothes. The first row costs 1 dollar. The next test puts the prices back.", t_shop_catalog, NULL },
    { "shop_phone", "Moves you next to the nearest phone. Type /test go to read it and end its call.", "Dial a number, then type go: the state shows dial or ringing, and then the call ends.", t_phone_state, t_phone_state_go },
    { "setting_list", "Lists the core settings of the game.", "Three settings show a value, a range and a default.", t_setting_list, NULL },
    { "setting_bots", "Sets the bot count to 2 and shows the setting event.", "A chat line shows the change. The bots in the city fall to 2. The next test puts the value back.", t_setting_bots, NULL },
    { "engine_list", "Lists the engine parameters.", "Three parameters show a value.", t_engine_list, NULL },
    { "config_read", "Reads the three settings of this plugin from plugin.ini.", "The values are hello, 3 and 1.50. Change one with the plugins config command, and a chat line shows it.", t_config_read, NULL },
    { "plugins_calls", "Calls subscribe with an extended bit, refuse and find outside their normal use.", "All three answers match the wanted values.", t_plugins_calls, NULL },
    { "kick", "Removes the player whose name you type with /test kick NAME. It refuses you, bots and administrators.", "The player leaves the game and cannot join again until you type /clearkick in the chat.", t_kick_run, NULL },
    { "pitch_scale", "Plays a game sound in 25 steps, from half pitch to double pitch. Type /test go to start it.", "The sound rises like a musical scale, in even steps. Say no if it jumps or stays flat.", t_scale, t_scale_go },
    { "item_grid", "Makes 16 items in a grid and moves all of them every tick in a wave. Type /test go to change the speed.", "The items move smoothly as one picture. Say no if they stutter, fall or vanish.", t_grid, t_grid_go },
    { "headline_frames", "Moves you to a newspaper. Type /test go to rewrite its first line again and again.", "Hold the newspaper open. The pattern of # and . moves. Say no if it never changes.", t_frames, t_frames_go },
    { "entity_tag", "Makes a car and stores a tag on it. Type /test go to remove the car, and again to read the tag.", "The tag reads back as hello. After the car goes, the same reference finds nothing (error 6).", t_tag, t_tag_go },
    { "sound_tour", "Plays every game sound once, from 0 to 46. Type /test go to start.", "You hear 47 sounds. Say which numbers are tones.", t_tour, t_tour_go },
    { "pitch_octaves", "Plays the phone ring in six octaves, twice. Type /test go to start.", "Six steps, each one octave higher. Say if two steps sound the same.", t_octaves, t_octaves_go },
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

static int same_name(const char *a, const char *b)
{
    while (*a && *b) {
        char x = *a >= 'A' && *a <= 'Z' ? (char)(*a + 32) : *a;
        char y = *b >= 'A' && *b <= 'Z' ? (char)(*b + 32) : *b;
        if (x != y)
            return 0;
        ++a;
        ++b;
    }
    return !*a && !*b;
}

static void kick_by_name(const char *name)
{
    uint32_t cursor = 0, round;
    if (!*name) {
        say("Type /test kick NAME.");
        return;
    }
    for (round = 0; round < 8; ++round) {
        SvEntity page[SV_ENTITY_PAGE_MAX];
        uint32_t count = 0, next = 0, i;
        SvResult r = game->entities(self, SV_ENTITY_PLAYER, cursor, page, SV_ENTITY_PAGE_MAX, &count, &next);
        if (r != SV_OK) {
            sayf("The player list gave error %d.", (int)r);
            return;
        }
        for (i = 0; i < count; ++i) {
            SvPlayerState other = { sizeof other };
            if (game->player(self, page[i], &other) != SV_OK || !same_name(other.name, name))
                continue;
            if (page[i] == tester || other.is_bot || other.is_admin) {
                say("Not done. The test refuses you, bots and administrators.");
                return;
            }
            r = change->kick(self, page[i]);
            if (r == SV_OK)
                sayf("Kicked %.20s. Type /clearkick in the chat now.", other.name);
            else
                sayf("The kick gave error %d.", (int)r);
            logf_("TESTROOM kick %s code=%d", other.name, (int)r);
            if (current && !strcmp(tests[current - 1].name, "kick")) {
                mark(r == SV_OK ? PASS : FAIL, r);
                next_test();
            }
            return;
        }
        if (next <= cursor)
            break;
        cursor = next;
    }
    say("No player has that name.");
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
        say("Other commands: /test go, again, <number>, list, report, stop.");
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
    } else if (!strcmp(word, "kick")) {
        const char *name = arguments + length;
        while (*name == ' ')
            ++name;
        kick_by_name(name);
    } else if (!strcmp(word, "again")) {
        if (!current) {
            say("No test runs. Type /test start.");
        } else {
            sayf("[%d/%d] %s", current, test_count, tests[current - 1].name);
            sayf("Does: %s", tests[current - 1].what);
            sayf("Expect: %s", tests[current - 1].expect);
        }
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
        say("Commands: /test start, yes, no, skip, go, again, kick NAME, <number>, list, report, stop.");
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
    h->get_api(owner, SV_API_ECONOMY, SV_GAME_ECONOMY_VERSION, (const void **)&economy);
    h->get_api(owner, SV_API_MISSION, SV_GAME_MISSION_VERSION, (const void **)&mission);
    h->get_api(owner, SV_API_SETTINGS, SV_GAME_SETTINGS_VERSION, (const void **)&settings);
    h->get_api(owner, SV_API_SHOP, SV_GAME_SHOP_VERSION, (const void **)&shop);
    h->get_api(owner, SV_API_CONFIG, SV_CONFIG_VERSION, (const void **)&config);
    h->get_api(owner, SV_API_PLUGINS, SV_PLUGINS_VERSION, (const void **)&plugins_api);
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
