#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include "systems_plugin.h"
#include "sv_game_storage.h"
#include "sv_game_write.h"

static const SvGameStorage *storage;
static const SvGameRead *read_api;
static const SvGameWrite *write_api;
static const SvHost *saved_host;
static SvEntity player, extra_player, account, extra_account;

static SvStorageKey key(uint32_t scope, SvEntity subject, const char *name)
{
    SvStorageKey result = {sizeof result, scope, subject, ""};
    strcpy(result.name, name);
    return result;
}

static SvStorageValue value(uint8_t byte, uint32_t length)
{
    SvStorageValue result = {sizeof result, length, {0}};
    memset(result.bytes, byte, sizeof result.bytes);
    return result;
}

static int lookup(SvOwner owner, uint32_t kind, const char *name, SvEntity *out)
{
    SvEntity entities[SV_ENTITY_PAGE_MAX];
    uint32_t cursor = 0, count, next, i;
    *out = 0;
    do {
        STEP(read_api->entities(owner, kind, cursor, entities, SV_ENTITY_PAGE_MAX, &count, &next) == SV_OK);
        for (i = 0; i < count; ++i) {
            if (kind == SV_ENTITY_PLAYER) {
                SvPlayerState state = {sizeof state};
                STEP(read_api->player(owner, entities[i], &state) == SV_OK);
                if (!strcmp(state.name, name))
                    *out = entities[i];
            } else {
                SvAccountState state = {sizeof state};
                STEP(read_api->account(owner, entities[i], &state) == SV_OK);
                if (!strcmp(state.name, name))
                    *out = entities[i];
            }
        }
        if (*out)
            return 0;
        if (next == cursor)
            break;
        cursor = next;
    } while (1);
    STEP(*out != 0);
    return 0;
}

static int missing(SvOwner owner, const SvStorageKey *entry, SvResult expected)
{
    SvStorageValue out, before;
    memset(&out, 0xa5, sizeof out);
    out.size = sizeof out;
    before = out;
    STEP(storage->get(owner, entry, &out) == expected);
    STEP(!memcmp(&out, &before, sizeof out));
    return 0;
}

static int present(SvOwner owner, const SvStorageKey *entry, uint8_t byte, uint32_t length)
{
    SvStorageValue out;
    uint32_t i;
    memset(&out, 0xa5, sizeof out);
    out.size = sizeof out;
    STEP(storage->get(owner, entry, &out) == SV_OK);
    STEP(out.size == sizeof out && out.length == length);
    for (i = 0; i < length; ++i)
        STEP(out.bytes[i] == byte);
    for (; i < sizeof out.bytes; ++i)
        STEP(out.bytes[i] == 0);
    return 0;
}

static DWORD WINAPI wrong_thread(void *data)
{
    SvOwner owner = *(SvOwner *)data;
    STEP(missing(owner, NULL, SV_STATE) == 0);
    STEP(storage->put(owner, NULL, NULL) == SV_STATE);
    STEP(storage->erase(owner, NULL) == SV_STATE);
    return 0;
}

