#ifndef SV_PLUGIN_GAME_EVENTS_H
#define SV_PLUGIN_GAME_EVENTS_H

/* The places where the game posts events for plugins. Each function does a few array reads and a queue post, and it
 * does nothing else when no plugin asked for the event. See docs/plugin-events.md. */

/* A human was made. A human with a player is a spawn. This also forgets who hit the human last. */
void sv_game_event_spawn(int player, int human);
/* A human with a player died. The killer is the owner of the last bullet that hit it, when that player still has a human. */
void sv_game_event_death(int human);
/* A player slot was released: a connection ended, or a bot went. The reference no longer resolves at delivery. */
void sv_game_event_leave(int player);
/* A bullet of this player hit this human and did damage. */
void sv_game_event_hit(int human, int owner);
/* A human got into (entered 1) or out of (entered 0) a seat of a vehicle. */
void sv_game_event_vehicle(int human, int object, int seat, int entered);
/* A gun fired a shot. The human is the shooter. */
void sv_game_event_shot(int item, int human);
/* The game removes a vehicle at the end of its life. Call it before the reference is retired. */
void sv_game_event_object_gone(int object);
/* An item went into (in 1) or out of (in 0) a slot of a human. */
void sv_game_event_item(int item, int human, int slot, int in);
/* Looks for a change of the round and posts it. The host calls it at the start of each tick. */
void sv_game_event_poll(void);
void sv_game_event_poll_reset(void);
/* 1 when a plugin dropped a chat text. A player with no reference gives no event. */
int sv_game_chat_denied(int player, char *text);
/* 1 when a plugin dropped a new player. The player and its account are made. */
int sv_game_join_denied(int player);

#endif
