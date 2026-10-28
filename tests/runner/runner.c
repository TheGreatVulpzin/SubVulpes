/* The plugin test runner. It links the game sources of the server, builds a small world with the real constructors, loads the
 * plugins of the current folder (plugins/plugins.ini) and runs a scenario file (.svtest). It needs no game data and no client.
 * Usage: subvulpes_plugin_test.exe SCENARIO
 * The folder where it runs holds plugins/ and user/plugins/. tools/sdk/run_plugin_test.ps1 prepares it.
 * A step is one line. Words start with the step name. The lines that start with # are comments.
 *   seed N                      fix the random seed
 *   spawn player NAME [team=N] [admin=0|1]
 *   tick N                      run N ticks of the plugin host
 *   say PLAYER TEXT             a chat text of the player. A text that starts with / can be a plugin command
 *   console TEXT                a command of the local operator. A plugin must handle it
 *   give TYPE PLAYER            make an item of the type and put it in a hand of the player
 *   set PROP PLAYER VALUE       set a property through the probe plugin
 *   expect log "TEXT"           some log line has the text
 *   expect no-log "TEXT"        no log line has the text
 *   expect count N "TEXT"       exactly N log lines have the text
 *   expect prop PROP PLAYER V   the property has the value (a number within 0.001, or the exact text of a vector)
 *   expect entities KIND N      the game has N players, humans, items or objects
 *   expect failed | no-failure  some plugin failed to run, or none did */
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <ctype.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "humans.h"
#include "items.h"
#include "logic.h"
#include "objects.h"
#include "player.h"
#include "plugin_entities.h"
#include "plugin_game_internal.h"
#include "plugin_host.h"
#include "server.h"
#include "sr_math.h"
#include "sr_os.h"
#include "sr_server.h"

extern void (*sv_test_log_hook)(const char *line);

#define MAX_LINE 4096
#define MAX_PLAYERS 16

static char *log_text;
static size_t log_size, log_capacity;
static struct { char name[32]; int index; } players[MAX_PLAYERS];
static int player_count, step_number;
static const char *scenario_name;

static void capture(const char *line)
{
    size_t length = strlen(line);
    if (log_size + length + 2 > log_capacity) {
        size_t wanted = (log_capacity ? log_capacity * 2 : 65536);
        while (wanted < log_size + length + 2)
            wanted *= 2;
        log_text = realloc(log_text, wanted);
        if (!log_text) {
            fprintf(stderr, "Out of memory for the log.\n");
            exit(2);
        }
        log_capacity = wanted;
    }
    memcpy(log_text + log_size, line, length);
    log_size += length;
    log_text[log_size++] = '\n';
    log_text[log_size] = 0;
}

static __declspec(noreturn) void fail(const char *format, ...)
{
    va_list args;
    printf("FAIL %s step %d: ", scenario_name, step_number);
    va_start(args, format);
    vprintf(format, args);
    va_end(args);
    printf("\n");
    exit(1);
}

/* The text of a quoted argument: "text". Returns NULL without both quotes. */
static char *quoted(char *text)
{
    char *first = strchr(text, '"'), *last = strrchr(text, '"');
    if (!first || last == first)
        return NULL;
    *last = 0;
    return first + 1;
}

static size_t count_lines(const char *needle)
{
    size_t total = 0;
    const char *line = log_text ? log_text : "";
    while (*line) {
        const char *end = strchr(line, '\n');
        size_t length = end ? (size_t)(end - line) : strlen(line);
        char copy[1024];
        if (length >= sizeof copy)
            length = sizeof copy - 1;
        memcpy(copy, line, length);
        copy[length] = 0;
        if (strstr(copy, needle))
            ++total;
        line += length + (end ? 1 : 0);
        if (!end)
            break;
    }
    return total;
}

static int find_player(const char *name)
{
    int i;
    for (i = 0; i < player_count; ++i)
        if (!strcmp(players[i].name, name))
            return players[i].index;
    fail("no player named %s (use spawn first)", name);
}

