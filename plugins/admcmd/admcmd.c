#include "../common/permissions.h"
#include <math.h>
#include <stdio.h>
#include <string.h>

static Official api;
typedef struct {
    SvEntity player, human;
    int immortal, frozen, heavy;
    SvVector position;
} Effect;
static Effect effects[256];
static SvEntity pending_npc;
static uint64_t reset_sequence;

typedef struct { int count; char word[4][64]; } Arguments;

static Effect *effect(const SvPlayerState *player)
{
    unsigned i;
    Effect *free_slot = NULL;
    for (i = 0; i < 256; ++i) {
        if (effects[i].human == player->human) return &effects[i];
        if (!effects[i].human && !free_slot) free_slot = &effects[i];
    }
    if (free_slot) {
        memset(free_slot, 0, sizeof *free_slot);
        free_slot->player = player->entity; free_slot->human = player->human;
    }
    return free_slot;
}

static SvResult target(SvEntity actor, const char *name, SvPlayerState *out)
{
    if (name && *name) return of_target(&api, actor, name, out);
    return actor ? api.read->player(api.owner, actor, out) : SV_INVALID;
}

static SvResult body(const SvPlayerState *player, SvHumanState *out)
{
    SvResult result;
    if (!player->human) return SV_STATE;
    result = api.read->human(api.owner, player->human, out);
    return result == SV_OK && out->health < 1 ? SV_STATE : result;
}

static SvResult heal(const SvPlayerState *player)
{
    SvHumanState human = { sizeof human };
    SvResult result = player->human ? api.read->human(api.owner, player->human, &human) : SV_STATE;
    if (result == SV_OK) result = of_set(&api, "human.health", player->human, 100);
    if (result == SV_OK) result = of_set(&api, "human.bleeding", player->human, 0);
    if (result == SV_OK) result = of_set(&api, "human.stun", player->human, 0);
    return result;
}

static SvResult move(const SvPlayerState *player, SvVector position)
{
    SvHumanState human = { sizeof human };
    SvResult result = body(player, &human);
    unsigned i;
    if (result == SV_OK) result = api.write->teleport(api.owner, player->human, position);
    if (result == SV_OK)
        for (i = 0; i < 256; ++i)
            if (effects[i].human == player->human && effects[i].frozen) effects[i].position = position;
    return result;
}

static SvResult ahead(const SvPlayerState *player, float distance, SvVector *out)
{
    SvBoneState head = { sizeof head };
    SvHumanState human = { sizeof human };
    SvResult result = body(player, &human);
    if (result == SV_OK) result = api.read->bone(api.owner, player->human, 2, &head);
    if (result == SV_OK) {
        out->x = head.position.x - head.rotation[2].x * distance;
        out->y = head.position.y - head.rotation[2].y * distance;
        out->z = head.position.z - head.rotation[2].z * distance;
    }
    return result;
}

static SvResult give_item(const SvPlayerState *player, uint32_t type, int kit)
{
    SvHumanState human = { sizeof human };
    SvInventoryState inventory = { sizeof inventory };
    SvEntity made[7] = { 0 };
    SvResult result = body(player, &human);
    uint32_t hand, i, count = kit ? 7u : 1u;
    if (result != SV_OK) return result;
    for (hand = 0; hand < 2; ++hand) {
        result = api.read->inventory(api.owner, player->human, hand, &inventory);
        if (result != SV_OK) return result;
        if (!inventory.count) break;
    }
    if (hand == 2) return SV_CONFLICT;
    for (i = 0; i < count; ++i) {
        uint32_t item_type = !i ? type : i < 5 ? type + 1 : 12;
        result = api.write->item_create(api.owner, item_type, human.position, &made[i]);
        if (result != SV_OK) break;
        if (i == 1) result = api.write->item_attach(api.owner, made[0], made[i]);
        else {
            uint32_t slot = !i ? hand : i < 5 ? 3 + (i - 2) / 2 : 6;
            result = api.write->item_equip(api.owner, made[i], player->human, slot);
        }
        if (result != SV_OK) break;
    }
    if (result != SV_OK) {
        for (i = count; i > 0; --i) if (made[i - 1]) {
            api.write->item_detach(api.owner, made[i - 1]);
            api.write->item_remove(api.owner, made[i - 1]);
        }
    } else for (i = 0; i < count; ++i) api.write->disown(api.owner, made[i]);
    return result;
}

