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

void sv_log(const char *format, ...)
{
    va_list args;
    va_start(args, format);
    vprintf(format, args);
    va_end(args);
    putchar('\n');
}

int sv_stop_requested(void)
{
    return 0;
}

void sv_profile_report(void)
{
}