static int invalid_inputs(SvOwner owner)
{
    SvStorageKey entry = key(SV_STORAGE_SESSION, player, "missing");
    SvStorageValue input = value(7, 1), out, before;
    HANDLE thread;
    DWORD code;
    uint32_t i;
    const char *bad_names[] = {"", "bad/name", "bad\\name", "bad:name", "bad name"};
    thread = CreateThread(NULL, 0, wrong_thread, &owner, 0, NULL);
    STEP(thread != NULL);
    STEP(WaitForSingleObject(thread, 5000) == WAIT_OBJECT_0);
    STEP(GetExitCodeThread(thread, &code));
    CloseHandle(thread);
    STEP(code == 0);
    STEP(missing(owner + 1, NULL, SV_STATE) == 0);
    STEP(storage->put(owner + 1, NULL, NULL) == SV_STATE);
    STEP(storage->erase(owner + 1, NULL) == SV_STATE);
    STEP(missing(owner, NULL, SV_INVALID) == 0);
    STEP(storage->put(owner, NULL, &input) == SV_INVALID);
    STEP(storage->erase(owner, NULL) == SV_INVALID);
    STEP(storage->get(owner, &entry, NULL) == SV_INVALID);
    memset(&out, 0xa5, sizeof out);
    out.size = sizeof out - 1;
    before = out;
    STEP(storage->get(owner, &entry, &out) == SV_INVALID);
    STEP(!memcmp(&out, &before, sizeof out));
    entry.size = sizeof entry - 1;
    STEP(missing(owner, &entry, SV_INVALID) == 0);
    STEP(storage->put(owner, &entry, &input) == SV_INVALID);
    STEP(storage->erase(owner, &entry) == SV_INVALID);
    entry.size = sizeof entry;
    for (i = 0; i < sizeof bad_names / sizeof bad_names[0]; ++i) {
        strcpy(entry.name, bad_names[i]);
        STEP(missing(owner, &entry, SV_INVALID) == 0);
        STEP(storage->put(owner, &entry, &input) == SV_INVALID);
        STEP(storage->erase(owner, &entry) == SV_INVALID);
    }
    memset(entry.name, 'x', sizeof entry.name);
    STEP(missing(owner, &entry, SV_INVALID) == 0);
    STEP(storage->put(owner, &entry, &input) == SV_INVALID);
    STEP(storage->erase(owner, &entry) == SV_INVALID);
    entry = key(0, player, "missing");
    for (i = 0; i < 2; ++i) {
        STEP(missing(owner, &entry, SV_INVALID) == 0);
        STEP(storage->put(owner, &entry, &input) == SV_INVALID);
        STEP(storage->erase(owner, &entry) == SV_INVALID);
        entry.scope = SV_STORAGE_SERVER + 1;
    }
    entry = key(SV_STORAGE_SERVER, player, "missing");
    STEP(missing(owner, &entry, SV_INVALID) == 0);
    STEP(storage->put(owner, &entry, &input) == SV_INVALID);
    STEP(storage->erase(owner, &entry) == SV_INVALID);
    entry = key(SV_STORAGE_SESSION, account, "missing");
    for (i = 0; i < 3; ++i) {
        STEP(missing(owner, &entry, SV_NOT_FOUND) == 0);
        STEP(storage->put(owner, &entry, &input) == SV_NOT_FOUND);
        STEP(storage->erase(owner, &entry) == SV_NOT_FOUND);
        entry.scope = i == 0 ? SV_STORAGE_ACCOUNT : SV_STORAGE_SESSION;
        entry.subject = i == 0 ? player : 0;
    }
    entry = key(SV_STORAGE_SESSION, player, "missing");
    STEP(missing(owner, &entry, SV_NOT_FOUND) == 0);
    STEP(storage->erase(owner, &entry) == SV_NOT_FOUND);
    STEP(storage->put(owner, &entry, NULL) == SV_INVALID);
    input.size = sizeof input - 1;
    STEP(storage->put(owner, &entry, &input) == SV_INVALID);
    input.size = sizeof input;
    input.length = SV_STORAGE_VALUE_MAX + 1;
    STEP(storage->put(owner, &entry, &input) == SV_INVALID);
    STEP(missing(owner, &entry, SV_NOT_FOUND) == 0);
    return 0;
}

