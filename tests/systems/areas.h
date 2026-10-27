#ifndef SV_SYSTEMS_AREAS_H
#define SV_SYSTEMS_AREAS_H

/* The system areas of the test program. Each area owns the files <area>_host.c and <area>_plugin.c.
 * The list is fixed. Do not edit it. See docs/parallel-work.md. */
#define SYSTEMS_AREAS(X) X(ai) X(economy) X(mission) X(settings) X(storage) X(engine) X(shop)

#endif
