/* Building plans, type tables and city objects. */
#ifndef SR_BUILDINGS_H
#define SR_BUILDINGS_H

#include "sr_runtime.h"

#define BLD_PTR(T, field, b) ((T *)((uint8_t *)&g_city.field + (b) * 0x9dc))
#define BLD_INT(field, b) (*BLD_PTR(int, field, b))

int createcityobject(int type, const vec3 *pos, float angle);
int city_createelevator(const vec3 *pos);
void cityobject_addbox(int type, float x, float y, float z, float sx, float sy, float sz);
void setupcityobjecttypes(void);
void setupbuildingtypes(void);
void newspaper_load(void);
/* The news lines are 256 bytes each, and a text may use at most 124 of them (the length that the file loader reads). */
enum { NEWS_MAX_LINES = 1024, NEWS_TEXT_MAX = 124 };
enum { NEWS_OK = 0, NEWS_BAD_LINE, NEWS_FULL, NEWS_BAD_TEXT };
void newspaper_send_line(int line);
int newspaper_set_line(int line, const char *text);
int newspaper_get_line(int line, char out[NEWS_TEXT_MAX + 1]);
void building_load(const char *name, int header_only);
void building_save(const char *name);
void building_setupbuild(void);
void building_setupfloorplannewer(void);
void building_rotatefloorplan(void);
int building_buildfloorplan(uint32_t area, int x, int y, int z,
                           int size_x, int size_z, int floors, int type);

#endif
