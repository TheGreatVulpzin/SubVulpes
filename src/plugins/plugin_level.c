#include "plugin_level.h"
#include "plugin_host.h"
#include "plugin_game_internal.h"
#include "area.h"
#include "buildings.h"
#include "logic.h"
#include <string.h>
#define g_city sr_data_city

static SvResult context(SvOwner owner)
{
    return sv_plugins_context(owner) && sv_game_is_ready() ? SV_OK : SV_STATE;
}

SvResult SV_CALL sv_level_building_count(SvOwner owner, uint32_t *count)
{
    if (context(owner) != SV_OK) return SV_STATE;
    if (!count) return SV_INVALID;
    if (sr_data_city.num_buildings < 0 || sr_data_city.num_buildings > (int32_t)SV_BUILDING_MAX) return SV_STATE;
    *count = (uint32_t)sr_data_city.num_buildings;
    return SV_OK;
}

SvResult SV_CALL sv_level_building(SvOwner owner, uint32_t index, SvBuildingState *state)
{
    uint32_t count;
    SvBuildingState value = {sizeof value};
    SvResult result = sv_level_building_count(owner, &count);
    if (result != SV_OK) return result;
    if (!state || state->size < sizeof value || index >= SV_BUILDING_MAX) return SV_INVALID;
    if (index >= count) return SV_NOT_FOUND;
    value.index = index;
    value.team = -1;
    value.native_type = BLD_INT(building_type, index);
    value.position = *(const SvVector *)BLD_PTR(vec3, building_pos, index);
    value.box_min = *(const SvVector *)BLD_PTR(vec3, building_min, index);
    value.box_max = *(const SvVector *)BLD_PTR(vec3, building_max, index);
    switch (value.native_type) {
    case 2: value.kind = SV_BUILDING_DEALERSHIP; break;
    case 4: case 5: case 7: value.kind = SV_BUILDING_SHOP; break;
    case 6: value.kind = SV_BUILDING_HOSPITAL; break;
    default: value.kind = SV_BUILDING_OTHER; break;
    }
    *state = value;
    return SV_OK;
}

SvResult SV_CALL sv_level_base(SvOwner owner, uint32_t team, SvBuildingState *state)
{
    SvBuildingState value = {sizeof value};
    if (context(owner) != SV_OK) return SV_STATE;
    if (team >= SV_BASE_COUNT || !state || state->size < sizeof value) return SV_INVALID;
    value.index = team;
    value.team = (int32_t)team;
    value.kind = SV_BUILDING_BASE;
    value.native_type = -1;
    value.position = value.box_min = *(const SvVector *)TEAM_FIELD(team0_base_min, team);
    value.box_max = *(const SvVector *)TEAM_FIELD(team0_base_max, team);
    if (!(value.box_max.x > value.box_min.x) || !(value.box_max.y > value.box_min.y) ||
        !(value.box_max.z > value.box_min.z)) return SV_NOT_FOUND;
    *state = value;
    return SV_OK;
}

static SvResult area_check(uint32_t index)
{
    const Area *a;
    if (index >= SV_AREA_COUNT) return SV_INVALID;
    a = &g_areas[index];
    if (!a->active) return SV_NOT_FOUND;
    if (a->size_x <= 0 || a->size_x > 512 || a->size_y <= 0 || a->size_y > 512 ||
        a->size_z <= 0 || a->size_z > 512 || !a->chunk_index || !(a->block_size > 0) ||
        a->block_size > 1024) return SV_STATE;
    return SV_OK;
}

SvResult SV_CALL sv_level_area(SvOwner owner, uint32_t index, SvAreaState *state)
{
    SvAreaState value = {sizeof value};
    SvResult result;
    const Area *a;
    if (context(owner) != SV_OK) return SV_STATE;
    if (!state || state->size < sizeof value) return SV_INVALID;
    result = area_check(index);
    if (result != SV_OK) return result;
    a = &g_areas[index];
    value.index = index;
    value.blocks_x = a->size_x * 64;
    value.blocks_y = a->size_y * 64;
    value.blocks_z = a->size_z * 64;
    value.block_size = a->block_size;
    memcpy(&value.origin, &a->origin, sizeof value.origin);
    memcpy(value.axes, a->axes, sizeof value.axes);
    *state = value;
    return SV_OK;
}

