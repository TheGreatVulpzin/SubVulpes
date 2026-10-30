#ifndef JB_MIDI_H
#define JB_MIDI_H

#include <stddef.h>
#include <stdint.h>

enum { JB_FILE_MAX = 1048576, JB_TRACK_MAX = 64, JB_EVENT_MAX = 200000 };

typedef struct {
    uint64_t tick;
    uint8_t channel, key, velocity, program;
} JbNote;

typedef struct {
    uint32_t notes;
    uint8_t first_program, low, high;
} JbChannel;

typedef struct {
    JbNote *notes;
    size_t count;
    uint64_t end_tick;
    JbChannel channels[16];
} JbSong;

/* The caller supplies an empty song. On error the song stays empty. */
int jb_midi_parse(const unsigned char *data, size_t size, JbSong *song, char *error, size_t capacity);
void jb_song_free(JbSong *song);

#endif