static int quotas(SvOwner owner)
{
    SvStorageKey entry = key(SV_STORAGE_SESSION, player, "quota");
    SvStorageValue input = value(0x73, SV_STORAGE_VALUE_MAX);
    uint32_t i;
    for (i = 0; i < SV_STORAGE_BYTES_MAX / SV_STORAGE_VALUE_MAX; ++i) {
        snprintf(entry.name, sizeof entry.name, "bytes.%u", i);
        STEP(storage->put(owner, &entry, &input) == SV_OK);
        STEP(present(owner, &entry, 0x73, SV_STORAGE_VALUE_MAX) == 0);
    }
    strcpy(entry.name, "bytes.overflow");
    input.length = 1;
    STEP(storage->put(owner, &entry, &input) == SV_LIMIT);
    STEP(missing(owner, &entry, SV_NOT_FOUND) == 0);
    input.length = SV_STORAGE_VALUE_MAX;
    strcpy(entry.name, "bytes.0");
    STEP(storage->put(owner, &entry, &input) == SV_OK);
    input = value(0x29, 1);
    STEP(storage->put(owner, &entry, &input) == SV_OK);
    STEP(present(owner, &entry, 0x29, 1) == 0);
    input.length = 0;
    STEP(storage->put(owner, &entry, &input) == SV_OK);
    STEP(present(owner, &entry, 0, 0) == 0);
    for (i = 0; i < SV_STORAGE_BYTES_MAX / SV_STORAGE_VALUE_MAX; ++i) {
        snprintf(entry.name, sizeof entry.name, "bytes.%u", i);
        STEP(storage->erase(owner, &entry) == SV_OK);
    }
    for (i = 0; i < SV_STORAGE_ENTRY_MAX; ++i) {
        snprintf(entry.name, sizeof entry.name, "entry.%u", i);
        STEP(storage->put(owner, &entry, &input) == SV_OK);
    }
    strcpy(entry.name, "entry.overflow");
    STEP(storage->put(owner, &entry, &input) == SV_LIMIT);
    STEP(missing(owner, &entry, SV_NOT_FOUND) == 0);
    strcpy(entry.name, "entry.0");
    STEP(storage->put(owner, &entry, &input) == SV_OK);
    STEP(present(owner, &entry, 0, 0) == 0);
    for (i = 0; i < SV_STORAGE_ENTRY_MAX; ++i) {
        snprintf(entry.name, sizeof entry.name, "entry.%u", i);
        STEP(storage->erase(owner, &entry) == SV_OK);
    }
    memset(entry.name, 'A', sizeof entry.name - 1);
    entry.name[sizeof entry.name - 1] = 0;
    STEP(storage->put(owner, &entry, &input) == SV_OK);
    STEP(storage->erase(owner, &entry) == SV_OK);
    strcpy(entry.name, "Az09._-");
    STEP(storage->put(owner, &entry, &input) == SV_OK);
    STEP(storage->erase(owner, &entry) == SV_OK);
    return 0;
}

static int persistent(SvOwner owner, int create)
{
    SvStorageKey entries[3];
    uint32_t i;
    STEP(lookup(owner, SV_ENTITY_ACCOUNT, "Systems test", &account) == 0);
    STEP(lookup(owner, SV_ENTITY_ACCOUNT, "Storage account", &extra_account) == 0);
    entries[0] = key(SV_STORAGE_SERVER, 0, "shared");
    entries[1] = key(SV_STORAGE_ACCOUNT, account, "shared");
    entries[2] = key(SV_STORAGE_ACCOUNT, extra_account, "shared");
    for (i = 0; i < 3; ++i) {
        if (create) {
            SvStorageValue input = value((uint8_t)(11 + i), i + 1);
            STEP(storage->put(owner, &entries[i], &input) == SV_OK);
        }
        STEP(present(owner, &entries[i], (uint8_t)(11 + i), i + 1) == 0);
    }
    return 0;
}

static int command_checks(SvOwner owner, const char *arguments)
{
    SvStorageKey entry = key(SV_STORAGE_SERVER, 0, "shared");
    SvStorageValue input = value(99, 4);
    uint32_t i;
    if (!strcmp(arguments, "inactive")) {
        STEP(missing(owner, NULL, SV_STATE) == 0);
        STEP(storage->put(owner, NULL, NULL) == SV_STATE);
        STEP(storage->erase(owner, NULL) == SV_STATE);
    } else if (!strcmp(arguments, "io")) {
        STEP(storage->put(owner, &entry, &input) == SV_IO);
        STEP(present(owner, &entry, 11, 1) == 0);
        STEP(storage->erase(owner, &entry) == SV_IO);
        STEP(present(owner, &entry, 11, 1) == 0);
    } else if (!strcmp(arguments, "left")) {
        entry = key(SV_STORAGE_SESSION, extra_player, "shared");
        STEP(missing(owner, &entry, SV_NOT_FOUND) == 0);
        STEP(storage->put(owner, &entry, &input) == SV_NOT_FOUND);
        STEP(storage->erase(owner, &entry) == SV_NOT_FOUND);
        STEP(lookup(owner, SV_ENTITY_PLAYER, "Storage account", &extra_player) == 0);
        entry.subject = extra_player;
        STEP(missing(owner, &entry, SV_NOT_FOUND) == 0);
        input.length = 0;
        /* Three persistent rows and one other session leave 124 free entries. */
        for (i = 0; i < SV_STORAGE_ENTRY_MAX - 4; ++i) {
            snprintf(entry.name, sizeof entry.name, "left.%u", i);
            STEP(storage->put(owner, &entry, &input) == SV_OK);
        }
        strcpy(entry.name, "left.overflow");
        STEP(storage->put(owner, &entry, &input) == SV_LIMIT);
        for (i = 0; i < SV_STORAGE_ENTRY_MAX - 4; ++i) {
            snprintf(entry.name, sizeof entry.name, "left.%u", i);
            STEP(storage->erase(owner, &entry) == SV_OK);
        }
        entry = key(SV_STORAGE_SESSION, player, "shared");
        STEP(present(owner, &entry, 41, 1) == 0);
    } else if (!strcmp(arguments, "isolated")) {
        STEP(missing(owner, &entry, SV_NOT_FOUND) == 0);
        entry = key(SV_STORAGE_ACCOUNT, account, "shared");
        STEP(missing(owner, &entry, SV_NOT_FOUND) == 0);
        entry = key(SV_STORAGE_SERVER, 0, "shared");
        STEP(storage->put(owner, &entry, &input) == SV_OK);
        STEP(present(owner, &entry, 99, 4) == 0);
    } else if (!strcmp(arguments, "loaded")) {
        STEP(persistent(owner, 0) == 0);
        entry = key(SV_STORAGE_SESSION, player, "shared");
        STEP(missing(owner, &entry, SV_NOT_FOUND) == 0);
        entry.subject = extra_player;
        STEP(missing(owner, &entry, SV_NOT_FOUND) == 0);
    } else if (!strcmp(arguments, "identity")) {
        entry = key(SV_STORAGE_ACCOUNT, extra_account, "shared");
        STEP(missing(owner, &entry, SV_NOT_FOUND) == 0);
        STEP(lookup(owner, SV_ENTITY_ACCOUNT, "Storage account", &extra_account) == 0);
        entry.subject = extra_account;
        STEP(missing(owner, &entry, SV_NOT_FOUND) == 0);
        entry.subject = account;
        STEP(present(owner, &entry, 12, 2) == 0);
    } else {
        return __LINE__;
    }
    return 0;
}

