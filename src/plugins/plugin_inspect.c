/* Read review metadata without loading plugin code or saving approval. */
#include "plugin_internal.h"
#include <bcrypt.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#pragma comment(lib, "bcrypt.lib")

static int plain_tree(const char *path)
{
    char full[MAX_PATH];
    DWORD length = GetFullPathNameA(path, sizeof full, full, NULL), attributes;
    size_t i;
    if (!length || length >= sizeof full)
        return 0;
    for (i = 0; i <= length; ++i) {
        if (!full[i] || (i > 2 && (full[i] == '\\' || full[i] == '/'))) {
            char saved = full[i];
            full[i] = 0;
            attributes = GetFileAttributesA(full);
            full[i] = saved;
            if (attributes == INVALID_FILE_ATTRIBUTES || (attributes & FILE_ATTRIBUTE_REPARSE_POINT))
                return 0;
        }
    }
    return 1;
}

/* Keep the file locked against replacement until the report is complete. */
static HANDLE digest_file(const char *path, LONGLONG limit, char output[65])
{
    HANDLE file = INVALID_HANDLE_VALUE;
    BCRYPT_ALG_HANDLE algorithm = NULL;
    BCRYPT_HASH_HANDLE hash = NULL;
    BY_HANDLE_FILE_INFORMATION info;
    LARGE_INTEGER size;
    unsigned char bytes[8192], digest[32];
    DWORD count;
    int i, ok = 0;
    if (!plain_tree(path))
        return INVALID_HANDLE_VALUE;
    file = CreateFileA(path, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING,
                       FILE_FLAG_OPEN_REPARSE_POINT, NULL);
    if (file == INVALID_HANDLE_VALUE)
        return file;
    if (!GetFileInformationByHandle(file, &info) ||
        (info.dwFileAttributes & (FILE_ATTRIBUTE_DIRECTORY | FILE_ATTRIBUTE_REPARSE_POINT)) ||
        !GetFileSizeEx(file, &size) || size.QuadPart <= 0 || size.QuadPart > limit ||
        BCryptOpenAlgorithmProvider(&algorithm, BCRYPT_SHA256_ALGORITHM, NULL, 0) < 0 ||
        BCryptCreateHash(algorithm, &hash, NULL, 0, NULL, 0, 0) < 0)
        goto done;
    for (;;) {
        if (!ReadFile(file, bytes, sizeof bytes, &count, NULL))
            goto done;
        if (!count)
            break;
        if (BCryptHashData(hash, bytes, count, 0) < 0)
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
    if (!ok) {
        CloseHandle(file);
        return INVALID_HANDLE_VALUE;
    }
    return file;
}

static void dangers(const SvManifest *manifest)
{
    static const struct {
        const char *name, *level, *text;
    } caps[] = {
        { "read", "low", "Reads game state and player data." },
        { "write", "high", "Changes the world and can kick players." },
        { "ai", "high", "Controls bots and their behavior." },
        { "economy", "high", "Changes money and the economy." },
        { "mission", "high", "Changes missions and their state." },
        { "settings", "high", "Changes server settings." },
        { "storage", "low", "Reads and writes plugin data." },
        { "engine", "high", "Controls engine operations." },
        { "shop", "high", "Changes shops and sales." },
        { "props", "high", "Changes player and world properties." },
        { "config", "low", "Reads plugin settings." },
        { "plugins", "high", "Uses APIs from other plugins." }
    };
    size_t i;
    for (i = 0; i < sizeof caps / sizeof caps[0]; ++i)
        if (manifest->cap_mask & (1u << i))
            printf("DANGER\t%s\t%s\t%s\n", caps[i].name, caps[i].level, caps[i].text);
    if (manifest->unsafe)
        puts("DANGER\tunsafe_events\thigh\tRuns callbacks inside the simulation. A fault can stop the server.");
    if (manifest->runtime == SV_RUNTIME_NATIVE)
        puts("DANGER\tnative\thighest\tNative code gets full process access. It can read private data, corrupt memory, hang or crash the server.");
}

static int inspect_folder(const char *folder, const char *expected_id)
{
    char path[MAX_PATH], absolute[MAX_PATH], manifest_hash[65], payload_hash[65];
    HANDLE metadata = INVALID_HANDLE_VALUE, payload = INVALID_HANDLE_VALUE;
    SvManifest manifest;
    DWORD count;
    int length, result = 1;
    length = snprintf(path, sizeof path, "%s/plugin.ini", folder);
    if (length < 0 || (size_t)length >= sizeof path)
        goto done;
    metadata = digest_file(path, 65536, manifest_hash);
    count = GetFullPathNameA(path, sizeof absolute, absolute, NULL);
    if (metadata == INVALID_HANDLE_VALUE || !count || count >= sizeof absolute ||
        !sv_manifest_read(absolute, &manifest, NULL, NULL) || (expected_id && strcmp(expected_id, manifest.id)))
        goto done;
    length = snprintf(path, sizeof path, "%s/%s", folder, manifest.library);
    if (length < 0 || (size_t)length >= sizeof path)
        goto done;
    payload = digest_file(path, 64 * 1024 * 1024, payload_hash);
    if (payload == INVALID_HANDLE_VALUE || sv_plugin_validate_folder(folder))
        goto done;
    printf("INSPECT\t1\t%s\t%s\t%s\n", manifest.id, payload_hash, manifest_hash);
    dangers(&manifest);
    result = 0;
done:
    if (payload != INVALID_HANDLE_VALUE)
        CloseHandle(payload);
    if (metadata != INVALID_HANDLE_VALUE)
        CloseHandle(metadata);
    return result;
}

/* The shipped installer owns ZIP bounds and entry checks. It asks this server to read its staging folder. */
static int inspect_package(const char *path)
{
    char package[MAX_PATH], root[MAX_PATH], script[MAX_PATH], powershell[MAX_PATH], command[2048], hash[65];
    HANDLE file;
    STARTUPINFOA startup = { sizeof startup };
    PROCESS_INFORMATION process;
    DWORD length, code = 1;
    int n;
    length = GetFullPathNameA(path, sizeof package, package, NULL);
    if (!length || length >= sizeof package)
        return 1;
    file = digest_file(package, 70 * 1024 * 1024, hash);
    if (file == INVALID_HANDLE_VALUE)
        return 1;
    length = GetModuleFileNameA(NULL, root, sizeof root);
    if (!length || length >= sizeof root || !strrchr(root, '\\'))
        goto done;
    *strrchr(root, '\\') = 0;
    n = snprintf(script, sizeof script, "%s/install_plugin.ps1", root);
    if (n < 0 || (size_t)n >= sizeof script || !plain_tree(script))
        goto done;
    length = GetSystemDirectoryA(powershell, sizeof powershell);
    if (!length || length >= sizeof powershell)
        goto done;
    n = snprintf(powershell + length, sizeof powershell - length,
                 "/WindowsPowerShell/v1.0/powershell.exe");
    if (n < 0 || (size_t)n >= sizeof powershell - length)
        goto done;
    startup.dwFlags = STARTF_USESTDHANDLES;
    startup.hStdInput = GetStdHandle(STD_INPUT_HANDLE);
    startup.hStdOutput = GetStdHandle(STD_OUTPUT_HANDLE);
    startup.hStdError = GetStdHandle(STD_ERROR_HANDLE);
    n = snprintf(command, sizeof command,
                 "powershell.exe -NoLogo -NoProfile -NonInteractive -ExecutionPolicy Bypass "
                 "-File \"%s\" -ServerDir \"%s\" -Package \"%s\" -Inspect -ServerInspect", script, root, package);
    if (n < 0 || (size_t)n >= sizeof command ||
        !CreateProcessA(powershell, command, NULL, NULL, TRUE, CREATE_NO_WINDOW, NULL, NULL, &startup, &process))
        goto done;
    if (WaitForSingleObject(process.hProcess, 30000) != WAIT_OBJECT_0) {
        TerminateProcess(process.hProcess, 1);
        WaitForSingleObject(process.hProcess, INFINITE);
    } else if (!GetExitCodeProcess(process.hProcess, &code)) {
        code = 1;
    }
    CloseHandle(process.hThread);
    CloseHandle(process.hProcess);
    if (!code)
        printf("PACKAGE\t%s\n", hash);
done:
    CloseHandle(file);
    return code ? 1 : 0;
}

int sv_plugin_inspect_cli(int argc, char **argv)
{
    int result;
    char folder[MAX_PATH];
    if (argc >= 2 && !strcmp(argv[1], "--plugin-trust-status")) {
        if (argc == 3 && !sv_plugin_trust_status(argv[2]))
            return 0;
        fputs("Cannot read the saved plugin review.\n", stderr);
        return 2;
    }
    if (argc >= 2 && !strcmp(argv[1], "--plugin-trust")) {
        if (argc == 9 && !strcmp(argv[8], "--native-confirmed") &&
            sv_plugin_trust_save(argv[2], argv[3], argv[4], argv[5], argv[6], argv[7])) {
            printf("APPROVED\t%s\n", argv[2]);
            return 0;
        }
        fputs("The plugin changed or the review is incomplete. Inspect and review it again.\n", stderr);
        return 2;
    }
    if (argc < 2 || (strcmp(argv[1], "--plugin-inspect") && strcmp(argv[1], "--plugin-inspect-folder")))
        return -1;
    if (argc != 3) {
        fputs("Use --plugin-inspect ZIP|ID.\n", stderr);
        return 2;
    }
    if (!strcmp(argv[1], "--plugin-inspect-folder"))
        result = inspect_folder(argv[2], NULL);
    else if (sv_plugin_id(argv[2]) && GetFileAttributesA(argv[2]) == INVALID_FILE_ATTRIBUTES) {
        snprintf(folder, sizeof folder, "plugins/%s", argv[2]);
        result = inspect_folder(folder, argv[2]);
        if (!result)
            puts("PACKAGE\t");
    } else {
        result = inspect_package(argv[2]);
    }
    if (result)
        fputs("Cannot inspect the plugin. Check the manifest, payload, package and installer.\n", stderr);
    return result;
}


static HANDLE dev_files[1 + 2 * SV_PLUGIN_MAX];
static unsigned dev_file_count;
static HANDLE dev_stop_event, dev_warning_thread;

static void dev_warning(void)
{
    sv_log("WARNING: development plugin approval is active for this run only. Native code has full process access. Approval is not saved.");
}

static DWORD WINAPI dev_warnings(void *unused)
{
    (void)unused;
    while (WaitForSingleObject(dev_stop_event, 60000) == WAIT_TIMEOUT)
        dev_warning();
    return 0;
}

void sv_plugin_dev_stop(void)
{
    if (dev_stop_event)
        SetEvent(dev_stop_event);
    if (dev_warning_thread) {
        WaitForSingleObject(dev_warning_thread, INFINITE);
        CloseHandle(dev_warning_thread);
    }
    if (dev_stop_event)
        CloseHandle(dev_stop_event);
    dev_warning_thread = dev_stop_event = NULL;
    while (dev_file_count)
        CloseHandle(dev_files[--dev_file_count]);
    sv_plugins_native_bypass(0);
}

static int dev_hold(const char *path, LONGLONG limit)
{
    char hash[65];
    HANDLE file;
    if (dev_file_count >= sizeof dev_files / sizeof dev_files[0])
        return 0;
    file = digest_file(path, limit, hash);
    if (file == INVALID_HANDLE_VALUE)
        return 0;
    dev_files[dev_file_count++] = file;
    return 1;
}

int sv_plugin_dev_start(const char *folder)
{
    SvModule *profile = NULL, current;
    uint32_t count = 0, i, inside_count = 0;
    char absolute[MAX_PATH], root[MAX_PATH], path[MAX_PATH], canonical[MAX_PATH];
    const char *problem = "Development approval needs a plain local folder and a valid native plugin profile.";
    DWORD length, attributes;
    size_t root_length;
    int inside, ok = 0;
    length = GetFullPathNameA(folder, sizeof absolute, absolute, NULL);
    if (!length || length >= sizeof absolute || !plain_tree(absolute))
        goto done;
    attributes = GetFileAttributesA(absolute);
    if (attributes == INVALID_FILE_ATTRIBUTES || !(attributes & FILE_ATTRIBUTE_DIRECTORY))
        goto done;
    length = GetLongPathNameA(absolute, root, sizeof root);
    if (!length || length >= sizeof root)
        goto done;
    root_length = strlen(root);
    while (root_length > 3 && (root[root_length - 1] == '\\' || root[root_length - 1] == '/'))
        root[--root_length] = 0;
    if (!dev_hold("plugins/plugins.ini", 65536))
        goto done;
    profile = calloc(SV_PLUGIN_MAX, sizeof *profile);
    if (!profile || !sv_plugin_profile(profile, &count))
        goto done;
    for (i = 0; i < count; ++i) {
        snprintf(path, sizeof path, "plugins/%s/plugin.ini", profile[i].id);
        if (!dev_hold(path, 65536) || !sv_plugin_declared(profile[i].id, &current) ||
            current.runtime != SV_RUNTIME_NATIVE || !dev_hold(current.path, 64 * 1024 * 1024) ||
            !sv_plugin_inspect(profile[i].id, &current))
            goto done;
        length = GetLongPathNameA(current.path, canonical, sizeof canonical);
        if (!length || length >= sizeof canonical)
            goto done;
        inside = !_strnicmp(canonical, root, root_length) &&
            (root[root_length - 1] == '\\' || root[root_length - 1] == '/' ||
             canonical[root_length] == '\\' || canonical[root_length] == '/');
        if (inside) {
            ++inside_count;
            /* Development approval stays in memory. */
        } else if (!sv_plugin_approved(&current)) {
            problem = "An unapproved plugin is outside the development folder. Review it or choose the correct folder.";
            goto done;
        }
    }
    if (!inside_count) {
        problem = "The folder holds no configured native plugin. Install the plugin and add it to the normal profile first.";
        goto done;
    }
    dev_stop_event = CreateEventA(NULL, TRUE, FALSE, NULL);
    if (!dev_stop_event)
        goto done;
    dev_warning_thread = CreateThread(NULL, 0, dev_warnings, NULL, 0, NULL);
    if (!dev_warning_thread)
        goto done;
    sv_plugins_native_bypass(1);
    dev_warning();
    ok = 1;
done:
    free(profile);
    if (!ok) {
        sv_plugin_dev_stop();
        fprintf(stderr, "%s\n", problem);
    }
    return ok;
}
