#ifndef DEF_UTIL
#define DEF_UTIL

#include <stdio.h>
#include <stdarg.h>

void dbg_setlevel(int Level);
int dbg_getlevel();
void dbg_printf(int Level, const char *fmt, ...);

#endif