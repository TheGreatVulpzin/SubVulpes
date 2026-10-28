#ifndef SV_GAME_STORAGE_H
#define SV_GAME_STORAGE_H

#include "sv_game.h"

#define SV_API_STORAGE 7u
#define SV_GAME_STORAGE_VERSION 1u
#define SV_STORAGE_KEY_SIZE 32u
#define SV_STORAGE_VALUE_MAX 1024u
#define SV_STORAGE_ENTRY_MAX 128u
#define SV_STORAGE_BYTES_MAX 65536u

/* SV_STORAGE_ENTITY tags an item, a vehicle or a human. It lives in memory and goes with the thing. */
enum { SV_STORAGE_SESSION = 1u, SV_STORAGE_ACCOUNT, SV_STORAGE_SERVER, SV_STORAGE_ENTITY };

#pragma pack(push, 8)
typedef struct {
    uint32_t size, scope;
    SvEntity subject;
    char name[SV_STORAGE_KEY_SIZE];
} SvStorageKey;

typedef struct {
    uint32_t size, length;
    uint8_t bytes[SV_STORAGE_VALUE_MAX];
} SvStorageValue;

typedef struct {
    uint32_t size, version;
    SvResult (SV_CALL *get)(SvOwner owner, const SvStorageKey *key, SvStorageValue *out);
    SvResult (SV_CALL *put)(SvOwner owner, const SvStorageKey *key, const SvStorageValue *value);
    SvResult (SV_CALL *erase)(SvOwner owner, const SvStorageKey *key);
} SvGameStorage;
#pragma pack(pop)

/* The core supplies the validated module id and preserved private root before enable. */
SvResult sv_storage_open(SvOwner owner, const char *id, const char *private_root);
/* The thing is gone (a player, a human, an item or a vehicle). Its session and entity values go with it. */
void sv_storage_leave(SvEntity player);
/* The world clears all things of one kind (SV_ENTITY_PLAYER and the others). Their values go too. */
void sv_storage_clear(uint32_t kind);
void sv_storage_release_owner(SvOwner owner);

#endif