static void spawn_player(char *name, int team, int admin)
{
    float pos[3], rot[9] = { 1, 0, 0, 0, 1, 0, 0, 0, 1 };
    int p, account, human, i;
    if (player_count >= MAX_PLAYERS)
        fail("at most %d players", MAX_PLAYERS);
    if (!*name || strlen(name) > 31 || strchr(name, ' '))
        fail("a player name has 1 to 31 characters and no space");
    for (i = 0; i < player_count; ++i)
        if (!strcmp(players[i].name, name))
            fail("the player %s exists", name);
    pos[0] = 10.0f + 3.0f * (float)player_count;
    pos[1] = 20.0f;
    pos[2] = 30.0f;
    p = createplayer();
    if (p < 0)
        fail("the game has no free player slot");
    strcpy(sr_players[p].name, name);
    sr_players[p].account_id[0] = 1000u + (uint32_t)player_count;
    sr_players[p].account_id[1] = 2000u + (uint32_t)player_count;
    account = createaccount(p);
    if (account < 0)
        fail("the game could not make an account");
    sr_players[p].account = account;
    human = createhuman(pos, rot, p);
    if (human < 0)
        fail("the game could not make a body");
    sr_players[p].human_id = human;
    sr_humans[human].account = account;
    sr_players[p].team = team;
    sr_humans[human].team = team;
    sr_players[p].is_admin = admin;
    strcpy(players[player_count].name, name);
    players[player_count++].index = p;
}

/* Runs a probe command and returns the new log text from the first line that starts with "PROBE ". */
static const char *probe(const char *format, ...)
{
    char command[300];
    size_t before = log_size;
    va_list args;
    const char *found;
    va_start(args, format);
    vsnprintf(command, sizeof command, format, args);
    va_end(args);
    if (!sv_plugins_command(0, command))
        fail("the probe plugin did not answer (is sv.probe enabled?)");
    found = log_text ? strstr(log_text + before, "PROBE ") : NULL;
    if (!found)
        fail("the probe gave no answer");
    return found;
}

static int numbers_equal(const char *a, const char *b)
{
    char *end_a, *end_b;
    double x = strtod(a, &end_a), y = strtod(b, &end_b);
    if (end_a == a || end_b == b || (*end_a && !isspace((unsigned char)*end_a)) || (*end_b && !isspace((unsigned char)*end_b)))
        return 0;
    return x - y < 0.001 && y - x < 0.001;
}

static void expect_prop(const char *prop, const char *player, const char *wanted)
{
    char prefix[160];
    const char *answer = probe("probe get %s %s", prop, player), *value;
    snprintf(prefix, sizeof prefix, "PROBE get %s %s = ", prop, player);
    if (strncmp(answer, prefix, strlen(prefix)))
        fail("the probe could not read %s of %s: %.80s", prop, player, answer);
    value = answer + strlen(prefix);
    if (!numbers_equal(value, wanted)) {
        char shown[80];
        size_t length = strcspn(value, "\n");
        snprintf(shown, sizeof shown, "%.*s", (int)(length < 79 ? length : 79), value);
        if (strncmp(value, wanted, strlen(wanted)) || (value[strlen(wanted)] && value[strlen(wanted)] != '\n'))
            fail("%s of %s is %s, and the scenario wants %s", prop, player, shown, wanted);
    }
}

