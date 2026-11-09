#include "../common/permissions.h"
#include "../patchouli/sv_patchouli.h"
#include "sv_ayalog.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define RING 128u
#define LINE 200u
#define SHOW_DEFAULT 10
#define SHOW_MAX 100

static Official api;
static char ring[RING][LINE];
static uint32_t written;                 /* lines written since the plugin started */
static int record_actions = 1, record_failures = 1;
static SvLogging service;

static const char *result_name(SvResult result)
{
    static const char *const names[] = { "SV_OK", "SV_INVALID", "SV_STATE", "SV_LIMIT", "SV_CONFLICT", "SV_DENIED",
        "SV_NOT_FOUND", "SV_INCOMPATIBLE", "SV_IO", "SV_DEPENDENCY", "SV_RESTART", "SV_FAILED" };
    return result >= 0 && (size_t)result < sizeof names / sizeof names[0] ? names[result] : "SV_UNKNOWN";
}

static int printable(const char *text, size_t low, size_t high)
{
    size_t length = text ? strnlen(text, high + 1) : 0, i;
    if (length < low || length > high) return 0;
    for (i = 0; i < length; ++i)
        if ((unsigned char)text[i] < 32 || (unsigned char)text[i] > 126) return 0;
    return 1;
}

static int word(const char *text)
{
    size_t i;
    if (!printable(text, 1, 23)) return 0;
    for (i = 0; text[i]; ++i)
        if (!((text[i] >= 'a' && text[i] <= 'z') || (text[i] >= '0' && text[i] <= '9') || text[i] == '_' || text[i] == '-'))
            return 0;
    return 1;
}

/* "NAME (acc N)", "NAME" for a player without an account number, or "console". */
static void describe(Official *ctx, SvEntity entity, char *out, size_t size)
{
    SvPlayerState player = { sizeof player };
    char name[40];
    size_t i;
    int32_t account;
    if (!entity) { snprintf(out, size, "console"); return; }
    if (ctx->read->player(ctx->owner, entity, &player) != SV_OK) { snprintf(out, size, "a player that left"); return; }
    snprintf(name, sizeof name, "%s", player.name);
    for (i = 0; name[i]; ++i)
        if ((unsigned char)name[i] < 32 || (unsigned char)name[i] > 126) name[i] = '?';
    account = of_account(ctx, &player);
    if (account >= 0) snprintf(out, size, "%s (acc %d)", name, account);
    else snprintf(out, size, "%s", name);
}

static SvResult SV_CALL record(SvOwner caller, const SvLogEntry *entry)
{
    Official ctx = api;
    char line[LINE], by[80], on[96] = "", reason[48] = "", extra[96] = "";
    if (!caller || !entry || entry->size < sizeof *entry || entry->kind > SV_LOG_FAILURE || !word(entry->action) ||
        (entry->detail && !printable(entry->detail, 0, 79)) || (entry->kind == SV_LOG_ACTION) != (entry->result == SV_OK))
        return SV_INVALID;
    if (entry->kind == SV_LOG_ACTION ? !record_actions : !record_failures) return SV_OK;
    ctx.owner = caller;
    describe(&ctx, entry->actor, by, sizeof by);
    if (entry->target) {
        char name[80];
        describe(&ctx, entry->target, name, sizeof name);
        snprintf(on, sizeof on, " target %s", name);
    }
    if (entry->result != SV_OK) snprintf(reason, sizeof reason, " %s(%d)", result_name(entry->result), (int)entry->result);
    if (entry->detail && *entry->detail) snprintf(extra, sizeof extra, " %s", entry->detail);
    snprintf(line, sizeof line, "%s by %s%s result %s%s%s", entry->action, by, on, entry->result == SV_OK ? "OK" : "FAIL",
             reason, extra);
    strcpy(ring[written % RING], line);
    ++written;
    return api.host->log(caller, line);
}

