#include "groups.h"
#include "../../plugins/common/official.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CHECK(x) do { if (!(x)) { fprintf(stderr, "%d: %s\n", __LINE__, #x); exit(1); } } while (0)

static uint32_t message_bytes, message_calls, stored_length;
static int api_mode, entity_mode;
static SvGameRead read_table = { sizeof read_table, SV_GAME_READ_VERSION };

static SvResult SV_CALL get_api(SvOwner owner, uint32_t capability, uint32_t version, const void **table)
{
    CHECK(owner == 1 && capability == SV_API_GAME_READ && version == SV_GAME_READ_VERSION);
    *table = api_mode == 2 ? NULL : &read_table;
    return api_mode == 1 ? SV_FAILED : SV_OK;
}

static SvResult SV_CALL entities(SvOwner owner, uint32_t kind, uint32_t cursor, SvEntity *out,
    uint32_t capacity, uint32_t *count, uint32_t *next)
{
    CHECK(owner == 1 && kind == SV_ENTITY_PLAYER && capacity == 1);
    *out = 123;
    *count = entity_mode ? 0 : 1;
    *next = cursor + (entity_mode ? 1 : 2);
    return SV_OK;
}
static SvResult SV_CALL message(SvOwner owner, uint32_t kind, SvEntity actor, const char *text)
{
    size_t i, length = strnlen(text, 64);
    CHECK(owner == 1 && kind == SV_MESSAGE_SERVER && actor == 1 && length > 0 && length <= 63);
    for (i = 0; i < length; ++i) CHECK(text[i] >= 32 && text[i] <= 126);
    message_bytes += (uint32_t)length; ++message_calls;
    return SV_OK;
}

static SvResult SV_CALL store_get(SvOwner owner, const SvStorageKey *key, SvStorageValue *value)
{
    CHECK(owner == 1 && key->scope == SV_STORAGE_SERVER);
    value->length = stored_length;
    memset(value->bytes, 0, sizeof value->bytes);
    return SV_OK;
}

static SvResult SV_CALL store_put(SvOwner owner, const SvStorageKey *key, const SvStorageValue *value)
{
    CHECK(owner == 1 && key->scope == SV_STORAGE_SERVER);
    stored_length = value->length;
    return SV_OK;
}

static void common_bounds(void)
{
    Official ctx = { 0 };
    SvGameWrite write = { sizeof write };
    SvGameStorage store = { sizeof store };
    SvHost host = { sizeof host, SV_PLUGIN_ABI };
    SvEntity found = 999;
    char text[201], output[1025];
    CHECK(of_init(&ctx, &host, 1, 0) == SV_INCOMPATIBLE);
    host.get_api = get_api;
    host.size = 1;
    CHECK(of_init(&ctx, &host, 1, 0) == SV_INCOMPATIBLE);
    host.size = sizeof host;
    read_table.size = 1;
    CHECK(of_init(&ctx, &host, 1, OF_CAP(SV_API_GAME_READ)) == SV_INCOMPATIBLE);
    read_table.size = sizeof read_table;
    api_mode = 1;
    CHECK(of_init(&ctx, &host, 1, OF_CAP(SV_API_GAME_READ)) == SV_INCOMPATIBLE);
    api_mode = 2;
    CHECK(of_init(&ctx, &host, 1, OF_CAP(SV_API_GAME_READ)) == SV_INCOMPATIBLE);
    api_mode = 0;
    CHECK(of_init(&ctx, &host, 1, OF_CAP(SV_API_GAME_READ)) == SV_OK);
    read_table.entities = entities;
    CHECK(of_slot(&ctx, SV_ENTITY_PLAYER, 0, &found) == SV_NOT_FOUND && found == 999);
    entity_mode = 1;
    CHECK(of_slot(&ctx, SV_ENTITY_PLAYER, 255, &found) == SV_NOT_FOUND && found == 999);
    write.message_to = message; store.get = store_get; store.put = store_put;
    ctx.owner = 1; ctx.write = &write; ctx.store = &store;
    memset(text, 'x', 200); text[200] = 0; text[4] = '\n'; text[5] = 0x7f;
    of_tell(&ctx, 1, "%s", text);
    CHECK(message_calls == 4 && message_bytes == 200);
    stored_length = 16;
    memset(output, 0x55, sizeof output);
    CHECK(of_get(&ctx, SV_STORAGE_SERVER, 0, "key", output, 8) == SV_INVALID && output[0] == 0x55);
    stored_length = 1025;
    CHECK(of_get(&ctx, SV_STORAGE_SERVER, 0, "key", output, sizeof output) == SV_INVALID && output[0] == 0x55);
    CHECK(of_put(&ctx, SV_STORAGE_SERVER, 0, "key", output, sizeof output) == SV_INVALID && stored_length == 1025);
    CHECK(of_put(&ctx, SV_STORAGE_SERVER, 0, "key", NULL, 8) == SV_INVALID && stored_length == 1025);
    CHECK(of_put(&ctx, SV_STORAGE_SERVER, 0, "key", output, 8) == SV_OK && stored_length == 8);
}

