#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "plugin_host.h"
#include "plugin_level.h"
#include "area.h"
#include "buildings.h"
#include "logic.h"
#include "sr_runtime.h"
#define g_city sr_data_city
#define WORLD_CHECK(test) do { if (!(test)) { fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #test); exit(1); } } while (0)
static uint32_t original_words[2];
static int sector_count, saved_material_count;

static void command(const char *arguments)
{
    char line[80];
    snprintf(line, sizeof line, "worldcheck %s", arguments);
    WORLD_CHECK(sv_plugins_command(0, line));
    WORLD_CHECK(!sv_plugins_failed());
}

void test_plugin_world(void)
{
    const vec3 near_origin = {10000,10000,10000}, far_origin = {10008,10000,10000};
    const uint16_t tex[8] = {7,7,7,7,7,7,7,7};
    Intersect saved, scratch;
    int index, count, i, old_chunk;
    sr_data_city.num_buildings = 2;
    BLD_INT(building_type, 0) = 2;
    BLD_INT(building_type, 1) = 6;
    *BLD_PTR(vec3, building_pos, 0) = (vec3){123,24,456};
    *(vec3 *)TEAM_FIELD(team0_base_min, 0) = (vec3){0,0,0};
    *(vec3 *)TEAM_FIELD(team0_base_max, 0) = (vec3){0,0,0};
    *(vec3 *)TEAM_FIELD(team0_base_min, 2) = (vec3){200,24,200};
    *(vec3 *)TEAM_FIELD(team0_base_max, 2) = (vec3){240,28,240};
    saved_material_count = sr_data_materials.count;
    sr_data_materials.count = 16;
    createarea(2, &near_origin, 2, 1, 2, 4);
    createarea(3, &far_origin, 2, 1, 2, 4);
    command("reads");
    sr_data_city.num_buildings = -1;
    command("bad-count");
    sr_data_city.num_buildings = SV_BUILDING_MAX + 1;
    command("bad-count");
    sr_data_city.num_buildings = 2;
    ((int *)(uintptr_t)g_areas[3].chunk_index)[0] = NUM_CHUNKS;
    command("bad-cell");
    ((int *)(uintptr_t)g_areas[3].chunk_index)[0] = -1;
    area_createblock(2, 1, 2, 1, BLOCK_NORMAL, tex);
    area_createblock(3, 1, 2, 1, BLOCK_NORMAL, tex);
    saved = sr_data_intersect;
    memset(&scratch, 0xa5, sizeof scratch);
    sr_data_intersect = scratch;
    command("surface");
    WORLD_CHECK(!memcmp(&scratch, &sr_data_intersect, sizeof scratch));
    sr_data_intersect = saved;
    area_createblock(3, 10, 4, 10, BLOCK_NORMAL, tex);
    area_createblock(3, 11, 4, 10, BLOCK_NORMAL, tex);
    original_words[0] = area_getblock(3, 10, 4, 10);
    original_words[1] = area_getblock(3, 11, 4, 10);
    area_setblock(3, 20, 4, 10, BLOCK_SPEC);
    area_setblock(3, 21, 4, 10, BLOCK_LINK | 1);
    index = area_blocksector(3, 22, 4, 10, &count);
    WORLD_CHECK(index >= 0);
    g_sectors[index].interior[count] = BLOCK_SPEC;
    sector_count = g_areas[3].num_sectors;
    /* Both exhausted pools must fail before they allocate or change the grid. */
    old_chunk = ((int *)(uintptr_t)g_areas[3].chunk_index)[3];
    for (i = 0; i < NUM_CHUNKS; ++i) g_areas[3].chunks[i].used = 1;
    command("pool-full");
    for (i = 0; i < NUM_CHUNKS; ++i) g_areas[3].chunks[i].used = 0;
    g_areas[3].chunks[0].used = 1;
    WORLD_CHECK(((int *)(uintptr_t)g_areas[3].chunk_index)[3] == old_chunk);
    command("blocks");
    WORLD_CHECK(area_getblocktexture(3, 10, 4, 10, 0) == 9);
    WORLD_CHECK(area_getblock(3, 127, 63, 127) != 0);
    sv_level_release(999);
    WORLD_CHECK(area_getblock(3, 127, 63, 127) != 0);
    puts("WORLD_API_PASS");
}

void test_plugin_world_stopped(void)
{
    WORLD_CHECK(area_getblock(3, 10, 4, 10) == original_words[0]);
    WORLD_CHECK(area_getblock(3, 11, 4, 10) == original_words[1]);
    WORLD_CHECK(area_getblocktexture(3, 10, 4, 10, 0) == 7);
    WORLD_CHECK(area_getblock(3, 127, 63, 127) == 0);
    WORLD_CHECK(g_areas[3].num_sectors == sector_count);
    sr_data_materials.count = saved_material_count;
    puts("WORLD_RESTORE_PASS");
}
