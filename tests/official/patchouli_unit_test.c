#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include "../../plugins/patchouli/patchouli.c"

#define CHECK(x) do { if (!(x)) { fprintf(stderr, "%d: %s\n", __LINE__, #x); exit(1); } } while (0)

static char reply[8192];
static int permitted;
static SvPermissions permissions;
static SvPlugins plugins;
static SvOwner last_owner;

static SvResult SV_CALL tell(SvOwner owner, uint32_t kind, SvEntity player, const char *text)
{
    (void)kind; (void)player;
    last_owner = owner;
    CHECK(strlen(reply) + strlen(text) + 2 < sizeof reply);
    strcat(reply, text); strcat(reply, "|"); return SV_OK;
}
static SvResult SV_CALL log_text(SvOwner owner, const char *text) { return tell(owner, 0, 0, text); }
static int SV_CALL can(SvEntity player, const char *node) { (void)player; (void)node; return permitted; }
static SvResult SV_CALL group_of(SvEntity p, SvPermissionGroup *o) { (void)p; (void)o; return SV_OK; }
static SvResult SV_CALL groups(SvPermissionGroup *o, uint32_t c, uint32_t s, uint32_t *n) { (void)o; (void)c; (void)s; (void)n; return SV_OK; }
static SvResult SV_CALL assign(SvEntity a, SvEntity p, const char *g) { (void)a; (void)p; (void)g; return SV_OK; }
static SvResult SV_CALL borrow(SvOwner owner, const char *name, uint32_t version, const void **out)
{
    (void)owner; CHECK(!strcmp(name, SV_PERMISSIONS_API) && version == 1);
    *out = &permissions; return SV_OK;
}
static SvResult SV_CALL read_player(SvOwner owner, SvEntity player, SvPlayerState *out)
{
    (void)owner; if (player == 99) return SV_NOT_FOUND;
    out->entity = player; return SV_OK;
}

static void send_event_leave(SvEntity player)
{
    SvGameEvent leave = { 0 };
    leave.base.size = sizeof leave; leave.base.kind = SV_EVENT_LEAVE; leave.player = player;
    CHECK(event(1, &leave.base) == SV_OK);
}

static SvPatchouliTopic make(const char *command, const char *node)
{
    SvPatchouliTopic topic = { sizeof topic };
    topic.command = command; topic.usage = "[PLAYER]"; topic.summary = "Does a thing"; topic.node = node;
    return topic;
}

