#include "sv_game_read.h"
#include "sv_game_write.h"
#include <string.h>
static const SvHost *world_host;
static const SvGameRead *world_read;
static const SvGameWrite *world_write;
#define WORLD_CHECK(test) do { if (!(test)) { world_host->log(owner, "World fixture failed: " #test); return SV_FAILED; } } while (0)

static SvResult SV_CALL world_command(SvOwner owner, uint64_t actor, const char *arguments)
{
    SvBuildingState building = {sizeof building};
    SvAreaState area = {sizeof area};
    SvBlockState block = {sizeof block};
    SvSurfaceHit hit = {sizeof hit};
    uint32_t count = 99, i;
    (void)actor;
    if (!strcmp(arguments, "reads")) {
        WORLD_CHECK(world_read->building_count(owner, &count) == SV_OK && count == 2);
        WORLD_CHECK(world_read->building_count(owner, NULL) == SV_INVALID);
        WORLD_CHECK(world_read->building_count(owner + 1, &count) == SV_STATE);
        WORLD_CHECK(world_read->building(owner, 0, &building) == SV_OK);
        WORLD_CHECK(building.index == 0 && building.kind == SV_BUILDING_DEALERSHIP && building.position.x == 123);
        WORLD_CHECK(world_read->building(owner, 1, &building) == SV_OK && building.kind == SV_BUILDING_HOSPITAL);
        WORLD_CHECK(world_read->building(owner, 2, &building) == SV_NOT_FOUND);
        WORLD_CHECK(world_read->building(owner, SV_BUILDING_MAX, &building) == SV_INVALID);
        WORLD_CHECK(world_read->building(owner, 0, NULL) == SV_INVALID);
        building.size = 0;
        WORLD_CHECK(world_read->building(owner, 0, &building) == SV_INVALID);
        building.size = sizeof building;
        WORLD_CHECK(world_read->base(owner, 2, &building) == SV_OK);
        WORLD_CHECK(building.kind == SV_BUILDING_BASE && building.team == 2 && building.position.x == 200);
        WORLD_CHECK(world_read->base(owner, 0, &building) == SV_NOT_FOUND);
        WORLD_CHECK(world_read->base(owner, SV_BASE_COUNT, &building) == SV_INVALID);
        WORLD_CHECK(world_read->base(owner, 0, NULL) == SV_INVALID);
        WORLD_CHECK(world_read->area(owner, 3, &area) == SV_OK);
        WORLD_CHECK(area.blocks_x == 128 && area.blocks_y == 64 && area.blocks_z == 128 && area.block_size == 4);
        WORLD_CHECK(world_read->area(owner, SV_AREA_COUNT, &area) == SV_INVALID);
        WORLD_CHECK(world_read->area(owner, 3, NULL) == SV_INVALID);
        WORLD_CHECK(world_read->block(owner, 3, 127, 63, 127, &block) == SV_OK && block.word == 0);
        WORLD_CHECK(world_read->block(owner, 3, 128, 0, 0, &block) == SV_INVALID);
        WORLD_CHECK(world_read->block(owner, 3, 0, 64, 0, &block) == SV_INVALID);
        WORLD_CHECK(world_read->block(owner, 3, 0, 0, 128, &block) == SV_INVALID);
        WORLD_CHECK(world_read->block(owner, 3, -1, 0, 0, &block) == SV_INVALID);
        WORLD_CHECK(world_read->block(owner, 3, 0, -1, 0, &block) == SV_INVALID);
        WORLD_CHECK(world_read->block(owner, 3, 0, 0, -1, &block) == SV_INVALID);
        WORLD_CHECK(world_read->block(owner, 3, 0, 0, 0, NULL) == SV_INVALID);
        WORLD_CHECK(world_read->block(owner, UINT32_MAX, 0, 0, 0, &block) == SV_INVALID);
        block.size = 0;
        WORLD_CHECK(world_read->block(owner, 3, 0, 0, 0, &block) == SV_INVALID);
    } else if (!strcmp(arguments, "bad-count")) {
        WORLD_CHECK(world_read->building_count(owner, &count) == SV_STATE && count == 99);
        WORLD_CHECK(world_read->building(owner, 0, &building) == SV_STATE);
    } else if (!strcmp(arguments, "bad-cell")) {
        WORLD_CHECK(world_read->block(owner, 3, 0, 0, 0, &block) == SV_STATE);
        WORLD_CHECK(world_write->block_set(owner, 3, 0, 0, 0, SV_BLOCK_SOLID, 7) == SV_STATE);
    } else if (!strcmp(arguments, "surface")) {
        SvVector from = {10000, 10010, 10006}, to = {10024, 10010, 10006};
        SvSurfaceHit before;
        WORLD_CHECK(world_read->surface(owner, from, to, &hit) == SV_OK);
        WORLD_CHECK(hit.hit.kind == SV_HIT_LEVEL && hit.area == 2 && hit.material == 7);
        WORLD_CHECK(hit.cell[0] == 1 && hit.cell[1] == 2 && hit.cell[2] == 1 && hit.face >= 0 && hit.face < 6);
        WORLD_CHECK(hit.hit.distance > 3.99f && hit.hit.distance < 4.01f);
        from.y = to.y = 10050;
        WORLD_CHECK(world_read->surface(owner, from, to, &hit) == SV_OK && hit.hit.kind == SV_HIT_NONE);
        WORLD_CHECK(hit.material == -1 && hit.area == -1 && hit.face == -1 && hit.hit.distance == 0);
        before = hit;
        WORLD_CHECK(world_read->surface(owner + 1, from, to, &hit) == SV_STATE);
        WORLD_CHECK(!memcmp(&hit, &before, sizeof hit));
        WORLD_CHECK(world_read->surface(owner, from, from, &hit) == SV_INVALID);
        to.x = 40000;
        WORLD_CHECK(world_read->surface(owner, from, to, &hit) == SV_INVALID);
        WORLD_CHECK(world_read->surface(owner, from, to, NULL) == SV_INVALID);
    } else if (!strcmp(arguments, "blocks")) {
        WORLD_CHECK(world_write->block_set(owner + 1, 3, 1, 1, 1, SV_BLOCK_SOLID, 7) == SV_STATE);
        WORLD_CHECK(world_write->block_set(owner, 3, 127, 63, 127, SV_BLOCK_SOLID, 8) == SV_OK);
        WORLD_CHECK(world_read->block(owner, 3, 127, 63, 127, &block) == SV_OK && block.word != 0 && block.materials[0] == 8);
        WORLD_CHECK(world_write->block_set(owner, 3, 127, 63, 127, SV_BLOCK_CLEAR, 0) == SV_OK);
        WORLD_CHECK(world_read->block(owner, 3, 127, 63, 127, &block) == SV_OK && block.word == 0);
        WORLD_CHECK(world_write->block_set(owner, 3, 127, 63, 127, SV_BLOCK_SOLID, 9) == SV_OK);
        WORLD_CHECK(world_write->block_set(owner, 3, 128, 0, 0, SV_BLOCK_SOLID, 7) == SV_INVALID);
        WORLD_CHECK(world_write->block_set(owner, 3, -1, 0, 0, SV_BLOCK_SOLID, 7) == SV_INVALID);
        WORLD_CHECK(world_write->block_set(owner, 3, 0, 64, 0, SV_BLOCK_SOLID, 7) == SV_INVALID);
        WORLD_CHECK(world_write->block_set(owner, 3, 0, 0, 128, SV_BLOCK_SOLID, 7) == SV_INVALID);
        WORLD_CHECK(world_write->block_set(owner, 3, 0, 0, 0, 2, 7) == SV_INVALID);
        WORLD_CHECK(world_write->block_set(owner, 3, 0, 0, 0, SV_BLOCK_SOLID, 1024) == SV_INVALID);
        WORLD_CHECK(world_write->block_set(owner, 3, 0, 0, 0, SV_BLOCK_SOLID, 16) == SV_INVALID);
        WORLD_CHECK(world_write->block_set(owner, 3, 10, 4, 10, SV_BLOCK_CLEAR, 0) == SV_OK);
        WORLD_CHECK(world_write->block_set(owner, 3, 10, 4, 10, SV_BLOCK_SOLID, 9) == SV_OK);
        WORLD_CHECK(world_write->block_set(owner, 3, 20, 4, 10, SV_BLOCK_CLEAR, 0) == SV_DENIED);
        WORLD_CHECK(world_write->block_set(owner, 3, 21, 4, 10, SV_BLOCK_CLEAR, 0) == SV_DENIED);
        WORLD_CHECK(world_write->block_set(owner, 3, 22, 4, 10, SV_BLOCK_CLEAR, 0) == SV_DENIED);
        for (i = 0; i < SV_BLOCK_EDIT_MAX; ++i) {
            SvResult result = world_write->block_set(owner, 3, (int32_t)(i % 128), 20, (int32_t)(i / 128), SV_BLOCK_SOLID, 7);
            if (result == SV_LIMIT) break;
            WORLD_CHECK(result == SV_OK);
        }
        WORLD_CHECK(i > 0 && i < SV_BLOCK_EDIT_MAX);
    } else if (!strcmp(arguments, "pool-full")) {
        WORLD_CHECK(world_write->block_set(owner, 3, 64, 0, 64, SV_BLOCK_SOLID, 7) == SV_LIMIT);
    } else return SV_INVALID;
    return SV_OK;
}

SvResult world_fixture_enable(const SvHost *host, SvOwner owner)
{
    const void *table;
    world_host = host;
    WORLD_CHECK(host->get_api(owner, SV_API_GAME_READ, 1, &table) == SV_OK);
    world_read = table;
    WORLD_CHECK(host->get_api(owner, SV_API_GAME_WRITE, 1, &table) == SV_OK);
    world_write = table;
    WORLD_CHECK(world_read->building_count(owner, NULL) == SV_STATE);
    WORLD_CHECK(world_write->block_set(owner, 0, 0, 0, 0, 0, 0) == SV_STATE);
    return host->command(owner, "worldcheck", SV_COMMAND_LOCAL, world_command);
}
