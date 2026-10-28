/* The probe plugin of the test runner. A scenario steps reads and changes the game through it, with the public API only.
 * Command: probe get PROP PLAYER [SLOT] | set PROP PLAYER VALUE [SLOT] | give TYPE PLAYER | count KIND.
 * Each answer is one log line that starts with "PROBE ". The runner reads these lines. */
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "sv_game_read.h"
#include "sv_game_write.h"
#include "sv_game_props.h"

static const SvHost *host;
static const SvGameRead *read_api;
static const SvGameWrite *write_api;
static const SvGameProps *props;

static void say(SvOwner owner, const char *format, ...)
{
    char text[200];
    va_list args;
    va_start(args, format);
    vsnprintf(text, sizeof text, format, args);
    va_end(args);
    host->log(owner, text);
}

/* Finds a player by name. */
static SvResult find_player(SvOwner owner, const char *name, SvPlayerState *out, SvEntity *entity)
{
    SvEntity page[SV_ENTITY_PAGE_MAX];
    uint32_t cursor = 0, count, next, i, round;
    for (round = 0; round < 8; ++round) {
        SvResult r = read_api->entities(owner, SV_ENTITY_PLAYER, cursor, page, SV_ENTITY_PAGE_MAX, &count, &next);
        if (r != SV_OK)
            return r;
        for (i = 0; i < count; ++i) {
            out->size = sizeof *out;
            if (read_api->player(owner, page[i], out) == SV_OK && !strcmp(out->name, name)) {
                *entity = page[i];
                return SV_OK;
            }
        }
        if (next <= cursor)
            break;
        cursor = next;
    }
    return SV_NOT_FOUND;
}

/* The entity that a property needs, taken from the player. */
static SvResult subject(const SvPlayerState *player, SvEntity player_entity, uint32_t kind, SvEntity *out)
{
    switch (kind) {
    case SV_ENTITY_PLAYER: *out = player_entity; return SV_OK;
    case SV_ENTITY_HUMAN: *out = player->human; return player->human ? SV_OK : SV_STATE;
    case SV_ENTITY_ACCOUNT: *out = player->account; return player->account ? SV_OK : SV_STATE;
    default: return SV_INVALID;
    }
}

