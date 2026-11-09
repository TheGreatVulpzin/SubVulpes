#include "../common/permissions.h"
#include "sv_patchouli.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define LINES_PER_PAGE 6u
#define PAGER_MAX 256u

typedef struct {
    SvOwner owner;
    char command[24], usage[48], summary[80], node[40];
} Topic;

typedef struct {
    SvEntity player;
    uint32_t shown;
    char title[32];
    char *text;   /* heap copy, freed when the player leaves or the text changes */
} Pager;

static Official api;
static Topic topics[SV_PATCHOULI_TOPIC_MAX];
static uint32_t topic_count;
static Pager pagers[PAGER_MAX];
static SvPatchouli service;

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

static SvResult SV_CALL service_topic(SvOwner caller, const SvPatchouliTopic *record)
{
    uint32_t i;
    Topic *slot = NULL;
    if (!caller || !record || record->size < sizeof *record || !word(record->command) ||
        !printable(record->usage ? record->usage : "", 0, 47) || !printable(record->summary, 1, 79) ||
        !printable(record->node ? record->node : "", 0, 39))
        return SV_INVALID;
    for (i = 0; i < topic_count; ++i)
        if (topics[i].owner == caller && !strcmp(topics[i].command, record->command)) slot = &topics[i];
    if (!slot) {
        if (topic_count == SV_PATCHOULI_TOPIC_MAX) return SV_LIMIT;
        slot = &topics[topic_count++];
    }
    slot->owner = caller;
    strcpy(slot->command, record->command);
    strcpy(slot->usage, record->usage ? record->usage : "");
    strcpy(slot->summary, record->summary);
    strcpy(slot->node, record->node ? record->node : "");
    return SV_OK;
}

static SvResult SV_CALL service_forget(SvOwner caller)
{
    uint32_t i, kept = 0;
    for (i = 0; i < topic_count; ++i)
        if (topics[i].owner != caller) topics[kept++] = topics[i];
    topic_count = kept;
    return SV_OK;
}

static uint32_t line_count(const char *text)
{
    uint32_t count = *text != 0;
    for (; *text; ++text) if (*text == '\n' && text[1]) ++count;
    return count;
}

/* Sends the lines first to first + count - 1 (counted from 0). A line longer than the message limit wraps. */
static void send_lines(Official *ctx, SvEntity player, const char *text, uint32_t first, uint32_t count)
{
    uint32_t index = 0;
    while (*text && count) {
        const char *end = strchr(text, '\n');
        size_t length = end ? (size_t)(end - text) : strlen(text);
        char line[160];
        if (index >= first) {
            if (length > sizeof line - 1) length = sizeof line - 1;
            memcpy(line, text, length); line[length] = 0;
            of_tell(ctx, player, "%s", line);
            --count;
        }
        ++index;
        text += length + (end != NULL);
    }
}

static void show(Official *ctx, SvEntity player, const char *title, const char *text, uint32_t page)
{
    uint32_t total = line_count(text), pages = (total + LINES_PER_PAGE - 1) / LINES_PER_PAGE;
    if (!player) {
        send_lines(ctx, 0, text, 0, total);
        return;
    }
    if (page < 1 || page > pages) { of_tell(ctx, player, "No page %u. This text has %u.", page, pages); return; }
    of_tell(ctx, player, "%s (page %u of %u)", title, page, pages);
    send_lines(ctx, player, text, (page - 1) * LINES_PER_PAGE, LINES_PER_PAGE);
    if (page < pages) of_tell(ctx, player, "Next: /page %u", page + 1);
}

static void pager_clear(Pager *pager)
{
    free(pager->text);
    memset(pager, 0, sizeof *pager);
}

/* Returns the slot of the player, or a free slot. NULL when every slot is in use. */
static Pager *pager_for(SvEntity player)
{
    uint32_t i;
    Pager *free_slot = NULL;
    for (i = 0; i < PAGER_MAX; ++i) {
        if (pagers[i].player == player) return &pagers[i];
        if (!pagers[i].player && !free_slot) free_slot = &pagers[i];
    }
    return free_slot;
}

/* Keeps a copy of the text for the player. A new text replaces the old one. */
static Pager *pager_store(SvEntity player, const char *title, const char *text)
{
    Pager *pager = pager_for(player);
    char *copy = pager ? _strdup(text) : NULL;
    if (!copy) return NULL;
    pager_clear(pager);
    pager->player = player; pager->text = copy;
    snprintf(pager->title, sizeof pager->title, "%s", title);
    return pager;
}

static SvResult SV_CALL service_pages(SvOwner caller, SvEntity player, const char *title, const char *text)
{
    Official ctx = api;
    SvPlayerState state = { sizeof state };
    Pager *pager;
    if (!caller || !player || !printable(title, 1, 31) || !text || !*text) return SV_INVALID;
    if (strnlen(text, SV_PATCHOULI_TEXT_MAX + 1) > SV_PATCHOULI_TEXT_MAX) return SV_LIMIT;
    if (api.read->player(caller, player, &state) != SV_OK) return SV_NOT_FOUND;
    pager = pager_store(player, title, text);
    if (!pager) return SV_LIMIT;
    ctx.owner = caller;
    pager->shown = 1;
    show(&ctx, player, pager->title, pager->text, 1);
    return SV_OK;
}

static int allowed(SvEntity actor, const Topic *topic)
{
    return !actor || !*topic->node || of_can(&api, actor, topic->node);
}