static SvResult heavy(const SvPlayerState *player)
{
    Effect *state;
    SvHumanState human = { sizeof human };
    SvBoneState bone = { sizeof bone };
    SvValue values[15];
    uint32_t i, id;
    SvResult result = body(player, &human);
    if (result != SV_OK) return result;
    state = effect(player);
    if (!state) return SV_LIMIT;
    if (state->heavy) return SV_CONFLICT;
    result = api.props->find(api.owner, "human.bone_mass", &id);
    for (i = 0; result == SV_OK && i < 15; ++i) {
        result = api.read->bone(api.owner, player->human, i, &bone);
        if (result == SV_OK) {
            if (!(bone.mass >= 0.5f && bone.mass <= 500.0f)) return SV_INVALID;
            values[i].f = bone.mass * 100000.0f;
        }
    }
    for (i = 0; result == SV_OK && i < 15; ++i)
        result = api.props->set(api.owner, id, player->human, i, values[i]);
    if (result == SV_OK) state->heavy = 1;
    return result;
}

static void list_players(SvEntity actor, const char *filter, int detailed)
{
    uint32_t slot;
    for (slot = 0; slot < 256; ++slot) {
        SvEntity entity;
        SvPlayerState player = { sizeof player };
        SvPermissionGroup group = { sizeof group };
        if (of_slot(&api, SV_ENTITY_PLAYER, slot, &entity) != SV_OK ||
            api.read->player(api.owner, entity, &player) != SV_OK) continue;
        if (filter && *filter) {
            SvPlayerState selected = { sizeof selected };
            if (of_target(&api, actor, filter, &selected) != SV_OK) return;
            if (selected.entity != entity) continue;
        }
        if (detailed && of_group(&api, entity, &group) == SV_OK)
            of_tell(&api, actor, "%u %s [%s]%s", slot, player.name, group.name, player.is_bot ? " bot" : "");
        else of_tell(&api, actor, "%u %s%s", slot, player.name, player.is_bot ? " bot" : "");
    }
}

