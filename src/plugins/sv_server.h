#ifndef SV_SERVER_API_H
#define SV_SERVER_API_H

#include "sv_plugin.h"

#define SV_API_SERVER_CONTROL 14u
#define SV_API_CONSOLE 15u
#define SV_SERVER_CONTROL_VERSION 1u
#define SV_CONSOLE_VERSION 1u
#define SV_CONSOLE_TEXT_MAX 4095u
#define SV_CONSOLE_LINE_MAX 300u

enum { SV_SERVER_SAVE = 1u, SV_SERVER_STOP, SV_SERVER_RESTART };

#pragma pack(push, 8)
typedef struct {
    uint32_t size, version;
    /* Queue one request at a callback safe point. A second request returns SV_STATE.
     * Stopping the plugin cancels its unconsumed request. Restart emits a notice
     * and uses controlled shutdown. The launcher must start the replacement. */
    SvResult (SV_CALL *request)(SvOwner owner, uint32_t request);
} SvServerControl;

typedef struct {
    uint32_t size, version;
    /* Run a registered plugin command as actor zero. The reply includes newline separators.
     * Capacity includes the terminator and is 1 to SV_CONSOLE_TEXT_MAX + 1.
     * SV_LIMIT means that the command ran but the reply was truncated.
     * Nested console calls return SV_STATE. Unregistered commands return SV_DENIED. */
    SvResult (SV_CALL *execute)(SvOwner owner, const char *line, char *reply, uint32_t capacity, uint32_t *length);
} SvConsole;
#pragma pack(pop)

#endif
