#include "plugin_internal.h"
#include "plugin_entities.h"
#include "sv_game_storage.h"
#include "plugin_game_internal.h"
#include "server.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* This private format does not contain game slots or session references. */
typedef struct {
    uint32_t scope, account[2], length;
    char name[SV_STORAGE_KEY_SIZE];
    uint8_t bytes[SV_STORAGE_VALUE_MAX];
} DiskRow;

typedef struct { DiskRow disk; SvEntity session; } Entry;
typedef struct { uint32_t magic, version, count; char id[SV_PLUGIN_ID_SIZE]; } Header;
typedef struct {
    SvOwner owner;
    char id[SV_PLUGIN_ID_SIZE], folder[MAX_PATH], file[MAX_PATH], temporary[MAX_PATH];
    HANDLE lock, directory;
    Entry *entries;
} Store;

static Store stores[SV_PLUGIN_MAX];
enum { STORAGE_MAGIC = 0x31535653u };

/* A session value and an entity value live in memory only, and they go when their subject goes. */
static int transient(uint32_t scope)
{
    return scope == SV_STORAGE_SESSION || scope == SV_STORAGE_ENTITY;
}

static int thing_exists(SvEntity subject)
{
    return sv_entity_resolve(SV_ENTITY_HUMAN, subject) >= 0 || sv_entity_resolve(SV_ENTITY_ITEM, subject) >= 0 ||
        sv_entity_resolve(SV_ENTITY_OBJECT, subject) >= 0;
}

static int name_valid(const char *name)
{
    size_t i, length = strnlen(name, SV_STORAGE_KEY_SIZE);
    if (!length || length >= SV_STORAGE_KEY_SIZE)
        return 0;
    for (i = 0; i < length; ++i)
        if (!(name[i] >= 'a' && name[i] <= 'z') && !(name[i] >= 'A' && name[i] <= 'Z') &&
            !(name[i] >= '0' && name[i] <= '9') && name[i] != '_' && name[i] != '-' && name[i] != '.')
            return 0;
    return 1;
}

static int plain_directory(const char *path)
{
    char prefix[MAX_PATH], *p;
    DWORD attributes;
    if (strnlen(path, MAX_PATH) >= MAX_PATH || path[1] != ':' || path[2] != '\\')
        return 0;
    strcpy(prefix, path);
    for (p = prefix + 3; ; ++p) {
        if (*p && *p != '\\')
            continue;
        {
            char saved = *p;
            *p = 0;
            attributes = GetFileAttributesA(prefix);
            *p = saved;
            if (attributes == INVALID_FILE_ATTRIBUTES || !(attributes & FILE_ATTRIBUTE_DIRECTORY) ||
                (attributes & FILE_ATTRIBUTE_REPARSE_POINT))
                return 0;
            if (!saved)
                break;
        }
    }
    return 1;
}

static int regular(HANDLE file)
{
    BY_HANDLE_FILE_INFORMATION info;
    return GetFileInformationByHandle(file, &info) && info.nNumberOfLinks == 1 &&
        !(info.dwFileAttributes & (FILE_ATTRIBUTE_DIRECTORY | FILE_ATTRIBUTE_REPARSE_POINT));
}

static Store *public_store(SvOwner owner)
{
    uint32_t i;
    for (i = 0; i < SV_PLUGIN_MAX; ++i)
        if (stores[i].owner == owner)
            return &stores[i];
    return NULL;
}

static SvResult context(SvOwner owner, Store **store)
{
    if (!sv_plugins_context(owner) || !sv_game_is_ready())
        return SV_STATE;
    *store = public_store(owner);
    return *store ? SV_OK : SV_STATE;
}

static SvResult key_copy(const SvStorageKey *key, Entry *out)
{
    int32_t index;
    if (!key || key->size < sizeof *key || key->scope < SV_STORAGE_SESSION || key->scope > SV_STORAGE_ENTITY ||
        !name_valid(key->name))
        return SV_INVALID;
    memset(out, 0, sizeof *out);
    out->disk.scope = key->scope;
    strcpy(out->disk.name, key->name);
    if (key->scope == SV_STORAGE_SERVER)
        return key->subject ? SV_INVALID : SV_OK;
    if (key->scope == SV_STORAGE_ENTITY) {
        if (!thing_exists(key->subject))
            return SV_NOT_FOUND;
        out->session = key->subject;
        return SV_OK;
    }
    index = sv_entity_resolve(key->scope == SV_STORAGE_SESSION ? SV_ENTITY_PLAYER : SV_ENTITY_ACCOUNT, key->subject);
    if (index < 0)
        return SV_NOT_FOUND;
    if (key->scope == SV_STORAGE_SESSION) {
        out->session = key->subject;
    } else {
        out->disk.account[0] = sr_accounts[index].id[0];
        out->disk.account[1] = sr_accounts[index].id[1];
    }
    return SV_OK;
}

