#ifndef JB_FILES_H
#define JB_FILES_H

#include "instruments.h"

enum { JB_NAME_MAX = 48, JB_LIST_MAX = 128 };
typedef struct {
    JbRoute routes[16];
    int resume[16], bank, fixed_sound, bpm, speed, key, master;
} JbMix;
/* text has size bytes followed by a null byte. */
int jb_mix_parse(char *text, size_t size, JbMix *mix, char *error, size_t capacity);
int jb_load_mix(const char *name, JbMix *mix, char *error, size_t capacity);
int jb_save_mix(const char *name, const JbMix *mix, char *error, size_t capacity);
int jb_song_name(const char *input, char name[JB_NAME_MAX + 1]);
int jb_resolve_song(const char *input, char name[JB_NAME_MAX + 1],
    char matches[8][JB_NAME_MAX + 1], size_t *count, char *error, size_t capacity);
int jb_load_instruments(JbInstruments *instruments, char *error, size_t capacity);
int jb_load_sounds(JbSounds *sounds, char *error, size_t capacity);
int jb_load_song(const char *name, JbSong *song, char *error, size_t capacity);
int jb_list_songs(char names[JB_LIST_MAX][JB_NAME_MAX + 1], size_t *count, int *more,
    char *error, size_t capacity);

#endif
