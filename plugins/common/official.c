#include "official.h"
#include <ctype.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

SvResult of_init(Official *ctx, const SvHost *host, SvOwner owner, uint32_t caps)
{
    const void *table;
    if (!host || host->abi != SV_PLUGIN_ABI || host->size < offsetof(SvHost, random) + sizeof host->random || !host->get_api)
        return SV_INCOMPATIBLE;
    memset(ctx, 0, sizeof *ctx);
    ctx->host = host;
    ctx->owner = owner;
#define GET(id, type, field, version, last) \
    if (caps & OF_CAP(id)) { \
        if (host->get_api(owner, id, version, &table) != SV_OK || !table || \
            ((const type *)table)->size < offsetof(type, last) + sizeof(((const type *)table)->last)) \
            return SV_INCOMPATIBLE; \
        ctx->field = (const type *)table; \
    }
    GET(SV_API_GAME_READ, SvGameRead, read, SV_GAME_READ_VERSION, connection)
    GET(SV_API_GAME_WRITE, SvGameWrite, write, SV_GAME_WRITE_VERSION, kicked_clear)
    GET(SV_API_GAME_PROPS, SvGameProps, props, SV_GAME_PROPS_VERSION, add)
    GET(SV_API_STORAGE, SvGameStorage, store, SV_GAME_STORAGE_VERSION, erase)
    GET(SV_API_CONFIG, SvConfig, config, SV_CONFIG_VERSION, get_text)
    GET(SV_API_PLUGINS, SvPlugins, plugins, SV_PLUGINS_VERSION, find)
#undef GET
    return SV_OK;
}

void of_tell(Official *ctx, SvEntity actor, const char *format, ...)
{
    char text[512], line[64];
    size_t offset = 0, length, i;
    va_list args;
    va_start(args, format);
    vsnprintf(text, sizeof text, format, args);
    va_end(args);
    for (i = 0; text[i]; ++i)
        if ((unsigned char)text[i] < 32 || (unsigned char)text[i] > 126)
            text[i] = '?';
    if (!actor) {
        ctx->host->log(ctx->owner, text);
        return;
    }
    length = strlen(text);
    while (offset < length) {
        size_t take = length - offset;
        if (take > sizeof line - 1) take = sizeof line - 1;
        memcpy(line, text + offset, take);
        line[take] = 0;
        ctx->write->message_to(ctx->owner, SV_MESSAGE_SERVER, actor, line);
        offset += take;
    }
}

/* A quoted token can hold a player name with spaces. */
int of_token(char **rest, char *word, size_t capacity)
{
    char *start = *rest, *end;
    size_t length;
    int quoted;
    if (!capacity) return -1;
    word[0] = 0;
    while (*start == ' ') ++start;
    if (!*start) { *rest = start; return 0; }
    quoted = *start == '"';
    if (quoted) ++start;
    end = start;
    while (*end && (quoted ? *end != '"' : *end != ' ')) ++end;
    length = (size_t)(end - start);
    if (!length || length >= capacity || (quoted && (*end != '"' || (end[1] && end[1] != ' '))))
        return -1;
    memcpy(word, start, length);
    word[length] = 0;
    if (quoted) ++end;
    while (*end == ' ') ++end;
    *rest = end;
    return 1;
}

int of_number(const char *text, int32_t low, int32_t high, int32_t *out)
{
    const char *p = text;
    char *end;
    __int64 value;
    if (!p || !*p) return 0;
    if (*p == '-') ++p;
    if (!*p) return 0;
    for (; *p; ++p) if (*p < '0' || *p > '9') return 0;
    value = _strtoi64(text, &end, 10);
    if (*end || value < low || value > high) return 0;
    *out = (int32_t)value;
    return 1;
}

SvResult of_slot(Official *ctx, uint32_t kind, uint32_t slot, SvEntity *out)
{
    SvEntity value;
    uint32_t count, next;
    SvResult result = ctx->read->entities(ctx->owner, kind, slot, &value, 1, &count, &next);
    if (result != SV_OK) return result;
    if (count != 1 || next != slot + 1) return SV_NOT_FOUND;
    *out = value;
    return SV_OK;
}

