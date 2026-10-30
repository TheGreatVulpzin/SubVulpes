#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include "midi.h"
#include <stdio.h>
#include <stdlib.h>

int main(int argc, char **argv)
{
    FILE *file;
    long size;
    unsigned char *data;
    char error[160];
    JbSong song;
    size_t i;
    SetErrorMode(SEM_NOGPFAULTERRORBOX | SEM_FAILCRITICALERRORS);
    if (argc != 2 || !(file = fopen(argv[1], "rb")))
        return 3;
    if (fseek(file, 0, SEEK_END) || (size = ftell(file)) < 0 || size > JB_FILE_MAX + 64) {
        fclose(file);
        return 3;
    }
    rewind(file);
    data = calloc((size_t)size + 64, 1);
    if (!data || fread(data, 1, (size_t)size, file) != (size_t)size) {
        fclose(file);
        free(data);
        return 3;
    }
    fclose(file);
    if (!jb_midi_parse(data, (size_t)size, &song, error, sizeof error)) {
        printf("ERROR %s\n", error);
        free(data);
        return 2;
    }
    free(data);
    printf("SONG %zu %llu\n", song.count, (unsigned long long)song.end_tick);
    for (i = 0; i < song.count; ++i) {
        JbNote *n = &song.notes[i];
        printf("NOTE %llu %u %u %u %u\n", (unsigned long long)n->tick,
            n->channel, n->key, n->velocity, n->program);
    }
    for (i = 0; i < 16; ++i) {
        JbChannel *c = &song.channels[i];
        if (c->notes)
            printf("CHANNEL %zu %u %u %u %u\n", i, c->notes, c->first_program, c->low, c->high);
    }
    jb_song_free(&song);
    return 0;
}
