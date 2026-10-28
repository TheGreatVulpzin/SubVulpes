#include "systems_host.h"
#include "sv_game_storage.h"
#include <winioctl.h>
#include <wchar.h>

typedef struct { uint32_t magic, version, count; char id[48]; } TestHeader;
typedef struct {
    uint32_t scope, account[2], length;
    char name[32];
    uint8_t bytes[1024];
} TestRow;
typedef struct { uint8_t *bytes; DWORD length; } Blob;
static char private_root[MAX_PATH], storage_file[MAX_PATH];
static int account_player, second_account;

static void command(const char *arguments)
{
    char text[96];
    snprintf(text, sizeof text, "storage.check %s", arguments);
    sr_players[world_player].godmode = 0;
    CHECK(sv_plugins_command(0, text));
    CHECK(sr_players[world_player].godmode == 1);
    CHECK(!sv_plugins_failed());
}

static Blob read_file(const char *path)
{
    Blob result = {NULL, 0};
    HANDLE file = CreateFileA(path, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 0, NULL);
    DWORD read;
    CHECK(file != INVALID_HANDLE_VALUE);
    result.length = GetFileSize(file, NULL);
    CHECK(result.length != INVALID_FILE_SIZE && result.length < 200000);
    result.bytes = malloc(result.length + 1);
    CHECK(result.bytes != NULL);
    CHECK(ReadFile(file, result.bytes, result.length, &read, NULL) && read == result.length);
    CHECK(CloseHandle(file));
    return result;
}

static void write_file(const char *path, const void *bytes, DWORD length)
{
    HANDLE file = CreateFileA(path, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, 0, NULL);
    DWORD wrote;
    CHECK(file != INVALID_HANDLE_VALUE);
    CHECK(WriteFile(file, bytes, length, &wrote, NULL) && wrote == length);
    CHECK(FlushFileBuffers(file));
    CHECK(CloseHandle(file));
}

static void check_file(const Blob *expected)
{
    Blob actual = read_file(storage_file);
    CHECK(actual.length == expected->length);
    CHECK(!memcmp(actual.bytes, expected->bytes, actual.length));
    free(actual.bytes);
}

static void bad_file(const void *bytes, DWORD length, const Blob *valid)
{
    write_file(storage_file, bytes, length);
    CHECK(sv_storage_open(1, "test.systems", private_root) == SV_IO);
    command("inactive");
    write_file(storage_file, valid->bytes, valid->length);
    CHECK(sv_storage_open(1, "test.systems", private_root) == SV_OK);
    command("loaded");
    sv_storage_release_owner(1);
}

