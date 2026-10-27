#ifndef SV_SYSTEMS_HOST_H
#define SV_SYSTEMS_HOST_H

/* Shared includes for the host side of the system areas. The host side is the test program:
 * it sets up the game before the tick and checks the game after the tick. */
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "areas.h"
#include "humans.h"
#include "items.h"
#include "logic.h"
#include "objects.h"
#include "player.h"
#include "plugin_entities.h"
#include "plugin_game_internal.h"
#include "plugin_host.h"
#include "server.h"
#include "sr_math.h"
#include "sr_server.h"
#include "world.h"

#define CHECK(test) do { if (!(test)) { \
    fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #test); exit(1); \
} } while (0)

#define DECLARE_HOST(area) void area##_prepare(void); void area##_verify(void);
SYSTEMS_AREAS(DECLARE_HOST)
#undef DECLARE_HOST

#endif
