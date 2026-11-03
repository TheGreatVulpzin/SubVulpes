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

int sv_plugin_version_min(const char *text)
{
    size_t i, length = text ? strlen(text) : 0;
    if (!length || length >= SV_PLUGIN_VERSION_SIZE || !isdigit((unsigned char)text[0]) || text[length - 1] == '.')
        return 0;
    for (i = 0; i < length; ++i) {
        if (text[i] == '.' ? text[i + 1] == '.' : !isdigit((unsigned char)text[i]))
            return 0;
    }
    return 1;
}

/* The groups of digits at the start of a version, up to eight. Text after the groups, such as "-beta", does not count. */
static unsigned version_groups(const char *text, unsigned long groups[8])
{
    unsigned count = 0;
    memset(groups, 0, 8 * sizeof groups[0]);
    while (count < 8 && isdigit((unsigned char)*text)) {
        unsigned long number = 0;
        while (isdigit((unsigned char)*text)) {
            if (number < 100000000ul)
                number = number * 10 + (unsigned long)(*text - '0');
            ++text;
        }
        groups[count++] = number;
        if (*text != '.' || !isdigit((unsigned char)text[1]))
            break;
        ++text;
    }
    return count;
}

int sv_plugin_version_at_least(const char *have, const char *want)
{
    unsigned long a[8], b[8];
    unsigned i;
    if (!version_groups(have, a))
        return 0;
    version_groups(want, b);
    for (i = 0; i < 8; ++i)
        if (a[i] != b[i])
            return a[i] > b[i];
    return 1;
}

static int absolute(const char *relative, char path[MAX_PATH])
{
    DWORD n = GetFullPathNameA(relative, MAX_PATH, path, NULL);
    return n && n < MAX_PATH;
}

typedef struct {
    SvModule *module;
    int print;
} ManifestReport;

static void manifest_problem(void *context, const char *key, const char *message)
{
    ManifestReport *report = context;
    if (!report->module->reason[0])
        snprintf(report->module->reason, sizeof report->module->reason, "%s: %s", key, message);
    if (report->print)
        sv_log("INVALID\t%s\t%s", key, message);
}

/* The icon is a plain PNG file of at most 128 KiB. It is square, from 64 to 512 pixels. The header decides, no decoder runs. */
static int icon_file_ok(const char *path)
{
    static const unsigned char signature[16] = { 0x89, 'P', 'N', 'G', 0x0d, 0x0a, 0x1a, 0x0a, 0, 0, 0, 13, 'I', 'H', 'D', 'R' };
    unsigned char head[24];
    BY_HANDLE_FILE_INFORMATION info;
    DWORD got;
    uint32_t width, height;
    int ok = 0;
    HANDLE file = CreateFileA(path, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_FLAG_OPEN_REPARSE_POINT, NULL);
    if (file == INVALID_HANDLE_VALUE)
        return 0;
    if (GetFileInformationByHandle(file, &info) && !(info.dwFileAttributes & (FILE_ATTRIBUTE_DIRECTORY | FILE_ATTRIBUTE_REPARSE_POINT)) &&
        !info.nFileSizeHigh && info.nFileSizeLow >= sizeof head && info.nFileSizeLow <= 131072 &&
        ReadFile(file, head, sizeof head, &got, NULL) && got == sizeof head && !memcmp(head, signature, sizeof signature)) {
        width = ((uint32_t)head[16] << 24) | ((uint32_t)head[17] << 16) | ((uint32_t)head[18] << 8) | head[19];
        height = ((uint32_t)head[20] << 24) | ((uint32_t)head[21] << 16) | ((uint32_t)head[22] << 8) | head[23];
        ok = width == height && width >= 64 && width <= 512;
    }
    CloseHandle(file);
    return ok;
}