static SvResult run(const char *name, SvEntity actor, const Arguments *args)
{
    SvPlayerState player = { sizeof player }, destination = { sizeof destination };
    SvHumanState human = { sizeof human }, place = { sizeof place };
    SvResult result;
    const char *first = args->count ? args->word[0] : "";
    const char *last = args->count > 1 ? args->word[1] : "";
    int32_t number = 0;
    SvVector position;
    if (!strcmp(name, "help")) {
        of_tell(&api, actor, "god heal kill item vehicle arm kit team heavy tp goto bring");
        of_tell(&api, actor, "npc sound give turn herobrine players where tpall freeze");
        of_tell(&api, actor, "unfreeze say round clear who id help. Optional target last.");
        of_tell(&api, actor, "arm/kit 1=AK47 2=M16 3=Magnum 4=MP5 5=Uzi 6=9mm");
        of_tell(&api, actor, "arm N [PLAYER]: weapon. kit N [PLAYER]: full loadout.");
        of_tell(&api, actor, "tp DEST [PLAYER]; bring PLAYER [DEST]; heal [PLAYER|all]");
        of_tell(&api, actor, "give money PLAYER N; give admin PLAYER; round reset");
        return SV_OK;
    }
    if (!strcmp(name, "players") || !strcmp(name, "who") || !strcmp(name, "id")) {
        list_players(actor, first, strcmp(name, "who") != 0); return SV_OK;
    }
    if (!strcmp(name, "clear")) {
        result = target(actor, first, &player);
        if (result != SV_OK) return result;
        for (number = 0; number < 20; ++number) api.write->message_to(api.owner, SV_MESSAGE_SERVER, player.entity, " ");
        return SV_OK;
    }
    if (!strcmp(name, "round")) return !strcmp(first, "reset") ? api.write->world_reset(api.owner) : SV_INVALID;
    if (!strcmp(name, "give")) {
        const SvPermissions *service;
        if (strcmp(first, "money") && strcmp(first, "admin")) return SV_INVALID;
        if (of_target(&api, actor, last, &player) != SV_OK) return SV_NOT_FOUND;
        if (player.is_bot || !player.account) return SV_INVALID;
        if (!strcmp(first, "money")) {
            if (args->count != 3 || !of_number(args->word[2], 1, 100000000, &number)) return SV_INVALID;
            return api.write->add_money(api.owner, player.account, number);
        } else {
            char group[SV_CONFIG_TEXT_MAX + 1];
            if (args->count != 2) return SV_INVALID;
            result = of_permissions(&api, &service);
            if (result != SV_OK) return result;
            result = api.config->get_text(api.owner, "admin_group", group, sizeof group);
            return result == SV_OK ? service->assign(actor, player.entity, group) : result;
        }
    }
    if (!strcmp(name, "tpall") || (!strcmp(name, "heal") && !strcmp(first, "all"))) {
        uint32_t slot, changed = 0, failed = 0;
        if (!strcmp(name, "tpall")) {
            if (target(actor, first, &destination) != SV_OK || body(&destination, &place) != SV_OK) return SV_STATE;
        }
        for (slot = 0; slot < 256; ++slot) {
            SvEntity entity;
            if (of_slot(&api, SV_ENTITY_PLAYER, slot, &entity) != SV_OK ||
                api.read->player(api.owner, entity, &player) != SV_OK || !player.human) continue;
            if (!strcmp(name, "tpall")) {
                if (entity == destination.entity) continue;
                position = place.position; position.x += 2.0f + (float)(changed % 8); position.z += (float)(changed / 8);
                result = move(&player, position);
            } else result = heal(&player);
            if (result == SV_OK) ++changed; else ++failed;
        }
        of_tell(&api, actor, "%s: %u changed, %u skipped.", name, changed, failed);
        return SV_OK;
    }
    if (!strcmp(name, "tp") || !strcmp(name, "goto") || !strcmp(name, "bring")) {
        if (!strcmp(name, "bring")) {
            if (of_target(&api, actor, first, &player) != SV_OK || target(actor, last, &destination) != SV_OK)
                return SV_NOT_FOUND;
        } else {
            if (target(actor, last, &player) != SV_OK) return SV_NOT_FOUND;
            if (!*first) {
                SvLineHit hit = { sizeof hit };
                if (body(&player, &human) != SV_OK) return SV_STATE;
                result = api.read->aim(api.owner, player.human, 2000, SV_LINE_ALL, &hit);
                if (result != SV_OK) return result;
                if (hit.kind == SV_HIT_NONE) return SV_NOT_FOUND;
                position = hit.position; position.y += 1;
                return move(&player, position);
            }
            if (of_target(&api, actor, first, &destination) != SV_OK) return SV_NOT_FOUND;
        }
        if (body(&destination, &place) != SV_OK) return SV_STATE;
        position = place.position; position.x += 2;
        return move(&player, position);
    }
    if (!strcmp(name, "item") || !strcmp(name, "arm") || !strcmp(name, "kit") || !strcmp(name, "vehicle") ||
        !strcmp(name, "team") || !strcmp(name, "sound") || !strcmp(name, "turn")) {
        int32_t low = 0, high = 0;
        if (!strcmp(name, "item")) high = 25;
        else if (!strcmp(name, "arm") || !strcmp(name, "kit")) { low = 1; high = 6; }
        else if (!strcmp(name, "vehicle")) high = 11;
        else if (!strcmp(name, "team")) high = 5;
        else if (!strcmp(name, "sound")) high = 46;
        else high = 65535;
        if (!of_number(first, low, high, &number)) {
            of_tell(&api, actor, "%s: number must be %d to %d.", name, low, high);
            return SV_INVALID;
        }
        result = target(actor, last, &player);
    } else result = target(actor, first, &player);
    if (result != SV_OK) return result;
    if (!strcmp(name, "team")) return of_set(&api, "player.team", player.entity, number);
    if (!strcmp(name, "where")) {
        result = body(&player, &human);
        if (result == SV_OK) of_tell(&api, actor, "%s: %.2f %.2f %.2f", player.name,
            human.position.x, human.position.y, human.position.z);
        return result;
    }
    if (!strcmp(name, "turn") || !strcmp(name, "herobrine")) {
        if (!strcmp(name, "turn")) {
            SvEntity account;
            SvAccountState saved = { sizeof saved };
            if (of_slot(&api, SV_ENTITY_ACCOUNT, (uint32_t)number, &account) != SV_OK) return SV_NOT_FOUND;
            result = api.read->account(api.owner, account, &saved);
            if (result != SV_OK) return result;
            memcpy(player.looks, saved.looks, sizeof saved.looks);
            result = api.write->set_looks(api.owner, player.entity, player.looks);
            if (result == SV_OK) result = api.write->set_name(api.owner, player.entity, saved.name);
            return result;
        }
        player.looks[2] = 3;
        return api.write->set_looks(api.owner, player.entity, player.looks);
    }
    if (!strcmp(name, "heal")) return heal(&player);
    if (body(&player, &human) != SV_OK) return SV_STATE;
    if (!strcmp(name, "kill")) {
        result = of_set(&api, "human.health", player.human, -5);
        if (result == SV_OK) result = of_set(&api, "human.bleeding", player.human, 1);
        return result;
    }
    if (!strcmp(name, "god") || !strcmp(name, "freeze") || !strcmp(name, "unfreeze")) {
        Effect *state = effect(&player);
        if (!state) return SV_LIMIT;
        if (!strcmp(name, "god")) {
            state->immortal = !state->immortal;
            of_tell(&api, actor, "Immortal %s: %s.", player.name, state->immortal ? "on" : "off");
            return state->immortal ? of_set(&api, "human.health", player.human, 100) : SV_OK;
        }
        if (!strcmp(name, "freeze") && human.vehicle) {
            result = api.write->vehicle_exit(api.owner, player.human, 1);
            if (result != SV_OK) return result;
            result = api.read->human(api.owner, player.human, &human);
            if (result != SV_OK) return result;
        }
        state->frozen = !strcmp(name, "freeze"); state->position = human.position;
        return SV_OK;
    }
    if (!strcmp(name, "heavy")) return heavy(&player);
    if (!strcmp(name, "item")) return give_item(&player, (uint32_t)number, 0);
    if (!strcmp(name, "arm") || !strcmp(name, "kit")) return give_item(&player, (uint32_t)(number - 1) * 2, !strcmp(name, "kit"));
    if (!strcmp(name, "npc")) {
        if (pending_npc) return SV_CONFLICT;
        pending_npc = player.entity; return SV_OK;
    }
    if (!strcmp(name, "vehicle")) {
        SvEntity object = 0;
        result = ahead(&player, 4, &position);
        if (result == SV_OK) result = api.write->object_create(api.owner, (uint32_t)number, position, human.axis, 0, &object);
        if (result == SV_OK) result = api.write->disown(api.owner, object);
        return result;
    }
    if (!strcmp(name, "sound")) {
        SvLineHit hit = { sizeof hit };
        result = api.read->aim(api.owner, player.human, 2000, SV_LINE_ALL, &hit);
        if (result != SV_OK) return result;
        if (hit.kind == SV_HIT_NONE) return SV_NOT_FOUND;
        return api.write->sound(api.owner, (uint32_t)number, hit.position, 1, 0.15f);
    }
    return SV_INVALID;
}