/* The lines from the oldest to the newest, at most count of them. */
static void show(SvEntity actor, int32_t count)
{
    char text[SV_PATCHOULI_TEXT_MAX + 1];
    const void *table = NULL;
    uint32_t keep = written < RING ? written : RING, first, i;
    size_t used = 0;
    if ((uint32_t)count < keep) keep = (uint32_t)count;
    if (!keep) { of_tell(&api, actor, "The log is empty."); return; }
    first = written - keep;
    text[0] = 0;
    for (i = 0; i < keep; ++i)
        used += (size_t)snprintf(text + used, sizeof text - used, "%s\n", ring[(first + i) % RING]);
    if (actor && api.plugins->borrow(api.owner, SV_PATCHOULI_API, SV_PATCHOULI_VERSION, &table) == SV_OK && table &&
        ((const SvPatchouli *)table)->size >= sizeof(SvPatchouli) && ((const SvPatchouli *)table)->pages &&
        ((const SvPatchouli *)table)->pages(api.owner, actor, "Log", text) == SV_OK)
        return;
    for (i = 0; i < keep; ++i) of_tell(&api, actor, "%s", ring[(first + i) % RING]);
}

static SvResult SV_CALL logs_command(SvOwner owner, uint64_t actor, const char *arguments)
{
    char copy[32], first[16];
    char *rest = copy;
    int32_t count = SHOW_DEFAULT;
    (void)owner;
    if (!of_can(&api, actor, "logging.view")) { of_tell(&api, actor, "You need the permission logging.view for this."); return SV_OK; }
    snprintf(copy, sizeof copy, "%s", arguments ? arguments : "");
    if (of_token(&rest, first, sizeof first) < 0 || *rest || (*first && !of_number(first, 1, SHOW_MAX, &count))) {
        of_tell(&api, actor, "Use: /logs [COUNT]. COUNT is 1 to %d.", SHOW_MAX);
        return SV_OK;
    }
    show(actor, count);
    return SV_OK;
}

static void read_settings(void)
{
    record_actions = of_int(&api, "record_actions", 1) != 0;
    record_failures = of_int(&api, "record_failures", 1) != 0;
}

static SvResult SV_CALL event(SvOwner owner, const SvEvent *base)
{
    (void)owner;
    if (base->kind == SV_EVENT_CONFIG) read_settings();
    return SV_OK;
}

static void register_topic(void)
{
    const void *table = NULL;
    SvPatchouliTopic topic = { sizeof topic };
    if (api.plugins->borrow(api.owner, SV_PATCHOULI_API, SV_PATCHOULI_VERSION, &table) != SV_OK || !table ||
        ((const SvPatchouli *)table)->size < sizeof(SvPatchouli)) return;
    topic.command = "logs"; topic.usage = "[COUNT]"; topic.summary = "Show the last lines of the action log"; topic.node = "logging.view";
    ((const SvPatchouli *)table)->topic(api.owner, &topic);
}

static SvResult SV_CALL enable(const SvHost *host, SvOwner owner)
{
    SvResult result = of_init(&api, host, owner, OF_CAP(SV_API_GAME_READ) | OF_CAP(SV_API_GAME_WRITE) |
        OF_CAP(SV_API_CONFIG) | OF_CAP(SV_API_PLUGINS));
    if (result != SV_OK) return result;
    memset(ring, 0, sizeof ring); written = 0;
    read_settings();
    service.size = sizeof service; service.version = SV_LOGGING_VERSION; service.record = record;
    result = api.plugins->provide(owner, SV_LOGGING_API, SV_LOGGING_VERSION, &service);
    if (result == SV_OK) result = host->command(owner, "logs", SV_COMMAND_PUBLIC, logs_command);
    if (result == SV_OK) result = host->subscribe(owner, SV_EVENT_CONFIG, event);
    if (result == SV_OK) register_topic();
    return result;
}

static void SV_CALL disable(SvOwner owner)
{
    const void *table = NULL;
    if (api.plugins && api.plugins->borrow(owner, SV_PATCHOULI_API, SV_PATCHOULI_VERSION, &table) == SV_OK && table &&
        ((const SvPatchouli *)table)->size >= sizeof(SvPatchouli))
        ((const SvPatchouli *)table)->forget(owner);
    memset(ring, 0, sizeof ring); written = 0;
}

SV_EXPORT SvResult SV_CALL sv_plugin_entry(uint32_t abi, SvPlugin *plugin)
{
    if (abi != SV_PLUGIN_ABI || !plugin || plugin->size < sizeof *plugin) return SV_INCOMPATIBLE;
    memset(plugin, 0, sizeof *plugin);
    plugin->size = sizeof *plugin; plugin->abi = SV_PLUGIN_ABI;
    strcpy(plugin->id, SV_OFFICIAL_ID); strcpy(plugin->version, SV_OFFICIAL_VERSION);
    plugin->flags = SV_PLUGIN_LIVE; plugin->enable = enable; plugin->disable = disable;
    return SV_OK;
}
