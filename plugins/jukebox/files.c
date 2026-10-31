#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include "files.h"
#include "jukebox_defaults.h"
#include "jukebox_sounds.h"
#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>

static int problem(char *error, size_t capacity, const char *text)
{
    if (capacity)
        snprintf(error, capacity, "%s", text);
    return 0;
}

static int plain_directory(const char *path, int create)
{
    DWORD attributes;
    if (create && !CreateDirectoryA(path, NULL) && GetLastError() != ERROR_ALREADY_EXISTS)
        return 0;
    attributes = GetFileAttributesA(path);
    return attributes != INVALID_FILE_ATTRIBUTES && (attributes & FILE_ATTRIBUTE_DIRECTORY) &&
        !(attributes & FILE_ATTRIBUTE_REPARSE_POINT);
}

static int folder(int create, char *error, size_t capacity)
{
    if (!plain_directory("user", create) || !plain_directory("user/jukebox", create))
        return problem(error, capacity, "Cannot use user/jukebox. It must be a normal folder.");
    return 1;
}

int jb_song_name(const char *input, char name[JB_NAME_MAX + 1])
{
    size_t i, length = strlen(input);
    if (length > 4 && !_stricmp(input + length - 4, ".mid"))
        length -= 4;
    if (!length || length > JB_NAME_MAX)
        return 0;
    for (i = 0; i < length; ++i) {
        unsigned char c = (unsigned char)input[i];
        if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_' || c == '-'))
            return 0;
    }
    memcpy(name, input, length);
    name[length] = 0;
    if (!_stricmp(name, "CON") || !_stricmp(name, "PRN") || !_stricmp(name, "AUX") || !_stricmp(name, "NUL") ||
        (length == 4 && (!_strnicmp(name, "COM", 3) || !_strnicmp(name, "LPT", 3)) && name[3] >= '1' && name[3] <= '9'))
        return 0;
    return 1;
}

static unsigned char *read_file(const char *path, size_t maximum, size_t *length, char *error, size_t capacity)
{
    HANDLE file = CreateFileA(path, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL | FILE_FLAG_OPEN_REPARSE_POINT, NULL);
    BY_HANDLE_FILE_INFORMATION info;
    LARGE_INTEGER size;
    unsigned char *data = NULL;
    DWORD read;
    if (file == INVALID_HANDLE_VALUE) {
        problem(error, capacity, "Cannot open the jukebox file. Check its name and read access.");
        return NULL;
    }
    if (GetFileType(file) != FILE_TYPE_DISK || !GetFileInformationByHandle(file, &info) ||
        (info.dwFileAttributes & (FILE_ATTRIBUTE_DIRECTORY | FILE_ATTRIBUTE_REPARSE_POINT))) {
        problem(error, capacity, "Jukebox input must be a normal file, not a link.");
    } else if (!GetFileSizeEx(file, &size) || size.QuadPart < 0 || (uint64_t)size.QuadPart > maximum) {
        problem(error, capacity, maximum == JB_FILE_MAX ? "MIDI file exceeds 1 MB." :
            maximum == 4096 ? "Preset exceeds 4096 bytes. File unchanged." :
            "Sound table exceeds 16384 bytes. File unchanged.");
    } else {
        *length = (size_t)size.QuadPart;
        data = malloc(*length + 1);
        if (!data) {
            problem(error, capacity, "Not enough memory for the jukebox file.");
        } else if (!ReadFile(file, data, (DWORD)*length, &read, NULL) || read != *length) {
            free(data);
            data = NULL;
            problem(error, capacity, "Cannot read the complete jukebox file.");
        } else {
            data[*length] = 0;
        }
    }
    CloseHandle(file);
    return data;
}

static unsigned char *load_table(const char *path, const char *defaults, size_t length, size_t *size, char *error, size_t capacity)
{
    HANDLE file;
    DWORD written;
    int ok;
    if (!folder(1, error, capacity))
        return NULL;
    /* CREATE_NEW fails when any file exists. Never replace owner edits, including old versions. */
    file = CreateFileA(path, GENERIC_WRITE, 0, NULL, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, NULL);
    if (file != INVALID_HANDLE_VALUE) {
        ok = WriteFile(file, defaults, (DWORD)length, &written, NULL) &&
            written == length && FlushFileBuffers(file);
        CloseHandle(file);
        if (!ok)
            return (problem(error, capacity, "Cannot create sound table. Check disk space and write access."), NULL);
    } else if (GetLastError() != ERROR_FILE_EXISTS && GetLastError() != ERROR_ALREADY_EXISTS) {
        return (problem(error, capacity, "Cannot create sound table. Existing files stay unchanged."), NULL);
    }
    return read_file(path, 16384, size, error, capacity);
}