SvResult sv_level_cell(uint32_t area, int32_t x, int32_t y, int32_t z, int *sector, int *cell)
{
    const Area *a;
    int chunk, s;
    SvResult result = area_check(area);
    if (result != SV_OK) return result;
    a = &g_areas[area];
    if (x < 0 || y < 0 || z < 0 || x >= a->size_x * 64 || y >= a->size_y * 64 || z >= a->size_z * 64)
        return SV_INVALID;
    chunk = ((const int *)(uintptr_t)a->chunk_index)[(a->size_z * (y >> 6) + (z >> 6)) * a->size_x + (x >> 6)];
    *sector = -1;
    *cell = (x & 7) + ((z & 7) + (y & 7) * 8) * 8;
    if (chunk == -1) return SV_OK;
    if (chunk < 0 || chunk >= NUM_CHUNKS || !a->chunks[chunk].used) return SV_STATE;
    s = a->chunks[chunk].sectors[((x >> 3) & 7) + (((z >> 3) & 7) + ((y >> 3) & 7) * 8) * 8];
    if (s == -1) return SV_OK;
    if (s < 0 || s >= NUM_SECTORS || !g_sectors[s].used || g_sectors[s].area != (int)area) return SV_STATE;
    *sector = s;
    return SV_OK;
}

SvResult SV_CALL sv_level_block(SvOwner owner, uint32_t area, int32_t x, int32_t y, int32_t z, SvBlockState *state)
{
    SvBlockState value = {sizeof value};
    int sector, cell;
    SvResult result;
    if (context(owner) != SV_OK) return SV_STATE;
    if (!state || state->size < sizeof value) return SV_INVALID;
    result = sv_level_cell(area, x, y, z, &sector, &cell);
    if (result != SV_OK) return result;
    if (sector != -1) {
        value.word = g_sectors[sector].blocks[cell];
        value.interior_word = g_sectors[sector].interior[cell];
        if (value.word) memcpy(value.materials, g_sectors[sector].textures[cell], sizeof value.materials);
    }
    *state = value;
    return SV_OK;
}

typedef struct {
    SvOwner owner;
    uint32_t area;
    int x, y, z;
    uint32_t word, color;
    uint16_t materials[8];
} BlockEdit;
static BlockEdit edits[SV_BLOCK_EDIT_MAX];
static uint64_t edit_sequence;
typedef struct { uint32_t area; int sector, chunk, new_chunk; } BlockAllocation;
static BlockAllocation allocations[SV_BLOCK_EDIT_MAX];
static uint32_t allocation_count;

void sv_level_clear(void)
{
    memset(edits, 0, sizeof edits);
    allocation_count = 0;
    edit_sequence = sv_game_reset_sequence();
}

static void current_edits(void)
{
    if (edit_sequence != sv_game_reset_sequence()) sv_level_clear();
}

static int edit_find(uint32_t area, int x, int y, int z)
{
    uint32_t i;
    for (i = 0; i < SV_BLOCK_EDIT_MAX; ++i)
        if (edits[i].owner && edits[i].area == area && edits[i].x == x && edits[i].y == y && edits[i].z == z)
            return (int)i;
    return -1;
}

static void edit_note(SvOwner owner, uint32_t area, int x, int y, int z, int sector, int cell)
{
    uint32_t i;
    BlockEdit *e;
    if (edit_find(area, x, y, z) >= 0) return;
    for (i = 0; i < SV_BLOCK_EDIT_MAX && edits[i].owner; ++i) {}
    e = &edits[i];
    memset(e, 0, sizeof *e);
    e->owner = owner; e->area = area; e->x = x; e->y = y; e->z = z;
    if (sector >= 0) {
        e->word = g_sectors[sector].blocks[cell];
        e->color = g_sectors[sector].colors[cell];
        memcpy(e->materials, g_sectors[sector].textures[cell], sizeof e->materials);
    }
}

