#ifndef SV_RUNTIME_SPIKE_H
#define SV_RUNTIME_SPIKE_H
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define REQUIRE(test) do { if (!(test)) { fprintf(stderr, "FAILED %s:%d: %s\n", __FILE__, __LINE__, #test); exit(1); } } while (0)
static unsigned short control_word(void)
{
    unsigned short value;
    __asm fnstcw value
    return value;
}
static void set_control_word(unsigned short value)
{
    __asm fldcw value
}
static double elapsed_us(LARGE_INTEGER start, LARGE_INTEGER stop, LARGE_INTEGER frequency, int count)
{
    return (double)(stop.QuadPart - start.QuadPart) * 1000000.0 / (double)frequency.QuadPart / count;
}
static unsigned char *read_module(const char *name, uint32_t *length)
{
    FILE *file = fopen(name, "rb");
    long size;
    unsigned char *data;
    REQUIRE(file != NULL);
    REQUIRE(fseek(file, 0, SEEK_END) == 0);
    size = ftell(file);
    REQUIRE(size > 0 && size < 65536 && fseek(file, 0, SEEK_SET) == 0);
    data = malloc((size_t)size);
    REQUIRE(data != NULL && fread(data, 1, (size_t)size, file) == (size_t)size);
    REQUIRE(fclose(file) == 0);
    *length = (uint32_t)size;
    return data;
}
#endif