static int same(const Entry *a, const Entry *b)
{
    return a->disk.scope == b->disk.scope && !strcmp(a->disk.name, b->disk.name) &&
        (!transient(a->disk.scope) || a->session == b->session) &&
        (a->disk.scope != SV_STORAGE_ACCOUNT ||
         (a->disk.account[0] == b->disk.account[0] && a->disk.account[1] == b->disk.account[1]));
}

static int find(const Entry *entries, const Entry *key)
{
    uint32_t i;
    for (i = 0; i < SV_STORAGE_ENTRY_MAX; ++i)
        if (entries[i].disk.scope && same(&entries[i], key))
            return (int)i;
    return -1;
}

static SvResult load(Store *store)
{
    Header header;
    HANDLE file;
    LARGE_INTEGER size;
    DWORD got;
    uint32_t i, bytes = 0;
    SvResult result = SV_IO;
    file = CreateFileA(store->file, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING,
        FILE_FLAG_OPEN_REPARSE_POINT, NULL);
    if (file == INVALID_HANDLE_VALUE)
        return GetLastError() == ERROR_FILE_NOT_FOUND ? SV_OK : SV_IO;
    if (!regular(file) || !GetFileSizeEx(file, &size) || size.QuadPart < (int64_t)sizeof header ||
        !ReadFile(file, &header, sizeof header, &got, NULL) || got != sizeof header ||
        header.magic != STORAGE_MAGIC || header.version != 1 || header.count > SV_STORAGE_ENTRY_MAX ||
        memcmp(header.id, store->id, sizeof header.id) ||
        size.QuadPart != (int64_t)(sizeof header + (uint64_t)header.count * sizeof(DiskRow)))
        goto done;
    for (i = 0; i < header.count; ++i) {
        Entry row;
        memset(&row, 0, sizeof row);
        if (!ReadFile(file, &row.disk, sizeof row.disk, &got, NULL) || got != sizeof row.disk ||
            (row.disk.scope != SV_STORAGE_ACCOUNT && row.disk.scope != SV_STORAGE_SERVER) ||
            !name_valid(row.disk.name) || row.disk.length > SV_STORAGE_VALUE_MAX ||
            (row.disk.scope == SV_STORAGE_SERVER && (row.disk.account[0] || row.disk.account[1])) ||
            row.disk.length > SV_STORAGE_BYTES_MAX - bytes || find(store->entries, &row) >= 0)
            goto done;
        bytes += row.disk.length;
        /* Remove unused bytes before any public read or later save. */
        memset(row.disk.bytes + row.disk.length, 0, SV_STORAGE_VALUE_MAX - row.disk.length);
        memset(row.disk.name + strlen(row.disk.name) + 1, 0, SV_STORAGE_KEY_SIZE - strlen(row.disk.name) - 1);
        store->entries[i] = row;
    }
    result = SV_OK;
done:
    CloseHandle(file);
    return result;
}

