/* Check metadata and content approval before loading executable code. */
#include "plugin_internal.h"
#include <bcrypt.h>
#include <ctype.h>
#include <stdio.h>
#include <string.h>

#pragma comment(lib, "bcrypt.lib")

static int native_bypass;

void sv_plugins_native_bypass(int enabled)
{
    native_bypass = enabled != 0;
}

static int plain_path(const char *path, int directory)
{
    DWORD attributes = GetFileAttributesA(path);
    return attributes != INVALID_FILE_ATTRIBUTES && !(attributes & FILE_ATTRIBUTE_REPARSE_POINT) &&
           !!(attributes & FILE_ATTRIBUTE_DIRECTORY) == directory;
}

int sv_plugin_id(const char *text)
{
    char stem[SV_PLUGIN_ID_SIZE], *dot;
    size_t i, length = text ? strlen(text) : 0;
    if (!length || length >= SV_PLUGIN_ID_SIZE || text[length - 1] == '.' ||
        !isalnum((unsigned char)text[0]) || strstr(text, ".."))
        return 0;
    for (i = 0; i < length; ++i)
        if (!(text[i] >= 'a' && text[i] <= 'z') && !(text[i] >= '0' && text[i] <= '9') &&
            text[i] != '.' && text[i] != '_' && text[i] != '-')
            return 0;
    strcpy(stem, text);
    dot = strchr(stem, '.');
    if (dot)
        *dot = 0;
    if (!strcmp(stem, "con") || !strcmp(stem, "prn") || !strcmp(stem, "aux") || !strcmp(stem, "nul") ||
        (strlen(stem) == 4 && (!strncmp(stem, "com", 3) || !strncmp(stem, "lpt", 3)) &&
         stem[3] >= '1' && stem[3] <= '9'))
        return 0;
    return 1;
}

static int absolute(const char *relative, char path[MAX_PATH])
{
    DWORD n = GetFullPathNameA(relative, MAX_PATH, path, NULL);
    return n && n < MAX_PATH;
}

static int value(const char *path, const char *key, char *out, DWORD size)
{
    char pending[512];
    DWORD n = GetPrivateProfileStringA("plugin", key, "", pending, sizeof pending, path);
    if (!n || n >= sizeof pending - 1 || n >= size)
        return 0;
    memcpy(out, pending, n + 1);
    return 1;
}

static int metadata(const char *id, SvModule *module)
{
    char folder[MAX_PATH], manifest[MAX_PATH], library[SV_PLUGIN_ID_SIZE];
    char number[16], dependencies[512], *next, *part;
    DWORD n;
    size_t i;
    if (!sv_plugin_id(id))
        return 0;
    memset(module, 0, sizeof *module);
    strcpy(module->id, id);
    module->lock = INVALID_HANDLE_VALUE;
    if (!plain_path("plugins", 1))
        return 0;
    snprintf(folder, sizeof folder, "plugins/%s", id);
    snprintf(manifest, sizeof manifest, "%s/plugin.ini", folder);
    if (!plain_path(folder, 1) || !plain_path(manifest, 0) || !absolute(manifest, module->path))
        return 0;
    if (!value(module->path, "id", library, sizeof library) || strcmp(library, id) ||
        !value(module->path, "version", module->version, sizeof module->version))
        return 0;
    for (i = 0; module->version[i]; ++i)
        if (!isalnum((unsigned char)module->version[i]) && module->version[i] != '.' &&
            module->version[i] != '-' && module->version[i] != '+')
            return 0;
    if (!value(module->path, "api", number, sizeof number) || strcmp(number, "1"))
        return 0;
    if (!value(module->path, "live", number, sizeof number) || (strcmp(number, "0") && strcmp(number, "1")))
        return 0;
    module->live = number[0] == '1';
    if (!value(module->path, "library", library, sizeof library) || !sv_plugin_id(library) ||
        strlen(library) < 5 || strcmp(library + strlen(library) - 4, ".dll"))
        return 0;
    n = GetPrivateProfileStringA("plugin", "dependencies", "", dependencies, sizeof dependencies, module->path);
    if (n >= sizeof dependencies - 1)
        return 0;
    if (n) {
        part = dependencies;
        for (;;) {
            next = strchr(part, ',');
            if (next)
                *next = 0;
            if (!sv_plugin_id(part) || !strcmp(part, id) || module->dependency_count == SV_PLUGIN_DEP_MAX)
                return 0;
            for (i = 0; i < module->dependency_count; ++i)
                if (!strcmp(module->dependencies[i], part))
                    return 0;
            strcpy(module->dependencies[module->dependency_count++], part);
            if (!next)
                break;
            part = next + 1;
        }
    }
    snprintf(manifest, sizeof manifest, "%s/%s", folder, library);
    if (!plain_path(manifest, 0) || !absolute(manifest, module->path))
        return 0;
    module->metadata_valid = 1;
    return 1;
}

