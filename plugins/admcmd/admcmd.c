#include "../common/permissions.h"
#include "../patchouli/sv_patchouli.h"
#include "../ayalog/sv_ayalog.h"
#include "names.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
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

typedef struct { int count; char word[6][64]; } Arguments;

/* What the last command did, for its answer and for the log. */
typedef struct {
    char target[32], detail[96], message[200], partial_detail[48];
    SvEntity target_entity;
    int target_is_bot;
    SvResult partial;   /* a part of a command that failed while the command worked */
} Report;
static Report report;

static void note_target(const SvPlayerState *player)
{
    snprintf(report.target, sizeof report.target, "%s", player->name);
    report.target_entity = player->entity;
    report.target_is_bot = player->is_bot;
}

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
    SvResult result = name && *name ? of_target(&api, actor, name, out) :
        actor ? api.read->player(api.owner, actor, out) : SV_INVALID;
    if (result == SV_OK) note_target(out);
    return result;
}

/* A number or a name for a command. Returns 1 when out holds the value. Otherwise it tells the actor and returns 0. */
static int resolve(SvEntity actor, const NameSet *set, const char *text, int32_t *out)
{
    char matches[160];
    const char *digits = text + (*text == '-');
    int numeric = *digits != 0;
    size_t i;
    if (of_number(text, set->low, set->high, out)) return 1;
    /* A number outside the range is an error. It must not match a part of a name, such as 7 in ak47. */
    for (i = 0; digits[i]; ++i) if (digits[i] < '0' || digits[i] > '9') numeric = 0;
    switch (numeric ? 0 : name_lookup(set, text, out, matches, sizeof matches)) {
    case 1:
        return 1;
    case -1:
        of_tell(&api, actor, "\"%.30s\" matches several: %s. Be more exact.", text, matches);
        break;
    default:
        if (set->count)
            of_tell(&api, actor, "\"%.30s\" is not a valid %s. Use a number from %d to %d, or a name. See /names %s.",
                    text, set->kind, set->low, set->high, set->kind);
        else
            of_tell(&api, actor, "\"%.30s\" is not a valid %s. Use a number from %d to %d.", text, set->kind, set->low, set->high);
    }
    return 0;
}

/* The main name of a value, or the number, for the answer. */
static void detail_of(const NameSet *set, int32_t value)
{
    const char *name = name_of(set, value);
    if (name) snprintf(report.detail, sizeof report.detail, "%s", name);
    else snprintf(report.detail, sizeof report.detail, "%d", value);
}

static void usage_line(const char *name, SvEntity actor);

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
    if (result == SV_OK && human.vehicle) result = api.write->vehicle_exit(api.owner, player->human, 1);
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

static const char *const weapon_name[6] = { "AK-47", "M-16", "Magnum", "MP5", "Uzi", "9mm" };

static void discard(SvEntity item)
{
    if (!item) return;
    api.write->item_detach(api.owner, item);
    api.write->item_remove(api.owner, item);
}

/* Slots to try, in order. Slots 0 and 1 are the hands. The rest are pockets. */
static const uint32_t anywhere[] = { 0, 1, 3, 4, 5, 6, 2 };
static const uint32_t pockets[] = { 3, 4, 5, 6, 2 };

/* Puts the item in the first slot that takes it. A hand must be empty. A full pocket refuses the item and keeps
 * what it holds. Returns SV_CONFLICT when no slot takes the item. */
static SvResult place_item(const SvPlayerState *player, SvEntity item, const uint32_t *order, uint32_t count,
                           uint32_t *slot)
{
    SvInventoryState inventory = { sizeof inventory };
    uint32_t i;
    for (i = 0; i < count; ++i) {
        if (order[i] < 2) {
            SvResult read = api.read->inventory(api.owner, player->human, order[i], &inventory);
            if (read != SV_OK) return read;
            if (inventory.count) continue;
        }
        if (api.write->item_equip(api.owner, item, player->human, order[i]) == SV_OK) {
            *slot = order[i];
            return SV_OK;
        }
    }
    return SV_CONFLICT;
}

/* One item in a free hand, or else in a pocket. It never replaces an item that the target holds. */
static SvResult give_item(const SvPlayerState *player, uint32_t type)
{
    SvHumanState human = { sizeof human };
    SvEntity item = 0;
    uint32_t slot = 0;
    SvResult result = body(player, &human);
    if (result != SV_OK) return result;
    result = api.write->item_create(api.owner, type, human.position, &item);
    if (result == SV_OK) result = place_item(player, item, anywhere, sizeof anywhere / sizeof anywhere[0], &slot);
    if (result == SV_OK) api.write->disown(api.owner, item);
    else discard(item);
    return result;
}

/* A weapon in a free hand or a pocket, one loaded magazine and up to three spare magazines in the pockets.
 * A magazine that does not fit is skipped and the rest stay. A weapon that fits nowhere removes everything made here. */
