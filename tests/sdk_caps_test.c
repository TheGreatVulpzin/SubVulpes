#include "plugin_internal.h"
#include "plugin_caps.h"
#include "sv_game_read.h"
#include "sv_game_write.h"
#include "sv_game_ai.h"
#include "sv_game_economy.h"
#include "sv_game_mission.h"
#include "sv_game_settings.h"
#include "sv_game_storage.h"
#include "sv_game_engine.h"
#include "sv_game_shop.h"
#include "sv_game_props.h"
#include "sv_config.h"
#include "sv_plugins_api.h"
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

void sv_log(const char *format, ...)
{
    va_list args;
    va_start(args, format);
    vfprintf(stderr, format, args);
    va_end(args);
}

/* These expected bits must remain independent of the production table order. */
static const struct {
    const char *name;
    uint32_t bit, api;
} expected[] = {
    { "read",     1u << 0,  SV_API_GAME_READ },
    { "write",    1u << 1,  SV_API_GAME_WRITE },
    { "ai",       1u << 2,  SV_API_AI },
    { "economy",  1u << 3,  SV_API_ECONOMY },
    { "mission",  1u << 4,  SV_API_MISSION },
    { "settings", 1u << 5,  SV_API_SETTINGS },
    { "storage",  1u << 6,  SV_API_STORAGE },
    { "engine",   1u << 7,  SV_API_ENGINE },
    { "shop",     1u << 8,  SV_API_SHOP },
    { "props",    1u << 9,  SV_API_GAME_PROPS },
    { "config",   1u << 10, SV_API_CONFIG },
    { "plugins",  1u << 11, SV_API_PLUGINS },
    { "network",  1u << 12, 0 }
};

static int check(const char *path)
{
    size_t i;
    if (SV_CAP_COUNT != sizeof expected / sizeof expected[0])
        return 1;
    for (i = 0; i < sizeof expected / sizeof expected[0]; ++i) {
        SvManifest manifest;
        char text[256];
        FILE *file = fopen(path, "wb");
        if (!file)
            return 1;
        fprintf(file, "[plugin]\nid=test.caps\nversion=1.0.0\nlibrary=caps.dll\napi=1\nlive=0\ncapabilities=%s\n",
            expected[i].name);
        if (fclose(file) || !sv_manifest_read(path, &manifest, NULL, NULL)) {
            fprintf(stderr, "Cannot read capability %s.\n", expected[i].name);
            return 1;
        }
        if (manifest.cap_mask != expected[i].bit) {
            fprintf(stderr, "%s: expected bit 0x%03x, got 0x%03x.\n",
                expected[i].name, expected[i].bit, manifest.cap_mask);
            return 1;
        }
        if (strcmp(sv_plugin_caps[i].name, expected[i].name) || (expected[i].api != i + 1u && strcmp(expected[i].name, "network")) ||
            !sv_manifest_caps_text(expected[i].bit, 0, text, sizeof text) || strcmp(text, expected[i].name)) {
            fprintf(stderr, "Capability order, API ID or output changed for %s.\n", expected[i].name);
            return 1;
        }
        if (!sv_plugin_caps[i].text || !*sv_plugin_caps[i].text || !sv_plugin_caps[i].level ||
            (strcmp(sv_plugin_caps[i].level, "low") && strcmp(sv_plugin_caps[i].level, "high") &&
             strcmp(sv_plugin_caps[i].level, "highest")))
            return 1;
    }
    puts("All 13 capability bits, API IDs and names match the fixed order.");
    return 0;
}

int main(void)
{
    char temporary[MAX_PATH], path[MAX_PATH];
    DWORD length;
    int result;
    if (!GetTempFileNameA(".", "cap", 0, temporary))
        return 1;
    length = GetFullPathNameA(temporary, sizeof path, path, NULL);
    if (!length || length >= sizeof path) {
        DeleteFileA(temporary);
        return 1;
    }
    result = check(path);
    DeleteFileA(path);
    return result;
}
