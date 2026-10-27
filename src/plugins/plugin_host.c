/* Call native modules only on the server thread and between lifecycle transitions. */
#include "plugin_internal.h"
#include <float.h>
#include <stdio.h>
#include <string.h>

typedef struct {
    SvOwner owner;
    char name[SV_PLUGIN_ID_SIZE];
    uint32_t permission;
    SvCommandFn callback;
} Command;

static SvModule modules[SV_PLUGIN_MAX];
static Command commands[SV_PLUGIN_COMMAND_MAX];
static uint32_t module_count;
static SvOwner current;
static DWORD server_thread;
static int started, stopping, failed;
/* 1 while a plugin runs a decision inside the simulation. Nothing that changes the game or the host may run then. */
static int in_simulation;
static HANDLE profile_lock = INVALID_HANDLE_VALUE;

static SvModule *owner_module(SvOwner owner)
{
    SvModule *module;
    if (!owner || owner > module_count || owner != current || GetCurrentThreadId() != server_thread)
        return NULL;
    module = &modules[owner - 1];
    return module->state == SV_MODULE_ACTIVE || module->enabling ? module : NULL;
}

int sv_plugins_context(SvOwner owner)
{
    return !stopping && owner_module(owner) != NULL;
}

int sv_plugins_write_context(SvOwner owner)
{
    return sv_plugins_context(owner) && !in_simulation;
}

static SvResult SV_CALL host_get_api(SvOwner owner, uint32_t capability, uint32_t version, const void **api)
{
    if (!sv_plugins_context(owner))
        return SV_STATE;
    if (!api)
        return SV_INVALID;
    *api = NULL;
    return sv_game_api(capability, version, api);
}

static SvResult SV_CALL host_log(SvOwner owner, const char *text)
{
    SvModule *module = owner_module(owner);
    if (!module)
        return SV_STATE;
    if (!text || strnlen(text, 513) > 512)
        return SV_INVALID;
    if (strpbrk(text, "\r\n"))
        return SV_INVALID;
    sv_log("[%s] %s", module->id, text);
    return SV_OK;
}

static SvResult SV_CALL host_status(SvOwner owner, SvStatus *status)
{
    if (!owner_module(owner))
        return SV_STATE;
    if (!status || status->size < sizeof *status)
        return SV_INVALID;
    sv_game_status(status);
    return SV_OK;
}

static SvResult SV_CALL host_command(SvOwner owner, const char *name, uint32_t permission, SvCommandFn callback)
{
    uint32_t i, slot = SV_PLUGIN_COMMAND_MAX;
    if (!owner_module(owner) || stopping || in_simulation)
        return SV_STATE;
    /* "plugins" is the management command. "admin" is the password login of the chat. */
    if (!sv_plugin_id(name) || !callback || permission > SV_COMMAND_LOCAL || !strcmp(name, "plugins") ||
        !strcmp(name, "admin"))
        return SV_INVALID;
    for (i = 0; i < SV_PLUGIN_COMMAND_MAX; ++i) {
        if (commands[i].owner && !strcmp(commands[i].name, name))
            return SV_CONFLICT;
        if (!commands[i].owner && slot == SV_PLUGIN_COMMAND_MAX)
            slot = i;
    }
    if (slot == SV_PLUGIN_COMMAND_MAX)
        return SV_LIMIT;
    commands[slot].owner = owner;
    commands[slot].permission = permission;
    commands[slot].callback = callback;
    strcpy(commands[slot].name, name);
    return SV_OK;
}

static SvResult SV_CALL host_subscribe(SvOwner owner, uint32_t events, SvEventFn callback)
{
    SvModule *module = owner_module(owner);
    if (!module || stopping || in_simulation)
        return SV_STATE;
    if ((events & SV_EVENT_DAMAGE) && !module->unsafe)
        return SV_DENIED;
    if (events & ~(SV_EVENT_TICK | SV_EVENT_STOP | SV_EVENT_LOST | SV_EVENT_SPAWN | SV_EVENT_DEATH | SV_EVENT_LEAVE |
                   SV_EVENT_CHAT | SV_EVENT_JOIN | SV_EVENT_PHONE_DIGIT | SV_EVENT_PHONE_CALL | SV_EVENT_DAMAGE) ||
        (events && !callback))
        return SV_INVALID;
    module->events = events;
    module->event_callback = callback;
    return SV_OK;
}

