/* Read temporary text fixtures without loading a server or game data. */
#include "files.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
int main(int argc, char **argv)
{
    FILE *file;
    char text[20001], error[160] = {0};
    size_t size;
    int ok;
    if (argc != 3 || !(file = fopen(argv[2], "rb"))) return 3;
    size = fread(text, 1, sizeof text - 1, file);
    fclose(file);
    text[size] = 0;
    if (!strcmp(argv[1], "sounds")) {
        JbSounds sounds;
        ok = jb_sounds_parse(text, size, &sounds, error, sizeof error);
    } else {
        JbMix mix;
        ok = jb_mix_parse(text, size, &mix, error, sizeof error);
    }
    puts(ok ? "ACCEPT" : error);
    return ok ? 0 : 2;
}
