#include "instruments.h"
#include <errno.h>
#include <float.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const char *const member_names[JB_MEMBERS] = {
    "ring", "digit0", "digit1", "digit2", "digit3", "digit4", "digit5", "digit6", "digit7", "digit8", "digit9"
};

const char *jb_bank_name(int bank)
{
    if (bank == JB_BANK_KEYPAD)
        return "keypad";
    return bank >= 0 && bank < JB_MEMBERS ? member_names[bank] : "nearest";
}

int jb_bank_index(const char *name, int *bank)
{
    int i;
    if (!strcmp(name, "nearest")) {
        *bank = JB_BANK_NEAREST;
        return 1;
    }
    if (!strcmp(name, "keypad")) {
        *bank = JB_BANK_KEYPAD;
        return 1;
    }
    for (i = 0; i < JB_MEMBERS; ++i)
        if (!strcmp(name, member_names[i])) {
            *bank = i;
            return 1;
        }
    return 0;
}

static int error_text(char *error, size_t capacity, const char *text)
{
    if (capacity)
        snprintf(error, capacity, "%s", text);
    return 0;
}

static char *trim(char *text)
{
    char *end;
    while (*text == ' ' || *text == '\t' || *text == '\r')
        ++text;
    end = text + strlen(text);
    while (end > text && (end[-1] == ' ' || end[-1] == '\t' || end[-1] == '\r'))
        --end;
    *end = 0;
    return text;
}

static int number(const char *text, double minimum, double maximum, int integer, double *value)
{
    char *end;
    errno = 0;
    *value = strtod(text, &end);
    return end != text && !*end && !errno && isfinite(*value) && *value >= minimum && *value <= maximum &&
        (!integer || floor(*value) == *value);
}

int jb_instruments_parse(const char *data, size_t size, JbInstruments *out, char *error, size_t capacity)
{
    unsigned seen[JB_MEMBERS + 1] = {0};
    unsigned char pitches[128] = {0};
    int version = 0, section = -3;
    size_t at = 0, i;
    memset(out, 0, sizeof *out);
    if (capacity)
        error[0] = 0;
    if (!size || size > 16384)
        return error_text(error, capacity, "Instrument file needs 1 to 16384 bytes. File unchanged.");
    while (at < size) {
        char line[192], *text, *equal;
        size_t start = at, length;
        double value;
        while (at < size && data[at] != '\n') {
            unsigned char c = (unsigned char)data[at++];
            if ((c < 32 && c != '\r' && c != '\t') || c > 126)
                return error_text(error, capacity, "Instrument file needs plain ASCII text. File unchanged.");
        }
        length = at - start;
        if (at < size)
            ++at;
        if (length >= sizeof line)
            return error_text(error, capacity, "Instrument line is too long. File unchanged.");
        memcpy(line, data + start, length);
        line[length] = 0;
        text = trim(line);
        if (!*text || *text == '#' || *text == ';')
            continue;
        if (*text == '[') {
            length = strlen(text);
            if (length < 3 || text[length - 1] != ']')
                return error_text(error, capacity, "Instrument section is invalid. File unchanged.");
            text[length - 1] = 0;
            ++text;
            if (!strcmp(text, "jukebox"))
                section = -1;
            else if (!strcmp(text, "drum_pitch"))
                section = -2;
            else if (!strcmp(text, "drums"))
                section = JB_MEMBERS;
            else {
                int bank;
                if (!jb_bank_index(text, &bank) || bank < 0)
                    return error_text(error, capacity, "Instrument section is unknown. File unchanged.");
                section = bank;
            }
            continue;
        }
        equal = strchr(text, '=');
        if (!equal)
            return error_text(error, capacity, "Instrument key needs an equals sign. File unchanged.");
        *equal++ = 0;
        text = trim(text);
        equal = trim(equal);
        if (section == -1) {
            if (strcmp(text, "version") || version || strcmp(equal, "1"))
                return error_text(error, capacity, "Instrument version must be 1, once. File unchanged.");
            version = 1;
        } else if (section == -2) {
            unsigned key;
            if (!number(text, 0, 127, 1, &value))
                return error_text(error, capacity, "Drum note needs 0 to 127. File unchanged.");
            key = (unsigned)value;
            if (pitches[key] || !number(equal, 0.125, 2.0, 0, &value))
                return error_text(error, capacity, "Drum pitch needs 0.125 to 2.0, once per note. File unchanged.");
            pitches[key] = 1;
            out->drum_pitch[key] = (float)value;
        } else if (section >= 0) {
            JbMember *member = section == JB_MEMBERS ? &out->drums : &out->members[section];
            unsigned flag;
            if (!strcmp(text, "sound")) {
                flag = 1;
                if (!number(equal, 0, 46, 1, &value))
                    return error_text(error, capacity, "Sound ID needs 0 to 46. File unchanged.");
                member->sound = (uint32_t)value;
            } else if (!strcmp(text, "base_hz")) {
                flag = 2;
                if (!number(equal, 20, 20000, 0, &value))
                    return error_text(error, capacity, "Base frequency needs 20 to 20000 Hz. File unchanged.");
                member->base_hz = value;
            } else {
                return error_text(error, capacity, "Instrument key is unknown. File unchanged.");
            }
            if (seen[section] & flag)
                return error_text(error, capacity, "Instrument key is repeated. File unchanged.");
            seen[section] |= flag;
        } else {
            return error_text(error, capacity, "Instrument key needs a section. File unchanged.");
        }
    }
    if (!version)
        return error_text(error, capacity, "Instrument version must be 1, once. File unchanged.");
    for (i = 0; i <= JB_MEMBERS; ++i)
        if (seen[i] != 3)
            return error_text(error, capacity, "Each instrument needs a sound and base_hz. File unchanged.");
    for (i = 0; i < 128; ++i)
        if (!pitches[i])
            return error_text(error, capacity, "Each drum note needs a pitch. File unchanged.");
    return 1;
}