static const SvHost host = {
    sizeof(SvHost), SV_PLUGIN_ABI, host_log, host_status, host_command, host_subscribe, host_get_api
};

static int find_module(const char *id)
{
    uint32_t i;
    if (!id)
        return -1;
    for (i = 0; i < module_count; ++i)
        if (!strcmp(modules[i].id, id))
            return (int)i;
    return -1;
}

static int needs(const SvModule *module, const char *id)
{
    uint32_t i;
    for (i = 0; i < module->dependency_count; ++i)
        if (!strcmp(module->dependencies[i], id))
            return 1;
    return 0;
}

static void clear_resources(uint32_t index)
{
    uint32_t i;
    for (i = 0; i < SV_PLUGIN_COMMAND_MAX; ++i)
        if (commands[i].owner == index + 1)
            memset(&commands[i], 0, sizeof commands[i]);
    modules[index].events = 0;
    modules[index].event_callback = NULL;
}

static unsigned enter(uint32_t index)
{
    unsigned control;
    _controlfp_s(&control, 0, 0);
    current = index + 1;
    return control;
}

static void leave(unsigned control)
{
    unsigned unused;
    current = 0;
    _controlfp_s(&unused, control, _MCW_EM | _MCW_RC | _MCW_PC);
}

static void deactivate(uint32_t index, SvModuleState state, const char *reason)
{
    SvModule *module = &modules[index];
    unsigned control;
    if (module->state == SV_MODULE_ACTIVE) {
        uint32_t i;
        for (i = 0; i < module_count; ++i)
            if (modules[i].state == SV_MODULE_ACTIVE && needs(&modules[i], module->id))
                deactivate(i, SV_MODULE_FAILED, "An active dependency stopped.");
        if (module->plugin.disable) {
            control = enter(index);
            module->plugin.disable(index + 1);
            leave(control);
        }
    }
    clear_resources(index);
    module->enabling = 0;
    module->state = state;
    snprintf(module->reason, sizeof module->reason, "%s", reason);
    if (module->required && state != SV_MODULE_ACTIVE && !stopping)
        failed = 1;
    if (state == SV_MODULE_FAILED)
        sv_log("Plugin failed: %s: %s", module->id, module->reason);
}

static SvResult activate(uint32_t index, int initial)
{
    SvModule *module = &modules[index];
    SvResult result;
    uint32_t i;
    unsigned control;
    if (module->state == SV_MODULE_ACTIVE)
        return SV_OK;
    if (module->visiting) {
        strcpy(module->reason, "Plugin dependency cycle.");
        return SV_DEPENDENCY;
    }
    if (module->state == SV_MODULE_FAILED)
        return SV_FAILED;
    if (!initial && !module->live) {
        module->state = SV_MODULE_RESTART;
        strcpy(module->reason, "Activation requires a server restart.");
        return SV_RESTART;
    }
    module->visiting = 1;
    for (i = 0; i < module->dependency_count; ++i) {
        int dependency = find_module(module->dependencies[i]);
        if (dependency < 0 || !modules[dependency].wanted || activate((uint32_t)dependency, initial) != SV_OK) {
            module->visiting = 0;
            deactivate(index, SV_MODULE_FAILED, "A dependency is missing, disabled, incompatible or failed.");
            return SV_DEPENDENCY;
        }
    }
    module->visiting = 0;
    _controlfp_s(&control, 0, 0);
    result = sv_plugin_load(module);
    leave(control);
    if (result != SV_OK) {
        module->state = SV_MODULE_FAILED;
        if (module->required)
            failed = 1;
        return result;
    }
    module->enabling = 1;
    control = enter(index);
    result = module->plugin.enable(&host, index + 1);
    leave(control);
    if (result != SV_OK) {
        /* disable also runs after a partially completed enable attempt. */
        module->state = SV_MODULE_ACTIVE;
        deactivate(index, SV_MODULE_FAILED, "Enable callback reported failure. Registrations removed.");
        return result;
    }
    module->enabling = 0;
    module->state = SV_MODULE_ACTIVE;
    module->reason[0] = 0;
    sv_log("Plugin active: %s %s", module->id, module->version);
    return SV_OK;
}