static void run_step(char *line)
{
    char *word = strtok(line, " \t"), *rest;
    if (!word)
        return;
    rest = strtok(NULL, "");
    rest = rest ? rest : "";
    while (*rest == ' ' || *rest == '\t')
        ++rest;
    if (!strcmp(word, "seed")) {
        os_srand((unsigned)strtoul(rest, NULL, 10));
    } else if (!strcmp(word, "spawn")) {
        char *kind = strtok(rest, " \t"), *name = strtok(NULL, " \t"), *option;
        int team = 1, admin = 0;
        if (!kind || strcmp(kind, "player") || !name)
            fail("use: spawn player NAME [team=N] [admin=0|1]");
        while ((option = strtok(NULL, " \t")) != NULL) {
            if (!strncmp(option, "team=", 5))
                team = atoi(option + 5);
            else if (!strncmp(option, "admin=", 6))
                admin = atoi(option + 6) != 0;
            else
                fail("unknown option %s", option);
        }
        spawn_player(name, team, admin);
    } else if (!strcmp(word, "tick")) {
        long n = strtol(rest, NULL, 10), i;
        if (n < 1 || n > 100000)
            fail("tick takes 1 to 100000");
        for (i = 0; i < n; ++i)
            sv_plugins_tick();
    } else if (!strcmp(word, "say")) {
        char *name = strtok(rest, " \t"), *text = strtok(NULL, ""), buffer[64];
        if (!name || !text)
            fail("use: say PLAYER TEXT");
        if (strlen(text) > 63)
            fail("a chat text has at most 63 characters");
        strcpy(buffer, text);
        server_playermessage(find_player(name), buffer);
    } else if (!strcmp(word, "console")) {
        if (!*rest)
            fail("use: console TEXT");
        if (!sv_plugins_command(0, rest))
            fail("no plugin handled the command: %s", rest);
    } else if (!strcmp(word, "give")) {
        char *type = strtok(rest, " \t"), *name = strtok(NULL, " \t");
        if (!type || !name)
            fail("use: give TYPE PLAYER");
        find_player(name);
        if (!strstr(probe("probe give %s %s", type, name), " ok"))
            fail("the item could not be made or equipped");
    } else if (!strcmp(word, "set")) {
        char *prop = strtok(rest, " \t"), *name = strtok(NULL, " \t"), *value = strtok(NULL, " \t");
        if (!prop || !name || !value)
            fail("use: set PROP PLAYER VALUE");
        find_player(name);
        if (!strstr(probe("probe set %s %s %s", prop, name, value), " ok"))
            fail("the property %s could not be set", prop);
    } else if (!strcmp(word, "expect")) {
        char *what = strtok(rest, " \t"), *argument = strtok(NULL, "");
        argument = argument ? argument : "";
        if (!what)
            fail("use: expect log|no-log|count|prop|entities|failed|no-failure");
        if (!strcmp(what, "log") || !strcmp(what, "no-log")) {
            char *text = quoted(argument);
            if (!text)
                fail("put the text in double quotes");
            if ((count_lines(text) > 0) != !strcmp(what, "log"))
                fail("%s \"%s\" does not hold", what, text);
        } else if (!strcmp(what, "count")) {
            long wanted = strtol(argument, &argument, 10);
            char *text = quoted(argument);
            size_t found;
            if (!text)
                fail("use: expect count N \"TEXT\"");
            found = count_lines(text);
            if (found != (size_t)wanted)
                fail("%lu log lines have \"%s\", and the scenario wants %ld", (unsigned long)found, text, wanted);
        } else if (!strcmp(what, "prop")) {
            char *prop = strtok(argument, " \t"), *name = strtok(NULL, " \t"), *value = strtok(NULL, "");
            if (!prop || !name || !value)
                fail("use: expect prop PROP PLAYER VALUE");
            find_player(name);
            expect_prop(prop, name, value);
        } else if (!strcmp(what, "entities")) {
            char *kind = strtok(argument, " \t"), *number = strtok(NULL, " \t"), expected[80];
            const char *answer;
            if (!kind || !number)
                fail("use: expect entities KIND N");
            answer = probe("probe count %s", kind);
            snprintf(expected, sizeof expected, "PROBE count %s = %s", kind, number);
            if (strncmp(answer, expected, strlen(expected)) || (answer[strlen(expected)] && answer[strlen(expected)] != '\n'))
                fail("%.60s, and the scenario wants %s %s", answer, kind, number);
        } else if (!strcmp(what, "failed")) {
            if (!sv_plugins_failed())
                fail("no plugin failed");
        } else if (!strcmp(what, "no-failure")) {
            if (sv_plugins_failed())
                fail("a plugin failed");
        } else {
            fail("unknown expectation %s", what);
        }
    } else {
        fail("unknown step %s", word);
    }
}

int main(int argc, char **argv)
{
    FILE *file;
    char line[MAX_LINE];
    if (argc != 2) {
        fprintf(stderr, "Usage: subvulpes_plugin_test.exe SCENARIO\n");
        return 2;
    }
    scenario_name = strrchr(argv[1], '\\');
    scenario_name = scenario_name ? scenario_name + 1 : argv[1];
    file = fopen(argv[1], "r");
    if (!file) {
        fprintf(stderr, "Cannot open the scenario %s.\n", argv[1]);
        return 2;
    }
    SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX);
    x87_init();
    sv_test_log_hook = capture;
    sr_itemtypes[0].inv_inertia = (vec3){ 1, 1, 1 };
    memset(&sr_data_objecttype[0], 0, sizeof sr_data_objecttype[0]);
    sr_data_server.max_players = 128;
    if (!sv_plugins_start()) {
        printf("FAIL %s: the plugin host did not start\n", scenario_name);
        return 1;
    }
    sv_game_ready();
    while (fgets(line, sizeof line, file)) {
        size_t length = strlen(line);
        while (length && (line[length - 1] == '\n' || line[length - 1] == '\r' || line[length - 1] == ' '))
            line[--length] = 0;
        if (!length || line[0] == '#')
            continue;
        ++step_number;
        printf("STEP %d: %s\n", step_number, line);
        run_step(line);
    }
    fclose(file);
    sv_plugins_stop();
    printf("PASS %s (%d steps)\n", scenario_name, step_number);
    return 0;
}
