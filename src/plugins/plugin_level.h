#ifndef SV_PLUGIN_LEVEL_H
#define SV_PLUGIN_LEVEL_H
#include "sv_game_read.h"
SvResult SV_CALL sv_level_building_count(SvOwner owner, uint32_t *count);
SvResult SV_CALL sv_level_building(SvOwner owner, uint32_t index, SvBuildingState *state);
SvResult SV_CALL sv_level_base(SvOwner owner, uint32_t team, SvBuildingState *state);
SvResult SV_CALL sv_level_area(SvOwner owner, uint32_t index, SvAreaState *state);
SvResult SV_CALL sv_level_block(SvOwner owner, uint32_t area, int32_t x, int32_t y, int32_t z, SvBlockState *state);
SvResult sv_level_cell(uint32_t area, int32_t x, int32_t y, int32_t z, int *sector, int *cell);
SvResult SV_CALL sv_level_block_set(SvOwner owner, uint32_t area, int32_t x, int32_t y, int32_t z,
    uint32_t value, uint32_t material);
void sv_level_release(SvOwner owner);
void sv_level_clear(void);
#endif
