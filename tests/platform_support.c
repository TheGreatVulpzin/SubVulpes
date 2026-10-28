/* Native platform services for focused tests of the real game sources. */
#include <stdarg.h>
#include <stdio.h>
#include "platform.h"

void *sr_import(const char *dll, const char *name)
{
    return plat_lookup(dll, name, 0);
}

void *sr_import_ordinal(const char *dll, unsigned ordinal)
{
    return plat_lookup(dll, NULL, ordinal);
}

/* A test program can set this hook to read each log line. The plugin test runner uses it. */
void (*sv_test_log_hook)(const char *line);

void sv_log(const char *format, ...)
{
    char line[1024];
    va_list args;
    va_start(args, format);
    vsnprintf(line, sizeof line, format, args);
    va_end(args);
    if (sv_test_log_hook)
        sv_test_log_hook(line);
    puts(line);
}

int sv_stop_requested(void)
{
    return 0;
}

void sv_profile_report(void)
{
}