static void show_list(SvEntity actor, uint32_t page)
{
    /* One line needs at most 160 bytes, so the buffer always holds every topic. */
    char *text = malloc((size_t)topic_count * 160 + 1);
    size_t used = 0;
    uint32_t i;
    if (!text) { of_tell(&api, actor, "Help is not available now."); return; }
    text[0] = 0;
    for (i = 0; i < topic_count; ++i) {
        if (!allowed(actor, &topics[i])) continue;
        used += (size_t)sprintf(text + used, "/%s %s - %s\n", topics[i].command, topics[i].usage, topics[i].summary);
    }
    if (!used) of_tell(&api, actor, "No command is available to you.");
    else if (actor) {
        Pager *pager = pager_store(actor, "Help", text);
        if (pager) { pager->shown = page; show(&api, actor, pager->title, pager->text, page); }
        else of_tell(&api, actor, "Help is not available now.");
    } else show(&api, 0, "Help", text, 1);
    free(text);
}

static void show_topic(SvEntity actor, const char *name)
{
    uint32_t i;
    for (i = 0; i < topic_count; ++i) {
        if (strcmp(topics[i].command, name) || !allowed(actor, &topics[i])) continue;
        of_tell(&api, actor, "/%s %s", topics[i].command, topics[i].usage);
        of_tell(&api, actor, "%s", topics[i].summary);
        if (*topics[i].node) of_tell(&api, actor, "Permission: %s", topics[i].node);
        return;
    }
    of_tell(&api, actor, "No help for %s.", name);
}

static SvResult SV_CALL help_command(SvOwner owner, uint64_t actor, const char *arguments)
{
    char copy[96], first[40];
    char *rest = copy;
    int32_t page;
    (void)owner;
    snprintf(copy, sizeof copy, "%s", arguments ? arguments : "");
    if (of_token(&rest, first, sizeof first) < 0 || *rest) { of_tell(&api, actor, "Use: /help [PAGE|COMMAND]"); return SV_OK; }
    if (!*first) show_list(actor, 1);
    else if (of_number(first, 1, 1000, &page)) show_list(actor, (uint32_t)page);
    else show_topic(actor, first[0] == '/' ? first + 1 : first);
    return SV_OK;
}

static SvResult SV_CALL page_command(SvOwner owner, uint64_t actor, const char *arguments)
{
    char copy[32], first[16];
    char *rest = copy;
    int32_t page = 0;
    uint32_t i;
    (void)owner;
    if (!actor) { of_tell(&api, 0, "The console sees all lines at once."); return SV_OK; }
    snprintf(copy, sizeof copy, "%s", arguments ? arguments : "");
    if (of_token(&rest, first, sizeof first) < 0 || *rest || (*first && !of_number(first, 1, 1000, &page))) {
        of_tell(&api, actor, "Use: /page [NUMBER]"); return SV_OK;
    }
    for (i = 0; i < PAGER_MAX; ++i) {
        if (pagers[i].player != actor || !pagers[i].text) continue;
        /* With no number, show the page after the last one shown. */
        if (!page) page = (int32_t)pagers[i].shown + 1;
        if ((uint32_t)page <= (line_count(pagers[i].text) + LINES_PER_PAGE - 1) / LINES_PER_PAGE) pagers[i].shown = (uint32_t)page;
        show(&api, actor, pagers[i].title, pagers[i].text, (uint32_t)page);
        return SV_OK;
    }
    of_tell(&api, actor, "There is no text to page. Try /help.");
    return SV_OK;
}

static SvResult SV_CALL event(SvOwner owner, const SvEvent *base)
{
    uint32_t i;
    (void)owner;
    if (base->kind == SV_EVENT_LEAVE && base->size >= sizeof(SvGameEvent))
        for (i = 0; i < PAGER_MAX; ++i)
            if (pagers[i].player == ((const SvGameEvent *)base)->player) pager_clear(&pagers[i]);
    return SV_OK;
}

static SvResult SV_CALL enable(const SvHost *host, SvOwner owner)
{
    uint32_t i;
    SvResult result = of_init(&api, host, owner, OF_CAP(SV_API_GAME_READ) | OF_CAP(SV_API_GAME_WRITE) |
        OF_CAP(SV_API_PLUGINS));
    if (result != SV_OK) return result;
    memset(topics, 0, sizeof topics); topic_count = 0;
    for (i = 0; i < PAGER_MAX; ++i) pager_clear(&pagers[i]);
    service.size = sizeof service; service.version = SV_PATCHOULI_VERSION;
    service.topic = service_topic; service.forget = service_forget; service.pages = service_pages;
    result = api.plugins->provide(owner, SV_PATCHOULI_API, SV_PATCHOULI_VERSION, &service);
    /* Another plugin may already own /help. Patchouli then keeps /page and the service. */
    if (result == SV_OK) { host->command(owner, "help", SV_COMMAND_PUBLIC, help_command); }
    if (result == SV_OK) { host->command(owner, "bmhelp", SV_COMMAND_PUBLIC, help_command); }
    if (result == SV_OK) result = host->command(owner, "page", SV_COMMAND_PUBLIC, page_command);
    if (result == SV_OK) result = host->subscribe(owner, SV_EVENT_LEAVE, event);
    return result;
}

static void SV_CALL disable(SvOwner owner)
{
    uint32_t i;
    (void)owner;
    memset(topics, 0, sizeof topics); topic_count = 0;
    for (i = 0; i < PAGER_MAX; ++i) pager_clear(&pagers[i]);
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