static const char *failure(SvResult result)
{
    switch (result) {
    case SV_INVALID: return "invalid input. Use /help";
    case SV_STATE: return "target or world is not ready";
    case SV_LIMIT: return "server resource limit";
    case SV_CONFLICT: return "occupied slot or request already active";
    case SV_DENIED: return "permission denied";
    case SV_NOT_FOUND: return "target or service not found";
    case SV_INCOMPATIBLE: return "service version is not compatible";
    case SV_IO: return "data write failed";
    default: return "host operation failed";
    }
}

static SvResult command(const char *name, int minimum, int maximum, SvEntity actor, const char *arguments)
{
    char node[64], line[256], *rest = line;
    Arguments args = { 0 };
    SvResult result;
    int parsed;
    snprintf(node, sizeof node, "admcmd.%s", name);
    if (!of_can(&api, actor, node)) { of_tell(&api, actor, "You do not have permission: %s", node); return SV_OK; }
    if (strnlen(arguments, sizeof line) >= sizeof line) { of_tell(&api, actor, "Command text is too long."); return SV_OK; }
    if (!strcmp(name, "say")) {
        result = api.write->message(api.owner, SV_MESSAGE_SERVER, arguments);
    } else {
        strcpy(line, arguments);
        while (*rest && args.count < 4) {
            parsed = of_token(&rest, args.word[args.count], sizeof args.word[0]);
            if (parsed != 1) break;
            ++args.count;
        }
        if (*rest || args.count < minimum || args.count > maximum) {
            of_tell(&api, actor, "%s: use /help for the command form.", name);
            result = SV_INVALID;
        }
        else result = run(name, actor, &args);
    }
    if (result != SV_OK) of_tell(&api, actor, "%s failed: %s.", name, failure(result));
    else if (strcmp(name, "clear")) of_tell(&api, actor, "%s done.", name);
    return SV_OK;
}

