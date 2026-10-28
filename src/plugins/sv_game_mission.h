#ifndef SV_GAME_MISSION_H
#define SV_GAME_MISSION_H

/* Existing mission representations, copied slots and checked mission resources. */
#include "sv_game.h"

#define SV_API_MISSION 5u
#define SV_GAME_MISSION_VERSION 1u
#define SV_MISSION_TEAMS 5u
#define SV_MISSION_SLOTS 16u
#define SV_MISSION_KINDS 7u
#define SV_MISSION_REWARD_MAX 0x7fffff
#define SV_MISSION_EXPIRE_MAX 0x3b0d30

#pragma pack(push, 8)
typedef struct {
    uint32_t size, team, slot;
    int32_t scenario;
    uint64_t reset;
    SvEntity item;
} SvMissionRef;

typedef struct {
    SvMissionRef reference;
    int32_t active, kind, partner, reward, expire, site, stake;
    SvEntity item;
} SvMissionState;

/* The host sets item_size to the size of one SvMissionState. A caller steps through the list with it. */
typedef struct {
    uint32_t size, team, item_size, reserved;
    SvMissionState slots[SV_MISSION_SLOTS];
} SvMissionSlots;

typedef struct {
    uint32_t size, kind;
    int32_t partner, reward, expire, site;
    SvEntity item;
} SvMissionStart;

typedef struct {
    uint32_t size, version;
    SvResult (SV_CALL *slots)(SvOwner owner, uint32_t team, SvMissionSlots *out);
    SvResult (SV_CALL *start)(SvOwner owner, uint32_t team, const SvMissionStart *input, SvMissionRef *out);
    SvResult (SV_CALL *cancel)(SvOwner owner, const SvMissionRef *mission);
} SvGameMission;
#pragma pack(pop)

/* Install after the core owns completion checks and mission lifecycle. */
void sv_mission_install(void);
void sv_mission_before_tick(void);
int sv_mission_before_complete(int32_t team, int32_t slot);
void sv_mission_release_owner(SvOwner owner);

#endif
