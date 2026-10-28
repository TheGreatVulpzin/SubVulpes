#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <float.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include "plugin_host.h"

int sv_game_start(void) { return 1; }
void sv_game_stop(void) {}
SvResult sv_game_api(uint32_t capability, uint32_t version, const void **api)
{
    (void)capability;
    (void)version;
    (void)api;
    return SV_NOT_FOUND;
}

void sv_log(const char *format, ...)
{
    va_list args;
    va_start(args, format);
    vprintf(format, args);
    va_end(args);
    putchar('\n');
    fflush(stdout);
}

void sv_game_status(SvStatus *status)
{
    memset(status, 0, sizeof *status);
    status->size = sizeof *status;
    status->tick = 42;
}

/* A reply to a player goes to the output, so a test can read it. */
void sv_game_tell(uint64_t actor, const char *text)
{
    printf("TELL %llu %s\n", (unsigned long long)actor, text);
}

/* Actor 7 is an administrator. Actor 8 is a player without administration rights. */
int sv_game_actor_admin(uint64_t actor)
{
    return actor == 7;
}

/* A damage decision of the host with this amount. The result is the final amount. */
static int decide(int amount)
{
    SvDamageEvent damage;
    memset(&damage, 0, sizeof damage);
    damage.base.size = sizeof damage;
    damage.base.kind = SV_EVENT_DAMAGE;
    damage.amount = damage.new_amount = amount;
    return sv_plugins_decide_damage(&damage);
}