int sv_plugin_profile(SvModule *modules, uint32_t *count)
{
    char path[MAX_PATH], section[4096], *entry, *setting;
    DWORD n, attributes = GetFileAttributesA("plugins");
    uint32_t i;
    *count = 0;
    if (attributes == INVALID_FILE_ATTRIBUTES && GetLastError() == ERROR_FILE_NOT_FOUND)
        return 1;
    if (!plain_path("plugins", 1) || !plain_path("plugins/plugins.ini", 0) ||
        !absolute("plugins/plugins.ini", path))
        return 0;
    n = GetPrivateProfileSectionA("plugins", section, sizeof section, path);
    if (n >= sizeof section - 2)
        return 0;
    for (entry = section; *entry; entry += strlen(entry) + 1) {
        char line[128];
        SvModule *module;
        if (*count == SV_PLUGIN_MAX || strlen(entry) >= sizeof line)
            return 0;
        strcpy(line, entry);
        setting = strchr(line, '=');
        if (!setting)
            return 0;
        *setting++ = 0;
        if (!sv_plugin_id(line) || (strcmp(setting, "required") && strcmp(setting, "enabled") &&
                                    strcmp(setting, "disabled")))
            return 0;
        for (i = 0; i < *count; ++i)
            if (!strcmp(modules[i].id, line))
                return 0;
        module = &modules[(*count)++];
        if (!metadata(line, module)) {
            module->state = SV_MODULE_FAILED;
            strcpy(module->reason, "Invalid or missing plugin metadata, binary, or ABI requirement.");
        }
        module->required = !strcmp(setting, "required");
        module->wanted = strcmp(setting, "disabled") != 0;
        if (GetFileAttributesA("user/plugins/state.ini") != INVALID_FILE_ATTRIBUTES) {
            char state_path[MAX_PATH], state[16];
            if (!plain_path("user", 1) || !plain_path("user/plugins", 1) ||
                !plain_path("user/plugins/state.ini", 0) || !absolute("user/plugins/state.ini", state_path))
                return 0;
            n = GetPrivateProfileStringA("plugins", module->id, "", state, sizeof state, state_path);
            if (n && strcmp(state, "enabled") && strcmp(state, "disabled"))
                return 0;
            if (n) {
                if (module->required && !strcmp(state, "disabled"))
                    return 0;
                module->wanted = !strcmp(state, "enabled");
            }
        }
    }
    return 1;
}

static int hash_file(HANDLE file, char output[65])
{
    BCRYPT_ALG_HANDLE algorithm = NULL;
    BCRYPT_HASH_HANDLE hash = NULL;
    unsigned char bytes[4096], digest[32];
    DWORD n;
    int ok = 0, i;
    if (BCryptOpenAlgorithmProvider(&algorithm, BCRYPT_SHA256_ALGORITHM, NULL, 0) < 0 ||
        BCryptCreateHash(algorithm, &hash, NULL, 0, NULL, 0, 0) < 0)
        goto done;
    for (;;) {
        if (!ReadFile(file, bytes, sizeof bytes, &n, NULL))
            goto done;
        if (!n)
            break;
        if (BCryptHashData(hash, bytes, n, 0) < 0)
            goto done;
    }
    if (BCryptFinishHash(hash, digest, sizeof digest, 0) < 0)
        goto done;
    for (i = 0; i < 32; ++i)
        sprintf(output + i * 2, "%02x", digest[i]);
    ok = 1;
done:
    if (hash)
        BCryptDestroyHash(hash);
    if (algorithm)
        BCryptCloseAlgorithmProvider(algorithm, 0);
    return ok;
}

