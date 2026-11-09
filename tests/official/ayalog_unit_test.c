#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include "../../plugins/ayalog/ayalog.c"

#define CHECK(x) do { if (!(x)) { fprintf(stderr, "%d: %s\n", __LINE__, #x); exit(1); } } while (0)

static char logged[16384], told[16384];
static SvOwner last_owner;
static int32_t actions_on = 1, failures_on = 1;
static SvPlugins plugins;

static SvResult SV_CALL host_log(SvOwner owner, const char *text)
{
    last_owner = owner;
    CHECK(strlen(logged) + strlen(text) + 2 < sizeof logged);
    strcat(logged, text); strcat(logged, "|");
    return SV_OK;
}
static SvResult SV_CALL tell(SvOwner owner, uint32_t kind, SvEntity player, const char *text)
{
    (void)owner; (void)kind; (void)player;
    CHECK(strlen(told) + strlen(text) + 2 < sizeof told);
    strcat(told, text); strcat(told, "|");
    return SV_OK;
}
static SvResult SV_CALL borrow(SvOwner owner, const char *name, uint32_t version, const void **out)
{
    (void)owner; (void)name; (void)version;
    *out = NULL; return SV_NOT_FOUND;
}
static SvResult SV_CALL get_int(SvOwner owner, const char *key, int32_t *value)
{
    (void)owner;
    *value = !strcmp(key, "record_actions") ? actions_on : failures_on;
    return SV_OK;
}
static SvResult SV_CALL read_player(SvOwner owner, SvEntity player, SvPlayerState *out)
{
    (void)owner;
    if (player == 99) return SV_NOT_FOUND;
    out->entity = player; out->is_admin = player == 1;
    snprintf(out->name, sizeof out->name, "%s", player == 1 ? "Admin" : player == 2 ? "Alex" : player == 3 ? "TestBot" : "Tab\tName");
    out->account = player == 3 ? 0 : player;
    out->is_bot = player == 3;
    return SV_OK;
}
static SvResult SV_CALL account_number(SvOwner owner, SvEntity account, uint32_t *number)
{
    (void)owner;
    *number = account == 1 ? 12 : 1234;
    return SV_OK;
}

static void clear(void) { logged[0] = 0; told[0] = 0; }

static SvLogEntry entry(const char *action, SvEntity actor, SvEntity target, SvResult result, const char *detail)
{
    SvLogEntry made = { sizeof made };
    made.kind = result == SV_OK ? SV_LOG_ACTION : SV_LOG_FAILURE;
    made.action = action; made.actor = actor; made.target = target; made.result = result; made.detail = detail;
    return made;
}

