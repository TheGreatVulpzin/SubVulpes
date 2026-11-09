#ifndef SV_PATCHOULI_H
#define SV_PATCHOULI_H

#include "sv_game.h"

/* Patchouli: Help System. It owns /help and /page for all plugins.
 * A plugin lists its commands with topic() and sends long text with pages().
 * Call these functions only during a host callback, with the owner of the caller. */
#define SV_PATCHOULI_API "sv.patchouli"
#define SV_PATCHOULI_VERSION 1u
#define SV_PATCHOULI_TOPIC_MAX 1024u
#define SV_PATCHOULI_TEXT_MAX 16384u

#pragma pack(push, 8)
/* All strings are copied. The command has no slash. The node is a permission node, or empty for a public command. */
typedef struct {
    uint32_t size;
    const char *command;   /* 1 to 23 bytes: lowercase letters, digits, underscore or hyphen */
    const char *usage;     /* 0 to 47 printable bytes, for example "[PLAYER] X Y Z" */
    const char *summary;   /* 1 to 79 printable bytes */
    const char *node;      /* 0 to 39 bytes */
} SvPatchouliTopic;

typedef struct {
    uint32_t size, version;
    /* Adds a topic. The same owner and command replace the old topic. SV_INVALID for a bad record,
     * SV_LIMIT when SV_PATCHOULI_TOPIC_MAX topics exist. */
    SvResult (SV_CALL *topic)(SvOwner caller, const SvPatchouliTopic *topic);
    /* Removes every topic of the caller. Call it in the disable callback. */
    SvResult (SV_CALL *forget)(SvOwner caller);
    /* Shows a text in pages of six lines. A line break is one newline. The first page goes to the player at once, and
     * /page shows the others. SV_INVALID for an empty title or text, SV_LIMIT for a text over SV_PATCHOULI_TEXT_MAX,
     * SV_NOT_FOUND for a player that is not online. */
    SvResult (SV_CALL *pages)(SvOwner caller, SvEntity player, const char *title, const char *text);
} SvPatchouli;
#pragma pack(pop)

#endif
