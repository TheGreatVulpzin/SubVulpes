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
/* 1 when a plugin dropped a chat text. A player with no reference gives no event. */
int sv_game_chat_denied(int player, const char *text);
/* 1 when a plugin dropped a new player. The player and its account are made. */
int sv_game_join_denied(int player);

#endif