int jb_load_instruments(JbInstruments *out, char *error, size_t capacity)
{
    size_t size;
    unsigned char *data = load_table("user/jukebox/instruments.ini", jb_default_instruments,
        sizeof jb_default_instruments - 1, &size, error, capacity);
    int ok;
    if (!data) return 0;
    ok = jb_instruments_parse((const char *)data, size, out, error, capacity);
    free(data);
    return ok;
}

int jb_load_sounds(JbSounds *out, char *error, size_t capacity)
{
    size_t size;
    unsigned char *data = load_table("user/jukebox/sounds.ini", jb_default_sounds,
        sizeof jb_default_sounds - 1, &size, error, capacity);
    int ok;
    if (!data) return 0;
    ok = jb_sounds_parse((const char *)data, size, out, error, capacity);
    free(data);
    return ok;
}

int jb_load_song(const char *input, JbSong *song, char *error, size_t capacity)
{
    char name[JB_NAME_MAX + 1], path[96];
    unsigned char *data;
    size_t size;
    int ok;
    if (!jb_song_name(input, name))
        return problem(error, capacity, "Song name needs 1 to 48 letters, digits, underscores or hyphens.");
    if (!folder(0, error, capacity))
        return 0;
    snprintf(path, sizeof path, "user/jukebox/%s.mid", name);
    data = read_file(path, JB_FILE_MAX, &size, error, capacity);
    if (!data)
        return 0;
    ok = jb_midi_parse(data, size, song, error, capacity);
    free(data);
    return ok;
}

static int name_compare(const void *left, const void *right)
{
    int result = _stricmp(left, right);
    return result ? result : strcmp(left, right);
}

int jb_list_songs(char names[JB_LIST_MAX][JB_NAME_MAX + 1], size_t *count, int *more,
    char *error, size_t capacity)
{
    WIN32_FIND_DATAA entry;
    HANDLE search;
    *count = 0;
    *more = 0;
    if (!folder(0, error, capacity))
        return 0;
    search = FindFirstFileA("user/jukebox/*.mid", &entry);
    if (search == INVALID_HANDLE_VALUE) {
        if (GetLastError() == ERROR_FILE_NOT_FOUND)
            return 1;
        return problem(error, capacity, "Cannot list user/jukebox files.");
    }
    do {
        char name[JB_NAME_MAX + 1];
        if (entry.dwFileAttributes & (FILE_ATTRIBUTE_DIRECTORY | FILE_ATTRIBUTE_REPARSE_POINT))
            continue;
        if (!jb_song_name(entry.cFileName, name))
            continue;
        if (*count == JB_LIST_MAX) {
            *more = 1;
            break;
        }
        strcpy(names[(*count)++], name);
    } while (FindNextFileA(search, &entry));
    FindClose(search);
    qsort(names, *count, sizeof names[0], name_compare);
    return 1;
}

static int mix_number(char **cursor, long low, long high, int *value)
{
    char *end, *start = *cursor;
    long n;
    errno = 0;
    n = strtol(start, &end, 10);
    if (end == start || errno || n < low || n > high || (*end && *end != ' ' && *end != '\n'))
        return 0;
    *value = (int)n;
    *cursor = end;
    if (**cursor == ' ') ++*cursor;
    return 1;
}

