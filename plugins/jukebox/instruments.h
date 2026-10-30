#ifndef JB_INSTRUMENTS_H
#define JB_INSTRUMENTS_H

#include "midi.h"

enum { JB_MEMBERS = 11, JB_BANK_NEAREST = -1, JB_BANK_KEYPAD = -2 };
enum { JB_OFF, JB_SYNTH, JB_DRUMS };
typedef struct { uint32_t sound; double base_hz; } JbMember;
typedef struct {
    JbMember members[JB_MEMBERS], drums;
    float drum_pitch[128];
} JbInstruments;
typedef struct { int output, muted, transpose; } JbRoute;
typedef struct { uint32_t sound; float pitch; int octaves; } JbTone;

const char *jb_bank_name(int bank);
int jb_bank_index(const char *name, int *bank);
int jb_instruments_parse(const char *data, size_t size, JbInstruments *out, char *error, size_t capacity);
JbTone jb_synth(const JbInstruments *instruments, int key, int bank, int keypad_octaves);
JbTone jb_drum(const JbInstruments *instruments, unsigned key, int transpose);
int jb_keypad_octaves(const JbInstruments *instruments, const JbSong *song, const JbRoute routes[16]);

#endif