#define COMMAND(name, low, high) \
    static SvResult SV_CALL cmd_##name(SvOwner owner, uint64_t actor, const char *args) \
    { (void)owner; return command(#name, low, high, actor, args); }
COMMAND(god, 0, 1) COMMAND(heal, 0, 1) COMMAND(kill, 0, 1) COMMAND(item, 1, 2)
COMMAND(vehicle, 1, 2) COMMAND(arm, 1, 2) COMMAND(kit, 1, 2) COMMAND(team, 1, 2)
COMMAND(heavy, 0, 1) COMMAND(tp, 0, 2) COMMAND(goto, 1, 2) COMMAND(bring, 1, 2)
COMMAND(npc, 0, 1) COMMAND(sound, 1, 2) COMMAND(give, 2, 3) COMMAND(turn, 1, 2)
COMMAND(herobrine, 0, 1) COMMAND(help, 0, 0) COMMAND(players, 0, 0) COMMAND(where, 0, 1)
COMMAND(tpall, 0, 1) COMMAND(freeze, 1, 1) COMMAND(unfreeze, 1, 1) COMMAND(say, 0, 0)
COMMAND(round, 1, 1) COMMAND(clear, 0, 1) COMMAND(who, 0, 0) COMMAND(id, 1, 1)
#undef COMMAND

static SvResult SV_CALL event(SvOwner owner, const SvEvent *event)
{
    unsigned i;
    uint32_t velocity;
    SvWorldState world = { sizeof world };
    if (event->kind != SV_EVENT_TICK) return SV_OK;
    if (api.read->world(owner, &world) != SV_OK) return SV_OK;
    if (reset_sequence != UINT64_MAX && reset_sequence != world.reset_sequence) {
        memset(effects, 0, sizeof effects); pending_npc = 0;
    }
    reset_sequence = world.reset_sequence;
    if (pending_npc) {
        SvPlayerState anchor = { sizeof anchor };
        SvEntity player = 0, human = 0;
        SvVector position = { 0 };
        SvResult result = api.read->player(owner, pending_npc, &anchor);
        pending_npc = 0;
        if (result == SV_OK) result = ahead(&anchor, 3, &position);
        if (result == SV_OK) result = api.write->npc_create(owner, position, 0, "NPC", &player, &human);
        if (result == SV_OK) api.write->disown(owner, player);
        else of_tell(&api, 0, "NPC request failed: %d.", result);
    }
    if (api.props->find(owner, "human.bone_velocity", &velocity) != SV_OK) return SV_FAILED;
    for (i = 0; i < 256; ++i) if (effects[i].human) {
        SvHumanState human = { sizeof human };
        SvPlayerState player = { sizeof player };
        if (api.read->player(owner, effects[i].player, &player) != SV_OK || player.human != effects[i].human ||
            api.read->human(owner, effects[i].human, &human) != SV_OK) {
            memset(&effects[i], 0, sizeof effects[i]); continue;
        }
        if (human.health < 1) effects[i].frozen = 0;
        if (effects[i].immortal) of_set(&api, "human.health", human.entity, 100);
        if (effects[i].frozen) {
            uint32_t bone;
            SvValue zero = { 0 };
            if (human.vehicle) api.write->vehicle_exit(owner, human.entity, 1);
            api.write->teleport(owner, human.entity, effects[i].position);
            for (bone = 0; bone < 15; ++bone) api.props->set(owner, velocity, human.entity, bone, zero);
        }
    }
    return SV_OK;
}

static SvResult SV_CALL enable(const SvHost *host, SvOwner owner)
{
    static const struct { const char *name; SvCommandFn callback; } commands[] = {
        { "god", cmd_god }, { "imortal", cmd_god }, { "heal", cmd_heal }, { "kill", cmd_kill },
        { "item", cmd_item }, { "vehicle", cmd_vehicle }, { "car", cmd_vehicle }, { "arm", cmd_arm }, { "kit", cmd_kit },
        { "team", cmd_team }, { "heavy", cmd_heavy }, { "heavydude", cmd_heavy }, { "tp", cmd_tp }, { "goto", cmd_goto },
        { "bring", cmd_bring }, { "npc", cmd_npc }, { "sound", cmd_sound }, { "give", cmd_give }, { "turn", cmd_turn },
        { "herobrine", cmd_herobrine }, { "help", cmd_help }, { "bmhelp", cmd_help }, { "players", cmd_players },
        { "where", cmd_where }, { "tpall", cmd_tpall }, { "freeze", cmd_freeze }, { "unfreeze", cmd_unfreeze },
        { "say", cmd_say }, { "round", cmd_round }, { "clear", cmd_clear }, { "who", cmd_who }, { "id", cmd_id }
    };
    uint32_t i;
    SvResult result = of_init(&api, host, owner, OF_CAP(SV_API_GAME_READ) | OF_CAP(SV_API_GAME_WRITE) |
        OF_CAP(SV_API_GAME_PROPS) | OF_CAP(SV_API_CONFIG) | OF_CAP(SV_API_PLUGINS));
    if (result != SV_OK) return result;
    if (api.plugins->size < offsetof(SvPlugins, borrow) + sizeof api.plugins->borrow) return SV_INCOMPATIBLE;
    memset(effects, 0, sizeof effects); pending_npc = 0; reset_sequence = UINT64_MAX;
    for (i = 0; result == SV_OK && i < sizeof commands / sizeof commands[0]; ++i)
        result = host->command(owner, commands[i].name, SV_COMMAND_PUBLIC, commands[i].callback);
    if (result == SV_OK) result = host->subscribe(owner, SV_EVENT_TICK, event);
    return result;
}

static void SV_CALL disable(SvOwner owner)
{
    (void)owner; memset(effects, 0, sizeof effects); pending_npc = 0;
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
