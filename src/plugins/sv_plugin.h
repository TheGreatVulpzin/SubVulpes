/* Public native ABI. This header does not depend on game record layouts. */
#ifndef SV_PLUGIN_H
#define SV_PLUGIN_H

#include <stdint.h>
#include <stddef.h>

#ifdef _WIN32
#define SV_CALL __cdecl
#define SV_EXPORT __declspec(dllexport)
#else
#define SV_CALL
#define SV_EXPORT
#endif

#ifdef __cplusplus
extern "C" {
#endif

#define SV_PLUGIN_ABI 1u
#define SV_PLUGIN_ID_SIZE 48
#define SV_PLUGIN_VERSION_SIZE 24

typedef uint32_t SvOwner;
typedef int32_t SvResult;
enum {
    SV_OK, SV_INVALID, SV_STATE, SV_LIMIT, SV_CONFLICT, SV_DENIED,
    SV_NOT_FOUND, SV_INCOMPATIBLE, SV_IO, SV_DEPENDENCY, SV_RESTART, SV_FAILED
};
enum { SV_COMMAND_PUBLIC, SV_COMMAND_ADMIN, SV_COMMAND_LOCAL };
enum { SV_EVENT_TICK = 1u, SV_EVENT_STOP = 2u, SV_EVENT_LOST = 4u, SV_EVENT_SPAWN = 8u, SV_EVENT_DEATH = 16u,
    SV_EVENT_LEAVE = 32u, SV_EVENT_CHAT = 64u, SV_EVENT_JOIN = 128u, SV_EVENT_PHONE_DIGIT = 256u,
    SV_EVENT_PHONE_CALL = 512u, SV_EVENT_DAMAGE = 1024u, SV_EVENT_CONFIG = 2048u, SV_EVENT_ROUND = 4096u,
    SV_EVENT_VEHICLE = 8192u, SV_EVENT_SHOT = 16384u, SV_EVENT_OBJECT_GONE = 32768u, SV_EVENT_ITEM = 65536u };
enum { SV_PLUGIN_LIVE = 1u };

#pragma pack(push, 8)
typedef struct {
    uint32_t size;
    uint32_t tick;
    uint32_t players;
    uint32_t bots;
    uint32_t uptime_ms;
} SvStatus;

typedef struct {
    uint32_t size;
    uint32_t kind;
    uint32_t tick;
} SvEvent;

/* Actor zero is the local operator. Other actor values are host references. */
typedef SvResult (SV_CALL *SvCommandFn)(SvOwner owner, uint64_t actor, const char *arguments);
typedef SvResult (SV_CALL *SvEventFn)(SvOwner owner, const SvEvent *event);

typedef struct SvHost {
    uint32_t size;
    uint32_t abi;
    SvResult (SV_CALL *log)(SvOwner owner, const char *text);
    SvResult (SV_CALL *status)(SvOwner owner, SvStatus *status);
    SvResult (SV_CALL *command)(SvOwner owner, const char *name, uint32_t permission, SvCommandFn callback);
    SvResult (SV_CALL *subscribe)(SvOwner owner, uint32_t events, SvEventFn callback);
    SvResult (SV_CALL *get_api)(SvOwner owner, uint32_t capability, uint32_t version, const void **api);
} SvHost;

#define SV_HOST_V1_SIZE ((uint32_t)offsetof(SvHost, get_api))

typedef struct {
    uint32_t size;
    uint32_t abi;
    char id[SV_PLUGIN_ID_SIZE];
    char version[SV_PLUGIN_VERSION_SIZE];
    uint32_t flags;
    SvResult (SV_CALL *enable)(const SvHost *host, SvOwner owner);
    void (SV_CALL *disable)(SvOwner owner);
} SvPlugin;
#pragma pack(pop)

/* Export this exact C symbol. The host owns the output buffer. */
typedef SvResult (SV_CALL *SvPluginEntry)(uint32_t abi, SvPlugin *plugin);

#ifdef __cplusplus
}
#endif
#endif