static SvResult give_weapon(SvEntity actor, const SvPlayerState *player, uint32_t number)
{
    SvHumanState human = { sizeof human };
    SvEntity weapon = 0, magazine;
    uint32_t type = (number - 1) * 2, slot = 0, i, loaded = 0, spare = 0;
    SvResult result = body(player, &human);
    if (result != SV_OK) return result;
    result = api.write->item_create(api.owner, type, human.position, &weapon);
    if (result == SV_OK) result = place_item(player, weapon, anywhere, sizeof anywhere / sizeof anywhere[0], &slot);
    if (result != SV_OK) { discard(weapon); return result; }
    for (i = 0; i < 4; ++i) {
        uint32_t magazine_slot;
        magazine = 0;
        result = api.write->item_create(api.owner, type + 1, human.position, &magazine);
        if (result == SV_OK)
            result = !i ? api.write->item_attach(api.owner, weapon, magazine)
                        : place_item(player, magazine, pockets, sizeof pockets / sizeof pockets[0], &magazine_slot);
        if (result != SV_OK) { discard(magazine); continue; }
        api.write->disown(api.owner, magazine);
        if (!i) loaded = 1; else ++spare;
    }
    api.write->disown(api.owner, weapon);
    snprintf(report.detail, sizeof report.detail, "%s", weapon_name[number - 1]);
    of_tell(&api, actor, "Gave %s to %s%s with %u loaded and %u spare magazines.", weapon_name[number - 1],
            player->name, slot < 2 ? "" : " in the inventory", loaded, spare);
    if (loaded + spare < 4) of_tell(&api, actor, "Skipped %u magazines: no room.", 4 - loaded - spare);
    return SV_OK;
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

/* One line for a player: "acc N  id:S  Name [group]". A bot has no account and no group. */
static void describe_player(SvEntity actor, uint32_t slot, const SvPlayerState *player, int detailed)
{
    SvPermissionGroup group = { sizeof group };
    char who[96] = "";
    int32_t account = of_account(&api, player);
    if (account >= 0) snprintf(who, sizeof who, "acc %d  ", account);
    if (player->is_bot) of_tell(&api, actor, "%sid:%u  %s [bot]", who, slot, player->name);
    else if (detailed && of_group(&api, player->entity, &group) == SV_OK)
        of_tell(&api, actor, "%sid:%u  %s [%s]", who, slot, player->name, group.name);
    else of_tell(&api, actor, "%sid:%u  %s", who, slot, player->name);
}

/* A filter selects one player. With no filter the list holds real players. "all" adds the bots. */
static void list_players(SvEntity actor, const char *filter, int detailed)
{
    uint32_t slot, shown = 0, hidden = 0;
    int all = filter && !strcmp(filter, "all");
    for (slot = 0; slot < 256; ++slot) {
        SvEntity entity;
        SvPlayerState player = { sizeof player };
        if (of_slot(&api, SV_ENTITY_PLAYER, slot, &entity) != SV_OK ||
            api.read->player(api.owner, entity, &player) != SV_OK) continue;
        if (filter && *filter && !all) {
            SvPlayerState selected = { sizeof selected };
            if (of_target(&api, actor, filter, &selected) != SV_OK) return;
            if (selected.entity != entity) continue;
        } else if (player.is_bot && !all) {
            ++hidden;
            continue;
        }
        describe_player(actor, slot, &player, detailed);
        ++shown;
    }
    if (filter && *filter && !all) return;
    if (hidden) of_tell(&api, actor, "%u players, %u bots hidden. Use /players all.", shown, hidden);
    else of_tell(&api, actor, "%u listed.", shown);
}

/* Reads one coordinate. "~" keeps the current value and "~N" adds N. 1 is good, 0 is not a number, -1 is out of range. */
static int coordinate(const char *text, float current, float *out)
{
    const char *digits = text + (text[0] == '~');
    char *end;
    double value;
    if (text[0] == '~' && !*digits) { *out = current; return 1; }
    if (!*digits || *digits == ' ') return 0;
    value = strtod(digits, &end);
    if (*end || end == digits || !(value > -1e9 && value < 1e9)) return 0;
    if (text[0] == '~') value += current;
    if (!(value >= -32768.0 && value <= 32768.0)) return -1;
    *out = (float)value;
    return 1;
}

/* /tp: the thing that moves comes first and the place second, as in Minecraft. */
static SvResult teleport_command(SvEntity actor, const Arguments *args)
{
    SvPlayerState mover = { sizeof mover }, destination = { sizeof destination };
    SvHumanState human = { sizeof human }, place = { sizeof place };
    SvVector position;
    SvResult result;
    int words = args->count, first = words == 4 ? 1 : 0;
    result = target(actor, words == 0 || words == 1 || words == 3 ? "" : args->word[0], &mover);
    if (result != SV_OK) return result;
    if (words == 0) {
        SvLineHit hit = { sizeof hit };
        if (body(&mover, &human) != SV_OK) return SV_STATE;
        result = api.read->aim(api.owner, mover.human, 2000, SV_LINE_ALL, &hit);
        if (result != SV_OK) return result;
        if (hit.kind == SV_HIT_NONE) return SV_NOT_FOUND;
        position = hit.position; position.y += 1;
        snprintf(report.detail, sizeof report.detail, "the aim point");
        return move(&mover, position);
    }
    if (words == 1 || words == 2) {
        result = of_target(&api, actor, args->word[words - 1], &destination);
        if (result != SV_OK) return result;
        snprintf(report.detail, sizeof report.detail, "%s", destination.name);
        if (body(&destination, &place) != SV_OK) return SV_STATE;
        position = place.position; position.x += 2;
        return move(&mover, position);
    }
    if (body(&mover, &human) != SV_OK) return SV_STATE;
    {
        int x = coordinate(args->word[first], human.position.x, &position.x);
        int y = coordinate(args->word[first + 1], human.position.y, &position.y);
        int z = coordinate(args->word[first + 2], human.position.z, &position.z);
        if (x == 0 || y == 0 || z == 0) {
            of_tell(&api, actor, "Use: /tp [PLAYER] X Y Z, or /tp [PLAYER] PLAYER. A place can use ~ for the current value.");
            return SV_INVALID;
        }
        if (x < 0 || y < 0 || z < 0) {
            of_tell(&api, actor, "That place is outside the world. Each value must be from -32768 to 32768.");
            return SV_INVALID;
        }
    }
    snprintf(report.detail, sizeof report.detail, "%.0f %.0f %.0f", position.x, position.y, position.z);
    return move(&mover, position);
}

/* /delh PLAYER ends the body of a player. A bot goes away with its body. With no name, the target is the human on the aim line of
 * the caller, up to 100 units. The caller is never the target. */
static SvResult delete_body(SvEntity actor, const char *name)
{
    SvPlayerState player = { sizeof player }, caller = { sizeof caller };
    SvHumanState mine = { sizeof mine }, other = { sizeof other };
    SvLineHit hit = { sizeof hit };
    SvResult result;
    if (name && *name) {
        result = of_target(&api, actor, name, &player);
        if (result != SV_OK) return result;
        if (player.entity == actor) return SV_INVALID;
    } else {
        if (!actor) return SV_INVALID;
        result = api.read->player(api.owner, actor, &caller);
        if (result == SV_OK) result = body(&caller, &mine);
        if (result != SV_OK) return SV_STATE;
        result = api.read->aim(api.owner, caller.human, 100, SV_LINE_HUMANS, &hit);
        if (result != SV_OK) return result;
        if (hit.kind != SV_HIT_HUMAN) return SV_NOT_FOUND;
        result = api.read->human(api.owner, hit.entity, &other);
        if (result != SV_OK) return result;
        if (!other.player) return api.write->human_remove(api.owner, hit.entity);
        result = api.read->player(api.owner, other.player, &player);
        if (result != SV_OK) return result;
    }
    note_target(&player);
    if (player.is_bot) return api.write->npc_remove(api.owner, player.entity);
    return player.human ? api.write->human_remove(api.owner, player.human) : SV_STATE;
}

/* /car NAME [color=C] [enter] [dist=N] [yaw=N] [PLAYER]. A modifier has an equals sign. The word "enter" stands alone.
 * The one word that is left is the player. */
static SvResult vehicle_command(SvEntity actor, const Arguments *args)
{
    SvPlayerState player = { sizeof player };
    SvHumanState human = { sizeof human };
    SvEntity object = 0;
    SvVector position = { 0 };
    int32_t type = 0, color = 0, distance = 4, yaw = 0;
    int i, enter = 0;
    const char *who = "";
    SvResult result;
    if (!resolve(actor, &vehicle_set, args->word[0], &type)) return SV_INVALID;
    for (i = 1; i < args->count; ++i) {
        const char *text = args->word[i];
        if (!strncmp(text, "color=", 6)) {
            if (!of_number(text + 6, 0, 5, &color)) { of_tell(&api, actor, "color must be a number from 0 to 5."); return SV_INVALID; }
        } else if (!strncmp(text, "dist=", 5)) {
            if (!of_number(text + 5, 2, 30, &distance)) { of_tell(&api, actor, "dist must be a number from 2 to 30."); return SV_INVALID; }
        } else if (!strncmp(text, "yaw=", 4)) {
            if (!of_number(text + 4, -180, 180, &yaw)) { of_tell(&api, actor, "yaw must be a number from -180 to 180."); return SV_INVALID; }
        } else if (!strcmp(text, "enter")) {
            enter = 1;
        } else if (strchr(text, '=')) {
            of_tell(&api, actor, "%.30s is not a modifier. Use color=, dist=, yaw= or enter.", text);
            return SV_INVALID;
        } else if (!*who) {
            who = text;
        } else {
            usage_line("vehicle", actor);
            return SV_INVALID;
        }
    }
    result = target(actor, who, &player);
    if (result == SV_OK) result = body(&player, &human);
    if (result == SV_OK) result = ahead(&player, (float)distance, &position);
    if (result == SV_OK)
        result = api.write->object_create(api.owner, (uint32_t)type, position, human.axis + (float)yaw * 0.017453292f, color, &object);
    if (result != SV_OK) return result;
    detail_of(&vehicle_set, type);
    snprintf(report.message, sizeof report.message, "Made a %s (color %d) in front of %s.", report.detail, color, player.name);
    if (enter) {
        size_t used = strlen(report.message);
        SvResult seat = api.write->vehicle_enter(api.owner, player.human, object, 0);
        if (seat == SV_OK)
            snprintf(report.message + used, sizeof report.message - used, " %s is in the driver seat.", player.name);
        else {
            snprintf(report.message + used, sizeof report.message - used, " The seat failed: %s did not get in.", player.name);
            report.partial = seat;
            snprintf(report.partial_detail, sizeof report.partial_detail, "vehicle_enter failed");
        }
    }
    return api.write->disown(api.owner, object);
}

static const NameSet *set_named(const char *text)
{
    static const NameSet *const sets[] = { &weapon_set, &item_set, &vehicle_set, &team_set, &sound_set };
    char lower[24];
    size_t length = strnlen(text, sizeof lower), i;
    if (!length || length >= sizeof lower) return NULL;
    for (i = 0; i <= length; ++i) lower[i] = (char)tolower((unsigned char)text[i]);
    for (i = 0; i < sizeof sets / sizeof sets[0]; ++i) {
        size_t kind = strlen(sets[i]->kind);
        if (!strncmp(sets[i]->kind, lower, length) || (length == kind + 1 && lower[kind] == 's' && !strncmp(sets[i]->kind, lower, kind)))
            return sets[i];
    }
    return NULL;
}

/* /names KIND lists the number and the names of each value. */
static SvResult names_command(SvEntity actor, const char *kind)
{
    const NameSet *set = set_named(kind);
    char text[SV_PATCHOULI_TEXT_MAX + 1], title[32], *line;
    const void *table = NULL;
    size_t used = 0;
    uint32_t i, j;
    if (!set) { usage_line("names", actor); return SV_INVALID; }
    text[0] = 0;
    for (i = 0; i < set->count; ++i) {
        int first = 1;
        for (j = 0; j < i; ++j) if (set->entries[j].value == set->entries[i].value) first = 0;
        if (!first) continue;
        used += (size_t)snprintf(text + used, sizeof text - used, "%d %s", set->entries[i].value, set->entries[i].name);
        for (j = i + 1; j < set->count; ++j)
            if (set->entries[j].value == set->entries[i].value)
                used += (size_t)snprintf(text + used, sizeof text - used, ", %s", set->entries[j].name);
        used += (size_t)snprintf(text + used, sizeof text - used, "\n");
    }
    snprintf(title, sizeof title, "Names of %ss", set->kind);
    if (actor && api.plugins->borrow(api.owner, SV_PATCHOULI_API, SV_PATCHOULI_VERSION, &table) == SV_OK && table &&
        ((const SvPatchouli *)table)->size >= sizeof(SvPatchouli) && ((const SvPatchouli *)table)->pages &&
        ((const SvPatchouli *)table)->pages(api.owner, actor, title, text) == SV_OK)
        return SV_OK;
    for (line = text; *line; ) {
        char *end = strchr(line, '\n');
        if (end) *end = 0;
        of_tell(&api, actor, "%s", line);
        line = end ? end + 1 : line + strlen(line);
    }
    return SV_OK;
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
        of_tell(&api, actor, "god heal kill item vehicle arm team heavy tp goto bring");
        of_tell(&api, actor, "npc sound give turn herobrine players where tpall freeze");
        of_tell(&api, actor, "unfreeze say round clear who id help. Optional target last.");
        of_tell(&api, actor, "arm 1=AK47 2=M16 3=Magnum 4=MP5 5=Uzi 6=9mm");
        of_tell(&api, actor, "arm [N] [PLAYER]: weapon and 4 magazines. item N [PLAYER].");
        of_tell(&api, actor, "tp [PLAYER] PLAYER|X Y Z; bring PLAYER [DEST]; heal [PLAYER|all]");
        of_tell(&api, actor, "A player is an account number, id:N or a name.");
        of_tell(&api, actor, "give money PLAYER N; give admin PLAYER; round reset");
        return SV_OK;
    }
    if (!strcmp(name, "players") || !strcmp(name, "who") || !strcmp(name, "id")) {
        list_players(actor, first, strcmp(name, "who") != 0); return SV_OK;
    }
    if (!strcmp(name, "delh")) return delete_body(actor, first);
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
        result = of_target(&api, actor, last, &player);
        if (result != SV_OK) return result;
        note_target(&player);
        if (player.is_bot || !player.account) return SV_INVALID;
        if (!strcmp(first, "money")) {
            if (args->count != 3 || !of_number(args->word[2], 1, 100000000, &number)) return SV_INVALID;
            result = api.write->add_money(api.owner, player.account, number);
            if (result == SV_OK) snprintf(report.detail, sizeof report.detail, "$%d", number);
            return result;
        } else {
            char group[SV_CONFIG_TEXT_MAX + 1];
            if (args->count != 2) return SV_INVALID;
            result = of_permissions(&api, &service);
            if (result != SV_OK) return result;
            result = api.config->get_text(api.owner, "admin_group", group, sizeof group);
            if (result != SV_OK) return result;
            result = service->assign(actor, player.entity, group);
            if (result == SV_OK) snprintf(report.detail, sizeof report.detail, "the group %s", group);
            return result;
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
        if (!strcmp(name, "tpall"))
            snprintf(report.message, sizeof report.message, failed ? "Moved %u players to %s. Skipped %u: no living body." :
                     "Moved %u players to %s.", changed, report.target, failed);
        else
            snprintf(report.message, sizeof report.message, failed ? "Healed %u players. Skipped %u: no living body." :
                     "Healed %u players.", changed, failed);
        return SV_OK;
    }
    if (!strcmp(name, "tp") || !strcmp(name, "goto")) return teleport_command(actor, args);
    if (!strcmp(name, "bring")) {
        result = of_target(&api, actor, first, &player);
        if (result != SV_OK) return result;
        result = target(actor, last, &destination);
        if (result != SV_OK) return result;
        note_target(&player);
        snprintf(report.detail, sizeof report.detail, "%s", destination.name);
        if (body(&destination, &place) != SV_OK) return SV_STATE;
        position = place.position; position.x += 2;
        return move(&player, position);
    }
    if (!strcmp(name, "vehicle")) return vehicle_command(actor, args);
    if (!strcmp(name, "names")) return names_command(actor, first);
    if (!strcmp(name, "arm") && !args->count) {
        number = 2;
        result = target(actor, "", &player);
    } else if (!strcmp(name, "item") || !strcmp(name, "arm") || !strcmp(name, "team") || !strcmp(name, "sound") ||
               !strcmp(name, "turn")) {
        const NameSet *set = !strcmp(name, "item") ? &item_set : !strcmp(name, "arm") ? &weapon_set :
            !strcmp(name, "team") ? &team_set : !strcmp(name, "sound") ? &sound_set : &account_set;
        if (!resolve(actor, set, first, &number)) return SV_INVALID;
        result = target(actor, last, &player);
        if (result == SV_OK && set == &account_set) snprintf(report.detail, sizeof report.detail, "account %d", number);
        else if (result == SV_OK && set != &weapon_set) detail_of(set, number);
    } else result = target(actor, first, &player);
    if (result != SV_OK) return result;
    if (!strcmp(name, "team")) return of_set(&api, "player.team", player.entity, number);
    if (!strcmp(name, "where")) {
        result = body(&player, &human);
        if (result == SV_OK) of_tell(&api, actor, "%s: %.1f %.1f %.1f", player.name,
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
            snprintf(report.message, sizeof report.message, "%s is %s.", player.name,
                     state->immortal ? "now immortal" : "no longer immortal");
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
    if (!strcmp(name, "item")) return give_item(&player, (uint32_t)number);
    if (!strcmp(name, "arm")) return give_weapon(actor, &player, (uint32_t)number);
    if (!strcmp(name, "npc")) {
        if (pending_npc) return SV_CONFLICT;
        pending_npc = player.entity; return SV_OK;
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

static const struct { const char *command, *usage, *summary; } topic_list[] = {
    { "god", "[PLAYER]", "Toggle health 100 each tick. Also /imortal" },
    { "heal", "[PLAYER|all]", "Set health 100, bleeding 0 and stun 0" },
    { "kill", "[PLAYER]", "Set health -5 and bleeding 1" },
    { "arm", "[N|NAME] [PLAYER]", "Weapon with 4 magazines. Default M-16. Names: /names weapon" },
    { "item", "N|NAME [PLAYER]", "Give an item in a free hand or a pocket. Names: /names item" },
    { "vehicle", "NAME [color= enter dist= yaw=] [PLAYER]", "Make a vehicle in front of the target. Also /car" },
    { "team", "N|NAME [PLAYER]", "Set the team 0 to 5. Names: /names team" },
    { "heavy", "[PLAYER]", "Make the body very heavy, once. Also /heavydude" },
    { "tp", "[PLAYER] PLAYER|X Y Z", "Move a player. The mover comes first. No arguments: aim hit" },
    { "goto", "PLAYER", "Move the caller beside a player" },
    { "bring", "PLAYER [DEST]", "Move a player beside DEST or the caller" },
    { "tpall", "[DEST]", "Move other players beside DEST or the caller" },
    { "freeze", "PLAYER", "Hold the body at its position" },
    { "unfreeze", "PLAYER", "Release a frozen body" },
    { "npc", "[PLAYER]", "Add a bot in front of the target" },
    { "sound", "N|NAME [PLAYER]", "Play a sound at the aim hit. Names: /names sound" },
    { "give", "money|admin PLAYER [N]", "Add money, or give the admin group" },
    { "turn", "ACCOUNT [PLAYER]", "Copy the name and looks of a saved account" },
    { "herobrine", "[PLAYER]", "Set look selector 2 to 3" },
    { "players", "[all]", "List players with account number, slot and group" },
    { "who", "[all]", "List players without groups" },
    { "id", "PLAYER", "Show account number, slot and group of a player" },
    { "where", "[PLAYER]", "Show the coordinates that /tp takes" },
    { "names", "weapon|item|vehicle|team|sound", "List the names that a command accepts" },
    { "say", "TEXT", "Send a server message to everybody" },
    { "round", "reset", "Queue the round reset" },
    { "clear", "[PLAYER]", "Send 20 blank private lines" },
    { "delh", "[PLAYER]", "End the body of a player, or of the human you aim at" }
};

/* The help system is optional. Without it, this plugin answers /help by itself. */
static int register_topics(SvOwner owner)
{
    const void *table = NULL;
    const SvPatchouli *service;
    SvPatchouliTopic topic = { sizeof topic };
    char node[40];
    uint32_t i;
    if (api.plugins->borrow(owner, SV_PATCHOULI_API, SV_PATCHOULI_VERSION, &table) != SV_OK || !table) return 0;
    service = (const SvPatchouli *)table;
    if (service->size < sizeof *service || service->version != SV_PATCHOULI_VERSION || !service->topic) return 0;
    for (i = 0; i < sizeof topic_list / sizeof topic_list[0]; ++i) {
        snprintf(node, sizeof node, "admcmd.%s", topic_list[i].command);
        topic.command = topic_list[i].command; topic.usage = topic_list[i].usage;
        topic.summary = topic_list[i].summary; topic.node = node;
        if (service->topic(owner, &topic) != SV_OK) return 0;
    }
    return 1;
}

static void forget_topics(SvOwner owner)
{
    const void *table = NULL;
    if (api.plugins && api.plugins->size >= offsetof(SvPlugins, borrow) + sizeof api.plugins->borrow &&
        api.plugins->borrow(owner, SV_PATCHOULI_API, SV_PATCHOULI_VERSION, &table) == SV_OK && table &&
        ((const SvPatchouli *)table)->size >= sizeof(SvPatchouli))
        ((const SvPatchouli *)table)->forget(owner);
}

static void usage_line(const char *name, SvEntity actor)
{
    uint32_t i;
    for (i = 0; i < sizeof topic_list / sizeof topic_list[0]; ++i)
        if (!strcmp(topic_list[i].command, name)) {
            of_tell(&api, actor, "Use: /%s %s", name, topic_list[i].usage);
            return;
        }
    of_tell(&api, actor, "Use: /%s", name);
}

/* One place for the answers. %t is the target, %d the detail and %a the caller. A command with no row says nothing when
 * it works. A command with a message in the report (a count, a toggle) uses that message in place of the row. */
static const struct { const char *command, *done, *you; } texts[] = {
    { "heal", "Healed %t.", "%a healed you." },
    { "kill", "Killed %t.", "%a killed you." },
    { "item", "Gave %d to %t.", "%a gave you %d." },
    { "arm", NULL, "%a gave you a %d." },
    { "vehicle", NULL, "%a made a vehicle in front of you." },
    { "team", "Set the team of %t to %d.", "%a set your team to %d." },
    { "heavy", "Made %t very heavy.", "%a made you very heavy." },
    { "tp", "Teleported %t to %d.", "%a teleported you to %d." },
    { "goto", "Teleported %t to %d.", "%a teleported you to %d." },
    { "bring", "Teleported %t to %d.", "%a teleported you to %d." },
    { "freeze", "Froze %t.", "%a froze you." },
    { "unfreeze", "Released %t.", "%a released you." },
    { "npc", "A bot will appear in front of %t.", NULL },
    { "sound", "Played the sound %d at the aim point of %t.", NULL },
    { "give", "Gave %d to %t.", "%a gave you %d." },
    { "turn", "Changed the look and name of %t to %d.", "%a changed your look." },
    { "herobrine", "Changed the look of %t.", NULL },
    { "say", "Sent your message to everybody.", NULL },
    { "round", "Queued the round reset.", NULL },
    { "delh", "Ended the body of %t.", "%a ended your body." }
};

static void actor_name(SvEntity actor, char *out, size_t size)
{
    SvPlayerState player = { sizeof player };
    if (actor && api.read->player(api.owner, actor, &player) == SV_OK) snprintf(out, size, "%s", player.name);
    else snprintf(out, size, "The console");
}

static void expand(char *out, size_t size, const char *format, SvEntity actor)
{
    size_t used = 0;
    for (; *format && used + 1 < size; ++format) {
        const char *text = NULL;
        char caller[40];
        if (*format == '%' && (format[1] == 't' || format[1] == 'd' || format[1] == 'a')) {
            if (format[1] == 't') text = report.target;
            else if (format[1] == 'd') text = report.detail;
            else { actor_name(actor, caller, sizeof caller); text = caller; }
            ++format;
            used += (size_t)snprintf(out + used, size - used, "%s", text);
            if (used + 1 > size) used = size - 1;
        } else out[used++] = *format;
    }
    out[used] = 0;
}

/* The answer of a command that worked, and the notice for the target when somebody else did it. */
static void announce(const char *name, SvEntity actor)
{
    char line[240];
    uint32_t i;
    for (i = 0; i < sizeof texts / sizeof texts[0] && strcmp(texts[i].command, name); ++i) {}
    if (report.message[0]) of_tell(&api, actor, "%s", report.message);
    else if (i < sizeof texts / sizeof texts[0] && texts[i].done) {
        expand(line, sizeof line, texts[i].done, actor);
        of_tell(&api, actor, "%s", line);
    }
    if (i < sizeof texts / sizeof texts[0] && texts[i].you && actor && report.target_entity &&
        report.target_entity != actor && !report.target_is_bot && of_int(&api, "notify_target", 1)) {
        expand(line, sizeof line, texts[i].you, actor);
        of_tell(&api, report.target_entity, "%s", line);
    }
}

/* A failure in plain words, with what to do. The technical code goes to the log service. */
static void explain(const char *name, SvResult result, SvEntity actor)
{
    const char *who = report.target[0] ? report.target : "The target";
    int gives = !strcmp(name, "arm") || !strcmp(name, "item");
    switch (result) {
    case SV_INVALID:
        usage_line(name, actor);
        break;
    case SV_STATE:
        of_tell(&api, actor, "%s has no living body. Wait until the player spawns.", who);
        break;
    case SV_CONFLICT:
        if (gives) of_tell(&api, actor, "Nothing was given: both hands and all pockets of %s are full.", who);
        else if (!strcmp(name, "heavy")) of_tell(&api, actor, "%s is already heavy.", who);
        else if (!strcmp(name, "npc")) of_tell(&api, actor, "Another bot is already waiting. Try again in a moment.");
        else of_tell(&api, actor, "A conflicting request is already waiting. Try again in a moment.");
        break;
    case SV_LIMIT:
        of_tell(&api, actor, "The server has no free slot for this.");
        break;
    case SV_DENIED:
        of_tell(&api, actor, "You are not allowed to do this.");
        break;
    case SV_NOT_FOUND:
        if (!strcmp(name, "give")) of_tell(&api, actor, "This needs the Permission System plugin, and it is not running.");
        else of_tell(&api, actor, "Nothing was found. Aim at the target, or check the name.");
        break;
    case SV_INCOMPATIBLE:
        of_tell(&api, actor, "A plugin that this command needs has the wrong version.");
        break;
    case SV_IO:
        of_tell(&api, actor, "The data could not be saved.");
        break;
    default:
        of_tell(&api, actor, "The game refused this.");
    }
}

/* The commands that the log service records when they work. Every failure is recorded. */
static int dangerous(const char *name)
{
    static const char *const names[] = { "heavy", "give", "round", "kill", "tpall", "delh" };
    size_t i;
    for (i = 0; i < sizeof names / sizeof names[0]; ++i)
        if (!strcmp(name, names[i])) return 1;
    return 0;
}

static void log_command(const char *name, SvEntity actor, SvResult result)
{
    const void *table = NULL;
    const SvLogging *service;
    SvLogEntry entry = { sizeof entry };
    SvResult logged = result != SV_OK ? result : report.partial;
    if (logged == SV_OK && !dangerous(name)) return;
    if (api.plugins->borrow(api.owner, SV_LOGGING_API, SV_LOGGING_VERSION, &table) != SV_OK || !table) return;
    service = (const SvLogging *)table;
    if (service->size < sizeof *service || service->version != SV_LOGGING_VERSION || !service->record) return;
    entry.kind = logged == SV_OK ? SV_LOG_ACTION : SV_LOG_FAILURE;
    entry.action = name; entry.actor = actor; entry.target = report.target_entity; entry.result = logged;
    entry.detail = logged != SV_OK && report.partial_detail[0] ? report.partial_detail : NULL;
    service->record(api.owner, &entry);
}

static SvResult command(const char *name, int minimum, int maximum, SvEntity actor, const char *arguments)
{
    char node[64], line[256], *rest = line;
    Arguments args = { 0 };
    SvResult result;
    snprintf(node, sizeof node, "admcmd.%s", name);
    if (!of_can(&api, actor, node)) { of_tell(&api, actor, "You need the permission %s for this.", node); return SV_OK; }
    if (strnlen(arguments, sizeof line) >= sizeof line) { of_tell(&api, actor, "Command text is too long."); return SV_OK; }
    memset(&report, 0, sizeof report); api.spoke = 0;
    if (!strcmp(name, "say")) {
        result = api.write->message(api.owner, SV_MESSAGE_SERVER, arguments);
    } else {
        strcpy(line, arguments);
        while (*rest && args.count < 6)
            if (of_token(&rest, args.word[args.count], sizeof args.word[0]) != 1) break;
            else ++args.count;
        if (*rest || args.count < minimum || args.count > maximum) {
            usage_line(name, actor);
            result = SV_INVALID;
        }
        else result = run(name, actor, &args);
    }
    if (result == SV_OK) announce(name, actor);
    else if (!api.spoke) explain(name, result, actor);
    log_command(name, actor, result);
    return SV_OK;
}

#define COMMAND(name, low, high) \
    static SvResult SV_CALL cmd_##name(SvOwner owner, uint64_t actor, const char *args) \
    { (void)owner; return command(#name, low, high, actor, args); }
COMMAND(god, 0, 1) COMMAND(heal, 0, 1) COMMAND(kill, 0, 1) COMMAND(item, 1, 2)
COMMAND(vehicle, 1, 5) COMMAND(arm, 0, 2) COMMAND(team, 1, 2)
COMMAND(heavy, 0, 1) COMMAND(tp, 0, 4) COMMAND(goto, 1, 4) COMMAND(bring, 1, 2)
COMMAND(npc, 0, 1) COMMAND(sound, 1, 2) COMMAND(give, 2, 3) COMMAND(turn, 1, 2)
COMMAND(herobrine, 0, 1) COMMAND(help, 0, 0) COMMAND(players, 0, 1) COMMAND(where, 0, 1)
COMMAND(tpall, 0, 1) COMMAND(freeze, 1, 1) COMMAND(unfreeze, 1, 1) COMMAND(say, 0, 0)
COMMAND(names, 0, 1) COMMAND(round, 1, 1) COMMAND(clear, 0, 1) COMMAND(who, 0, 1) COMMAND(id, 1, 1) COMMAND(delh, 0, 1)
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
        { "item", cmd_item }, { "vehicle", cmd_vehicle }, { "car", cmd_vehicle }, { "arm", cmd_arm },
        { "team", cmd_team }, { "heavy", cmd_heavy }, { "heavydude", cmd_heavy }, { "tp", cmd_tp }, { "goto", cmd_goto },
        { "bring", cmd_bring }, { "npc", cmd_npc }, { "sound", cmd_sound }, { "give", cmd_give }, { "turn", cmd_turn },
        { "herobrine", cmd_herobrine }, { "help", cmd_help }, { "bmhelp", cmd_help }, { "players", cmd_players },
        { "where", cmd_where }, { "tpall", cmd_tpall }, { "freeze", cmd_freeze }, { "unfreeze", cmd_unfreeze },
        { "say", cmd_say }, { "round", cmd_round }, { "clear", cmd_clear }, { "who", cmd_who }, { "id", cmd_id },
        { "delh", cmd_delh }, { "names", cmd_names }
    };
    uint32_t i;
    int patchouli;
    SvResult result = of_init(&api, host, owner, OF_CAP(SV_API_GAME_READ) | OF_CAP(SV_API_GAME_WRITE) |
        OF_CAP(SV_API_GAME_PROPS) | OF_CAP(SV_API_CONFIG) | OF_CAP(SV_API_PLUGINS));
    if (result != SV_OK) return result;
    if (api.plugins->size < offsetof(SvPlugins, borrow) + sizeof api.plugins->borrow) return SV_INCOMPATIBLE;
    memset(effects, 0, sizeof effects); pending_npc = 0; reset_sequence = UINT64_MAX;
    patchouli = register_topics(owner);
    for (i = 0; result == SV_OK && i < sizeof commands / sizeof commands[0]; ++i) {
        if (patchouli && (!strcmp(commands[i].name, "help") || !strcmp(commands[i].name, "bmhelp"))) continue;
        result = host->command(owner, commands[i].name, SV_COMMAND_PUBLIC, commands[i].callback);
    }
    if (result == SV_OK) result = host->subscribe(owner, SV_EVENT_TICK, event);
    return result;
}

static void SV_CALL disable(SvOwner owner)
{
    forget_topics(owner); memset(effects, 0, sizeof effects); pending_npc = 0;
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
