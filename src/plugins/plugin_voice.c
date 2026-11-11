/* Voice routing (N48). The game gives each listener 8 voice slots. A direct voice is heard within 256 units. A rule says that one
 * speaker is never heard by one listener, or is heard at any distance. The rules live in a short list, and the voice code asks
 * sv_voice_mode for each pair of players. */
#include "plugin_host.h"
#include "plugin_entities.h"
#include "sv_game_write.h"
#include "plugin_game_internal.h"
#include "player.h"
#include "sr_runtime.h"
#include "sr_server.h"
#include <string.h>

#define RULE_MAX 256

typedef struct {
    SvOwner owner;
    SvEntity speaker, listener;
    int32_t speaker_index, listener_index;
    uint64_t reset;
    uint32_t mode;
} Rule;

static Rule rules[RULE_MAX];
static int rules_used;

static int rule_live(const Rule *r)
{
    return r->owner && r->reset == sv_game_reset_sequence() &&
        sv_entity_resolve(SV_ENTITY_PLAYER, r->speaker) == r->speaker_index &&
        sv_entity_resolve(SV_ENTITY_PLAYER, r->listener) == r->listener_index;
}

SvResult SV_CALL sv_voice_route(SvOwner owner, SvEntity speaker, SvEntity listener, uint32_t mode)
{
    int32_t s, l;
    int i, free_slot = -1, found = -1;
    if (!sv_plugins_write_context(owner) || !sv_game_is_ready())
        return SV_STATE;
    s = sv_entity_resolve(SV_ENTITY_PLAYER, speaker);
    l = sv_entity_resolve(SV_ENTITY_PLAYER, listener);
    if (s < 0 || l < 0)
        return SV_NOT_FOUND;
    if (s == l || mode > SV_VOICE_FORCE)
        return SV_INVALID;
    /* Only a real player has a connection to listen with. */
    if (sr_players[l].is_bot)
        return SV_DENIED;
    for (i = 0; i < RULE_MAX; ++i) {
        if (rules[i].owner && !rule_live(&rules[i]))
            memset(&rules[i], 0, sizeof rules[i]);
        if (!rules[i].owner) {
            if (free_slot < 0)
                free_slot = i;
        } else if (rules[i].speaker_index == s && rules[i].listener_index == l) {
            found = i;
        }
    }
    if (found >= 0) {
        if (rules[found].owner != owner)
            return SV_CONFLICT;
        if (mode == SV_VOICE_DEFAULT) {
            memset(&rules[found], 0, sizeof rules[found]);
            return SV_OK;
        }
        rules[found].mode = mode;
        return SV_OK;
    }
    if (mode == SV_VOICE_DEFAULT)
        return SV_OK;
    if (free_slot < 0)
        return SV_LIMIT;
    rules[free_slot] = (Rule){owner, speaker, listener, s, l, sv_game_reset_sequence(), mode};
    if (free_slot >= rules_used)
        rules_used = free_slot + 1;
    return SV_OK;
}

int sv_voice_mode(int speaker, int listener)
{
    int i;
    for (i = 0; i < rules_used; ++i)
        if (rules[i].owner && rules[i].speaker_index == speaker && rules[i].listener_index == listener)
            return rule_live(&rules[i]) ? (int)rules[i].mode : SV_VOICE_DEFAULT;
    return SV_VOICE_DEFAULT;
}

void sv_voice_release(SvOwner owner)
{
    int i;
    for (i = 0; i < RULE_MAX; ++i)
        if (rules[i].owner == owner)
            memset(&rules[i], 0, sizeof rules[i]);
}
