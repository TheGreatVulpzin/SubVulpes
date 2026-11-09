#ifndef SV_LOGGING_H
#define SV_LOGGING_H

#include "sv_game.h"

/* Logging System. A plugin records what an administrator did with record(), and the service writes one line in a fixed
 * format to the server log and keeps the last lines in memory for /logs. Call it only during a host callback, with the
 * owner of the caller. The line looks like this:
 *   ACTION by NAME (acc N) target NAME (acc N) result OK
 *   ACTION by console result FAIL SV_CONFLICT(4) DETAIL */
#define SV_LOGGING_API "sv.logging"
#define SV_LOGGING_VERSION 1u

enum {
    SV_LOG_ACTION = 0,    /* an action that worked and that the owner wants in the record */
    SV_LOG_FAILURE = 1    /* an action that failed */
};

#pragma pack(push, 8)
typedef struct {
    uint32_t size, kind;
    const char *action;   /* 1 to 23 bytes: lowercase letters, digits, underscore or hyphen */
    SvEntity actor;       /* the player that acted, or 0 for the console */
    SvEntity target;      /* the player that was the target, or 0 for none */
    SvResult result;      /* SV_OK for an action that worked */
    const char *detail;   /* 0 to 79 printable bytes, or NULL */
} SvLogEntry;

typedef struct {
    uint32_t size, version;
    /* Writes one line. The line goes to the server log under the ID of the caller. The service reads the names of the
     * actor and the target with the owner of the caller, so the caller needs the read service.
     * SV_INVALID for a bad record, and SV_OK when the settings of the service turn that kind off. */
    SvResult (SV_CALL *record)(SvOwner caller, const SvLogEntry *entry);
} SvLogging;
#pragma pack(pop)

#endif