int jb_mix_parse(char *text, size_t size, JbMix *mix, char *error, size_t capacity)
{
    JbMix next = {0};
    char *at = text;
    size_t i;
    int version, channel;
    if (!size || size > 4096) /* MIX_BOUND: size */
        return problem(error, capacity, "Preset needs 1 to 4096 bytes.");
    for (i = 0; i < size; ++i)
        if ((text[i] < 32 && text[i] != '\n' && !(text[i] == '\r' && i + 1 < size && text[i + 1] == '\n')) || (unsigned char)text[i] > 126) /* MIX_BOUND: ascii */
            return problem(error, capacity, "Preset needs plain ASCII text.");
    {
        size_t used = 0;
        for (i = 0; i < size; ++i) if (text[i] != '\r') text[used++] = text[i];
        text[used] = 0;
    }
    if (strncmp(at, "jukebox-mix ", 12)) return problem(error, capacity, "Preset header is invalid.");
    at += 12;
    if (!mix_number(&at, 2, 2, &version) || *at++ != '\n') /* MIX_BOUND: version */
        return problem(error, capacity, "Preset version must be 2. Old presets are ignored.");
    if (!mix_number(&at, JB_FIXED, JB_MEMBERS - 1, &next.bank)) /* MIX_BOUND: mode */
        return problem(error, capacity, "Preset mode is invalid.");
    if (!mix_number(&at, 0, 46, &next.fixed_sound)) /* MIX_BOUND: fixed */
        return problem(error, capacity, "Preset fixed sound needs 0 to 46.");
    if (!mix_number(&at, 0, 300, &next.bpm) || (next.bpm && next.bpm < 20)) /* MIX_BOUND: bpm */
        return problem(error, capacity, "Preset BPM needs 0 or 20 to 300.");
    if (!mix_number(&at, 25, 400, &next.speed)) /* MIX_BOUND: speed */
        return problem(error, capacity, "Preset speed needs 25 to 400.");
    if (!mix_number(&at, -48, 48, &next.key)) /* MIX_BOUND: key */
        return problem(error, capacity, "Preset key needs -48 to 48.");
    if (!mix_number(&at, 0, 200, &next.master) || *at++ != '\n') /* MIX_BOUND: master */
        return problem(error, capacity, "Preset master needs 0 to 200.");
    for (i = 0; i < 16; ++i) {
        JbRoute *r = &next.routes[i];
        if (!mix_number(&at, 1, 16, &channel) || channel != (int)i + 1) /* MIX_BOUND: channel */
            return problem(error, capacity, "Preset needs channels 1 to 16 in order.");
        if (!mix_number(&at, JB_OFF, JB_DRUMS, &r->output)) /* MIX_BOUND: output */
            return problem(error, capacity, "Preset output needs 0 to 2.");
        if (!mix_number(&at, JB_SYNTH, JB_DRUMS, &next.resume[i])) /* MIX_BOUND: resume */
            return problem(error, capacity, "Preset resume output needs 1 or 2.");
        if (!mix_number(&at, 0, 1, &r->muted)) /* MIX_BOUND: muted */
            return problem(error, capacity, "Preset mute needs 0 or 1.");
        if (!mix_number(&at, -1, 46, &r->voice)) /* MIX_BOUND: voice */
            return problem(error, capacity, "Preset voice needs -1 to 46.");
        if (!mix_number(&at, 0, 127, &r->root)) /* MIX_BOUND: root */
            return problem(error, capacity, "Preset root needs 0 to 127.");
        if (!mix_number(&at, -36, 36, &r->transpose)) /* MIX_BOUND: transpose */
            return problem(error, capacity, "Preset transpose needs -36 to 36.");
        if (!mix_number(&at, 0, 1, &r->mono)) /* MIX_BOUND: mono */
            return problem(error, capacity, "Preset mono needs 0 or 1.");
        if (!mix_number(&at, 0, 1, &r->automatic)) /* MIX_BOUND: automatic */
            return problem(error, capacity, "Preset automatic mute needs 0 or 1.");
        if (!mix_number(&at, 0, 200, &r->volume) || *at++ != '\n') /* MIX_BOUND: volume */
            return problem(error, capacity, "Preset volume needs 0 to 200.");
    }
    if (*at) /* MIX_BOUND: trailing */
        return problem(error, capacity, "Preset has extra data.");
    *mix = next;
    return 1;
}

int jb_load_mix(const char *input, JbMix *mix, char *error, size_t capacity)
{
    char name[JB_NAME_MAX + 1], path[96];
    unsigned char *data;
    size_t size;
    int ok;
    DWORD attributes;
    if (!jb_song_name(input, name) || !folder(0, error, capacity)) return -1;
    snprintf(path, sizeof path, "user/jukebox/%s.mix", name);
    attributes = GetFileAttributesA(path);
    if (attributes == INVALID_FILE_ATTRIBUTES && GetLastError() == ERROR_FILE_NOT_FOUND) return 0;
    data = read_file(path, 4096, &size, error, capacity);
    if (!data) return -1;
    ok = jb_mix_parse((char *)data, size, mix, error, capacity);
    free(data);
    return ok ? 1 : -1;
}

