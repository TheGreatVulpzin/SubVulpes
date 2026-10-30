#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include "files.h"
#include "jukebox_defaults.h"
#include <stdio.h>
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
            "Instrument file exceeds 16384 bytes. File unchanged.");
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

int jb_load_instruments(JbInstruments *instruments, char *error, size_t capacity)
{
    const char *path = "user/jukebox/instruments.ini";
    HANDLE file;
    unsigned char *data;
    size_t size;
    DWORD written;
    int ok;
    if (!folder(1, error, capacity))
        return 0;
    /* CREATE_NEW fails when any file exists. Never replace owner edits, including old versions. */
    file = CreateFileA(path, GENERIC_WRITE, 0, NULL, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, NULL);
    if (file != INVALID_HANDLE_VALUE) {
        ok = WriteFile(file, jb_default_instruments, (DWORD)(sizeof jb_default_instruments - 1), &written, NULL) &&
            written == sizeof jb_default_instruments - 1 && FlushFileBuffers(file);
        CloseHandle(file);
        if (!ok)
            return problem(error, capacity, "Cannot create instruments.ini. Check disk space and write access.");
    } else if (GetLastError() != ERROR_FILE_EXISTS && GetLastError() != ERROR_ALREADY_EXISTS) {
        return problem(error, capacity, "Cannot create instruments.ini. Existing files stay unchanged.");
    }
    data = read_file(path, 16384, &size, error, capacity);
    if (!data)
        return 0;
    ok = jb_instruments_parse((const char *)data, size, instruments, error, capacity);
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
