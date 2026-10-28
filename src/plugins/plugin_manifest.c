#include "plugin_manifest.h"
#include "plugin_internal.h"
#include <stdlib.h>
#include <string.h>

enum {
    FIELD_ID, FIELD_VERSION, FIELD_LIBRARY, FIELD_API, FIELD_LIVE, FIELD_UNSAFE, FIELD_ROLE,
    FIELD_DEPENDENCIES, FIELD_CONFLICTS, FIELD_RUNTIME, FIELD_CAPS, FIELD_NAME, FIELD_DESCRIPTION,
    FIELD_AUTHOR, FIELD_HOMEPAGE, FIELD_COUNT,
    MANIFEST_LIMIT = 65536, SECTION_LIMIT = SV_SETTING_MAX + 1
};

static const char *const field_names[FIELD_COUNT] = {
    "id", "version", "library", "api", "live", "unsafe_events", "role", "dependencies", "conflicts",
    "runtime", "capabilities", "name", "description", "author", "homepage"
};
static const char *const cap_names[] = {
    "read", "write", "ai", "economy", "mission", "settings", "storage", "engine", "shop", "props", "config", "plugins"
};
static const char *const setting_names[] = {
    "type", "live", "admin", "label", "help", "default", "min", "max", "maxlen", "choices"
};

typedef struct {
    SvManifestReport report;
    void *context;
    unsigned errors;
    HANDLE file;
    char *value[FIELD_COUNT];
} Reader;

static void problem(Reader *reader, const char *key, const char *message)
{
    ++reader->errors;
    if (reader->report)
        reader->report(reader->context, key, message);
}

static int lookup(const char *text, const char *const *names, size_t count)
{
    size_t i;
    for (i = 0; i < count; ++i)
        if (!_stricmp(text, names[i]))
            return (int)i;
    return -1;
}

static char *trim(char *text)
{
    char *end;
    while (*text == ' ' || *text == '\t')
        ++text;
    end = text + strlen(text);
    while (end > text && (end[-1] == ' ' || end[-1] == '\t'))
        --end;
    *end = 0;
    return text;
}

static int printable(const char *text)
{
    const unsigned char *p = (const unsigned char *)text;
    for (; *p; ++p)
        if (*p < 0x20 || *p > 0x7e)
            return 0;
    return 1;
}

static int absolute_path(const char *path)
{
    size_t length = path ? strlen(path) : 0;
    return (length >= 3 && ((path[0] >= 'A' && path[0] <= 'Z') || (path[0] >= 'a' && path[0] <= 'z')) &&
            path[1] == ':' && (path[2] == '/' || path[2] == '\\')) ||
           (length >= 3 && path[0] == '\\' && path[1] == '\\');
}

static char *read_file(const char *path, Reader *reader)
{
    HANDLE file;
    LARGE_INTEGER size;
    BY_HANDLE_FILE_INFORMATION info;
    DWORD got;
    char *data = NULL;
    if (!absolute_path(path)) {
        problem(reader, "ini", "The manifest path must be absolute.");
        return NULL;
    }
    file = CreateFileA(path, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_FLAG_OPEN_REPARSE_POINT, NULL);
    if (file == INVALID_HANDLE_VALUE) {
        problem(reader, "ini", "Cannot read the manifest.");
        return NULL;
    }
    if (!GetFileInformationByHandle(file, &info) ||
        (info.dwFileAttributes & (FILE_ATTRIBUTE_DIRECTORY | FILE_ATTRIBUTE_REPARSE_POINT)) ||
        !GetFileSizeEx(file, &size) || size.QuadPart < 0 || size.QuadPart > MANIFEST_LIMIT) {
        problem(reader, "ini", "The manifest must be a plain file of at most 64 KiB.");
    } else {
        data = malloc((size_t)size.QuadPart + 1);
        if (!data) {
            problem(reader, "ini", "Cannot allocate memory for the manifest.");
        } else if (!ReadFile(file, data, (DWORD)size.QuadPart, &got, NULL) || got != (DWORD)size.QuadPart) {
            problem(reader, "ini", "Cannot read the complete manifest.");
            free(data);
            data = NULL;
        } else if (memchr(data, 0, got)) {
            problem(reader, "ini", "The manifest contains an embedded NUL byte.");
            free(data);
            data = NULL;
        } else {
            data[got] = 0;
        }
    }
    if (data)
        reader->file = file;
    else
        CloseHandle(file);
    return data;
}