int main(void)
{
    SvHost host = { sizeof host, SV_PLUGIN_ABI };
    SvGameRead read = { sizeof read, 1 };
    SvGameWrite write = { sizeof write, 1 };
    SvConfig config = { sizeof config, 1 };
    SvLogEntry bad, good;
    char text[200];
    unsigned i;
    SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX);
    host.log = host_log; read.player = read_player; read.account_number = account_number; write.message_to = tell;
    config.get_int = get_int;
    api.host = &host; api.owner = 1; api.read = &read; api.write = &write; api.config = &config; api.plugins = &plugins;
    api.has_account_number = 1;
    plugins.size = sizeof plugins; plugins.borrow = borrow;

    /* An empty log says so. */
    clear();
    CHECK(logs_command(1, 1, "") == SV_OK && strstr(told, "The log is empty."));

    /* The line has a fixed form. It goes to the log under the owner of the caller. */
    clear(); last_owner = 0;
    good = entry("kill", 1, 2, SV_OK, NULL);
    CHECK(record(5, &good) == SV_OK && last_owner == 5);
    CHECK(!strcmp(logged, "kill by Admin (acc 12) target Alex (acc 1234) result OK|"));
    clear();
    good = entry("heal", 0, 2, SV_CONFLICT, "item_equip failed");
    CHECK(record(5, &good) == SV_OK);
    CHECK(!strcmp(logged, "heal by console target Alex (acc 1234) result FAIL SV_CONFLICT(4) item_equip failed|"));
    clear();
    good = entry("npc", 1, 3, SV_OK, NULL);
    CHECK(record(5, &good) == SV_OK && !strcmp(logged, "npc by Admin (acc 12) target TestBot result OK|"));
    clear();
    good = entry("round", 1, 0, SV_OK, "");
    CHECK(record(5, &good) == SV_OK && !strcmp(logged, "round by Admin (acc 12) result OK|"));
    clear();
    good = entry("kill", 1, 99, SV_OK, NULL);
    CHECK(record(5, &good) == SV_OK && strstr(logged, "target a player that left"));
    clear();
    good = entry("kill", 4, 0, SV_OK, NULL);
    CHECK(record(5, &good) == SV_OK && strstr(logged, "by Tab?Name (acc 1234)"));

    /* A bad record changes nothing. */
    clear();
    good = entry("kill", 1, 2, SV_OK, NULL);
    CHECK(record(0, &good) == SV_INVALID && record(5, NULL) == SV_INVALID);
    bad = good; bad.size = sizeof bad - 1; CHECK(record(5, &bad) == SV_INVALID);
    bad = good; bad.kind = 2; CHECK(record(5, &bad) == SV_INVALID);
    bad = good; bad.action = "Kill"; CHECK(record(5, &bad) == SV_INVALID);
    bad = good; bad.action = ""; CHECK(record(5, &bad) == SV_INVALID);
    bad = good; bad.action = NULL; CHECK(record(5, &bad) == SV_INVALID);
    bad = good; bad.kind = SV_LOG_FAILURE; CHECK(record(5, &bad) == SV_INVALID);
    bad = good; bad.result = SV_STATE; CHECK(record(5, &bad) == SV_INVALID);
    bad = good; bad.detail = "tab\there"; CHECK(record(5, &bad) == SV_INVALID);
    memset(text, 'x', 80); text[80] = 0;
    bad = good; bad.detail = text; CHECK(record(5, &bad) == SV_INVALID);
    CHECK(!logged[0]);

    /* The settings turn each kind off. A turned off kind is not an error. */
    actions_on = 0; failures_on = 1; read_settings();
    clear();
    CHECK(record(5, &good) == SV_OK && !logged[0]);
    bad = entry("kill", 1, 2, SV_STATE, NULL);
    CHECK(record(5, &bad) == SV_OK && strstr(logged, "FAIL SV_STATE(2)"));
    actions_on = 1; failures_on = 0; read_settings();
    clear();
    CHECK(record(5, &bad) == SV_OK && !logged[0]);
    CHECK(record(5, &good) == SV_OK && logged[0]);
    actions_on = 1; failures_on = 1; read_settings();

    /* /logs shows the last lines, oldest first, and the ring keeps 128. */
    clear();
    for (i = 0; i < 130; ++i) {
        snprintf(text, sizeof text, "n%u", i);
        good = entry("kill", 1, 2, SV_OK, text);
        CHECK(record(5, &good) == SV_OK);
    }
    clear();
    CHECK(logs_command(1, 1, "3") == SV_OK);
    CHECK(strstr(told, "n127") && strstr(told, "n129") && !strstr(told, "n126"));
    CHECK(strstr(told, "n127") < strstr(told, "n129"));
    clear();
    CHECK(logs_command(1, 1, "100") == SV_OK && strstr(told, "n30") && !strstr(told, "n29|"));
    clear();
    CHECK(logs_command(1, 1, "") == SV_OK && strstr(told, "n120") && !strstr(told, "n119"));
    clear();
    CHECK(logs_command(1, 1, "0") == SV_OK && strstr(told, "Use: /logs [COUNT]"));
    CHECK(logs_command(1, 1, "101") == SV_OK && strstr(told, "Use: /logs [COUNT]"));
    CHECK(logs_command(1, 1, "x") == SV_OK && strstr(told, "Use: /logs [COUNT]"));
    CHECK(logs_command(1, 1, "1 2") == SV_OK && strstr(told, "Use: /logs [COUNT]"));

    /* A player without the node cannot read the log. */
    clear();
    CHECK(logs_command(1, 2, "") == SV_OK && strstr(told, "You need the permission logging.view for this."));
    puts("Logging contracts passed.");
    return 0;
}