SvResult sv_plugins_enable(const char *id)
{
    int index = find_module(id);
    if (!started || stopping || current || GetCurrentThreadId() != server_thread)
        return SV_STATE;
    if (index < 0)
        return SV_NOT_FOUND;
    if (!modules[index].metadata_valid)
        return SV_INCOMPATIBLE;
    if (!sv_plugin_save_state(id, 1))
        return SV_IO;
    modules[index].wanted = 1;
    return activate((uint32_t)index, 0);
}

SvResult sv_plugins_disable(const char *id)
{
    int index = find_module(id);
    uint32_t i;
    if (!started || stopping || current || GetCurrentThreadId() != server_thread)
        return SV_STATE;
    if (index < 0)
        return SV_NOT_FOUND;
    if (modules[index].required)
        return SV_DENIED;
    for (i = 0; i < module_count; ++i)
        if (modules[i].state == SV_MODULE_ACTIVE && needs(&modules[i], id))
            return SV_DEPENDENCY;
    if (modules[index].state == SV_MODULE_ACTIVE && !modules[index].live) {
        if (!sv_plugin_save_state(id, 0))
            return SV_IO;
        modules[index].wanted = 0;
        strcpy(modules[index].reason, "Disable saved. This module remains active until server restart.");
        return SV_RESTART;
    }
    if (!sv_plugin_save_state(id, 0))
        return SV_IO;
    deactivate((uint32_t)index, SV_MODULE_DISABLED, "Disabled by the operator.");
    modules[index].wanted = 0;
    return SV_OK;
}

void sv_plugins_list(void)
{
    static const char *states[] = { "disabled", "active", "failed", "restart" };
    uint32_t i;
    for (i = 0; i < module_count; ++i) {
        SvModule *module = &modules[i];
        sv_log("PLUGIN\t%s\t%s\t%s\t%u\t%s\t%s", module->id, module->version,
            module->state == SV_MODULE_ACTIVE && !module->wanted ? "restart" : states[module->state],
            module->required, module->hash, module->reason);
        if (module->unsafe && (module->events & SV_EVENT_DAMAGE))
            sv_log("Plugin %s takes damage decisions inside the simulation (unsafe_events). Its owner takes the risk.", module->id);
        if (module->calls)
            sv_log("Plugin metrics: %s, %llu callbacks, %llu us total.", module->id,
                   (unsigned long long)module->calls, (unsigned long long)module->elapsed_us);
    }
    sv_log("Plugins: %u configured.", module_count);
}

int sv_plugins_start(void)
{
    uint32_t i;
    if (started)
        return 0;
    server_thread = GetCurrentThreadId();
    memset(modules, 0, sizeof modules);
    memset(commands, 0, sizeof commands);
    failed = stopping = 0;
    sv_events_clear();
    started = 1;
    if (!sv_game_start()) {
        failed = 1;
        module_count = 0;
        sv_log("Cannot allocate game references. Normal startup refused.");
        return 0;
    }
    profile_lock = sv_plugin_lock();
    if (profile_lock == INVALID_HANDLE_VALUE) {
        failed = 1;
        module_count = 0;
        sv_log("Cannot lock plugin state. Another host or installer may be using this server folder.");
        return 0;
    }
    if (!sv_plugin_profile(modules, &module_count)) {
        failed = 1;
        sv_log("Plugin profile is invalid. Normal startup refused.");
        return 0;
    }
    for (i = 0; i < module_count; ++i) {
        if (modules[i].wanted)
            (void)activate(i, 1);
        if (modules[i].required && modules[i].state != SV_MODULE_ACTIVE)
            failed = 1;
    }
    sv_plugins_list();
    return !failed;
}