static void scan(char *data, Reader *reader)
{
    char *section_names[SECTION_LIMIT];
    size_t section_count = 0, i;
    char *line = data, *next;
    int section = 0, plugin_seen = 0;
    uint32_t setting_keys = 0;
    if (strlen(data) >= 3 && !memcmp(data, "\xef\xbb\xbf", 3))
        line += 3;
    for (; *line; line = next) {
        char *text, *value, *end;
        int index;
        next = line + strcspn(line, "\r\n");
        if (*next) {
            char newline = *next;
            *next++ = 0;
            if (newline == '\r' && *next == '\n')
                ++next;
        }
        text = trim(line);
        if (!*text || *text == ';')
            continue;
        if (*text == '[') {
            section = 0;
            setting_keys = 0;
            end = strchr(text + 1, ']');
            if (!end || (*trim(end + 1) && *trim(end + 1) != ';')) {
                problem(reader, "ini", "A section line is malformed.");
                continue;
            }
            *end = 0;
            text = trim(text + 1);
            if (!_stricmp(text, "plugin")) {
                section = 1;
                plugin_seen = 1;
            } else if (!_strnicmp(text, "setting.", 8) && text[8]) {
                section = 2;
            } else {
                problem(reader, "ini", "The manifest contains an unknown section.");
                continue;
            }
            for (i = 0; i < section_count; ++i)
                if (!_stricmp(text, section_names[i])) {
                    problem(reader, "ini", "The manifest repeats a section.");
                    break;
                }
            if (i == section_count) {
                if (section_count == SECTION_LIMIT)
                    problem(reader, "settings", "The manifest declares too many settings.");
                else
                    section_names[section_count++] = text;
            }
            continue;
        }
        value = strchr(text, '=');
        if (!value) {
            problem(reader, "ini", "A key line must contain an equals sign.");
            continue;
        }
        *value++ = 0;
        text = trim(text);
        value = trim(value);
        end = value + strlen(value);
        if (end - value >= 2 && (*value == '\'' || *value == '"') && end[-1] == *value) {
            end[-1] = 0;
            ++value;
        }
        if (section == 1) {
            index = lookup(text, field_names, FIELD_COUNT);
            if (index < 0)
                problem(reader, "plugin", "The plugin section contains an unknown key.");
            else if (reader->value[index])
                problem(reader, field_names[index], "The plugin section repeats this key.");
            else
                reader->value[index] = value;
        } else if (section == 2) {
            index = lookup(text, setting_names, sizeof setting_names / sizeof setting_names[0]);
            if (index < 0)
                problem(reader, "settings", "A setting section contains an unknown key.");
            else if (setting_keys & (1u << (unsigned)index))
                problem(reader, "settings", "A setting section repeats a key.");
            else
                setting_keys |= 1u << (unsigned)index;
        } else {
            problem(reader, "ini", "A key must belong to a known section.");
        }
    }
    if (!plugin_seen)
        problem(reader, "plugin", "The manifest needs one plugin section.");
}

static const char *field(const Reader *reader, int index)
{
    return reader->value[index] ? reader->value[index] : "";
}

static int copy_field(Reader *reader, int index, char *out, size_t capacity, int required)
{
    const char *text = field(reader, index);
    size_t length = strlen(text);
    if ((required && !length) || length >= capacity || !printable(text)) {
        problem(reader, field_names[index], "The value has an invalid length or contains nonprintable text.");
        return 0;
    }
    memcpy(out, text, length + 1);
    return 1;
}

static void flag(Reader *reader, int index, uint32_t *out, int required)
{
    const char *text = field(reader, index);
    if (!reader->value[index] && !required)
        return;
    if (strcmp(text, "0") && strcmp(text, "1"))
        problem(reader, field_names[index], "The value must be 0 or 1.");
    else
        *out = text[0] == '1';
}

static int version_ok(const char *text)
{
    const unsigned char *p = (const unsigned char *)text;
    for (; *p; ++p)
        if (!(*p >= 'a' && *p <= 'z') && !(*p >= 'A' && *p <= 'Z') && !(*p >= '0' && *p <= '9') &&
            *p != '.' && *p != '-' && *p != '+')
            return 0;
    return 1;
}

