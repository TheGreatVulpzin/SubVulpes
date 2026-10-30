#ifndef JB_FILES_H
#define JB_FILES_H

#include "instruments.h"

enum { JB_NAME_MAX = 48, JB_LIST_MAX = 128 };
int jb_song_name(const char *input, char name[JB_NAME_MAX + 1]);
int jb_load_instruments(JbInstruments *instruments, char *error, size_t capacity);
int jb_load_song(const char *name, JbSong *song, char *error, size_t capacity);
int jb_list_songs(char names[JB_LIST_MAX][JB_NAME_MAX + 1], size_t *count, int *more,
    char *error, size_t capacity);

#endif