int jb_save_mix(const char *input, const JbMix *mix, char *error, size_t capacity)
{
    char name[JB_NAME_MAX + 1], path[96], temporary[100], text[4096];
    size_t used;
    int i, ok;
    HANDLE file;
    DWORD attributes, written;
    if (!jb_song_name(input, name) || !folder(0, error, capacity))
        return problem(error, capacity, "Load a file song before saving a preset.");
    snprintf(path, sizeof path, "user/jukebox/%s.mix", name);
    snprintf(temporary, sizeof temporary, "%s.tmp", path);
    attributes = GetFileAttributesA(path);
    if (attributes != INVALID_FILE_ATTRIBUTES && (attributes & (FILE_ATTRIBUTE_DIRECTORY | FILE_ATTRIBUTE_REPARSE_POINT)))
        return problem(error, capacity, "Preset must be a normal file.");
    used = (size_t)snprintf(text, sizeof text, "jukebox-mix 2\n%d %d %d %d %d %d\n",
        mix->bank, mix->fixed_sound, mix->bpm, mix->speed, mix->key, mix->master);
    for (i = 0; i < 16; ++i) {
        const JbRoute *r = &mix->routes[i];
        used += (size_t)snprintf(text + used, sizeof text - used, "%d %d %d %d %d %d %d %d %d %d\n",
            i + 1, r->output, mix->resume[i], r->muted, r->voice, r->root, r->transpose, r->mono, r->automatic, r->volume);
    }
    file = CreateFileA(temporary, GENERIC_WRITE, 0, NULL, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, NULL);
    if (file == INVALID_HANDLE_VALUE)
        return problem(error, capacity, "Cannot create preset temporary file. Check access or a stale .mix.tmp file.");
    ok = WriteFile(file, text, (DWORD)used, &written, NULL) && written == used && FlushFileBuffers(file);
    CloseHandle(file);
    if (ok) ok = MoveFileExA(temporary, path, MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH) != 0;
    if (!ok) {
        DeleteFileA(temporary);
        return problem(error, capacity, "Cannot save preset. Check disk space and write access.");
    }
    return 1;
}

int jb_resolve_song(const char *input, char name[JB_NAME_MAX + 1],
    char matches[8][JB_NAME_MAX + 1], size_t *count, char *error, size_t capacity)
{
    char query[JB_NAME_MAX + 1];
    int pass;
    *count = 0;
    if (!jb_song_name(input, query))
        return problem(error, capacity, "Song name needs 1 to 48 letters, digits, underscores or hyphens.") - 1;
    if (!folder(0, error, capacity)) return -1;
    for (pass = 0; pass < 3; ++pass) {
        WIN32_FIND_DATAA entry;
        HANDLE search = FindFirstFileA("user/jukebox/*.mid", &entry);
        if (search == INVALID_HANDLE_VALUE) {
            if (GetLastError() == ERROR_FILE_NOT_FOUND) return 0;
            problem(error, capacity, "Cannot list user/jukebox files.");
            return -1;
        }
        do {
            char candidate[JB_NAME_MAX + 1];
            size_t i, used;
            int match = 0;
            if ((entry.dwFileAttributes & (FILE_ATTRIBUTE_DIRECTORY | FILE_ATTRIBUTE_REPARSE_POINT)) ||
                !jb_song_name(entry.cFileName, candidate)) continue;
            if (!pass) match = !_stricmp(candidate, query);
            else if (pass == 1) match = !_strnicmp(candidate, query, strlen(query));
            else {
                size_t length = strlen(candidate), query_length = strlen(query);
                for (i = 0; i + query_length <= length; ++i)
                    if (!_strnicmp(candidate + i, query, query_length)) { match = 1; break; }
            }
            if (!match) continue;
            ++*count;
            used = *count < 8 ? *count : 8;
            /* Retain only the first eight sorted names, but count every match. */
            for (i = 0; i < used; ++i)
                if (i == *count - 1 || name_compare(candidate, matches[i]) < 0) break;
            if (i < 8) {
                size_t j;
                for (j = used - 1; j > i; --j) strcpy(matches[j], matches[j - 1]);
                strcpy(matches[i], candidate);
            }
        } while (FindNextFileA(search, &entry));
        {
            DWORD last = GetLastError();
            FindClose(search);
            if (last != ERROR_NO_MORE_FILES) {
                problem(error, capacity, "Cannot finish listing user/jukebox files.");
                return -1;
            }
        }
        if (*count) {
            if (*count == 1) { strcpy(name, matches[0]); return 1; }
            return 2;
        }
    }
    return 0;
}
