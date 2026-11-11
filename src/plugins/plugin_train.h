#ifndef SV_PLUGIN_TRAIN_H
#define SV_PLUGIN_TRAIN_H

#include "sv_plugin.h"

/* The target speed of a locomotive. thr is the value that the game computed. The function gives the value that a plugin set with
 * train_speed, or thr when no plugin set one. The locomotive code calls it once for each tick. */
float sv_train_throttle(int obj, float thr);

/* Ends the speed settings of a plugin that stops. */
void sv_train_release(SvOwner owner);

#endif
