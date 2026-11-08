#include "sv_server.h"
#include "sv_game_write.h"
#include "sv_plugins_api.h"
#include <stdio.h>
#include <string.h>

static const SvHost *host;
static const SvServerControl *control;
static const SvConsole *console;
static const SvGameWrite *write;

typedef struct {
    uint32_t size, version;
    const SvServerControl *control;
    const SvConsole *console;
} InfraTables;
static InfraTables tables;

#define CHECK(test) do { if (!(test)) { \
    char failure[160]; \
    snprintf(failure, sizeof failure, "Infrastructure fixture failed at line %d: %s", __LINE__, #test); \
    host->log(owner, failure); return SV_FAILED; \
} } while (0)

static SvResult SV_CALL reply_command(SvOwner owner, uint64_t actor, const char *arguments)
{
    char text[512], output[32];
    uint32_t length;
    int i;
    CHECK(actor == 0);
    if (!strcmp(arguments, "basic")) {
        CHECK(host->log(owner, "alpha") == SV_OK);
        CHECK(write->message_to(owner, SV_MESSAGE_SERVER, 0, "beta") == SV_OK);
    } else if (!strncmp(arguments, "arguments ", 10)) {
        CHECK(host->log(owner, arguments + 10) == SV_OK);
    } else if (!strcmp(arguments, "failure")) {
        CHECK(host->log(owner, "failure") == SV_OK);
        return SV_FAILED;
    } else if (!strcmp(arguments, "recursive")) {
        CHECK(console->execute(owner, "infrareply basic", output, sizeof output, &length) == SV_STATE);
        CHECK(host->log(owner, "nested refused") == SV_OK);
    } else if (!strcmp(arguments, "large")) {
        memset(text, 'x', sizeof text - 1);
        text[sizeof text - 1] = 0;
        for (i = 0; i < 10; ++i)
            CHECK(host->log(owner, text) == SV_OK);
    } else {
        CHECK(!strcmp(arguments, "empty"));
    }
    return SV_OK;
}

static SvResult console_checks(SvOwner owner)
{
    struct { unsigned char before; char text[8]; unsigned char after; } bounded = {0x31, {0}, 0x72};
    char output[SV_CONSOLE_TEXT_MAX + 1], line[SV_CONSOLE_LINE_MAX + 2];
    uint32_t length = 99;
    CHECK(console->execute(owner, "infrareply basic", output, sizeof output, &length) == SV_OK);
    CHECK(length == 11 && !strcmp(output, "alpha\nbeta\n"));
    CHECK(console->execute(owner, "runtimeother reply", output, sizeof output, &length) == SV_OK);
    CHECK(length == 12 && !strcmp(output, "other owner\n"));
    CHECK(console->execute(owner, "/infrareply arguments a b", output, sizeof output, &length) == SV_OK);
    CHECK(length == 4 && !strcmp(output, "a b\n"));
    CHECK(console->execute(owner, "infrareply failure", output, sizeof output, &length) == SV_FAILED);
    CHECK(length == 8 && !strcmp(output, "failure\n"));
    CHECK(console->execute(owner, "infrareply recursive", output, sizeof output, &length) == SV_OK);
    CHECK(!strcmp(output, "nested refused\n"));
    CHECK(console->execute(owner, "infrareply basic", bounded.text, sizeof bounded.text, &length) == SV_LIMIT);
    CHECK(length == 7 && !strcmp(bounded.text, "alpha\nb"));
    CHECK(bounded.before == 0x31 && bounded.after == 0x72);
    CHECK(console->execute(owner, "infrareply large", output, sizeof output, &length) == SV_LIMIT);
    CHECK(length == SV_CONSOLE_TEXT_MAX && output[length] == 0);
    CHECK(console->execute(owner, "infrareply empty", output, 1, &length) == SV_OK && length == 0 && !output[0]);
    CHECK(console->execute(owner, "infrareply basic", output, 1, &length) == SV_LIMIT && length == 0 && !output[0]);
    CHECK(console->execute(owner, "admin password", output, sizeof output, &length) == SV_DENIED);
    CHECK(console->execute(owner, "plugins disable test.read", output, sizeof output, &length) == SV_DENIED);
    CHECK(console->execute(owner, "not_registered", output, sizeof output, &length) == SV_DENIED);
    CHECK(console->execute(owner, NULL, output, sizeof output, &length) == SV_INVALID);
    CHECK(console->execute(owner, "", output, sizeof output, &length) == SV_INVALID);
    CHECK(console->execute(owner, "infrareply\n", output, sizeof output, &length) == SV_INVALID);
    CHECK(console->execute(owner, "infrareply\r", output, sizeof output, &length) == SV_INVALID);
    CHECK(console->execute(owner, "infrareply empty", NULL, sizeof output, &length) == SV_INVALID);
    CHECK(console->execute(owner, "infrareply empty", output, 0, &length) == SV_INVALID);
    CHECK(console->execute(owner, "infrareply empty", output, sizeof output + 1, &length) == SV_INVALID);
    CHECK(console->execute(owner, "infrareply empty", output, sizeof output, NULL) == SV_INVALID);
    memset(line, 'x', sizeof line - 1);
    line[sizeof line - 1] = 0;
    CHECK(console->execute(owner, line, output, sizeof output, &length) == SV_INVALID);
    line[SV_PLUGIN_ID_SIZE] = 0;
    CHECK(console->execute(owner, line, output, sizeof output, &length) == SV_INVALID);
    strcpy(line, "infrareply ");
    memset(line + 11, 'x', 256);
    line[267] = 0;
    CHECK(console->execute(owner, line, output, sizeof output, &length) == SV_INVALID);
    CHECK(host->log(owner, "INFRA_OWNER_RESTORED") == SV_OK);
    return SV_OK;
}