static int metadata_folder(const char *folder, const char *id, SvModule *module, int print)
{
    char manifest[MAX_PATH], binary[MAX_PATH];
    SvManifest parsed;
    ManifestReport report = { module, print };
    int length;
    memset(module, 0, sizeof *module);
    module->lock = INVALID_HANDLE_VALUE;
    if (id)
        snprintf(module->id, sizeof module->id, "%s", id);
    length = snprintf(manifest, sizeof manifest, "%s/plugin.ini", folder);
    if (length < 0 || (size_t)length >= sizeof manifest || !plain_path(folder, 1) ||
        !plain_path(manifest, 0) || !absolute(manifest, module->path)) {
        manifest_problem(&report, "ini", "Cannot read a plain plugin folder and manifest.");
        return 0;
    }
    if (!sv_manifest_read(module->path, &parsed, manifest_problem, &report))
        return 0;
    if (id && strcmp(parsed.id, id)) {
        manifest_problem(&report, "id", "The manifest ID differs from its installed folder.");
        return 0;
    }
    strcpy(module->id, parsed.id);
    strcpy(module->version, parsed.version);
    strcpy(module->name, parsed.name);
    strcpy(module->description, parsed.description);
    strcpy(module->author, parsed.author);
    strcpy(module->homepage, parsed.homepage);
    module->runtime = parsed.runtime;
    module->cap_mask = parsed.cap_mask;
    module->live = parsed.live;
    module->unsafe = parsed.unsafe;
    module->library_role = parsed.library_role;
    module->dependency_count = parsed.dependency_count;
    module->conflict_count = parsed.conflict_count;
    memcpy(module->dependencies, parsed.dependencies, sizeof module->dependencies);
    memcpy(module->dependency_min, parsed.dependency_min, sizeof module->dependency_min);
    memcpy(module->conflicts, parsed.conflicts, sizeof module->conflicts);
    module->config = parsed.config;
    if (id)
        sv_config_load(&module->config, id);
    length = snprintf(binary, sizeof binary, "%s/%s", folder, parsed.library);
    if (length < 0 || (size_t)length >= sizeof binary || !plain_path(binary, 0) || !absolute(binary, module->path)) {
        manifest_problem(&report, "library", "Cannot read a plain plugin payload.");
        return 0;
    }
    /* The icon is for display only. The check runs when a folder or a package is validated, never when the plugin loads. */
    if (print && parsed.icon[0]) {
        char icon[MAX_PATH];
        length = snprintf(icon, sizeof icon, "%s/%s", folder, parsed.icon);
        if (length < 0 || (size_t)length >= sizeof icon || !plain_path(icon, 0) || !icon_file_ok(icon)) {
            manifest_problem(&report, "icon", "The icon must be a plain square PNG file, 64 to 512 pixels, up to 128 KiB.");
            return 0;
        }
    }
    module->metadata_valid = 1;
    return 1;
}

