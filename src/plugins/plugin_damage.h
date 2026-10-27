#ifndef SV_PLUGIN_DAMAGE_H
#define SV_PLUGIN_DAMAGE_H

/* The damage scales that plugins set through the property table. See docs/plugin-events.md.
 *
 * A scale is a decimal number from 0 to 100, 1 by default. The core multiplies each damage by the scales that apply and
 * rounds the result to the nearest whole number (a half goes up). A scale of 0 cancels the damage. At the value 1 the
 * damage stays the number that the game made, so the game behaves as before. The scales live beside the game records
 * and not in them, because the record layouts are fixed. */

/* The damage of a hit on a human. The victim has its taken scale. The shooter, a player index or -1, has the dealt scale
 * of its human. An amount of 0 or less stays as it is. */
int sv_damage_human(int victim, int shooter_player, int amount);
/* The damage to the health of an object, from a bullet, a crash or any other cause. */
int sv_damage_object(int object, int amount);
/* The taken scale of an object, for the damage that is not a whole number (a window loses a decimal amount). */
float sv_damage_scale_object(int object);
/* A new body or a new object starts again at 1. */
void sv_damage_reset_human(int human);
void sv_damage_reset_object(int object);

/* The fields that the property table reads and writes. The index must be in range. */
float *sv_damage_field_human_taken(int human);
float *sv_damage_field_human_dealt(int human);
float *sv_damage_field_object_taken(int object);

#endif
