#ifndef SV_PLUGIN_MADE_H
#define SV_PLUGIN_MADE_H

#include "sv_game.h"

/* The items, vehicles and bots that plugins made. The host removes the ones that a plugin made when that plugin stops, unless
 * the plugin gave them up with disown. See docs/plugin-libraries.md. */
#define SV_MADE_MAX 2048

void sv_made_clear(void);
/* The number of entries in the table. The tests read it. */
uint32_t sv_made_count(void);
/* 1 when the table has room for one more entry. Entries of things that are gone leave the table first when it is full. */
int sv_made_room(void);
/* Records a thing that the plugin made. Call sv_made_room first, so the table cannot refuse the entry. */
void sv_made_add(SvOwner owner, uint32_t kind, SvEntity entity);
/* The plugin keeps the thing: the host does not remove it at the stop. SV_NOT_FOUND when the plugin did not make it, or when
 * it is gone. */
SvResult sv_made_disown(SvOwner owner, SvEntity entity);
/* Ends the things that the plugin made and still exist. The game removes them in its next lifetime passes. */
void sv_made_release(SvOwner owner);

#endif