static int name_matches(const char *display, const char *name, int pass)
{
    char lower[32], needle[64];
    size_t i, length = strnlen(name, sizeof needle);
    if (length >= sizeof needle) return 0;
    for (i = 0; i < sizeof lower; ++i) lower[i] = (char)tolower((unsigned char)display[i]);
    for (i = 0; i <= length; ++i) needle[i] = (char)tolower((unsigned char)name[i]);
    return (pass == 0 && !strcmp(lower, needle)) || (pass == 1 && !strncmp(lower, needle, length)) ||
        (pass == 2 && strstr(lower, needle));
}

SvResult of_target(Official *ctx, SvEntity actor, const char *name, SvPlayerState *out)
{
    SvEntity page[256], found = 0;
    uint32_t count, next, i;
    int32_t slot;
    int pass;
    if (!name || !*name) return SV_INVALID;
    if (of_number(name, 0, 255, &slot)) {
        if (of_slot(ctx, SV_ENTITY_PLAYER, (uint32_t)slot, &found) != SV_OK) return SV_NOT_FOUND;
        return ctx->read->player(ctx->owner, found, out);
    }
    if (ctx->read->entities(ctx->owner, SV_ENTITY_PLAYER, 0, page, 256, &count, &next) != SV_OK)
        return SV_STATE;
    for (pass = 0; pass < 3; ++pass) {
        unsigned matches = 0;
        for (i = 0; i < count; ++i) {
            SvPlayerState state = { sizeof state };
            if (ctx->read->player(ctx->owner, page[i], &state) != SV_OK) continue;
            if (name_matches(state.name, name, pass)) {
                found = page[i];
                ++matches;
            }
        }
        if (matches == 1) return ctx->read->player(ctx->owner, found, out);
        if (matches > 1) {
            of_tell(ctx, actor, "Ambiguous player name. Use a full name or ID.");
            for (i = 0; i < count; ++i) {
                SvPlayerState state = { sizeof state };
                if (ctx->read->player(ctx->owner, page[i], &state) == SV_OK && name_matches(state.name, name, pass))
                    of_tell(ctx, actor, "%s", state.name);
            }
            return SV_CONFLICT;
        }
    }
    return SV_NOT_FOUND;
}

SvResult of_set(Official *ctx, const char *name, SvEntity entity, int32_t number)
{
    uint32_t id;
    SvValue value;
    SvResult result = ctx->props->find(ctx->owner, name, &id);
    value.i = number;
    return result == SV_OK ? ctx->props->set(ctx->owner, id, entity, 0, value) : result;
}

int32_t of_int(Official *ctx, const char *key, int32_t fallback)
{
    int32_t value;
    return ctx->config->get_int(ctx->owner, key, &value) == SV_OK ? value : fallback;
}

static SvStorageKey key_for(uint32_t scope, SvEntity subject, const char *name)
{
    SvStorageKey key = { sizeof key, 0, 0, { 0 } };
    key.scope = scope;
    key.subject = subject;
    if (strnlen(name, sizeof key.name) < sizeof key.name) strcpy(key.name, name);
    return key;
}

SvResult of_get(Official *ctx, uint32_t scope, SvEntity subject, const char *name, void *out, uint32_t size)
{
    SvStorageKey key = key_for(scope, subject, name);
    SvStorageValue value = { sizeof value };
    SvResult result = ctx->store->get(ctx->owner, &key, &value);
    if (result != SV_OK) return result;
    if (size != value.length || size > sizeof value.bytes) return SV_INVALID;
    memcpy(out, value.bytes, size);
    return SV_OK;
}

SvResult of_put(Official *ctx, uint32_t scope, SvEntity subject, const char *name, const void *data, uint32_t size)
{
    SvStorageKey key = key_for(scope, subject, name);
    SvStorageValue value = { sizeof value };
    if (!data || size > sizeof value.bytes) return SV_INVALID;
    value.length = size;
    memcpy(value.bytes, data, size);
    return ctx->store->put(ctx->owner, &key, &value);
}

SvResult of_erase(Official *ctx, uint32_t scope, SvEntity subject, const char *name)
{
    SvStorageKey key = key_for(scope, subject, name);
    return ctx->store->erase(ctx->owner, &key);
}