static int open_binary(SvModule *module)
{
    LARGE_INTEGER size;
    BY_HANDLE_FILE_INFORMATION info;
    unsigned char header[64], pe[26];
    DWORD n, offset;
    module->lock = CreateFileA(module->path, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING,
                               FILE_FLAG_OPEN_REPARSE_POINT, NULL);
    if (module->lock == INVALID_HANDLE_VALUE)
        return 0;
    if (!GetFileInformationByHandle(module->lock, &info) ||
        (info.dwFileAttributes & (FILE_ATTRIBUTE_DIRECTORY | FILE_ATTRIBUTE_REPARSE_POINT)) ||
        !GetFileSizeEx(module->lock, &size) || size.QuadPart < 90 || size.QuadPart > 64 * 1024 * 1024 ||
        !ReadFile(module->lock, header, sizeof header, &n, NULL) || n != sizeof header ||
        header[0] != 'M' || header[1] != 'Z')
        goto fail;
    memcpy(&offset, header + 60, 4);
    if (offset > (uint64_t)size.QuadPart - sizeof pe ||
        SetFilePointer(module->lock, (LONG)offset, NULL, FILE_BEGIN) == INVALID_SET_FILE_POINTER ||
        !ReadFile(module->lock, pe, sizeof pe, &n, NULL) || n != sizeof pe ||
        memcmp(pe, "PE\0\0\x4c\x01", 6) || pe[24] != 0x0b || pe[25] != 0x01 || !(pe[23] & 0x20))
        goto fail;
    if (SetFilePointer(module->lock, 0, NULL, FILE_BEGIN) == INVALID_SET_FILE_POINTER ||
        !hash_file(module->lock, module->hash))
        goto fail;
    return 1;
fail:
    CloseHandle(module->lock);
    module->lock = INVALID_HANDLE_VALUE;
    return 0;
}

int sv_plugin_approved(const SvModule *module)
{
    static const char *files[] = { "plugins/native-approvals.ini", "user/plugins/native-approvals.ini" };
    char path[MAX_PATH], hash[66];
    size_t i;
    for (i = 0; i < sizeof files / sizeof files[0]; ++i) {
        if (i == 1 && (!plain_path("user", 1) || !plain_path("user/plugins", 1)))
            continue;
        if (plain_path(files[i], 0) && absolute(files[i], path) &&
            GetPrivateProfileStringA("native", module->id, "", hash, sizeof hash, path) == 64 &&
            !strcmp(hash, module->hash))
            return 1;
    }
    return 0;
}

SvResult sv_plugin_load(SvModule *module)
{
    SvPluginEntry entry;
    SvResult result;
    if (!module->metadata_valid)
        return SV_INCOMPATIBLE;
    if (module->library)
        return module->validated ? SV_OK : SV_INCOMPATIBLE;
    if (!open_binary(module)) {
        strcpy(module->reason, "Cannot read a plain x86 DLL of at most 64 MiB.");
        return SV_IO;
    }
    if (!sv_plugin_approved(module) && !native_bypass) {
        strcpy(module->reason, "Native approval required. Inspect and approve this exact SHA-256 before loading.");
        sv_log("Native plugin %s needs approval: %s", module->id, module->hash);
        sv_plugin_unload(module);
        return SV_DENIED;
    }
    if (native_bypass)
        sv_log("WARNING: native approval bypass is active for %s. Full process access and crash risk remain.", module->id);
    /* Adjacent helper DLLs have no content approval in this ABI. */
    module->library = LoadLibraryExA(module->path, NULL, LOAD_LIBRARY_SEARCH_SYSTEM32);
    if (!module->library) {
        snprintf(module->reason, sizeof module->reason, "DLL load failed (%lu).", GetLastError());
        sv_plugin_unload(module);
        return SV_IO;
    }
    *(FARPROC *)&entry = GetProcAddress(module->library, "sv_plugin_entry");
    if (!entry) {
        strcpy(module->reason, "Missing sv_plugin_entry export.");
        return SV_INCOMPATIBLE;
    }
    memset(&module->plugin, 0, sizeof module->plugin);
    module->plugin.size = sizeof module->plugin;
    result = entry(SV_PLUGIN_ABI, &module->plugin);
    if (result != SV_OK || module->plugin.size != sizeof module->plugin ||
        module->plugin.abi != SV_PLUGIN_ABI || !memchr(module->plugin.id, 0, sizeof module->plugin.id) ||
        !memchr(module->plugin.version, 0, sizeof module->plugin.version) ||
        strcmp(module->plugin.id, module->id) || strcmp(module->plugin.version, module->version) ||
        !module->plugin.enable || module->plugin.flags != (module->live ? SV_PLUGIN_LIVE : 0u)) {
        strcpy(module->reason, "Plugin descriptor differs from its manifest or ABI.");
        return SV_INCOMPATIBLE;
    }
    module->validated = 1;
    return SV_OK;
}

