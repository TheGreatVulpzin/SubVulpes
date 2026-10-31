#include "instruments.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CHECK(x) do { if (!(x)) { fprintf(stderr, "CHECK line %d: %s\n", __LINE__, #x); return 1; } } while (0)
int main(int argc, char **argv)
{
    FILE *file;
    char text[16385], error[160];
    size_t size;
    JbInstruments instruments;
    JbTone tone;
    JbNote notes[] = {{0,0,60,100,0}, {15,0,64,100,0}, {30,0,67,100,0}, {45,0,72,100,0}};
    JbSong song = {0};
    JbRoute routes[16] = {0};
    int bank, key, transpose, i;
    CHECK(argc == 2 && (file = fopen(argv[1], "rb")) != NULL);
    size = fread(text, 1, sizeof text, file);
    fclose(file);
    CHECK(size < sizeof text && jb_instruments_parse(text, size, &instruments, error, sizeof error));
    CHECK(instruments.members[0].sound == 27 && instruments.members[0].base_hz == 1997);
    CHECK(instruments.members[1].sound == 28 && instruments.members[1].base_hz == 941);
    CHECK(instruments.members[2].sound == 29 && instruments.members[2].base_hz == 697);
    CHECK(instruments.drums.sound == 21 && instruments.drums.base_hz == 67);
    CHECK(instruments.drum_pitch[36] < instruments.drum_pitch[38] && instruments.drum_pitch[38] < instruments.drum_pitch[42]);
    CHECK(jb_bank_index("nearest", &bank) && bank == JB_BANK_NEAREST);
    CHECK(jb_bank_index("keypad", &bank) && bank == JB_BANK_KEYPAD);
    CHECK(!jb_bank_index("unknown", &bank));
    for (bank = JB_BANK_KEYPAD; bank < JB_MEMBERS; ++bank)
        for (key = -36; key <= 163; ++key) {
            tone = jb_synth(&instruments, key, bank, 0);
            CHECK(isfinite(tone.pitch) && tone.pitch >= 0.125 && tone.pitch <= 2);
            CHECK(tone.sound >= 27 && tone.sound <= 37);
            if (bank == JB_BANK_KEYPAD)
                CHECK(tone.pitch == 1 && tone.sound >= 28);
            else if (bank >= 0)
                CHECK(tone.sound == (unsigned)bank + 27);
        }
    tone = jb_synth(&instruments, 60, JB_BANK_NEAREST, 0);
    CHECK(tone.sound == 29 && !tone.octaves && fabs(tone.pitch - 0.375359491) < 0.000002);
    tone = jb_synth(&instruments, 81, JB_BANK_NEAREST, 0);
    CHECK(tone.sound == 35 && fabs(tone.pitch - 1.03286385) < 0.000002);
    tone = jb_synth(&instruments, 0, JB_BANK_NEAREST, 0);
    CHECK(tone.octaves == 4 && tone.sound == 29);
    tone = jb_synth(&instruments, 127, JB_BANK_NEAREST, 0);
    CHECK(tone.octaves == -2 && tone.sound == 27);
    for (key = 0; key < 128; ++key)
        for (transpose = -36; transpose <= 36; ++transpose) {
            tone = jb_drum(&instruments, (unsigned)key, transpose);
            CHECK(tone.sound == 21 && isfinite(tone.pitch) && tone.pitch >= 0.125 && tone.pitch <= 2);
        }
    for (bank = 0; bank <= 46; ++bank)
        for (key = -84; key <= 211; ++key) {
            tone = jb_voice(bank, 60, key);
            CHECK(tone.sound == (unsigned)bank && tone.pitch >= 0.125 && tone.pitch <= 2);
        }
    CHECK(jb_voice(39, 109, 109).pitch == 1);
    CHECK(jb_voice(21, 60, 72).pitch == 2);
    CHECK(jb_voice(0, 60, 24).pitch == 0.125);
    CHECK(jb_voice(0, 60, 12).pitch == 0.125 && jb_voice(0, 60, 12).octaves == 1);
    CHECK(jb_voice(46, 60, 84).pitch == 2 && jb_voice(46, 60, 84).octaves == -1);
    song.notes = notes;
    song.count = sizeof notes / sizeof notes[0];
    for (i = 0; i < 16; ++i)
        routes[i].output = i == 9 ? JB_DRUMS : JB_SYNTH, routes[i].voice = -1;
    CHECK(jb_keypad_octaves(&instruments, &song, routes) == 1);
    routes[0].transpose = 12;
    CHECK(jb_keypad_octaves(&instruments, &song, routes) == 0);
    routes[0].transpose = -12;
    CHECK(jb_keypad_octaves(&instruments, &song, routes) == 2);
    routes[0].muted = 1;
    CHECK(jb_keypad_octaves(&instruments, &song, routes) == 0);
    puts("JUKEBOX_INSTRUMENTS_PASS: all banks, notes, drum pitches and keypad transposition");
    return 0;
}
