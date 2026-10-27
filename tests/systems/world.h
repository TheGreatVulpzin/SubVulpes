#ifndef SV_SYSTEMS_WORLD_H
#define SV_SYSTEMS_WORLD_H

/* One player, one human, one item, one object and one account, made with the real constructors.
 * The indices are filled by world_start. */
extern int world_player, world_human, world_item, world_object, world_account;

/* Loads the plugin profile of the current folder and builds the world. */
void world_start(void);

#endif