/* Gives one event to every active plugin that asked for its kind. */
static void deliver(const SvEvent *event)
{
    uint32_t i;
    for (i = 0; i < module_count; ++i) {
        SvModule *module = &modules[i];
        SvResult result;
        LARGE_INTEGER before, after, frequency;
        unsigned control;
        if (module->state != SV_MODULE_ACTIVE || !(module->events & event->kind))
            continue;
        QueryPerformanceCounter(&before);
        control = enter(i);
        result = module->event_callback(i + 1, event);
        leave(control);
        QueryPerformanceCounter(&after);
        QueryPerformanceFrequency(&frequency);
        ++module->calls;
        module->elapsed_us += (uint64_t)((after.QuadPart - before.QuadPart) * 1000000.0 / frequency.QuadPart);
        if (result != SV_OK)
            deactivate(i, SV_MODULE_FAILED, "Event callback reported failure.");
    }
}

static void dispatch(uint32_t kind)
{
    SvStatus status = { sizeof status };
    SvEvent event = { sizeof event, kind, 0 };
    sv_game_status(&status);
    event.tick = status.tick;
    deliver(&event);
}

SvResult sv_plugins_decide(const SvEvent *event)
{
    uint32_t i;
    if (!started || stopping || failed || current || GetCurrentThreadId() != server_thread || !event)
        return SV_OK;
    for (i = 0; i < module_count; ++i) {
        SvModule *module = &modules[i];
        SvResult result;
        LARGE_INTEGER before, after, frequency;
        unsigned control;
        if (module->state != SV_MODULE_ACTIVE || !(module->events & event->kind))
            continue;
        QueryPerformanceCounter(&before);
        control = enter(i);
        result = module->event_callback(i + 1, event);
        leave(control);
        QueryPerformanceCounter(&after);
        QueryPerformanceFrequency(&frequency);
        ++module->calls;
        module->elapsed_us += (uint64_t)((after.QuadPart - before.QuadPart) * 1000000.0 / frequency.QuadPart);
        if (result == SV_DENIED)
            return SV_DENIED;
        if (result != SV_OK)
            deactivate(i, SV_MODULE_FAILED, "Event callback reported failure.");
    }
    return SV_OK;
}

/* The limits of a decision inside the simulation. A decision that takes longer than the first limit counts as slow. More
 * than the second number of slow decisions in the window disables the plugin at the next safe point. */
enum { DECISION_SLOW_US = 200, DECISION_SLOW_LIMIT = 10, DECISION_WINDOW_MS = 60000 };

/* The host does not run the disable callback of a plugin in the middle of a calculation. It marks the plugin, and the next
 * tick disables it. A marked plugin gets no more decisions. */
static void note_slow(SvModule *module)
{
    ULONGLONG now = GetTickCount64();
    if (!module->slow_count || now - module->slow_start_ms > DECISION_WINDOW_MS) {
        module->slow_count = 0;
        module->slow_start_ms = now;
    }
    if (++module->slow_count > DECISION_SLOW_LIMIT && !module->doom)
        module->doom = "Too many slow damage decisions.";
}

int sv_plugins_decide_damage(SvDamageEvent *event)
{
    uint32_t i;
    int amount;
    if (!event)
        return 0;
    amount = event->amount;
    if (!started || stopping || failed || current || in_simulation || GetCurrentThreadId() != server_thread)
        return amount;
    for (i = 0; i < module_count; ++i) {
        SvModule *module = &modules[i];
        SvResult result;
        LARGE_INTEGER before, after, frequency;
        unsigned control;
        uint64_t microseconds;
        if (module->state != SV_MODULE_ACTIVE || module->doom || !module->unsafe || !(module->events & SV_EVENT_DAMAGE))
            continue;
        event->amount = amount;
        event->new_amount = amount;
        QueryPerformanceCounter(&before);
        control = enter(i);
        in_simulation = 1;
        result = module->event_callback(i + 1, &event->base);
        in_simulation = 0;
        leave(control);
        QueryPerformanceCounter(&after);
        QueryPerformanceFrequency(&frequency);
        microseconds = (uint64_t)((after.QuadPart - before.QuadPart) * 1000000.0 / frequency.QuadPart);
        ++module->calls;
        module->elapsed_us += microseconds;
        if (microseconds > DECISION_SLOW_US)
            note_slow(module);
        if (result == SV_DENIED) {
            event->amount = event->new_amount = 0;
            return 0;
        }
        if (result != SV_OK) {
            module->doom = "Event callback reported failure.";
            continue;
        }
        if (event->new_amount >= 0 && event->new_amount <= SV_DAMAGE_MAX)
            amount = event->new_amount;
    }
    event->amount = event->new_amount = amount;
    return amount;
}

