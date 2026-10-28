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

#define VERSION "0.1.2"
#define LINE_MAX_CHARS 60
#define MAX_TESTS 32

static const SvHost *host;
static SvOwner self;
static const SvGameRead *game;
static const SvGameWrite *change;
static const SvGameProps *props;

enum { TODO, PASS, FAIL, SKIP };
/* What a test changed on the player, so the next test or /test stop can undo it. */
enum { T_HEALTH = 1, T_BLEED = 2, T_STUN = 4, T_DEALT = 8, T_LOOKS = 16 };

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
static void cleanup(void)
{
    SvPlayerState player = { sizeof player };
    SvHumanState human = { sizeof human };
    int i;
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
    r = change->npc_create(self, beside(&human, 4.0f, 0.5f, 0.0f), 0.0f, &npc, &npc_human);
    remember(made_players, &made_player_count, 4, npc);
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
    { "npc", "Makes a bot named NPC 4 units beside you.", "A bot stands beside you. It does not walk.", t_npc, NULL },
    { "vehicle_exit", "Makes a car. You get in. Then /test go throws you out of it.", "You leave the car at once, without flying away.", t_exit, t_exit_go },
    { "aim", "Reads what your aim hits. Type /test go after you look somewhere else.", "The answer matches what you look at: ground, a car or a human.", t_aim, t_aim_go },
    { "looks", "Sets your five look settings to 0, then raises one for each /test go.", "Each go changes one part of your look. Say yes if every part changes something. The next test puts your looks back.", t_looks, t_looks_go },
    { "damage_dealt_zero", "Makes a bot and a Magnum for you, and sets your damage to 0.", "Shoot the bot. It takes no damage.", t_dealt, NULL },
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