static SvResult SV_CALL check_command(SvOwner owner, uint64_t actor, const char *arguments)
{
    (void)actor;
    if (!strcmp(arguments, "save")) {
        CHECK(control->request(owner, SV_SERVER_SAVE) == SV_OK);
        CHECK(control->request(owner, SV_SERVER_SAVE) == SV_STATE);
        CHECK(control->request(owner, SV_SERVER_STOP) == SV_STATE);
    } else if (!strcmp(arguments, "stop")) {
        CHECK(control->request(owner, SV_SERVER_STOP) == SV_OK);
        CHECK(control->request(owner, SV_SERVER_SAVE) == SV_STATE);
    } else if (!strcmp(arguments, "restart")) {
        CHECK(control->request(owner, SV_SERVER_RESTART) == SV_OK);
        CHECK(control->request(owner, SV_SERVER_STOP) == SV_STATE);
    } else if (!strcmp(arguments, "invalid")) {
        CHECK(control->request(owner, 0) == SV_INVALID);
        CHECK(control->request(owner, UINT32_MAX) == SV_INVALID);
    } else if (!strcmp(arguments, "console")) {
        return console_checks(owner);
    } else {
        return SV_INVALID;
    }
    return SV_OK;
}

SvResult infra_plugin_simulation(SvOwner owner)
{
    char output[8];
    uint32_t length;
    return control->request(owner, SV_SERVER_SAVE) == SV_STATE &&
        console->execute(owner, "infrareply empty", output, sizeof output, &length) == SV_STATE ? SV_OK : SV_FAILED;
}

SvResult infra_plugin_enable(const SvHost *api, SvOwner owner)
{
    const void *table = NULL;
    const SvPlugins *services;
    host = api;
    CHECK(host->get_api(owner, SV_API_SERVER_CONTROL, SV_SERVER_CONTROL_VERSION, &table) == SV_OK);
    control = table;
    CHECK(control && control->size >= sizeof *control && control->version == SV_SERVER_CONTROL_VERSION);
    CHECK(host->get_api(owner, SV_API_CONSOLE, SV_CONSOLE_VERSION, &table) == SV_OK);
    console = table;
    CHECK(console && console->size >= sizeof *console && console->version == SV_CONSOLE_VERSION);
    CHECK(host->get_api(owner, SV_API_GAME_WRITE, SV_GAME_WRITE_VERSION, &table) == SV_OK);
    write = table;
    CHECK(host->get_api(owner, SV_API_PLUGINS, SV_PLUGINS_VERSION, &table) == SV_OK);
    services = table;
    tables = (InfraTables){sizeof tables, 1, control, console};
    CHECK(services->provide(owner, "test.read.infra", 1, &tables) == SV_OK);
    CHECK(host->command(owner, "infracheck", SV_COMMAND_LOCAL, check_command) == SV_OK);
    CHECK(host->command(owner, "infrareply", SV_COMMAND_LOCAL, reply_command) == SV_OK);
    return SV_OK;
}