static int homepage_ok(const char *text)
{
    const char *host, *end, *colon;
    unsigned port = 0;
    if (!*text)
        return 1;
    if (!_strnicmp(text, "https://", 8))
        host = text + 8;
    else if (!_strnicmp(text, "http://", 7))
        host = text + 7;
    else
        return 0;
    if (strpbrk(text, " <>") || strchr(text, 0x5c) || strchr(text, 0x22))
        return 0;
    end = host + strcspn(host, "/?#");
    if (host == end || memchr(host, '@', (size_t)(end - host)))
        return 0;
    if (*host == '[') {
        const char *close = memchr(host, ']', (size_t)(end - host));
        if (!close || close == host + 1 || (close + 1 != end && close[1] != ':'))
            return 0;
        colon = close + 1 == end ? NULL : close + 1;
    } else {
        const char *at;
        colon = memchr(host, ':', (size_t)(end - host));
        if (colon == host)
            return 0;
        for (at = host; at < (colon ? colon : end); ++at)
            if (!(*at >= 'a' && *at <= 'z') && !(*at >= 'A' && *at <= 'Z') &&
                !(*at >= '0' && *at <= '9') && *at != '.' && *at != '-')
                return 0;
    }
    if (colon) {
        if (++colon == end)
            return 0;
        for (; colon < end; ++colon) {
            if (*colon < '0' || *colon > '9')
                return 0;
            port = port * 10 + (unsigned)(*colon - '0');
            if (port > 65535u)
                return 0;
        }
    }
    return 1;
}

static void capabilities(Reader *reader, SvManifest *manifest)
{
    char *part = reader->value[FIELD_CAPS], *next;
    if (!part || !*part)
        return;
    for (;;) {
        int index;
        next = strchr(part, ',');
        if (next)
            *next = 0;
        part = trim(part);
        index = lookup(part, cap_names, sizeof cap_names / sizeof cap_names[0]);
        if (index < 0)
            problem(reader, "capabilities", "The list contains an unknown or empty capability.");
        else if (manifest->cap_mask & (1u << (unsigned)index))
            problem(reader, "capabilities", "The list repeats a capability.");
        else
            manifest->cap_mask |= 1u << (unsigned)index;
        if (!next)
            break;
        part = next + 1;
    }
}

static void id_list(Reader *reader, int index, const char *self, char ids[8][48], char versions[8][24], uint32_t *count)
{
    char *part = reader->value[index], *next;
    if (!part || !*part)
        return;
    for (;;) {
        char *minimum;
        uint32_t i;
        int valid = 1;
        next = strchr(part, ',');
        if (next)
            *next = 0;
        minimum = strchr(part, '@');
        if (minimum) {
            *minimum++ = 0;
            if (!versions || !sv_plugin_version_min(minimum)) {
                problem(reader, field_names[index], "A version requirement is invalid or is not allowed here.");
                valid = 0;
            }
        }
        if (!sv_plugin_id(part) || !strcmp(part, self)) {
            problem(reader, field_names[index], "The list contains an invalid ID or the plugin ID itself.");
            valid = 0;
        }
        for (i = 0; i < *count; ++i)
            if (!strcmp(ids[i], part)) {
                problem(reader, field_names[index], "The list repeats an ID.");
                valid = 0;
                break;
            }
        if (*count == 8) {
            problem(reader, field_names[index], "The list permits at most eight IDs.");
            valid = 0;
        }
        if (valid) {
            strcpy(ids[*count], part);
            if (versions)
                strcpy(versions[*count], minimum ? minimum : "");
            ++*count;
        }
        if (!next)
            break;
        part = next + 1;
    }
}

