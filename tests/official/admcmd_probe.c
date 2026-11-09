#include "../../plugins/common/permissions.h"
#include <stdio.h>
#include <string.h>

static Official api;
#define CHECK(x) do { if (!(x)) { api.host->log(api.owner, "ADM_PROBE_FAILED " #x); return SV_FAILED; } } while (0)

static SvResult SV_CALL command(SvOwner owner, uint64_t actor, const char *arguments)
{
    char line[256], operation[64], name[64], text[256], *rest = line;
    SvPlayerState player = { sizeof player };
    SvHumanState human = { sizeof human };
    uint32_t slot;
    (void)actor;
    CHECK(strlen(arguments) < sizeof line);
    strcpy(line, arguments);
    CHECK(of_token(&rest, operation, sizeof operation) == 1);
    if (!strcmp(operation, "api")) {
        const void *table = NULL;
        CHECK(api.plugins->find(owner, SV_PERMISSIONS_API, 1, &table) == SV_DEPENDENCY && !table);
        CHECK(api.plugins->borrow(owner, SV_PERMISSIONS_API, 2, &table) == SV_INCOMPATIBLE && !table);
        CHECK(api.plugins->borrow(owner, SV_PERMISSIONS_API, 1, NULL) == SV_INVALID);
        CHECK(api.plugins->borrow(owner + 1, SV_PERMISSIONS_API, 1, &table) == SV_STATE);
        CHECK(api.plugins->borrow(owner, "missing.service", 1, &table) == SV_NOT_FOUND && !table);
        CHECK(api.plugins->borrow(owner, SV_PERMISSIONS_API, 1, &table) == SV_OK && table);
        return api.host->log(owner, "ADM_OPTIONAL_API_OK");
    }
    if (!strcmp(operation, "bot")) {
        SvEntity made_player, made_human;
        CHECK(api.write->npc_create(owner, (SvVector){ 20, 20, 20 }, 0, "TestBot", &made_player, &made_human) == SV_OK);
        return api.host->log(owner, "ADM_BOT_CREATED");
    }
    CHECK(of_token(&rest, name, sizeof name) == 1 && !*rest);
    CHECK(of_target(&api, 0, name, &player) == SV_OK);
    CHECK(api.read->human(owner, player.human, &human) == SV_OK);
    if (!strcmp(operation, "push")) {
        uint32_t id;
        SvValue value;
        value.v = (SvVector){ 4, 5, 6 };
        CHECK(api.props->find(owner, "human.bone_velocity", &id) == SV_OK);
        CHECK(api.props->set(owner, id, player.human, 14, value) == SV_OK);
        return SV_OK;
    }
    if (!strcmp(operation, "block")) {
        unsigned i;
        for (i = 0; i < 2; ++i) {
            SvEntity item;
            CHECK(api.write->item_create(owner, 12, human.position, &item) == SV_OK);
            CHECK(api.write->item_equip(owner, item, player.human, 6) == SV_OK);
        }
        return SV_OK;
    }
    if (!strcmp(operation, "pockets") || !strcmp(operation, "magpockets")) {
        /* Fill pockets 2 to 6 with weapons (pockets) or magazines (magpockets) until the game refuses another one. */
        uint32_t filler = strcmp(operation, "pockets") ? 1 : 0;
        unsigned pocket, i;
        for (pocket = 2; pocket < 7; ++pocket)
            for (i = 0; i < 12; ++i) {
                SvEntity item;
                CHECK(api.write->item_create(owner, filler, human.position, &item) == SV_OK);
                if (api.write->item_equip(owner, item, player.human, pocket) != SV_OK) {
                    api.write->item_remove(owner, item);
                    break;
                }
            }
        return SV_OK;
    }
    if (!strcmp(operation, "state")) {
        SvBoneState bone = { sizeof bone };
        CHECK(api.read->bone(owner, player.human, 14, &bone) == SV_OK);
        snprintf(text, sizeof text, "ADM_STATE %s health=%d team=%d looks=%d,%d,%d money=%d bot=%d mass=%.1f",
            player.name, human.health, player.team, player.looks[0], player.looks[1], player.looks[2], player.money, player.is_bot, bone.mass);
        api.host->log(owner, text);
        snprintf(text, sizeof text, "ADM_POSITION %s %.2f %.2f %.2f velocity=%.2f,%.2f,%.2f",
            player.name, human.position.x, human.position.y, human.position.z, bone.velocity.x, bone.velocity.y, bone.velocity.z);
        return api.host->log(owner, text);
    }
    if (!strcmp(operation, "inventory") || !strcmp(operation, "empty")) {
        for (slot = 0; slot < 7; ++slot) {
            SvInventoryState inventory = { sizeof inventory };
            uint32_t i;
            CHECK(api.read->inventory(owner, player.human, slot, &inventory) == SV_OK);
            for (i = 0; i < inventory.count; ++i) {
                SvItemState item = { sizeof item };
                CHECK(api.read->item(owner, inventory.items[i], &item) == SV_OK);
                snprintf(text, sizeof text, "ADM_ITEM %s slot=%u type=%d children=%u", player.name, slot, item.type, item.child_count);
                api.host->log(owner, text);
                if (!strcmp(operation, "empty")) {
                    uint32_t child;
                    for (child = 0; child < item.child_count; ++child) {
                        CHECK(api.write->item_detach(owner, item.children[child]) == SV_OK);
                        CHECK(api.write->item_remove(owner, item.children[child]) == SV_OK);
                    }
                    CHECK(api.write->item_detach(owner, item.entity) == SV_OK);
                    CHECK(api.write->item_remove(owner, item.entity) == SV_OK);
                }
            }
        }
        return SV_OK;
    }
    return SV_INVALID;
}

static SvResult SV_CALL enable(const SvHost *host, SvOwner owner)
{
    SvResult result = of_init(&api, host, owner, OF_CAP(SV_API_GAME_READ) | OF_CAP(SV_API_GAME_WRITE) |
        OF_CAP(SV_API_GAME_PROPS) | OF_CAP(SV_API_PLUGINS));
    if (result == SV_OK) result = host->command(owner, "admprobe", SV_COMMAND_LOCAL, command);
    return result;
}

SV_EXPORT SvResult SV_CALL sv_plugin_entry(uint32_t abi, SvPlugin *plugin)
{
    if (abi != SV_PLUGIN_ABI || !plugin || plugin->size < sizeof *plugin) return SV_INCOMPATIBLE;
    memset(plugin, 0, sizeof *plugin);
    plugin->size = sizeof *plugin; plugin->abi = SV_PLUGIN_ABI;
    strcpy(plugin->id, "test.admcmd"); strcpy(plugin->version, "0.1.0");
    plugin->flags = SV_PLUGIN_LIVE; plugin->enable = enable;
    return SV_OK;
}
