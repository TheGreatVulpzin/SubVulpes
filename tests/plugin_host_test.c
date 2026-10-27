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

/* Actor 7 is an administrator. Actor 8 is a player without administration rights. */
int sv_game_actor_admin(uint64_t actor)
{
    return actor == 7;
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