static SvResult SV_CALL command(SvOwner owner, uint64_t actor, const char *arguments)
{
    int line = command_checks(owner, arguments);
    char message[96];
    (void)actor;
    if (line) {
        snprintf(message, sizeof message, "Storage command %s failed at line %d.", arguments, line);
        saved_host->log(owner, message);
        return SV_FAILED;
    }
    return write_api->set_godmode(owner, player, 1);
}

int storage_fixture(const SvHost *host, SvOwner owner)
{
    const void *table = NULL;
    SvStorageKey entry;
    SvStorageValue input = value(41, 1);
    saved_host = host;
    STEP(host->get_api(owner, SV_API_STORAGE, SV_GAME_STORAGE_VERSION, &table) == SV_OK);
    storage = table;
    STEP(storage && storage->size >= sizeof *storage && storage->version == SV_GAME_STORAGE_VERSION);
    STEP(host->get_api(owner, SV_API_GAME_READ, SV_GAME_READ_VERSION, &table) == SV_OK);
    read_api = table;
    STEP(host->get_api(owner, SV_API_GAME_WRITE, SV_GAME_WRITE_VERSION, &table) == SV_OK);
    write_api = table;
    STEP(lookup(owner, SV_ENTITY_PLAYER, "Systems test", &player) == 0);
    STEP(lookup(owner, SV_ENTITY_PLAYER, "Storage account", &extra_player) == 0);
    STEP(lookup(owner, SV_ENTITY_ACCOUNT, "Systems test", &account) == 0);
    STEP(invalid_inputs(owner) == 0);
    STEP(quotas(owner) == 0);
    STEP(persistent(owner, 1) == 0);
    entry = key(SV_STORAGE_SERVER, 0, "erase");
    STEP(storage->put(owner, &entry, &input) == SV_OK);
    STEP(storage->erase(owner, &entry) == SV_OK);
    STEP(missing(owner, &entry, SV_NOT_FOUND) == 0);
    entry = key(SV_STORAGE_SESSION, player, "shared");
    STEP(storage->put(owner, &entry, &input) == SV_OK);
    STEP(present(owner, &entry, 41, 1) == 0);
    entry.subject = extra_player;
    input.bytes[0] = 42;
    STEP(storage->put(owner, &entry, &input) == SV_OK);
    STEP(present(owner, &entry, 42, 1) == 0);
    entry.subject = player;
    STEP(present(owner, &entry, 41, 1) == 0);
    STEP(persistent(owner, 0) == 0);
    STEP(host->command(owner, "storage.check", SV_COMMAND_LOCAL, command) == SV_OK);
    return 0;
}
