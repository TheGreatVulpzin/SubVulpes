#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "plugin_host.h"
#include "sv_server.h"
#include "sr_runtime.h"
#include "sr_server.h"
#include "server.h"

#define CHECK(test) do { if (!(test)) { \
    fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #test); exit(1); \
} } while (0)

extern void (*sv_test_log_hook)(const char *line);
static void (*previous_log)(const char *line);
static int command_failed, owner_restored, save_failed;

static void command_log(const char *line)
{
    if (strstr(line, "Plugin command finished:") ||
        strstr(line, "Plugin command result:") || strstr(line, "Infrastructure fixture failed"))
        command_failed = 1;
    if (strstr(line, "INFRA_OWNER_RESTORED"))
        owner_restored = 1;
    if (strstr(line, "Account save failed."))
        save_failed = 1;
    if (previous_log)
        previous_log(line);
}

static void command(const char *text)
{
    command_failed = 0;
    CHECK(sv_plugins_command(0, text));
    CHECK(!command_failed);
}

static void check_account_file(void)
{
    FILE *file = fopen("user/server.srk", "rb");
    uint32_t header[3], ids[2];
    int32_t money, playtime, looks[3];
    char name[32];
    int i;
    CHECK(file != NULL);
    CHECK(fread(header, sizeof header, 1, file) == 1);
    CHECK(header[0] == 3 && header[1] == 0x7139ab42u && header[2] == 2);
    for (i = 0; i < 2; ++i) {
        CHECK(fread(ids, sizeof ids, 1, file) == 1);
        CHECK(ids[0] == 0x12345678u + (uint32_t)i && ids[1] == 0x87654321u - (uint32_t)i);
        CHECK(fread(name, sizeof name, 1, file) == 1);
        CHECK(!memcmp(name, i ? "Second fake account" : "First fake account", i ? 19 : 18));
        CHECK(name[31] == (char)('A' + i));
        CHECK(fread(&money, sizeof money, 1, file) == 1 && money == -1234567 + i);
        CHECK(fread(&playtime, sizeof playtime, 1, file) == 1 && playtime == 7654321 + i);
        CHECK(fread(looks, sizeof looks, 1, file) == 1);
        CHECK(looks[0] == 2 - i && looks[1] == 7 - i && looks[2] == 3 - i);
    }
    CHECK(ftell(file) == 132 && fgetc(file) == EOF);
    CHECK(!ferror(file) && fclose(file) == 0);
}

void infra_host_check(void)
{
    Account saved_accounts[2] = {sr_accounts[0], sr_accounts[1]};
    int saved_count = sr_numofaccounts, i;
    uint32_t saved_key = sr_account_key;
    HANDLE locked;
    WIN32_FIND_DATAA found;
    CHECK(CreateDirectoryA("user", NULL) || GetLastError() == ERROR_ALREADY_EXISTS);
    CHECK(DeleteFileA("user/server.srk") || GetLastError() == ERROR_FILE_NOT_FOUND);
    previous_log = sv_test_log_hook;
    sv_test_log_hook = command_log;
    owner_restored = save_failed = 0;
    sv_plugins_list();
    command("runtimeother denied");
    command("infracheck console");
    CHECK(owner_restored);
    command("infracheck invalid");
    CHECK(!sv_game_control_poll() && !sv_game_control_stopping());
    sr_numofaccounts = 2;
    sr_account_key = 0x7139ab42u;
    for (i = 0; i < 2; ++i) {
        Account *account = &sr_accounts[i];
        memset(account, 0, sizeof *account);
        account->id[0] = 0x12345678u + (uint32_t)i;
        account->id[1] = 0x87654321u - (uint32_t)i;
        memset(account->name, 'A' + i, sizeof account->name);
        memcpy(account->name, i ? "Second fake account" : "First fake account", i ? 19 : 18);
        account->money = -1234567 + i;
        account->playtime = 7654321 + i;
        account->look_a = 2 - i;
        account->look_b = 7 - i;
        account->look_c = 3 - i;
    }
    command("infracheck save");
    CHECK(GetFileAttributesA("user/server.srk") == INVALID_FILE_ATTRIBUTES && GetLastError() == ERROR_FILE_NOT_FOUND);
    CHECK(sv_game_control_poll() == SV_SERVER_SAVE && !save_failed);
    check_account_file();
    sr_numofaccounts = 0;
    CHECK(sv_game_control_poll() == 0);
    check_account_file();
    locked = CreateFileA("user/server.srk", GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    CHECK(locked != INVALID_HANDLE_VALUE);
    command("infracheck save");
    CHECK(sv_game_control_poll() == SV_SERVER_SAVE && save_failed);
    CHECK(CloseHandle(locked));
    check_account_file();
    for (i = 0; i < 2; ++i) {
        sr_numofaccounts = i ? 65536 : -1;
        save_failed = 0;
        command("infracheck save");
        CHECK(sv_game_control_poll() == SV_SERVER_SAVE && save_failed);
        check_account_file();
    }
    locked = FindFirstFileA("user/server.srk.*.tmp", &found);
    CHECK(locked == INVALID_HANDLE_VALUE && GetLastError() == ERROR_FILE_NOT_FOUND);
    sr_accounts[0] = saved_accounts[0];
    sr_accounts[1] = saved_accounts[1];
    sr_numofaccounts = saved_count;
    sr_account_key = saved_key;
    CHECK(DeleteFileA("user/server.srk"));
    if (getenv("SV_TEST_RESTART") && !strcmp(getenv("SV_TEST_RESTART"), "1")) {
        command("infracheck restart");
        CHECK(!sv_game_control_stopping() && !sv_game_control_restarting());
        CHECK(sv_game_control_poll() == SV_SERVER_RESTART && sv_game_control_restarting());
    } else {
        command("infracheck stop");
        CHECK(!sv_game_control_stopping());
        CHECK(sv_game_control_poll() == SV_SERVER_STOP && !sv_game_control_restarting());
    }
    CHECK(sv_game_control_stopping());
    CHECK(sv_game_control_poll() == 0 && sv_game_control_stopping());
    sv_test_log_hook = previous_log;
    puts("PLUGIN_INFRA_PASS");
}

void infra_host_cleanup_check(void)
{
    previous_log = sv_test_log_hook;
    sv_test_log_hook = command_log;
    command("infracheck save");
    CHECK(sv_plugins_disable("test.read") == SV_OK);
    CHECK(!sv_game_control_poll() && !sv_game_control_stopping());
    CHECK(GetFileAttributesA("user/server.srk") == INVALID_FILE_ATTRIBUTES && GetLastError() == ERROR_FILE_NOT_FOUND);
    sv_test_log_hook = previous_log;
    puts("INFRA_CLEANUP_PASS");
}