#define CHECK(condition) do { if (!(condition)) { \
    fprintf(stderr, "Failed line %d: %s\n", __LINE__, #condition); return 1; } } while (0)

int main(int argc, char **argv)
{
    unsigned before, after;
    int ok;
    if (argc != 2)
        return 2;
    ok = sv_plugins_start();
    if (!strcmp(argv[1], "required-fail")) {
        CHECK(!ok && sv_plugins_failed());
    } else if (!strcmp(argv[1], "optional-fail")) {
        CHECK(ok && !sv_plugins_failed());
        CHECK(!sv_plugins_command(0, "fixture"));
    } else if (!strcmp(argv[1], "invalid-metadata")) {
        CHECK(ok);
        CHECK(sv_plugins_disable("test.fixture") == SV_OK);
        CHECK(sv_plugins_enable("test.fixture") == SV_INCOMPATIBLE);
    } else if (!strcmp(argv[1], "lifecycle")) {
        CHECK(ok);
        CHECK(sv_plugins_command(0, "fixture"));
        CHECK(sv_plugins_disable("test.fixture") == SV_OK);
        CHECK(!sv_plugins_command(0, "fixture"));
        CHECK(sv_plugins_enable("test.fixture") == SV_OK);
        CHECK(sv_plugins_command(0, "fixture"));
        CHECK(sv_plugins_disable("missing") == SV_NOT_FOUND);
        CHECK(sv_plugins_command(1, "plugins disable test.fixture"));
        CHECK(sv_plugins_command(0, "fixture"));
    } else if (!strcmp(argv[1], "restart-enable")) {
        CHECK(ok);
        CHECK(sv_plugins_enable("test.fixture") == SV_RESTART);
        CHECK(!sv_plugins_command(0, "fixture"));
    } else if (!strcmp(argv[1], "restart")) {
        CHECK(ok);
        CHECK(sv_plugins_disable("test.fixture") == SV_RESTART);
        CHECK(sv_plugins_command(0, "fixture"));
    } else if (!strcmp(argv[1], "required")) {
        CHECK(ok);
        CHECK(sv_plugins_disable("test.fixture") == SV_DENIED);
    } else if (!strcmp(argv[1], "dependency")) {
        CHECK(ok);
        CHECK(sv_plugins_disable("test.fixture") == SV_DEPENDENCY);
        CHECK(sv_plugins_disable("sv.status") == SV_OK);
        CHECK(sv_plugins_disable("test.fixture") == SV_OK);
    } else if (!strcmp(argv[1], "library")) {
        /* The consumer finds the API of the library, and the library function logs as the consumer. */
        CHECK(ok && sv_plugins_command(0, "consumer"));
        CHECK(sv_plugins_disable("test.library") == SV_DEPENDENCY);
        CHECK(sv_plugins_disable("test.consumer") == SV_OK);
        CHECK(!sv_plugins_command(0, "consumer"));
        CHECK(sv_plugins_disable("test.library") == SV_OK);
    } else if (!strcmp(argv[1], "library-stop")) {
        /* The server stops with both active. The consumer stops first. */
        CHECK(ok && sv_plugins_command(0, "consumer"));
    } else if (!strcmp(argv[1], "consumer-failed")) {
        CHECK(ok && !sv_plugins_command(0, "consumer"));
        sv_plugins_list();
    } else if (!strcmp(argv[1], "conflict-enable")) {
        /* A runtime enable that would conflict is refused and not saved. After the other plugin stops it works. */
        CHECK(ok && sv_plugins_command(0, "fixture"));
        CHECK(sv_plugins_enable("test.library") == SV_CONFLICT);
        CHECK(sv_plugins_disable("test.fixture") == SV_OK);
        CHECK(sv_plugins_enable("test.library") == SV_OK);
        CHECK(sv_plugins_enable("test.fixture") == SV_CONFLICT);
        sv_plugins_list();
    } else if (!strcmp(argv[1], "start-list")) {
        /* Start, then list the plugins. The first plugin of a conflict wins. */
        CHECK(ok);
        sv_plugins_list();
    } else if (!strcmp(argv[1], "event-fail")) {
        CHECK(ok);
        sv_plugins_tick();
        CHECK(!sv_plugins_failed());
        CHECK(!sv_plugins_command(0, "fixture"));
        CHECK(sv_plugins_command(0, "status"));
    } else if (!strcmp(argv[1], "required-event-fail")) {
        CHECK(ok);
        sv_plugins_tick();
        CHECK(sv_plugins_failed());
    } else if (!strcmp(argv[1], "permissions")) {
        CHECK(ok);
        CHECK(sv_plugins_command(0, "perm_public") && sv_plugins_command(0, "perm_admin") &&
              sv_plugins_command(0, "perm_local"));
        CHECK(sv_plugins_command(8, "perm_public") && sv_plugins_command(8, "perm_admin") &&
              sv_plugins_command(8, "perm_local"));
        CHECK(sv_plugins_command(7, "perm_admin") && sv_plugins_command(7, "perm_local"));
        CHECK(!sv_plugins_command(7, "perm_missing"));
    } else if (!strcmp(argv[1], "limit")) {
        CHECK(ok);
        CHECK(sv_plugins_command(0, "lim126") && !sv_plugins_command(0, "lim127"));
        CHECK(sv_plugins_disable("test.fixture") == SV_OK);
        CHECK(!sv_plugins_command(0, "lim0") && !sv_plugins_command(0, "fixture"));
        CHECK(sv_plugins_enable("test.fixture") == SV_OK);
        CHECK(sv_plugins_command(0, "lim126") && sv_plugins_command(0, "fixture"));
    } else if (!strcmp(argv[1], "config")) {
        /* The settings. The test script reads the output. Actor 7 is an administrator and actor 8 is not. */
        CHECK(ok && sv_plugins_command(0, "cfg"));
        CHECK(sv_plugins_command(0, "plugins config test.fixture price 300"));
        CHECK(sv_plugins_command(0, "cfg"));
        CHECK(sv_plugins_command(0, "plugins config test.fixture rate 2"));
        CHECK(sv_plugins_command(0, "cfg"));
        CHECK(sv_plugins_command(0, "plugins config test.fixture price 2000000"));
        CHECK(sv_plugins_command(0, "plugins config test.fixture price -1"));
        CHECK(sv_plugins_command(0, "plugins config test.fixture price 12x"));
        CHECK(sv_plugins_command(0, "plugins config test.fixture price 5 6"));
        CHECK(sv_plugins_command(0, "plugins config test.fixture nosuch 1"));
        CHECK(sv_plugins_command(0, "plugins config test.fixture enabled 2"));
        CHECK(sv_plugins_command(0, "plugins config test.fixture mode extreme"));
        CHECK(sv_plugins_command(0, "plugins config test.fixture rate 3.5"));
        CHECK(sv_plugins_command(0, "plugins config test.fixture rate 1e1"));
        CHECK(sv_plugins_command(0, "plugins config test.fixture name bad;name"));
        CHECK(sv_plugins_command(0, "plugins config test.fixture name this-name-is-longer-than-twenty-chars"));
        CHECK(sv_plugins_command(0, "plugins config test.fixture mode hard"));
        CHECK(sv_plugins_command(0, "plugins config test.fixture token s3cret"));
        CHECK(sv_plugins_command(0, "cfg"));
        CHECK(sv_plugins_command(0, "plugins config test.fixture price"));
        CHECK(sv_plugins_command(0, "cfg"));
        CHECK(sv_plugins_command(0, "plugins config test.fixture price 300"));
        CHECK(sv_plugins_command(0, "plugins config"));
        CHECK(sv_plugins_command(0, "plugins config missing"));
        CHECK(sv_plugins_command(0, "plugins config test.fixture"));
        CHECK(sv_plugins_command(7, "plugins config test.fixture price 77"));
        CHECK(sv_plugins_command(7, "plugins config test.fixture rate 1"));
        CHECK(sv_plugins_command(7, "plugins config test.fixture"));
        CHECK(sv_plugins_command(8, "plugins config test.fixture price 1"));
        CHECK(sv_plugins_command(8, "plugins config test.fixture"));
        CHECK(sv_plugins_command(8, "plugins disable test.fixture"));
    } else if (!strcmp(argv[1], "config-read")) {
        CHECK(ok && sv_plugins_command(0, "cfg"));
    } else if (!strcmp(argv[1], "unsafe-budget")) {
        unsigned i;
        /* The guard on 88 runs inside the plugin. A fast answer does not count. Ten slow answers are allowed, and the
         * eleventh marks the plugin. The tick after it disables the plugin. */
        CHECK(ok && sv_plugins_command(0, "fixture"));
        CHECK(decide(88) == 88);
        for (i = 0; i < 10; ++i)
            CHECK(decide(99) == 99 && decide(1) == 1);
        sv_plugins_tick();
        CHECK(sv_plugins_command(0, "fixture"));
        CHECK(decide(99) == 99);
        CHECK(sv_plugins_command(0, "fixture"));
        sv_plugins_tick();
        CHECK(!sv_plugins_command(0, "fixture") && !sv_plugins_failed());
        CHECK(decide(99) == 99);
    } else if (!strcmp(argv[1], "unsafe-fail")) {
        /* A failure in a decision marks the plugin, and the next tick disables it. The amount stays. */
        CHECK(ok && sv_plugins_command(0, "fixture"));
        CHECK(decide(7) == 7);
        CHECK(sv_plugins_command(0, "fixture"));
        sv_plugins_tick();
        CHECK(!sv_plugins_command(0, "fixture") && !sv_plugins_failed());
    } else if (!strcmp(argv[1], "status-local")) {
        CHECK(ok);
        CHECK(sv_plugins_command(0, "status"));
        CHECK(sv_plugins_command(8, "status"));
    } else if (!strcmp(argv[1], "metrics")) {
        CHECK(ok);
        sv_plugins_tick();
        sv_plugins_tick();
        sv_plugins_tick();
        sv_plugins_list();
    } else if (!strcmp(argv[1], "fpu")) {
        CHECK(ok);
        _controlfp_s(&before, 0, 0);
        sv_plugins_tick();
        _controlfp_s(&after, 0, 0);
        CHECK((before & (_MCW_RC | _MCW_PC)) == (after & (_MCW_RC | _MCW_PC)));
    } else if (!strcmp(argv[1], "control")) {
        unsigned i;
        CHECK(ok);
        sv_control_start();
        for (i = 0; i < 40; ++i) {
            sv_control_poll();
            Sleep(5);
        }
        sv_control_stop();
        CHECK(!sv_plugins_command(0, "fixture"));
    } else if (!strcmp(argv[1], "hold")) {
        CHECK(ok);
        puts("READY");
        fflush(stdout);
        Sleep(2500);
    } else {
        return 2;
    }
    sv_plugins_stop();
    puts("PASS");
    return 0;
}
