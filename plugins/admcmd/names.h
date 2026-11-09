#ifndef SV_ADMCMD_NAMES_H
#define SV_ADMCMD_NAMES_H

#include <ctype.h>
#include <stdint.h>
#include <string.h>

/* Names that stand in for the numbers of a command. The first name of a value is its main name.
 * A name is lowercase. The player types it in any case. */
typedef struct {
    const char *name;
    int32_t value;
} NameEntry;

typedef struct {
    const char *kind;        /* "weapon", "item", ... */
    int32_t low, high;       /* the number range */
    const NameEntry *entries;
    uint32_t count;
} NameSet;

static const NameEntry weapon_names[] = {
    { "ak47", 1 }, { "ak-47", 1 }, { "ak", 1 }, { "m16", 2 }, { "m-16", 2 }, { "magnum", 3 },
    { "mp5", 4 }, { "uzi", 5 }, { "9mm", 6 }
};

/* The 26 item types that /item gives. */
static const NameEntry item_names[] = {
    { "ak-47", 0 }, { "ak47", 0 }, { "ak-47-magazine", 1 }, { "m-16", 2 }, { "m16", 2 }, { "m-16-magazine", 3 },
    { "magnum", 4 }, { "magnum-magazine", 5 }, { "45-bullets", 5 }, { "mp5", 6 }, { "mp5-magazine", 7 },
    { "uzi", 8 }, { "uzi-magazine", 9 }, { "9mm", 10 }, { "9mm-magazine", 11 }, { "bandage", 12 },
    { "briefcase", 13 }, { "open-briefcase", 14 }, { "cash", 15 }, { "cash-2", 16 }, { "black-disk", 17 },
    { "green-disk", 18 }, { "blue-disk", 19 }, { "white-disk", 20 }, { "gold-disk", 21 }, { "red-disk", 22 },
    { "cell-phone", 23 }, { "phone", 23 }, { "key", 24 }, { "door", 25 }
};

/* Types 12 and 13 are the train pieces. Only the track system makes them. */
static const NameEntry vehicle_names[] = {
    { "towncar", 0 }, { "town-car", 0 }, { "towncar2", 1 }, { "town-car-2", 1 }, { "metro", 2 }, { "limo", 3 },
    { "turbo", 4 }, { "turbos", 5 }, { "turbo-s", 5 }, { "beamer", 6 }, { "van", 7 }, { "minivan", 8 },
    { "truck", 9 }, { "trailer", 10 }, { "heli", 11 }, { "helicopter", 11 }
};

/* The company teams of the game. Team 5 has no name in the game data. */
static const NameEntry team_names[] = {
    { "goldmen", 0 }, { "goldmen-inc", 0 }, { "monsota", 1 }, { "oxs", 2 }, { "oxs-international", 2 },
    { "nexaco", 3 }, { "pentacom", 4 }
};

/* The sounds of the client that make a noise. The others are silent. */
static const NameEntry sound_names[] = {
    { "magnum", 0 }, { "smg", 1 }, { "rifle", 2 }, { "engine", 8 }, { "squeal", 10 }, { "ricochet1", 11 },
    { "ricochet2", 12 }, { "ricochet3", 13 }, { "ricochet4", 14 }, { "ricochet5", 15 }, { "ricochet6", 16 },
    { "ricochet7", 17 }, { "ricochet8", 18 }, { "crash1", 19 }, { "crash2", 20 }, { "thud", 21 }, { "thud2", 22 },
    { "metal1", 23 }, { "metal2", 24 }, { "glass", 25 }, { "ring", 27 }, { "digit0", 28 }, { "digit1", 29 },
    { "digit2", 30 }, { "digit3", 31 }, { "digit4", 32 }, { "digit5", 33 }, { "digit6", 34 }, { "digit7", 35 },
    { "digit8", 36 }, { "digit9", 37 }, { "reload", 38 }, { "ping", 39 }, { "gear", 40 }, { "heli", 41 },
    { "stream", 42 }
};

#define NAME_SET(kind, low, high, table) { kind, low, high, table, (uint32_t)(sizeof table / sizeof table[0]) }
static const NameSet weapon_set = NAME_SET("weapon", 1, 6, weapon_names);
static const NameSet item_set = NAME_SET("item", 0, 25, item_names);
static const NameSet vehicle_set = NAME_SET("vehicle", 0, 11, vehicle_names);
static const NameSet team_set = NAME_SET("team", 0, 5, team_names);
static const NameSet sound_set = NAME_SET("sound", 0, 46, sound_names);
/* A set with no names: only the number works. */
static const NameSet account_set = { "account number", 0, 65535, NULL, 0 };
#undef NAME_SET

enum { NAME_MATCHES_MAX = 8 };

/* Looks up a name. Pass 0 wants the whole name, pass 1 a prefix and pass 2 a part inside the name. The first pass with a
 * match decides. 1 means one value, 0 means none and -1 means several values. Then matches lists the main names. */
static int name_lookup(const NameSet *set, const char *text, int32_t *value, char *matches, size_t size)
{
    char lower[48];
    size_t length = strnlen(text, sizeof lower);
    uint32_t pass, i, j, distinct;
    int32_t values[NAME_MATCHES_MAX];
    if (!length || length >= sizeof lower) return 0;
    for (i = 0; i <= length; ++i) lower[i] = (char)tolower((unsigned char)text[i]);
    for (pass = 0; pass < 3; ++pass) {
        distinct = 0;
        for (i = 0; i < set->count; ++i) {
            const char *name = set->entries[i].name;
            int hit = pass == 0 ? !strcmp(name, lower) : pass == 1 ? !strncmp(name, lower, length) : strstr(name, lower) != NULL;
            if (!hit) continue;
            for (j = 0; j < distinct && values[j] != set->entries[i].value; ++j) {}
            if (j == distinct && distinct < NAME_MATCHES_MAX) values[distinct++] = set->entries[i].value;
        }
        if (distinct == 1) { *value = values[0]; return 1; }
        if (distinct > 1) {
            size_t used = 0;
            if (size) matches[0] = 0;
            for (j = 0; j < distinct; ++j)
                for (i = 0; i < set->count; ++i)
                    if (set->entries[i].value == values[j]) {
                        size_t room = size > used ? size - used : 0;
                        size_t need = strlen(set->entries[i].name) + (used ? 2 : 0);
                        if (need + 1 <= room) {
                            if (used) { matches[used++] = ','; matches[used++] = ' '; }
                            strcpy(matches + used, set->entries[i].name);
                            used += strlen(set->entries[i].name);
                        }
                        break;
                    }
            return -1;
        }
    }
    return 0;
}

/* The main name of a value, or NULL. */
static const char *name_of(const NameSet *set, int32_t value)
{
    uint32_t i;
    for (i = 0; i < set->count; ++i)
        if (set->entries[i].value == value) return set->entries[i].name;
    return NULL;
}

#endif