static void malformed_files(const Blob *valid)
{
    uint8_t *copy = malloc(valid->length + 1);
    TestHeader *header;
    TestRow *rows;
    uint32_t i;
    DWORD large_length = (DWORD)(sizeof(TestHeader) + 129 * sizeof(TestRow));
    uint8_t *large = calloc(1, large_length);
    CHECK(copy != NULL && large != NULL);
    CHECK(sizeof(TestHeader) == 60 && sizeof(TestRow) == 1072);
    memcpy(copy, valid->bytes, valid->length);
    header = (TestHeader *)copy;
    CHECK(header->count == 3);
    header->version = 2;
    bad_file(copy, valid->length, valid);
    memcpy(copy, valid->bytes, valid->length);
    header->magic ^= 1;
    bad_file(copy, valid->length, valid);
    memcpy(copy, valid->bytes, valid->length);
    strcpy(header->id, "test.other");
    bad_file(copy, valid->length, valid);
    memset(header->id, 'x', sizeof header->id);
    bad_file(copy, valid->length, valid);
    memcpy(copy, valid->bytes, valid->length);
    header->id[sizeof header->id - 1] = 0x7f;
    bad_file(copy, valid->length, valid);
    memcpy(copy, valid->bytes, valid->length);
    header->count = UINT32_MAX;
    bad_file(copy, valid->length, valid);
    bad_file(valid->bytes, (DWORD)sizeof(TestHeader) - 1, valid);
    bad_file(valid->bytes, valid->length - 1, valid);
    memcpy(copy, valid->bytes, valid->length);
    copy[valid->length] = 0x77;
    bad_file(copy, valid->length + 1, valid);
    rows = (TestRow *)(copy + sizeof(TestHeader));
    memcpy(copy, valid->bytes, valid->length);
    rows[0].length = SV_STORAGE_VALUE_MAX + 1;
    bad_file(copy, valid->length, valid);
    memcpy(copy, valid->bytes, valid->length);
    strcpy(rows[0].name, "bad/key");
    bad_file(copy, valid->length, valid);
    memcpy(copy, valid->bytes, valid->length);
    memset(rows[0].name, 'x', sizeof rows[0].name);
    bad_file(copy, valid->length, valid);
    memcpy(copy, valid->bytes, valid->length);
    rows[1] = rows[0];
    bad_file(copy, valid->length, valid);
    memcpy(copy, valid->bytes, valid->length);
    rows[0].scope = SV_STORAGE_SESSION;
    bad_file(copy, valid->length, valid);
    memcpy(copy, valid->bytes, valid->length);
    CHECK(rows[0].scope == SV_STORAGE_SERVER);
    rows[0].account[0] = 1;
    bad_file(copy, valid->length, valid);
    header = (TestHeader *)large;
    *header = (TestHeader){0x31535653u, 1, 129, "test.systems"};
    rows = (TestRow *)(large + sizeof *header);
    for (i = 0; i < 129; ++i) {
        rows[i].scope = SV_STORAGE_SERVER;
        snprintf(rows[i].name, sizeof rows[i].name, "row.%u", i);
    }
    /* Exact file length and valid empty rows isolate the count bound. */
    bad_file(large, large_length, valid);
    header->count = 65;
    for (i = 0; i < header->count; ++i)
        rows[i].length = SV_STORAGE_VALUE_MAX;
    bad_file(large, (DWORD)(sizeof *header + header->count * sizeof *rows), valid);
    free(large);
    free(copy);
}

static void junction(const char *path, const char *target)
{
    struct {
        DWORD tag;
        WORD length, reserved, substitute_offset, substitute_length, print_offset, print_length;
        WCHAR paths[2 * MAX_PATH + 4];
    } data;
    HANDLE directory;
    DWORD returned;
    int count;
    CHECK(CreateDirectoryA(path, NULL));
    directory = CreateFileA(path, GENERIC_WRITE, 0, NULL, OPEN_EXISTING,
        FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OPEN_REPARSE_POINT, NULL);
    CHECK(directory != INVALID_HANDLE_VALUE);
    memset(&data, 0, sizeof data);
    data.tag = IO_REPARSE_TAG_MOUNT_POINT;
    memcpy(data.paths, L"\\??\\", 4 * sizeof(WCHAR));
    count = MultiByteToWideChar(CP_ACP, 0, target, -1, data.paths + 4, MAX_PATH);
    CHECK(count > 0);
    memcpy(data.paths + 4 + count, data.paths + 4, (size_t)count * sizeof(WCHAR));
    data.substitute_length = (WORD)((count + 3) * sizeof(WCHAR));
    data.print_offset = (WORD)((count + 4) * sizeof(WCHAR));
    data.print_length = (WORD)((count - 1) * sizeof(WCHAR));
    data.length = (WORD)(8 + (4 + 2 * count) * sizeof(WCHAR));
    CHECK(DeviceIoControl(directory, FSCTL_SET_REPARSE_POINT, &data, (DWORD)data.length + 8,
        NULL, 0, &returned, NULL));
    CHECK(CloseHandle(directory));
}