static SvResult SV_CALL probe(SvOwner owner, uint64_t actor, const char *arguments)
{
    char line[200], *word[6];
    int words = 0;
    SvPlayerState player = { sizeof player };
    SvEntity player_entity = 0, target = 0;
    SvPropInfo info = { sizeof info };
    uint32_t id = 0, slot = 0;
    SvValue value;
    SvResult r;
    (void)actor;
    snprintf(line, sizeof line, "%s", arguments);
    for (char *p = strtok(line, " "); p && words < 6; p = strtok(NULL, " "))
        word[words++] = p;
    if (words >= 2 && !strcmp(word[0], "count")) {
        static const struct { const char *name; uint32_t kind; } kinds[] = {
            { "players", SV_ENTITY_PLAYER }, { "humans", SV_ENTITY_HUMAN }, { "items", SV_ENTITY_ITEM }, { "objects", SV_ENTITY_OBJECT } };
        uint32_t k, total = 0, cursor = 0, round;
        for (k = 0; k < 4 && strcmp(word[1], kinds[k].name); ++k) {}
        if (k == 4)
            return say(owner, "PROBE count %s error %d", word[1], (int)SV_INVALID), SV_OK;
        for (round = 0; round < 16; ++round) {
            SvEntity page[SV_ENTITY_PAGE_MAX];
            uint32_t count = 0, next = 0;
            if (read_api->entities(owner, kinds[k].kind, cursor, page, SV_ENTITY_PAGE_MAX, &count, &next) != SV_OK)
                break;
            total += count;
            if (next <= cursor)
                break;
            cursor = next;
        }
        say(owner, "PROBE count %s = %u", word[1], total);
        return SV_OK;
    }
    if (words >= 3 && !strcmp(word[0], "give")) {
        SvHumanState human = { sizeof human };
        SvEntity item = 0;
        r = find_player(owner, word[2], &player, &player_entity);
        if (r == SV_OK && !player.human)
            r = SV_STATE;
        if (r == SV_OK)
            r = read_api->human(owner, player.human, &human);
        if (r == SV_OK)
            r = write_api->item_create(owner, (uint32_t)atoi(word[1]), human.position, &item);
        if (r == SV_OK)
            r = write_api->item_equip(owner, item, player.human, 0);
        say(owner, "PROBE give %s %s %s", word[1], word[2], r == SV_OK ? "ok" : "error");
        if (r != SV_OK)
            say(owner, "PROBE code %d", (int)r);
        return SV_OK;
    }
    if (words < 3 || (strcmp(word[0], "get") && strcmp(word[0], "set")) || (!strcmp(word[0], "set") && words < 4)) {
        say(owner, "PROBE usage error");
        return SV_OK;
    }
    r = props->find(owner, word[1], &id);
    if (r == SV_OK)
        r = props->info(owner, id, &info);
    if (r == SV_OK)
        r = find_player(owner, word[2], &player, &player_entity);
    if (r == SV_OK)
        r = subject(&player, player_entity, info.entity_kind, &target);
    if (r != SV_OK) {
        say(owner, "PROBE %s %s %s error %d", word[0], word[1], word[2], (int)r);
        return SV_OK;
    }
    if (!strcmp(word[0], "get")) {
        if (words >= 4)
            slot = (uint32_t)atoi(word[3]);
        r = props->get(owner, id, target, slot, &value);
        if (r != SV_OK)
            say(owner, "PROBE get %s %s error %d", word[1], word[2], (int)r);
        else if (info.type == SV_PROP_INT)
            say(owner, "PROBE get %s %s = %d", word[1], word[2], (int)value.i);
        else if (info.type == SV_PROP_FLOAT)
            say(owner, "PROBE get %s %s = %.3f", word[1], word[2], value.f);
        else
            say(owner, "PROBE get %s %s = %.3f %.3f %.3f", word[1], word[2], value.v.x, value.v.y, value.v.z);
        return SV_OK;
    }
    if (words >= 5)
        slot = (uint32_t)atoi(word[4]);
    if (info.type == SV_PROP_INT)
        value.i = atoi(word[3]);
    else if (info.type == SV_PROP_FLOAT)
        value.f = (float)atof(word[3]);
    else {
        say(owner, "PROBE set %s %s error %d", word[1], word[2], (int)SV_INVALID);
        return SV_OK;
    }
    r = props->set(owner, id, target, slot, value);
    if (r == SV_OK)
        say(owner, "PROBE set %s %s ok", word[1], word[2]);
    else
        say(owner, "PROBE set %s %s error %d", word[1], word[2], (int)r);
    return SV_OK;
}

static SvResult SV_CALL enable(const SvHost *h, SvOwner owner)
{
    SvResult r;
    if (h->abi != SV_PLUGIN_ABI || h->size < SV_HOST_V1_SIZE)
        return SV_INCOMPATIBLE;
    host = h;
    r = h->get_api(owner, SV_API_GAME_READ, SV_GAME_READ_VERSION, (const void **)&read_api);
    if (r == SV_OK)
        r = h->get_api(owner, SV_API_GAME_WRITE, SV_GAME_WRITE_VERSION, (const void **)&write_api);
    if (r == SV_OK)
        r = h->get_api(owner, SV_API_GAME_PROPS, SV_GAME_PROPS_VERSION, (const void **)&props);
    return r == SV_OK ? h->command(owner, "probe", SV_COMMAND_LOCAL, probe) : r;
}

SV_EXPORT SvResult SV_CALL sv_plugin_entry(uint32_t abi, SvPlugin *plugin)
{
    if (abi != SV_PLUGIN_ABI || !plugin || plugin->size < sizeof *plugin)
        return SV_INCOMPATIBLE;
    memset(plugin, 0, sizeof *plugin);
    plugin->size = sizeof *plugin;
    plugin->abi = SV_PLUGIN_ABI;
    strcpy(plugin->id, "sv.probe");
    strcpy(plugin->version, "0.1.0");
    plugin->enable = enable;
    return SV_OK;
}
