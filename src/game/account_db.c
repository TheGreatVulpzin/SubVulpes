/* Account names and account files. */
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include "player.h"
#include "plugin_entities.h"
#include "plugin_game_events.h"
#include "sv_game_settings.h"

#include <stdio.h>
#include <string.h>

#include "logic.h"
#include "server.h"
#include "sr_os.h"
#include "sr_server.h"

#define g_parser sr_data_parser
#define g_sysinfo sr_data_sysinfo
typedef size_t (__cdecl *file_transfer)(void *, size_t, size_t, void *);
#define game_fread (*((file_transfer *)&sr_data_file_read))
#define game_fwrite (*((file_transfer *)&sr_data_file_write))

int account_nameexists(const char *name)
{
    size_t len = strlen(name);
    int i;
    if (len > 24)
        len = 24;
    for (i = 0; i < sr_numofaccounts; ++i)
        if (strncmp(name, sr_accounts[i].name, len) == 0)
            return 1;
    return 0;
}

int createaccount(int player)
{
    Player *p = &sr_players[player];
    Account *a;
    int i, k;
    size_t len;
    char suffix[32];

    for (i = 0; i < sr_numofaccounts; ++i) {
        a = &sr_accounts[i];
        if (p->account_id[0] != a->id[0])
            continue;
        if (p->account_id[1] != a->id[1])
            return -1;
        if (a->name_locked) {
            memcpy(p->name, a->name, 32);
        } else {
            memcpy(a->name, p->name, 32);
            a->name_locked = 5;
        }
        return i;
    }
    if (sr_numofaccounts > 0xfffe)
        return -1;
    a = &sr_accounts[i];
    memset(a, 0, sizeof(*a));
    a->id[0] = p->account_id[0];
    a->id[1] = p->account_id[1];
    len = strlen(p->name);
    if (len > 24)
        len = 24;
    for (k = 1; account_nameexists(p->name) && k < 1000; ++k) {
        p->name[len] = 0;
        sprintf(suffix, "(%d)", k);
        strcat(p->name, suffix);
        p->name[31] = 0;
    }
    memcpy(a->name, p->name, 32);
    a->money = sv_settings_native(SV_SETTING_NEW_ACCOUNT_MONEY, 1000);
    a->look_a = os_rand() % 3;
    a->look_b = os_rand() & 7;
    a->look_c = os_rand() & 3;
    ++sr_numofaccounts;
    sv_entity_created(SV_ENTITY_ACCOUNT, (uint32_t)i);
    return i;
}

static int account_write(HANDLE file, const void *data, DWORD size)
{
    DWORD written = 0;
    return WriteFile(file, data, size, &written, NULL) && written == size;
}

int sv_accounts_save(void)
{
    static LONG sequence;
    HANDLE directory, file = INVALID_HANDLE_VALUE;
    BY_HANDLE_FILE_INFORMATION info;
    DWORD attributes;
    uint32_t header[3];
    char temporary[96];
    int count = sr_numofaccounts, i, attempt, saved = 0;
    if ((unsigned)count > 65535u)
        return 0;
    /* Keep the directory in place until the replacement ends. */
    directory = CreateFileA("user", GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING,
        FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OPEN_REPARSE_POINT, NULL);
    if (directory == INVALID_HANDLE_VALUE)
        return 0;
    if (!GetFileInformationByHandle(directory, &info) ||
        !(info.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) || (info.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT))
        goto done;
    attributes = GetFileAttributesA("user/server.srk");
    if (attributes == INVALID_FILE_ATTRIBUTES) {
        if (GetLastError() != ERROR_FILE_NOT_FOUND)
            goto done;
    } else if (attributes & (FILE_ATTRIBUTE_DIRECTORY | FILE_ATTRIBUTE_REPARSE_POINT)) {
        goto done;
    }
    for (attempt = 0; attempt < 32; ++attempt) {
        snprintf(temporary, sizeof temporary, "user/server.srk.%lu.%lu.tmp",
            (unsigned long)GetCurrentProcessId(), (unsigned long)InterlockedIncrement(&sequence));
        file = CreateFileA(temporary, GENERIC_WRITE, 0, NULL, CREATE_NEW,
            FILE_ATTRIBUTE_NORMAL | FILE_FLAG_OPEN_REPARSE_POINT, NULL);
        if (file != INVALID_HANDLE_VALUE)
            break;
        if (GetLastError() != ERROR_FILE_EXISTS && GetLastError() != ERROR_ALREADY_EXISTS)
            goto done;
    }
    if (file == INVALID_HANDLE_VALUE)
        goto done;
    header[0] = 3;
    header[1] = sr_account_key;
    header[2] = (uint32_t)count;
    saved = account_write(file, header, sizeof header);
    for (i = 0; saved && i < count; ++i) {
        const Account *account = &sr_accounts[i];
        unsigned char record[60];
        memcpy(record, account->id, 8);
        memcpy(record + 8, account->name, 32);
        memcpy(record + 40, &account->money, 4);
        memcpy(record + 44, &account->playtime, 4);
        memcpy(record + 48, &account->look_a, 4);
        memcpy(record + 52, &account->look_b, 4);
        memcpy(record + 56, &account->look_c, 4);
        saved = account_write(file, record, sizeof record);
    }
    if (saved)
        saved = FlushFileBuffers(file) != 0;
    if (!CloseHandle(file))
        saved = 0;
    if (saved)
        saved = MoveFileExA(temporary, "user/server.srk", MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH) != 0;
    if (!saved)
        DeleteFileA(temporary);
done:
    CloseHandle(directory);
    if (saved)
        sv_game_event_account_save(0, SV_ACCOUNT_SAVE_DISK);
    return saved;
}

void saveaccountsserver(void)
{
    (void)sv_accounts_save();
}

void saveaccountsclient(void)
{
    int version = 1, i, j;
    char name[64];
    os_chdir(g_sysinfo.config_dir);
    sprintf(name, "client.srk");
    g_parser.file = os_fopen(name, "wb");
    if (g_parser.file) {
        game_fwrite(&version, 4, 1, g_parser.file);
        game_fwrite(&g_numofclientaccounts, 4, 1, g_parser.file);
        for (i = 0; i < g_numofclientaccounts; ++i)
            for (j = 0; j < 4; ++j)
                game_fwrite(&g_clientaccounts[i][j], 4, 1, g_parser.file);
        os_fclose(g_parser.file);
    }
    os_chdir(g_sysinfo.start_dir);
}

void loadaccountsclient(void)
{
    int version, i, j;
    char name[64];
    g_numofclientaccounts = 0;
    os_chdir(g_sysinfo.config_dir);
    sprintf(name, "client.srk");
    g_parser.file = os_fopen(name, "rb");
    if (g_parser.file) {
        game_fread(&version, 4, 1, g_parser.file);
        if (version == 1) {
            game_fread(&g_numofclientaccounts, 4, 1, g_parser.file);
            for (i = 0; i < g_numofclientaccounts; ++i)
                for (j = 0; j < 4; ++j)
                    game_fread(&g_clientaccounts[i][j], 4, 1, g_parser.file);
        }
        os_fclose(g_parser.file);
    }
    os_chdir(g_sysinfo.start_dir);
}