int main(void)
{
    SvHost host = { sizeof host, SV_PLUGIN_ABI };
    SvGameRead read = { sizeof read, 1 };
    SvGameWrite write = { sizeof write, 1 };
    SvPatchouliTopic topic, bad;
    char text[600], name[24];
    unsigned i;
    SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX);
    host.log = log_text; read.player = read_player; write.message_to = tell;
    api.host = &host; api.owner = 1; api.read = &read; api.write = &write; api.plugins = &plugins;
    plugins.size = sizeof plugins; plugins.borrow = borrow;
    permissions = (SvPermissions){ sizeof permissions, 1, can, group_of, groups, assign };

    /* A topic record needs a valid size, a lowercase command, a summary and bounded text. */
    topic = make("heal", "a.heal");
    CHECK(service_topic(5, &topic) == SV_OK && topic_count == 1);
    CHECK(service_topic(0, &topic) == SV_INVALID && service_topic(5, NULL) == SV_INVALID);
    bad = topic; bad.size = sizeof bad - 1; CHECK(service_topic(5, &bad) == SV_INVALID);
    bad = topic; bad.command = "Heal"; CHECK(service_topic(5, &bad) == SV_INVALID);
    bad = topic; bad.command = ""; CHECK(service_topic(5, &bad) == SV_INVALID);
    bad = topic; bad.command = "/heal"; CHECK(service_topic(5, &bad) == SV_INVALID);
    bad = topic; bad.command = "abcdefghijklmnopqrstuvwx"; CHECK(service_topic(5, &bad) == SV_INVALID);
    bad = topic; bad.summary = ""; CHECK(service_topic(5, &bad) == SV_INVALID);
    bad = topic; bad.summary = NULL; CHECK(service_topic(5, &bad) == SV_INVALID);
    memset(text, 'x', 80); text[80] = 0; bad = topic; bad.summary = text; CHECK(service_topic(5, &bad) == SV_INVALID);
    bad = topic; bad.usage = text; CHECK(service_topic(5, &bad) == SV_INVALID);
    bad = topic; bad.node = text; CHECK(service_topic(5, &bad) == SV_INVALID);
    bad = topic; bad.summary = "bad\ttab"; CHECK(service_topic(5, &bad) == SV_INVALID);
    CHECK(topic_count == 1);

    /* The same owner and command replace the topic. Another owner gets its own topic. */
    topic.summary = "New summary"; CHECK(service_topic(5, &topic) == SV_OK && topic_count == 1);
    CHECK(!strcmp(topics[0].summary, "New summary"));
    CHECK(service_topic(6, &topic) == SV_OK && topic_count == 2);
    CHECK(service_forget(6) == SV_OK && topic_count == 1 && topics[0].owner == 5);
    for (i = 0; i < SV_PATCHOULI_TOPIC_MAX - 1; ++i) {
        snprintf(name, sizeof name, "c%u", i);
        topic.command = name; CHECK(service_topic(7, &topic) == SV_OK);
    }
    topic.command = "overflow"; CHECK(service_topic(7, &topic) == SV_LIMIT && topic_count == SV_PATCHOULI_TOPIC_MAX);
    CHECK(service_forget(7) == SV_OK && topic_count == 1);

    /* Pages show six lines, then the next page on request. */
    for (i = 0, text[0] = 0; i < 14; ++i) snprintf(text + strlen(text), sizeof text - strlen(text), "line%u\n", i);
    CHECK(service_pages(0, 1, "Title", text) == SV_INVALID && service_pages(5, 0, "Title", text) == SV_INVALID);
    CHECK(service_pages(5, 1, "", text) == SV_INVALID && service_pages(5, 1, "Title", "") == SV_INVALID);
    CHECK(service_pages(5, 1, "Title", NULL) == SV_INVALID && service_pages(5, 1, NULL, text) == SV_INVALID);
    CHECK(service_pages(5, 99, "Title", text) == SV_NOT_FOUND);
    {
        static char big[SV_PATCHOULI_TEXT_MAX + 2];
        memset(big, 'y', sizeof big - 1); big[sizeof big - 1] = 0;
        CHECK(service_pages(5, 1, "Title", big) == SV_LIMIT);
    }
    reply[0] = 0; last_owner = 0;
    CHECK(service_pages(5, 1, "Title", text) == SV_OK);
    CHECK(last_owner == 5);
    CHECK(!strcmp(reply, "Title (page 1 of 3)|line0|line1|line2|line3|line4|line5|Next: /page 2|"));
    reply[0] = 0; CHECK(page_command(1, 1, "") == SV_OK);
    CHECK(!strcmp(reply, "Title (page 2 of 3)|line6|line7|line8|line9|line10|line11|Next: /page 3|"));
    reply[0] = 0; CHECK(page_command(1, 1, "") == SV_OK);
    CHECK(!strcmp(reply, "Title (page 3 of 3)|line12|line13|"));
    reply[0] = 0; CHECK(page_command(1, 1, "") == SV_OK && strstr(reply, "No page 4. This text has 3."));
    reply[0] = 0; CHECK(page_command(1, 1, "1") == SV_OK && strstr(reply, "(page 1 of 3)"));
    reply[0] = 0; CHECK(page_command(1, 1, "x") == SV_OK && strstr(reply, "Use: /page [NUMBER]"));
    reply[0] = 0; CHECK(page_command(1, 1, "1 2") == SV_OK && strstr(reply, "Use: /page [NUMBER]"));
    reply[0] = 0; CHECK(page_command(1, 2, "") == SV_OK && strstr(reply, "There is no text to page."));
    reply[0] = 0; CHECK(page_command(1, 0, "") == SV_OK && strstr(reply, "console"));
    send_event_leave(1);
    reply[0] = 0; CHECK(page_command(1, 1, "") == SV_OK && strstr(reply, "There is no text to page."));

    /* The list hides topics whose node the player lacks. A public topic and the console see them. */
    topic = make("open", ""); CHECK(service_topic(5, &topic) == SV_OK);
    permitted = 0; reply[0] = 0;
    CHECK(help_command(1, 1, "") == SV_OK && strstr(reply, "/open [PLAYER] - Does a thing") && !strstr(reply, "/heal"));
    permitted = 1; reply[0] = 0;
    CHECK(help_command(1, 1, "") == SV_OK && strstr(reply, "/heal [PLAYER] - New summary") && strstr(reply, "(page 1 of 1)"));
    permitted = 0; reply[0] = 0;
    CHECK(help_command(1, 0, "") == SV_OK && strstr(reply, "/heal [PLAYER] - New summary") && strstr(reply, "/open"));
    reply[0] = 0; CHECK(help_command(1, 1, "heal") == SV_OK && !strstr(reply, "/heal [PLAYER]") && strstr(reply, "No help for heal."));
    permitted = 1; reply[0] = 0;
    CHECK(help_command(1, 1, "/heal") == SV_OK && !strcmp(reply, "/heal [PLAYER]|New summary|Permission: a.heal|"));
    reply[0] = 0; CHECK(help_command(1, 1, "missing") == SV_OK && strstr(reply, "No help for missing."));
    reply[0] = 0; CHECK(help_command(1, 1, "9") == SV_OK && strstr(reply, "No page 9. This text has 1."));
    reply[0] = 0; CHECK(help_command(1, 1, "a b") == SV_OK && strstr(reply, "Use: /help [PAGE|COMMAND]"));
    service_forget(5);
    reply[0] = 0; CHECK(help_command(1, 1, "") == SV_OK && strstr(reply, "No command is available to you."));

    /* Many topics make several pages, and every command shows once. */
    permitted = 1; topic = make("a", "");
    for (i = 0; i < 20; ++i) {
        snprintf(name, sizeof name, "t%u", i);
        topic.command = name; CHECK(service_topic(8, &topic) == SV_OK);
    }
    reply[0] = 0; CHECK(help_command(1, 1, "") == SV_OK && strstr(reply, "(page 1 of 4)") && strstr(reply, "Next: /page 2"));
    reply[0] = 0; CHECK(help_command(1, 1, "4") == SV_OK && strstr(reply, "/t19 ") && !strstr(reply, "Next:"));
    /* A full list is never cut: the last of 1000 topics is on the last page. */
    service_forget(8); topic = make("a", "");
    for (i = 0; i < 1000; ++i) {
        snprintf(name, sizeof name, "t%u", i);
        topic.command = name; CHECK(service_topic(8, &topic) == SV_OK);
    }
    reply[0] = 0; CHECK(help_command(1, 1, "167") == SV_OK && strstr(reply, "/t999 ") && strstr(reply, "(page 167 of 167)"));
    service_forget(8);

    /* A text of the full size fits. A full pager table refuses a new player and keeps the texts of the others. */
    send_event_leave(1);
    {
        static char full[SV_PATCHOULI_TEXT_MAX + 1];
        memset(full, 'z', sizeof full - 1); full[sizeof full - 1] = 0;
        CHECK(service_pages(5, 1000, "Full", full) == SV_OK);
    }
    for (i = 1; i < PAGER_MAX; ++i) CHECK(service_pages(5, 1000 + i, "Text", "one\ntwo\n") == SV_OK);
    reply[0] = 0; CHECK(service_pages(5, 5000, "Text", "one\n") == SV_LIMIT && !reply[0]);
    reply[0] = 0; CHECK(page_command(1, 1000, "1") == SV_OK && strstr(reply, "Full (page 1 of 1)"));
    reply[0] = 0; CHECK(service_pages(5, 1001, "Again", "three\n") == SV_OK && strstr(reply, "Again (page 1 of 1)"));
    send_event_leave(1002);
    CHECK(service_pages(5, 5000, "Text", "one\n") == SV_OK);
    for (i = 0; i < PAGER_MAX; ++i) pager_clear(&pagers[i]);
    puts("Patchouli contracts passed.");
    return 0;
}