static void path_hazards(const Blob *valid)
{
    char alias[MAX_PATH], folder[MAX_PATH], held[MAX_PATH];
    snprintf(alias, sizeof alias, "%s\\linked-root", private_root);
    junction(alias, private_root);
    CHECK(sv_storage_open(1, "test.systems", alias) == SV_IO);
    command("inactive");
    CHECK(RemoveDirectoryA(alias));
    snprintf(alias, sizeof alias, "%s\\test.link", private_root);
    snprintf(folder, sizeof folder, "%s\\test.systems", private_root);
    junction(alias, folder);
    CHECK(sv_storage_open(1, "test.link", private_root) == SV_IO);
    command("inactive");
    CHECK(RemoveDirectoryA(alias));
    snprintf(held, sizeof held, "%s\\test.systems\\storage.held", private_root);
    CHECK(MoveFileExA(storage_file, held, 0));
    CHECK(CreateHardLinkA(storage_file, held, NULL));
    CHECK(sv_storage_open(1, "test.systems", private_root) == SV_IO);
    command("inactive");
    CHECK(DeleteFileA(storage_file));
    CHECK(MoveFileExA(held, storage_file, 0));
    check_file(valid);
}

void storage_prepare(void)
{
    SvStorageKey entry = {sizeof entry, SV_STORAGE_SERVER, 0, "before"};
    SvStorageValue input = {sizeof input, 0, {0}};
    char cwd[MAX_PATH], missing_root[MAX_PATH], id[48];
    uint32_t i;
    DWORD length = GetCurrentDirectoryA(sizeof cwd, cwd);
    CHECK(length && length < sizeof cwd);
    /* The runner starts inside its temporary folder under build/server/tests. */
    snprintf(private_root, sizeof private_root, "%s\\storage-%lu-%lu", cwd,
        (unsigned long)GetCurrentProcessId(), (unsigned long)GetTickCount());
    CHECK(strlen(private_root) + 80 < MAX_PATH);
    CHECK(CreateDirectoryA(private_root, NULL));
    snprintf(storage_file, sizeof storage_file, "%s\\test.systems\\storage.bin", private_root);
    CHECK(sv_game_storage.get(1, &entry, &input) == SV_STATE);
    CHECK(sv_game_storage.put(1, &entry, &input) == SV_STATE);
    CHECK(sv_game_storage.erase(1, &entry) == SV_STATE);
    /* The host opened the storage of the test plugin when it started the plugin. Close it, so the cases start clean. */
    sv_storage_release_owner(1);
    CHECK(sv_storage_open(0, "test.systems", private_root) == SV_INVALID);
    CHECK(sv_storage_open(SV_PLUGIN_MAX + 1, "test.systems", private_root) == SV_INVALID);
    CHECK(sv_storage_open(1, NULL, private_root) == SV_INVALID);
    CHECK(sv_storage_open(1, "bad/id", private_root) == SV_INVALID);
    CHECK(sv_storage_open(1, "test.systems", NULL) == SV_INVALID);
    snprintf(missing_root, sizeof missing_root, "%s\\absent", private_root);
    CHECK(sv_storage_open(1, "test.systems", missing_root) == SV_IO);
    CHECK(sv_storage_open(1, "test.systems", private_root) == SV_OK);
    CHECK(sv_storage_open(1, "test.other", private_root) == SV_CONFLICT);
    CHECK(sv_storage_open(2, "test.systems", private_root) == SV_CONFLICT);
    for (i = 2; i <= SV_PLUGIN_MAX; ++i) {
        snprintf(id, sizeof id, "test.slot.%u", i);
        CHECK(sv_storage_open(i, id, private_root) == SV_OK);
    }
    for (i = 2; i <= SV_PLUGIN_MAX; ++i)
        sv_storage_release_owner(i);
    sv_storage_release_owner(0);
    sv_storage_leave(0);
    account_player = createplayer();
    CHECK(account_player >= 0);
    strcpy(sr_players[account_player].name, "Storage account");
    sr_players[account_player].account_id[0] = 789;
    sr_players[account_player].account_id[1] = 456;
    second_account = createaccount(account_player);
    CHECK(second_account >= 0 && second_account != world_account);
    CHECK(sr_players[account_player].account == -1);
}

