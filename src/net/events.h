/* The network events of the server. See events.c. */
#ifndef SR_NET_EVENTS_H
#define SR_NET_EVENTS_H

#include <stdint.h>

#include "sr_types.h"

void createevent_createobject(int object);
void createevent_updateitem(uint32_t item);
void createevent_updateiteminfo(uint32_t item);
void createevent_updateplayer(int player);
void createevent_updateplayer_finance(int player);
void createevent_updatehuman(int human, int player);
/* A message that only the connection of one player gets. The other clients see a filler. */
void createevent_message_to(int kind, const char *text, int player);
/* 1 when event number index goes to one player only and connection conn is not that player. */
int event_hidden_from(int index, int conn);
void createevent_updatestocks(void);
void createevent_updatecarmarket(int row);
void eventsimulation(void);
void createevent_bullet(int bullettype, const vec3 *pos, const vec3 *vel, int item);
void createevent_bullethit(int param, int kind, const vec3 *pos, const vec3 *vec);
void createevent_updateobject(int object, int state, int value, const vec3 *pos, const vec3 *vel);
void createevent_sound(int sound, const vec3 *pos, float sound_a, float sound_b);
void createevent_updateblock_quad(int area, int x, int y, int z, int block, int face, const vec3 *pos,
                                  const vec3 *normal);

#endif /* SR_NET_EVENTS_H */