void sv_plugin_unload(SvModule *module)
{
    if (module->library)
        FreeLibrary(module->library);
    if (module->lock && module->lock != INVALID_HANDLE_VALUE)
        CloseHandle(module->lock);
    module->library = NULL;
    module->validated = 0;
    module->lock = INVALID_HANDLE_VALUE;
}

int sv_plugin_inspect(const char *id, SvModule *module)
{
    if (!metadata(id, module) || !open_binary(module))
        return 0;
    sv_plugin_unload(module);
    return 1;
}

int sv_plugin_approve(const char *id, const char *hash)
{
    SvModule module;
    char path[MAX_PATH];
    if (!hash || strlen(hash) != 64 || !sv_plugin_inspect(id, &module) || strcmp(hash, module.hash))
        return 0;
    if (!CreateDirectoryA("user", NULL) && GetLastError() != ERROR_ALREADY_EXISTS)
        return 0;
    if (!plain_path("user", 1) ||
        (!CreateDirectoryA("user/plugins", NULL) && GetLastError() != ERROR_ALREADY_EXISTS) ||
        !plain_path("user/plugins", 1) || !absolute("user/plugins/native-approvals.ini", path))
        return 0;
    if (GetFileAttributesA(path) != INVALID_FILE_ATTRIBUTES && !plain_path(path, 0))
        return 0;
    return WritePrivateProfileStringA("native", id, hash, path) != 0;
}

int sv_plugin_save_state(const char *id, int enabled)
{
    char path[MAX_PATH];
    if (!sv_plugin_id(id))
        return 0;
    if (!CreateDirectoryA("user", NULL) && GetLastError() != ERROR_ALREADY_EXISTS)
        return 0;
    if (!plain_path("user", 1) ||
        (!CreateDirectoryA("user/plugins", NULL) && GetLastError() != ERROR_ALREADY_EXISTS) ||
        !plain_path("user/plugins", 1) || !absolute("user/plugins/state.ini", path))
        return 0;
    if (GetFileAttributesA(path) != INVALID_FILE_ATTRIBUTES && !plain_path(path, 0))
        return 0;
    return WritePrivateProfileStringA("plugins", id, enabled ? "enabled" : "disabled", path) != 0;
}

HANDLE sv_plugin_lock(void)
{
    HANDLE lock;
    BY_HANDLE_FILE_INFORMATION info;
    if (!CreateDirectoryA("user", NULL) && GetLastError() != ERROR_ALREADY_EXISTS)
        return INVALID_HANDLE_VALUE;
    if (!plain_path("user", 1) ||
        (!CreateDirectoryA("user/plugins", NULL) && GetLastError() != ERROR_ALREADY_EXISTS) ||
        !plain_path("user/plugins", 1))
        return INVALID_HANDLE_VALUE;
    lock = CreateFileA("user/plugins/host.lock", GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ, NULL,
        OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL | FILE_FLAG_OPEN_REPARSE_POINT, NULL);
    if (lock != INVALID_HANDLE_VALUE && (!GetFileInformationByHandle(lock, &info) ||
        (info.dwFileAttributes & (FILE_ATTRIBUTE_DIRECTORY | FILE_ATTRIBUTE_REPARSE_POINT)))) {
        CloseHandle(lock);
        return INVALID_HANDLE_VALUE;
    }
    return lock;
}