/* The world clears the handles of all humans, items and vehicles at a reset. Then it gives a new handle to each live one. */
static void rebind_things(void)
{
    static const uint32_t kinds[3] = { SV_ENTITY_HUMAN, SV_ENTITY_ITEM, SV_ENTITY_OBJECT };
    uint32_t k, i;
    for (k = 0; k < 3; ++k) {
        sv_entities_clear(kinds[k]);
        for (i = 0; i < sv_entity_capacity(kinds[k]); ++i)
            sv_entity_created(kinds[k], i);
    }
}

void storage_verify(void)
{
    Blob valid = read_file(storage_file);
    HANDLE locked;
    SvEntity stale;
    uint32_t second_word = sr_accounts[second_account].id[1];
    CHECK(valid.length == sizeof(TestHeader) + 3 * sizeof(TestRow));
    locked = CreateFileA(storage_file, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 0, NULL);
    CHECK(locked != INVALID_HANDLE_VALUE);
    command("io");
    check_file(&valid);
    CHECK(CloseHandle(locked));
    stale = sv_entity_at(SV_ENTITY_PLAYER, account_player);
    CHECK(stale != 0);
    sv_entity_deleted(SV_ENTITY_PLAYER, (uint32_t)account_player);
    sv_entity_created(SV_ENTITY_PLAYER, (uint32_t)account_player);
    CHECK(sv_entity_resolve(SV_ENTITY_PLAYER, stale) == -1);
    sv_storage_leave(stale);
    command("left");
    check_file(&valid);
    /* Entity values: three things, then the death of each one, then a reset of the world. Nothing reaches the disk. */
    command("entity");
    check_file(&valid);
    sv_entity_deleted(SV_ENTITY_HUMAN, (uint32_t)world_human);
    sv_entity_created(SV_ENTITY_HUMAN, (uint32_t)world_human);
    sv_entity_deleted(SV_ENTITY_ITEM, (uint32_t)world_item);
    sv_entity_created(SV_ENTITY_ITEM, (uint32_t)world_item);
    sv_entity_deleted(SV_ENTITY_OBJECT, (uint32_t)world_object);
    sv_entity_created(SV_ENTITY_OBJECT, (uint32_t)world_object);
    command("entity_gone");
    check_file(&valid);
    command("entity");
    rebind_things();
    command("entity_gone");
    check_file(&valid);
    sv_storage_release_owner(1);
    command("inactive");
    /* Reorder owners so an owner-check removal exposes another plugin's rows. */
    CHECK(sv_storage_open(2, "test.systems", private_root) == SV_OK);
    CHECK(sv_storage_open(1, "test.isolated", private_root) == SV_OK);
    command("isolated");
    sv_storage_release_owner(1);
    sv_storage_release_owner(2);
    CHECK(sv_storage_open(1, "test.systems", private_root) == SV_OK);
    command("loaded");
    check_file(&valid);
    sr_accounts[second_account].id[1] ^= 0x55u;
    sv_entity_deleted(SV_ENTITY_ACCOUNT, (uint32_t)second_account);
    sv_entity_created(SV_ENTITY_ACCOUNT, (uint32_t)second_account);
    command("identity");
    sr_accounts[second_account].id[1] = second_word;
    sv_entity_deleted(SV_ENTITY_ACCOUNT, (uint32_t)second_account);
    sv_entity_created(SV_ENTITY_ACCOUNT, (uint32_t)second_account);
    command("loaded");
    sv_storage_release_owner(1);
    malformed_files(&valid);
    path_hazards(&valid);
    CHECK(sv_storage_open(1, "test.systems", private_root) == SV_OK);
    command("loaded");
    sv_storage_release_owner(1);
    free(valid.bytes);
    puts("STORAGE_PASS");
}
