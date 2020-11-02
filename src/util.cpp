#include "util.h"

static int debug_level = 0;

void dbg_setlevel(int Level)
{
    debug_level=Level;
}

int dbg_getlevel()
{
    return debug_level;
}

void dbg_printf(int Level, const char *fmt, ...)
{
    if (Level <= debug_level)
    {
        va_list args;
        va_start(args, fmt);
        vfprintf(stderr, fmt, args);
        va_end(args);
    }
}