static SvResult save(const Store *store, uint32_t index, const Entry *replacement)
{
    Header header = {STORAGE_MAGIC, 1, 0, ""};
    HANDLE file;
    LARGE_INTEGER zero;
    DWORD wrote;
    uint32_t i;
    int okay;
    DWORD attributes;
    if (!plain_directory(store->folder))
        return SV_IO;
    attributes = GetFileAttributesA(store->file);
    if (attributes != INVALID_FILE_ATTRIBUTES && (attributes & (FILE_ATTRIBUTE_DIRECTORY | FILE_ATTRIBUTE_REPARSE_POINT)))
        return SV_IO;
    strcpy(header.id, store->id);
    for (i = 0; i < SV_STORAGE_ENTRY_MAX; ++i) {
        const Entry *entry = i == index ? replacement : &store->entries[i];
        if (entry->disk.scope && !transient(entry->disk.scope))
            ++header.count;
    }
    file = CreateFileA(store->temporary, GENERIC_READ | GENERIC_WRITE, 0, NULL, OPEN_ALWAYS,
        FILE_FLAG_OPEN_REPARSE_POINT | FILE_FLAG_WRITE_THROUGH, NULL);
    if (file == INVALID_HANDLE_VALUE)
        return SV_IO;
    zero.QuadPart = 0;
    if (!regular(file)) {
        CloseHandle(file);
        return SV_IO;
    }
    okay = SetFilePointerEx(file, zero, NULL, FILE_BEGIN) && SetEndOfFile(file) &&
        WriteFile(file, &header, sizeof header, &wrote, NULL) && wrote == sizeof header;
    for (i = 0; okay && i < SV_STORAGE_ENTRY_MAX; ++i) {
        const Entry *entry = i == index ? replacement : &store->entries[i];
        if (entry->disk.scope && !transient(entry->disk.scope))
            okay = WriteFile(file, &entry->disk, sizeof(DiskRow), &wrote, NULL) && wrote == sizeof(DiskRow);
    }
    if (okay)
        okay = FlushFileBuffers(file) != 0;
    if (!CloseHandle(file))
        okay = 0;
    if (okay)
        okay = MoveFileExA(store->temporary, store->file, MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH) != 0;
    if (!okay) {
        DeleteFileA(store->temporary);
        return SV_IO;
    }
    return SV_OK;
}

SvResult sv_storage_open(SvOwner owner, const char *id, const char *private_root)
{
    Store pending;
    uint32_t i, slot = SV_PLUGIN_MAX;
    char root[MAX_PATH], lock_path[MAX_PATH];
    DWORD n;
    SvResult result;
    BY_HANDLE_FILE_INFORMATION info;
    if (!owner || owner > SV_PLUGIN_MAX || !id || strnlen(id, SV_PLUGIN_ID_SIZE) >= SV_PLUGIN_ID_SIZE ||
        !sv_plugin_id(id) || !private_root || strnlen(private_root, MAX_PATH) >= MAX_PATH)
        return SV_INVALID;
    for (i = 0; i < SV_PLUGIN_MAX; ++i) {
        if (stores[i].owner && (stores[i].owner == owner || !strcmp(stores[i].id, id)))
            return SV_CONFLICT;
        if (!stores[i].owner && slot == SV_PLUGIN_MAX)
            slot = i;
    }
    if (slot == SV_PLUGIN_MAX)
        return SV_LIMIT;
    n = GetFullPathNameA(private_root, sizeof root, root, NULL);
    if (!n || n >= sizeof root || n + strlen(id) + 16 >= MAX_PATH)
        return SV_INVALID;
    if (!plain_directory(root))
        return SV_IO;
    memset(&pending, 0, sizeof pending);
    pending.lock = pending.directory = INVALID_HANDLE_VALUE;
    snprintf(pending.folder, sizeof pending.folder, "%s\\%s", root, id);
    if (!CreateDirectoryA(pending.folder, NULL) && GetLastError() != ERROR_ALREADY_EXISTS)
        return SV_IO;
    if (!plain_directory(pending.folder))
        return SV_IO;
    pending.directory = CreateFileA(pending.folder, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING,
        FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OPEN_REPARSE_POINT, NULL);
    if (pending.directory == INVALID_HANDLE_VALUE || !GetFileInformationByHandle(pending.directory, &info) ||
        !(info.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) || (info.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT))
        goto failed;
    snprintf(lock_path, sizeof lock_path, "%s\\storage.lock", pending.folder);
    pending.lock = CreateFileA(lock_path, GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ, NULL, OPEN_ALWAYS,
        FILE_FLAG_OPEN_REPARSE_POINT, NULL);
    if (pending.lock == INVALID_HANDLE_VALUE || !regular(pending.lock))
        goto failed;
    pending.entries = calloc(SV_STORAGE_ENTRY_MAX, sizeof *pending.entries);
    if (!pending.entries) {
        result = SV_LIMIT;
        goto cleanup;
    }
    pending.owner = owner;
    strcpy(pending.id, id);
    snprintf(pending.file, sizeof pending.file, "%s\\storage.bin", pending.folder);
    snprintf(pending.temporary, sizeof pending.temporary, "%s\\storage.tmp", pending.folder);
    result = load(&pending);
    if (result != SV_OK)
        goto cleanup;
    stores[slot] = pending;
    return SV_OK;
failed:
    result = SV_IO;
cleanup:
    if (pending.lock != INVALID_HANDLE_VALUE)
        CloseHandle(pending.lock);
    if (pending.directory != INVALID_HANDLE_VALUE)
        CloseHandle(pending.directory);
    free(pending.entries);
    return result;
}

