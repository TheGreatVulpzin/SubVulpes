/* The driving of a car by the AI. See ai_car.c. */
#ifndef SR_AI_CAR_H
#define SR_AI_CAR_H

void ai_car_nextwaypoint(int player);
void ai_playercar(int player, int object);

/* Player.ai_chase is 0 for no chase, 1 for all four effects (the scenario writes 1), or a mix of these bits. */
enum { AI_CHASE_SPEED = 2, AI_CHASE_LATERAL = 4, AI_CHASE_LANE = 8, AI_CHASE_LIGHTS = 16 };

#endif
