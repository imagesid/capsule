#include "debug.h"
#include <stdio.h>
#include <stdarg.h>
#include <string.h>   // for strrchr

void debug_log(const char *file, int line, const char *func, const char *fmt, ...) {
    const char *fname = strrchr(file, '/');
    if (fname)
        fname++;  // skip the '/'
    else
        fname = file;

    va_list args;
    fprintf(stderr, "[DEBUG] %s:%d (%s): ", fname, line, func);
    
    // va_list args;
    // // fprintf(stderr, "[DEBUG] %s:%d (%s): ", file, line, func);
    va_start(args, fmt);
    vfprintf(stderr, fmt, args);
    va_end(args);
    fprintf(stderr, "\n");
}