static void reap(void)
{
    uint32_t i;
    for (i = 0; i < module_count; ++i) {
        const char *reason = modules[i].doom;
        if (!reason)
            continue;
        modules[i].doom = NULL;
        if (modules[i].state == SV_MODULE_ACTIVE)
            deactivate(i, SV_MODULE_FAILED, reason);
    }
}

/* The records that the core posted since the last tick. A record that a callback causes is for the next tick. */
static void deliver_queue(void)
{
    SvGameEvent record;
    uint32_t count = sv_events_pending();
    while (count-- && sv_events_next(&record))
        deliver(&record.base);
}

int sv_plugins_wants(uint32_t kind)
{
    uint32_t i;
    if (!started || stopping || GetCurrentThreadId() != server_thread)
        return 0;
    for (i = 0; i < module_count; ++i)
        if (modules[i].state == SV_MODULE_ACTIVE && (modules[i].events & kind))
            return 1;
    return 0;
}

void sv_plugins_tick(void)
{
    if (started && !stopping && !current && !failed) {
        reap();
        deliver_queue();
        dispatch(SV_EVENT_TICK);
    }
}

int sv_plugins_failed(void)
{
    return failed;
}

void sv_plugins_stop(void)
{
    uint32_t i;
    if (!started)
        return;
    stopping = 1;
    sv_events_clear();
    dispatch(SV_EVENT_STOP);
    for (i = module_count; i > 0; --i)
        deactivate(i - 1, SV_MODULE_DISABLED, "Server stopped.");
    for (i = module_count; i > 0; --i)
        sv_plugin_unload(&modules[i - 1]);
    sv_game_stop();
    if (profile_lock != INVALID_HANDLE_VALUE)
        CloseHandle(profile_lock);
    profile_lock = INVALID_HANDLE_VALUE;
    started = 0;
}

int sv_plugins_command(uint64_t actor, const char *text)
{
    char name[SV_PLUGIN_ID_SIZE], arguments[256];
    const char *split;
    size_t length;
    uint32_t i;
    if (!started || stopping || current || !text || strlen(text) > 300)
        return 0;
    if (*text == '/')
        ++text;
    split = strchr(text, ' ');
    length = split ? (size_t)(split - text) : strlen(text);
    if (!length || length >= sizeof name || (split && strlen(split + 1) >= sizeof arguments))
        return 0;
    memcpy(name, text, length);
    name[length] = 0;
    strcpy(arguments, split ? split + 1 : "");
    if (!strcmp(name, "plugins")) {
        char *id = strchr(arguments, ' ');
        SvResult result = SV_INVALID;
        if (actor) {
            sv_log("Plugin management requires the local operator.");
            return 1;
        }
        if (!strcmp(arguments, "list")) {
            sv_plugins_list();
            return 1;
        }
        if (id) {
            *id++ = 0;
            if (!strcmp(arguments, "enable"))
                result = sv_plugins_enable(id);
            else if (!strcmp(arguments, "disable"))
                result = sv_plugins_disable(id);
        }
        {
            static const char *results[] = { "OK", "invalid request", "wrong state", "resource limit",
                "conflict", "denied", "not found", "incompatible API", "I/O failure",
                "dependency blocked", "restart required", "callback failure" };
            sv_log("Plugin management result: %s (%d)",
                   result >= SV_OK && result <= SV_FAILED ? results[result] : "unknown failure", result);
        }
        sv_plugins_list();
        return 1;
    }
    for (i = 0; i < SV_PLUGIN_COMMAND_MAX; ++i) {
        Command command = commands[i];
        SvResult result;
        unsigned control;
        if (!command.owner || strcmp(command.name, name))
            continue;
        if (actor && (command.permission == SV_COMMAND_LOCAL ||
            (command.permission == SV_COMMAND_ADMIN && !sv_game_actor_admin(actor)))) {
            sv_log("Plugin command permission denied: %s", name);
            return 1;
        }
        control = enter(command.owner - 1);
        result = command.callback(command.owner, actor, arguments);
        leave(control);
        if (result != SV_OK)
            sv_log("Plugin command result: %s %d", name, result);
        return 1;
    }
    return 0;
}