SvResult SV_CALL sv_level_block_set(SvOwner owner, uint32_t area, int32_t x, int32_t y, int32_t z,
    uint32_t value, uint32_t material)
{
    static const int offset[7][3] = {{0,0,0},{-1,0,0},{1,0,0},{0,-1,0},{0,1,0},{0,0,-1},{0,0,1}};
    int sector[7], cell[7], needed = 0, free_count = 0, i, old_chunk = -1;
    uint16_t materials[8];
    Area *a;
    SvResult result;
    if (!sv_plugins_write_context(owner) || !sv_game_is_ready()) return SV_STATE;
    if (value > SV_BLOCK_SOLID || material >= 1024 || (value &&
        (sr_data_materials.count <= 0 || material >= (uint32_t)sr_data_materials.count))) return SV_INVALID;
    result = sv_level_cell(area, x, y, z, &sector[0], &cell[0]);
    if (result != SV_OK) return result;
    if (sector[0] >= 0) {
        const BlockSector *s = &g_sectors[sector[0]];
        if ((s->blocks[cell[0]] && BLOCK_KIND(s->blocks[cell[0]]) != BLOCK_NORMAL) ||
            s->interior[cell[0]] || s->cityobjects[cell[0]]) return SV_DENIED;
    }
    current_edits();
    for (i = 0; i < (int)SV_BLOCK_EDIT_MAX; ++i) free_count += edits[i].owner == 0;
    a = &g_areas[area];
    for (i = 0; i < 7; ++i) {
        int xx = x + offset[i][0], yy = y + offset[i][1], zz = z + offset[i][2], found;
        result = sv_level_cell(area, xx, yy, zz, &sector[i], &cell[i]);
        if (result == SV_INVALID) { sector[i] = -2; continue; }
        if (result != SV_OK) return result;
        found = edit_find(area, xx, yy, zz);
        if (found >= 0 && edits[found].owner != owner) return SV_CONFLICT;
        if (found < 0) ++needed;
    }
    if (needed > free_count) return SV_LIMIT;
    if (sector[0] < 0 && value) {
        old_chunk = ((int *)(uintptr_t)a->chunk_index)[(a->size_z * (y >> 6) + (z >> 6)) * a->size_x + (x >> 6)];
        if (allocation_count == SV_BLOCK_EDIT_MAX || area_getnextsector() < 0 ||
            (old_chunk == -1 && area_getnextchunk((int)area) < 0)) return SV_LIMIT;
        sector[0] = area_reservecell(area, x, y, z);
        if (sector[0] < 0) return SV_LIMIT;
        allocations[allocation_count++] = (BlockAllocation){area, sector[0],
            ((int *)(uintptr_t)a->chunk_index)[(a->size_z * (y >> 6) + (z >> 6)) * a->size_x + (x >> 6)], old_chunk == -1};
    }
    for (i = 0; i < 7; ++i)
        if (sector[i] != -2) edit_note(owner, area, x + offset[i][0], y + offset[i][1], z + offset[i][2], sector[i], cell[i]);
    /* The core updates the exposed faces on both sides of each cube. */
    if (sector[0] >= 0) area_deleteblock(area, x, y, z);
    if (value) {
        for (i = 0; i < 8; ++i) materials[i] = (uint16_t)material;
        area_createblock(area, x, y, z, BLOCK_NORMAL, materials);
    }
    return SV_OK;
}

static int sector_empty(const BlockSector *s)
{
    int i;
    if (s->num_meshes) return 0;
    for (i = 0; i < 512; ++i)
        if (s->blocks[i] || s->interior[i] || s->cityobjects[i]) return 0;
    return 1;
}

void sv_level_release(SvOwner owner)
{
    uint32_t i, kept = 0;
    current_edits();
    for (i = 0; i < SV_BLOCK_EDIT_MAX; ++i) {
        BlockEdit *e = &edits[i];
        int sector, cell;
        if (!e->owner || e->owner != owner) continue;
        if (sv_level_cell(e->area, e->x, e->y, e->z, &sector, &cell) == SV_OK && sector >= 0) {
            g_sectors[sector].blocks[cell] = e->word;
            g_sectors[sector].colors[cell] = e->color;
            memcpy(g_sectors[sector].textures[cell], e->materials, sizeof e->materials);
            g_sectors[sector].dirty = 1;
        }
        memset(e, 0, sizeof *e);
    }
    for (i = 0; i < allocation_count; ++i) {
        BlockAllocation *n = &allocations[i];
        Area *a = &g_areas[n->area];
        int k;
        if (n->sector < 0 || !sector_empty(&g_sectors[n->sector])) continue;
        for (k = 0; k < 512; ++k)
            if (a->chunks[n->chunk].sectors[k] == n->sector) a->chunks[n->chunk].sectors[k] = -1;
        g_sectors[n->sector].used = 0;
        --a->num_sectors;
        n->sector = -1;
    }
    for (i = 0; i < allocation_count; ++i) {
        BlockAllocation *n = &allocations[i];
        Area *a = &g_areas[n->area];
        int k, used = 0;
        if (n->new_chunk) {
            for (k = 0; k < 512; ++k) used |= a->chunks[n->chunk].sectors[k] != -1;
            if (!used) {
                int count = a->size_x * a->size_y * a->size_z;
                int *indices = (int *)(uintptr_t)a->chunk_index;
                for (k = 0; k < count; ++k) if (indices[k] == n->chunk) indices[k] = -1;
                a->chunks[n->chunk].used = 0;
                n->new_chunk = 0;
            }
        }
        if (n->sector >= 0 || n->new_chunk) allocations[kept++] = *n;
    }
    allocation_count = kept;
}
