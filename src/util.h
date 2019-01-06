#ifndef DEF_UTIL
#define DEF_UTIL

#include <stdio.h>
#include <stdarg.h>

void dbg_printf(const char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    vfprintf(stderr, fmt, args);
    va_end(args);
}
#endif