static double frequency(int key)
{
    return 440.0 * pow(2.0, (key - 69) / 12.0);
}

static double fold(double pitch, int *octaves)
{
    *octaves = 0;
    while (pitch < 0.125) {
        pitch *= 2;
        ++*octaves;
    }
    while (pitch > 2.0) {
        pitch *= 0.5;
        --*octaves;
    }
    return pitch;
}

JbTone jb_synth(const JbInstruments *instruments, int key, int bank, int keypad_octaves)
{
    JbTone tone = {0};
    double hz = frequency(key), best = DBL_MAX;
    int i, selected = 0, pass;
    if (bank == JB_BANK_KEYPAD) {
        hz *= pow(2.0, keypad_octaves);
        for (i = 1; i < JB_MEMBERS; ++i) {
            double distance = fabs(log(hz / instruments->members[i].base_hz) / log(2.0));
            if (distance < best) {
                best = distance;
                selected = i;
            }
        }
        tone.sound = instruments->members[selected].sound;
        tone.pitch = 1.0f;
        tone.octaves = keypad_octaves;
        return tone;
    }
    if (bank >= 0 && bank < JB_MEMBERS) {
        tone.sound = instruments->members[bank].sound;
        tone.pitch = (float)fold(hz / instruments->members[bank].base_hz, &tone.octaves);
        return tone;
    }
    /* Try unchanged notes first. Only a note outside the entire bank moves by octaves. */
    for (pass = 0; pass < 2; ++pass) {
        int best_moves = 100;
        for (i = 0; i < JB_MEMBERS; ++i) {
            double pitch = hz / instruments->members[i].base_hz, score;
            int octaves = 0, moves;
            if (!pass && (pitch < 0.125 || pitch > 2.0))
                continue;
            if (pass)
                pitch = fold(pitch, &octaves);
            moves = abs(octaves);
            score = fabs(pitch - 1.0);
            if (moves < best_moves || (moves == best_moves && score < best)) {
                best_moves = moves;
                best = score;
                tone.sound = instruments->members[i].sound;
                tone.pitch = (float)pitch;
                tone.octaves = octaves;
            }
        }
        if (best != DBL_MAX)
            break;
    }
    return tone;
}

JbTone jb_drum(const JbInstruments *instruments, unsigned key, int transpose)
{
    JbTone tone = {0};
    tone.sound = instruments->drums.sound;
    tone.pitch = (float)fold(instruments->drum_pitch[key] * pow(2.0, transpose / 12.0), &tone.octaves);
    return tone;
}

int jb_keypad_octaves(const JbInstruments *instruments, const JbSong *song, const JbRoute routes[16])
{
    uint32_t histogram[200] = {0};
    double best = DBL_MAX;
    int shift, chosen = 0, key, member;
    size_t i;
    for (i = 0; i < song->count; ++i) {
        const JbNote *note = &song->notes[i];
        const JbRoute *route = &routes[note->channel];
        if (route->output == JB_SYNTH && !route->muted)
            ++histogram[note->key + route->transpose + 36];
    }
    /* MIDI notes plus the allowed transposition and base frequencies need at most 16 octaves. */
    for (shift = -16; shift <= 16; ++shift) {
        double cost = 0;
        for (key = 0; key < 200; ++key) {
            double nearest = DBL_MAX, hz;
            if (!histogram[key])
                continue;
            hz = frequency(key - 36) * pow(2.0, shift);
            for (member = 1; member < JB_MEMBERS; ++member) {
                double distance = fabs(log(hz / instruments->members[member].base_hz) / log(2.0));
                if (distance < nearest)
                    nearest = distance;
            }
            cost += nearest * histogram[key];
        }
        if (cost < best - 1e-9 || (fabs(cost - best) <= 1e-9 && abs(shift) < abs(chosen))) {
            best = cost;
            chosen = shift;
        }
    }
    return chosen;
}