int sv_plugins_cli(int argc, char **argv)
{
    SvModule module;
    if (argc == 2 && !strcmp(argv[1], "--plugins-list")) {
        uint32_t i;
        sv_log("SubVulpes-plugin-cli-v1");
        if (!sv_plugin_profile(modules, &module_count)) {
            sv_log("Plugin profile is invalid.");
            return 2;
        }
        for (i = 0; i < module_count; ++i) {
            SvModule *item = &modules[i];
            const char *state = "failed";
            if (item->state != SV_MODULE_FAILED && sv_plugin_inspect(item->id, &module)) {
                strcpy(item->hash, module.hash);
                state = !item->wanted ? "disabled" : sv_plugin_approved(&module) ? "ready" : "approval";
                strcpy(item->reason, "Changes apply at the next server start.");
            }
            sv_log("PLUGIN\t%s\t%s\t%s\t%u\t%s\t%s", item->id, item->version, state,
                item->required, item->hash, item->reason);
        }
        return 0;
    }
    if (argc == 3 && (!strcmp(argv[1], "--plugin-enable") || !strcmp(argv[1], "--plugin-disable"))) {
        int index, enabled = !strcmp(argv[1], "--plugin-enable");
        uint32_t i;
        HANDLE lock = sv_plugin_lock();
        if (lock == INVALID_HANDLE_VALUE) {
            sv_log("Stop the server before changing state with an offline command.");
            return 2;
        }
        if (!sv_plugin_profile(modules, &module_count) || (index = find_module(argv[2])) < 0) {
            CloseHandle(lock);
            sv_log("Plugin is not in a valid installed profile.");
            return 2;
        }
        if (!enabled && modules[index].required) {
            CloseHandle(lock);
            sv_log("A required plugin cannot be disabled.");
            return 2;
        }
        if (enabled && !modules[index].metadata_valid) {
            CloseHandle(lock);
            sv_log("Cannot enable invalid plugin metadata. Install a compatible package first.");
            return 2;
        }
        for (i = 0; i < module_count; ++i) {
            if (!enabled && modules[i].wanted && needs(&modules[i], argv[2])) {
                CloseHandle(lock);
                sv_log("Disable dependent plugins first.");
                return 2;
            }
        }
        if (!sv_plugin_save_state(argv[2], enabled)) {
            CloseHandle(lock);
            sv_log("Cannot save the private plugin state.");
            return 2;
        }
        CloseHandle(lock);
        sv_log("Plugin state saved. It applies at the next server start.");
        return 0;
    }
    if (argc == 2 && !strcmp(argv[1], "--plugins-check")) {
        int ok = sv_plugins_start();
        uint32_t i;
        for (i = 0; i < module_count; ++i)
            if (modules[i].wanted && modules[i].state != SV_MODULE_ACTIVE)
                ok = 0;
        sv_plugins_stop();
        return ok ? 0 : 2;
    }
    if (argc == 3 && !strcmp(argv[1], "--plugin-hash")) {
        if (!sv_plugin_inspect(argv[2], &module)) {
            sv_log("Cannot inspect the plugin metadata or x86 DLL.");
            return 2;
        }
        sv_log("Native code can read process data, corrupt memory, block or crash the server.");
        if (module.unsafe)
            sv_log("This plugin declares unsafe_events: it takes damage decisions inside the simulation, and the owner takes the risk.");
        sv_log("SHA256\t%s\t%s", module.id, module.hash);
        return 0;
    }
    if (argc == 4 && !strcmp(argv[1], "--plugin-approve")) {
        sv_log("Native approval accepts full process access and crash risk. A hash proves identity only.");
        if (sv_plugin_inspect(argv[2], &module) && module.unsafe)
            sv_log("This plugin declares unsafe_events: it takes damage decisions inside the simulation. A slow or wrong answer slows or breaks the game for every player, and the owner takes the risk.");
        if (!sv_plugin_approve(argv[2], argv[3])) {
            sv_log("Approval failed. Check the exact current SHA-256 and the private storage directory.");
            return 2;
        }
        sv_log("Native content approved: %s", argv[2]);
        return 0;
    }
    return -1;
}