void sv_storage_leave(SvEntity player)
{
    uint32_t i, j;
    if (!player)
        return;
    for (i = 0; i < SV_PLUGIN_MAX; ++i)
        if (stores[i].owner)
            for (j = 0; j < SV_STORAGE_ENTRY_MAX; ++j)
                if (transient(stores[i].entries[j].disk.scope) && stores[i].entries[j].session == player)
                    memset(&stores[i].entries[j], 0, sizeof(Entry));
}

void sv_storage_clear(uint32_t kind)
{
    uint32_t i, j;
    for (i = 0; i < SV_PLUGIN_MAX; ++i)
        if (stores[i].owner)
            for (j = 0; j < SV_STORAGE_ENTRY_MAX; ++j)
                if (transient(stores[i].entries[j].disk.scope) &&
                    sv_entity_resolve(kind, stores[i].entries[j].session) >= 0)
                    memset(&stores[i].entries[j], 0, sizeof(Entry));
}

void sv_storage_release_owner(SvOwner owner)
{
    uint32_t i;
    if (!owner)
        return;
    for (i = 0; i < SV_PLUGIN_MAX; ++i)
        if (stores[i].owner == owner) {
            CloseHandle(stores[i].lock);
            CloseHandle(stores[i].directory);
            free(stores[i].entries);
            memset(&stores[i], 0, sizeof stores[i]);
        }
}

static SvResult SV_CALL get(SvOwner owner, const SvStorageKey *key, SvStorageValue *out)
{
    Store *store;
    Entry normalized;
    int index;
    SvStorageValue value;
    SvResult result = context(owner, &store);
    if (result != SV_OK)
        return result;
    if (!out || out->size < sizeof *out)
        return SV_INVALID;
    result = key_copy(key, &normalized);
    if (result != SV_OK)
        return result;
    index = find(store->entries, &normalized);
    if (index < 0)
        return SV_NOT_FOUND;
    memset(&value, 0, sizeof value);
    value.size = sizeof value;
    value.length = store->entries[index].disk.length;
    memcpy(value.bytes, store->entries[index].disk.bytes, value.length);
    *out = value;
    return SV_OK;
}

static SvResult change(Store *store, const Entry *row, int erase)
{
    Entry replacement;
    uint32_t i, count = 0, bytes = 0;
    int index = find(store->entries, row), empty = -1;
    SvResult result;
    if (erase && index < 0)
        return SV_NOT_FOUND;
    for (i = 0; i < SV_STORAGE_ENTRY_MAX; ++i) {
        if (store->entries[i].disk.scope) {
            ++count;
            bytes += store->entries[i].disk.length;
        } else if (empty < 0) {
            empty = (int)i;
        }
    }
    if (!erase && index < 0 && count == SV_STORAGE_ENTRY_MAX)
        return SV_LIMIT;
    if (index >= 0)
        bytes -= store->entries[index].disk.length;
    else
        index = empty;
    if (!erase && row->disk.length > SV_STORAGE_BYTES_MAX - bytes)
        return SV_LIMIT;
    replacement = *row;
    if (erase)
        memset(&replacement, 0, sizeof replacement);
    result = transient(row->disk.scope) ? SV_OK : save(store, (uint32_t)index, &replacement);
    if (result == SV_OK)
        store->entries[index] = replacement;
    return result;
}

static SvResult SV_CALL put(SvOwner owner, const SvStorageKey *key, const SvStorageValue *value)
{
    Store *store;
    Entry normalized;
    SvResult result = context(owner, &store);
    if (result != SV_OK)
        return result;
    result = key_copy(key, &normalized);
    if (result != SV_OK)
        return result;
    if (!value || value->size < sizeof *value || value->length > SV_STORAGE_VALUE_MAX)
        return SV_INVALID;
    normalized.disk.length = value->length;
    memcpy(normalized.disk.bytes, value->bytes, value->length);
    return change(store, &normalized, 0);
}

static SvResult SV_CALL erase(SvOwner owner, const SvStorageKey *key)
{
    Store *store;
    Entry normalized;
    SvResult result = context(owner, &store);
    if (result != SV_OK)
        return result;
    result = key_copy(key, &normalized);
    return result == SV_OK ? change(store, &normalized, 1) : result;
}

const SvGameStorage sv_game_storage = {
    sizeof(SvGameStorage), SV_GAME_STORAGE_VERSION, get, put, erase
};