int main(void)
{
    PmGroups groups;
    PmGroup *child;
    int32_t number = 999;
    char words[] = "\"Two Words\" tail", short_word[] = "abc", word[24], *rest = words;
    common_bounds();
    CHECK(of_token(&rest, NULL, 0) == -1);
    CHECK(of_token(&rest, word, sizeof word) == 1 && !strcmp(word, "Two Words") && !strcmp(rest, "tail"));
    rest = short_word;
    CHECK(of_token(&rest, word, 3) == -1);
    CHECK(of_number("0", 0, 100, &number) && number == 0);
    CHECK(of_number("100", 0, 100, &number) && number == 100);
    CHECK(!of_number("-1", 0, 100, &number) && !of_number("101", 0, 100, &number));
    CHECK(!of_number("18446744073709551616", 0, 100, &number));
    CHECK(!of_number("1x", 0, 100, &number) && !of_number("+1", 0, 100, &number) && !of_number("", 0, 100, &number));
    pm_defaults(&groups);
    CHECK(pm_valid(&groups) && groups.count == 5);
    CHECK(pm_can(&groups, pm_find(&groups, "admin"), "core.kick"));
    CHECK(pm_can(&groups, pm_find(&groups, "admin"), "admcmd.heal"));
    CHECK(!pm_can(&groups, pm_find(&groups, "admin"), "unrelated.node"));
    CHECK(pm_can(&groups, pm_find(&groups, "moderator"), "core.kick"));
    CHECK(!pm_can(&groups, pm_find(&groups, "moderator"), "core.cash"));
    CHECK(pm_can(&groups, pm_find(&groups, "member"), "chatadd.msg"));
    CHECK(!pm_can(&groups, -1, "core.kick"));
    CHECK(!pm_name("") && !pm_name("UPPER") && !pm_name("../x") && !pm_name("a b") && !pm_name("1abc"));
    CHECK(!pm_name("abcdefghijklmnopqrstuvwx") && pm_name("abcdefghijklmnopqrstuvw"));
    CHECK(pm_node("admcmd.*", 0) && pm_node("-core.give", 1) && pm_node("*", 0));
    CHECK(!pm_node("-core.give", 0) && !pm_node("core..give", 0) && !pm_node("core.", 0));
    CHECK(!pm_node("core.*.give", 0) && !pm_node("Core.give", 0));
    CHECK(pm_node("a.abcdefghijklmnopqrstuvwxyzabcdefghijk", 0));
    CHECK(!pm_node("a.abcdefghijklmnopqrstuvwxyzabcdefghijkl", 0));
    child = &groups.group[groups.count++];
    memset(child, 0, sizeof *child);
    child->format = 1; child->level = 25;
    strcpy(child->name, "helper"); strcpy(child->parent, "admin");
    strcpy(child->nodes[0], "-core.give"); child->node_count = 1;
    CHECK(pm_valid(&groups));
    CHECK(!pm_can(&groups, 5, "core.give") && pm_can(&groups, 5, "core.cash"));
    strcpy(child->nodes[1], "core.give"); child->node_count = 2;
    CHECK(!pm_can(&groups, 5, "core.give"));
    strcpy(child->nodes[0], "-core.*");
    CHECK(!pm_can(&groups, 5, "core.give") && !pm_can(&groups, 5, "core.cash"));
    strcpy(child->parent, "helper"); CHECK(!pm_valid(&groups));
    CHECK(!pm_can(&groups, 5, "absent.node"));
    strcpy(child->parent, "missing"); CHECK(!pm_valid(&groups));
    strcpy(child->parent, "admin"); child->level = 101; CHECK(!pm_valid(&groups));
    child->level = -1; CHECK(!pm_valid(&groups)); child->level = 25;
    child->node_count = PM_NODE_MAX + 1; CHECK(!pm_valid(&groups)); child->node_count = 2;
    groups.count = SV_PERMISSION_GROUP_MAX + 1; CHECK(!pm_valid(&groups)); groups.count = 6;
    memset(child->prefix, 'x', sizeof child->prefix); CHECK(!pm_valid(&groups)); child->prefix[0] = 0;
    CHECK(pm_valid(&groups));
    CHECK(pm_default("owner") && !pm_default("helper"));
    groups.group[4].level = 99; CHECK(!pm_valid(&groups)); groups.group[4].level = 100;
    {
        uint32_t i;
        struct { PmGroups state; PmGroup sentinel; } full;
        pm_defaults(&full.state);
        for (i = 5; i < SV_PERMISSION_GROUP_MAX; ++i) {
            PmGroup *g = &full.state.group[i];
            memset(g, 0, sizeof *g);
            g->format = 1;
            snprintf(g->name, sizeof g->name, "extra%u", i);
            strcpy(g->parent, "guest");
        }
        full.state.count = SV_PERMISSION_GROUP_MAX;
        CHECK(pm_valid(&full.state));
        memset(&full.sentinel, 0, sizeof full.sentinel);
        full.sentinel.format = 1;
        strcpy(full.sentinel.name, "sentinel"); strcpy(full.sentinel.parent, "guest");
        full.state.count = SV_PERMISSION_GROUP_MAX + 1;
        CHECK(!pm_valid(&full.state));
        memset(&groups.group[6], 0, sizeof groups.group[6]);
        groups.group[6].node_count = 1;
        strcpy(groups.group[6].nodes[0], "sentinel.node");
        CHECK(!pm_can(&groups, 6, "sentinel.node"));
        for (i = 0; i < PM_NODE_MAX; ++i) strcpy(child->nodes[i], "test.node");
        memcpy(&groups.group[6], "extra.node", sizeof "extra.node");
        child->node_count = PM_NODE_MAX + 1;
        CHECK(!pm_valid(&groups));
    }
    puts("Permission group rules passed.");
    return 0;
}