static int metadata(const char *id, SvModule *module)
{
    char folder[MAX_PATH];
    if (!sv_plugin_id(id) || !plain_path("plugins", 1)) {
        memset(module, 0, sizeof *module);
        module->lock = INVALID_HANDLE_VALUE;
        return 0;
    }
    snprintf(folder, sizeof folder, "plugins/%s", id);
    return metadata_folder(folder, id, module, 0);
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

int sv_plugin_validate_folder(const char *folder)
{
    SvModule module;
    ManifestReport report = { &module, 1 };
    char caps[256];
    const char *library;
    if (!metadata_folder(folder, NULL, &module, 1))
        return 1;
    if (module.runtime != SV_RUNTIME_NATIVE) {
        manifest_problem(&report, "runtime", "This runtime adapter is not available in this server.");
        return 1;
    }
    if (!open_binary(&module)) {
        manifest_problem(&report, "library", "Cannot read a plain x86 DLL of at most 64 MiB.");
        return 1;
    }
    sv_plugin_unload(&module);
    library = strrchr(module.path, '\\');
    if (!library)
        library = strrchr(module.path, '/');
    library = library ? library + 1 : module.path;
    if (!sv_manifest_caps_text(module.cap_mask, module.unsafe, caps, sizeof caps))
        return 1;
    sv_log("MANIFEST\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s", module.id, module.version, library,
        sv_manifest_runtime_name(module.runtime), caps, module.name, module.description, module.author, module.homepage);
    return 0;
}

/* One record binds the review to exact content and declared permissions. */
typedef struct {
    char version[24], runtime[8], caps[256], payload[65], manifest[65], bundle[65], time[21];
} SvApproval;

static int approval_hash(const char *text)
{
    size_t i;
    if (strlen(text) != 64)
        return 0;
    for (i = 0; i < 64; ++i)
        if (!((text[i] >= '0' && text[i] <= '9') || (text[i] >= 'a' && text[i] <= 'f')))
            return 0;
    return 1;
}

/* 0 means absent, -1 means unsafe, and 1 means a plain path. */
static int approval_path(char path[MAX_PATH], int create)
{
    static const char *folders[] = { "user", "user/plugins" };
    size_t i;
    DWORD attributes, error;
    for (i = 0; i < sizeof folders / sizeof folders[0]; ++i) {
        attributes = GetFileAttributesA(folders[i]);
        if (attributes == INVALID_FILE_ATTRIBUTES) {
            error = GetLastError();
            if (!create)
                return error == ERROR_FILE_NOT_FOUND || error == ERROR_PATH_NOT_FOUND ? 0 : -1;
            if (!CreateDirectoryA(folders[i], NULL) && GetLastError() != ERROR_ALREADY_EXISTS)
                return -1;
        }
        if (!plain_path(folders[i], 1))
            return -1;
    }
    if (!absolute("user/plugins/approvals.ini", path))
        return -1;
    attributes = GetFileAttributesA(path);
    if (attributes == INVALID_FILE_ATTRIBUTES) {
        error = GetLastError();
        return error == ERROR_FILE_NOT_FOUND || error == ERROR_PATH_NOT_FOUND ? (create ? 1 : 0) : -1;
    }
    return plain_path(path, 0) ? 1 : -1;
}

static int approval_time(const char *text)
{
    size_t i;
    if (strlen(text) != 20)
        return 0;
    for (i = 0; i < 20; ++i) {
        char separator = i == 4 || i == 7 ? '-' : i == 10 ? 'T' : i == 13 || i == 16 ? ':' : i == 19 ? 'Z' : 0;
        if (separator ? text[i] != separator : (text[i] < '0' || text[i] > '9'))
            return 0;
    }
    return 1;
}

static int approval_read(const char *id, SvApproval *saved)
{
    char path[MAX_PATH], record[640], *field[8], *next;
    char *destinations[] = { saved->version, saved->runtime, saved->caps, saved->payload,
                            saved->manifest, saved->bundle, saved->time };
    static const size_t capacities[] = { 24, 8, 256, 65, 65, 65, 21 };
    DWORD count;
    size_t i;
    int state = approval_path(path, 0);
    if (state <= 0)
        return state;
    count = GetPrivateProfileStringA("approvals", id, "!", record, sizeof record, path);
    if (!strcmp(record, "!")) {
        char present[2];
        /* A literal marker is an invalid record, not a missing approval. */
        return GetPrivateProfileStringA("approvals", id, "", present, sizeof present, path) ? -1 : 0;
    }
    if (!count || count >= sizeof record - 1)
        return -1;
    next = record;
    for (i = 0; i < 8; ++i) {
        field[i] = next;
        next = strchr(next, '|');
        if ((i < 7) != (next != NULL))
            return -1;
        if (next)
            *next++ = 0;
    }
    if (strcmp(field[0], "1"))
        return -1;
    memset(saved, 0, sizeof *saved);
    for (i = 0; i < 7; ++i) {
        if (!field[i + 1][0] || strlen(field[i + 1]) >= capacities[i])
            return -1;
        strcpy(destinations[i], field[i + 1]);
    }
    for (i = 0; saved->version[i]; ++i)
        if (!isalnum((unsigned char)saved->version[i]) && !strchr(".+-", saved->version[i]))
            return -1;
    if (strcmp(saved->runtime, "native") && strcmp(saved->runtime, "lua") && strcmp(saved->runtime, "wasm"))
        return -1;
    if (!approval_hash(saved->payload) || !approval_hash(saved->manifest) ||
        (strcmp(saved->bundle, "-") && !approval_hash(saved->bundle)) || !approval_time(saved->time))
        return -1;
    if (!strcmp(saved->caps, "-"))
        saved->caps[0] = 0;
    for (i = 0; saved->caps[i]; ++i)
        if (!(saved->caps[i] >= 'a' && saved->caps[i] <= 'z') && saved->caps[i] != '_' && saved->caps[i] != ',')
            return -1;
    if (!strcmp(saved->bundle, "-"))
        saved->bundle[0] = 0;
    return 1;
}

static int approval_manifest(const SvModule *module, char digest[65])
{
    char path[MAX_PATH], *base;
    HANDLE file;
    LARGE_INTEGER size;
    int ok;
    strcpy(path, module->path);
    base = strrchr(path, '\\');
    if (!base || (size_t)(base - path) + 1 + sizeof "plugin.ini" > sizeof path)
        return 0;
    strcpy(base + 1, "plugin.ini");
    if (!plain_path(path, 0))
        return 0;
    file = CreateFileA(path, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING,
                       FILE_FLAG_OPEN_REPARSE_POINT, NULL);
    if (file == INVALID_HANDLE_VALUE)
        return 0;
    ok = GetFileSizeEx(file, &size) && size.QuadPart > 0 && size.QuadPart <= 65536 && hash_file(file, digest);
    CloseHandle(file);
    return ok;
}

static int approval_matches(const SvApproval *saved, const SvModule *module)
{
    char caps[256], manifest[65];
    if (strcmp(saved->payload, module->hash))
        return 0;
    if (strcmp(saved->runtime, sv_manifest_runtime_name(module->runtime)))
        return 0;
    if (!sv_manifest_caps_text(module->cap_mask, module->unsafe, caps, sizeof caps) || strcmp(saved->caps, caps))
        return 0;
    if (!approval_manifest(module, manifest) || strcmp(saved->manifest, manifest))
        return 0;
    return 1;
}

int sv_plugin_trust_status(const char *id)
{
    SvApproval saved;
    int state;
    if (!sv_plugin_id(id))
        return 1;
    state = approval_read(id, &saved);
    if (state < 0)
        return 1;
    if (!state) {
        printf("TRUST_NONE\t%s\n", id);
        return 0;
    }
    printf("TRUST\t1\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\n", id, saved.version, saved.runtime,
           saved.caps, saved.payload, saved.manifest, saved.bundle, saved.time);
    return 0;
}

int sv_plugin_trust_save(const char *id, const char *payload, const char *manifest,
                         const char *runtime, const char *caps, const char *bundle)
{
    SvModule module;
    char path[MAX_PATH], current[65], declared[256], record[640];
    SYSTEMTIME now;
    int length;
    if (!approval_hash(payload) || !approval_hash(manifest) ||
        (strcmp(bundle, "-") && !approval_hash(bundle)) || !sv_plugin_inspect(id, &module) ||
        strcmp(payload, module.hash) || strcmp(runtime, sv_manifest_runtime_name(module.runtime)) ||
        !sv_manifest_caps_text(module.cap_mask, module.unsafe, declared, sizeof declared) ||
        strcmp(!strcmp(caps, "-") ? "" : caps, declared) ||
        !approval_manifest(&module, current) || strcmp(manifest, current) || approval_path(path, 1) != 1)
        return 0;
    GetSystemTime(&now);
    length = snprintf(record, sizeof record, "1|%s|%s|%s|%s|%s|%s|%04u-%02u-%02uT%02u:%02u:%02uZ",
                      module.version, runtime, declared[0] ? declared : "-", payload, manifest, bundle,
                      now.wYear, now.wMonth, now.wDay, now.wHour, now.wMinute, now.wSecond);
    if (length < 0 || (size_t)length >= sizeof record)
        return 0;
    return WritePrivateProfileStringA("approvals", id, record, path) != 0;
}

int sv_plugin_approved(const SvModule *module)
{
    static const char *files[] = { "plugins/native-approvals.ini", "user/plugins/native-approvals.ini" };
    char path[MAX_PATH], hash[66];
    SvApproval saved;
    int state = approval_read(module->id, &saved);
    size_t i;
    if (state)
        return state > 0 && approval_matches(&saved, module);
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
    if (module->runtime != SV_RUNTIME_NATIVE) {
        strcpy(module->reason, "This runtime adapter is not available in this server.");
        return SV_INCOMPATIBLE;
    }
    if (module->library)
        return module->validated ? SV_OK : SV_INCOMPATIBLE;
    if (!open_binary(module)) {
        strcpy(module->reason, "Cannot read a plain x86 DLL of at most 64 MiB.");
        return SV_IO;
    }
    if (!sv_plugin_approved(module) && !native_bypass) {
        strcpy(module->reason, "Native approval required. Inspect and approve this exact SHA-256 before loading.");
        sv_log("Plugin %s needs approval: %s", module->id, module->hash);
        sv_plugin_unload(module);
        return SV_DENIED;
    }
    if (native_bypass)
        sv_log("WARNING: %s is running without approval. Plugin approval bypass is enabled.", module->id);
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

/* The manifest only: the declaration and the saved values, with no read of the DLL. A running server holds the DLL, so this
 * is the call for a listing that must work beside it. */
int sv_plugin_declared(const char *id, SvModule *module)
{
    return metadata(id, module);
}

int sv_plugin_inspect(const char *id, SvModule *module)
{
    if (!metadata(id, module) || module->runtime != SV_RUNTIME_NATIVE || !open_binary(module))
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
    {
        char scoped[MAX_PATH];
        int state = approval_path(scoped, 0);
        if (state < 0 || !WritePrivateProfileStringA("native", id, hash, path))
            return 0;
        /* An explicit legacy approval keeps its full native-content meaning. */
        return !state || WritePrivateProfileStringA("approvals", id, NULL, scoped) != 0;
    }
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

int sv_plugin_private_file(const char *id, char path[MAX_PATH], int create)
{
    char relative[MAX_PATH];
    if (!sv_plugin_id(id))
        return 0;
    if (create && !CreateDirectoryA("user", NULL) && GetLastError() != ERROR_ALREADY_EXISTS)
        return 0;
    if (!plain_path("user", 1))
        return 0;
    if (create && !CreateDirectoryA("user/plugins", NULL) && GetLastError() != ERROR_ALREADY_EXISTS)
        return 0;
    if (!plain_path("user/plugins", 1))
        return 0;
    snprintf(relative, sizeof relative, "user/plugins/%s.settings.ini", id);
    if (!absolute(relative, path))
        return 0;
    return GetFileAttributesA(path) == INVALID_FILE_ATTRIBUTES || plain_path(path, 0);
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