static void validate(Reader *reader, SvManifest *out)
{
    static const char *const runtimes[] = { "native", "lua", "wasm" };
    static const char *const extensions[] = { ".dll", ".lua", ".wasm" };
    const char *text;
    int index, library_valid;
    uint32_t i, j;
    if (copy_field(reader, FIELD_ID, out->id, sizeof out->id, 1) && !sv_plugin_id(out->id))
        problem(reader, "id", "The plugin ID is not safe for a path.");
    if (copy_field(reader, FIELD_VERSION, out->version, sizeof out->version, 1) && !version_ok(out->version))
        problem(reader, "version", "The package version contains invalid characters.");
    if (strcmp(field(reader, FIELD_API), "1"))
        problem(reader, "api", "The API version must be 1.");
    flag(reader, FIELD_LIVE, &out->live, 1);
    flag(reader, FIELD_UNSAFE, &out->unsafe, 0);
    text = field(reader, FIELD_ROLE);
    if (reader->value[FIELD_ROLE] && strcmp(text, "plugin") && strcmp(text, "library"))
        problem(reader, "role", "The role must be plugin or library.");
    else
        out->library_role = !strcmp(text, "library");
    text = reader->value[FIELD_RUNTIME] ? field(reader, FIELD_RUNTIME) : "native";
    index = lookup(text, runtimes, sizeof runtimes / sizeof runtimes[0]);
    if (index < 0)
        problem(reader, "runtime", "The runtime must be native, lua, or wasm.");
    else
        out->runtime = (uint32_t)index;
    library_valid = copy_field(reader, FIELD_LIBRARY, out->library, sizeof out->library, 1);
    if (library_valid && !sv_plugin_id(out->library))
        problem(reader, "library", "The library must be a safe basename.");
    if (library_valid && index >= 0) {
        size_t length = strlen(out->library), suffix = strlen(extensions[index]);
        if (length <= suffix || strcmp(out->library + length - suffix, extensions[index]))
            problem(reader, "library", "The library extension does not match the runtime.");
    }
    capabilities(reader, out);
    copy_field(reader, FIELD_NAME, out->name, sizeof out->name, reader->value[FIELD_NAME] != NULL);
    copy_field(reader, FIELD_DESCRIPTION, out->description, sizeof out->description, 0);
    copy_field(reader, FIELD_AUTHOR, out->author, sizeof out->author, 0);
    if (copy_field(reader, FIELD_HOMEPAGE, out->homepage, sizeof out->homepage, 0) && !homepage_ok(out->homepage))
        problem(reader, "homepage", "The homepage needs an HTTP or HTTPS URL with a host and no spaces.");
    id_list(reader, FIELD_DEPENDENCIES, out->id, out->dependencies, out->dependency_min, &out->dependency_count);
    id_list(reader, FIELD_CONFLICTS, out->id, out->conflicts, NULL, &out->conflict_count);
    for (i = 0; i < out->dependency_count; ++i)
        for (j = 0; j < out->conflict_count; ++j)
            if (!strcmp(out->dependencies[i], out->conflicts[j]))
                problem(reader, "conflicts", "A dependency cannot also be a conflict.");
}

static void setting_problem(void *context, const char *key, const char *message)
{
    problem(context, key, message);
}

int sv_manifest_read(const char *path, SvManifest *out, SvManifestReport report, void *context)
{
    Reader reader = {0};
    char *data;
    reader.report = report;
    reader.context = context;
    if (!out) {
        problem(&reader, "ini", "The manifest output is required.");
        return 0;
    }
    memset(out, 0, sizeof *out);
    data = read_file(path, &reader);
    if (!data)
        return 0;
    scan(data, &reader);
    validate(&reader, out);
    sv_config_declare_report(path, &out->config, NULL, 0, setting_problem, &reader);
    CloseHandle(reader.file);
    free(data);
    return reader.errors == 0;
}

const char *sv_manifest_runtime_name(uint32_t runtime)
{
    switch (runtime) {
    case SV_RUNTIME_NATIVE: return "native";
    case SV_RUNTIME_LUA: return "lua";
    case SV_RUNTIME_WASM: return "wasm";
    default: return "unknown";
    }
}

int sv_manifest_caps_text(uint32_t mask, uint32_t unsafe, char *out, size_t capacity)
{
    size_t i, used = 0;
    if (!out || !capacity)
        return 0;
    out[0] = 0;
    if (mask & ~((1u << (sizeof cap_names / sizeof cap_names[0])) - 1u))
        return 0;
    for (i = 0; i <= sizeof cap_names / sizeof cap_names[0]; ++i) {
        const char *text;
        size_t length;
        if (i == sizeof cap_names / sizeof cap_names[0]) {
            if (!unsafe)
                break;
            text = "unsafe_events";
        } else {
            if (!(mask & (1u << (unsigned)i)))
                continue;
            text = cap_names[i];
        }
        length = strlen(text);
        if (length + (used != 0) >= capacity - used) {
            out[0] = 0;
            return 0;
        }
        if (used)
            out[used++] = ',';
        memcpy(out + used, text, length + 1);
        used += length;
    }
    return 1;
}
