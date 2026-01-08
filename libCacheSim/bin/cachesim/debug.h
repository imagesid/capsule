// debug.h
#ifndef DEBUG_HELPER_H
#define DEBUG_HELPER_H

#include <stdio.h>
#include <stdint.h>
#include <stdarg.h>

void debug_log(const char *file, int line, const char *func, const char *fmt, ...);

#define DPRINT(fmt, ...) \
    debug_log(__FILE__, __LINE__, __func__, fmt, ##__VA_ARGS__)

#define DPRINT_INT(x)    DPRINT(#x " = %d", (int)(x))
#define DPRINT_UINT64(x) DPRINT(#x " = %llu", (unsigned long long)(x))
#define DPRINT_FLOAT(x)  DPRINT(#x " = %f", (double)(x))
#define DPRINT_STR(x)    DPRINT(#x " = \"%s\"", (x) ? (x) : "(null)")
#define DPRINT_PTR(x)    DPRINT(#x " = %p", (void*)(x))


// Shortest name: d()
// Works like printf, but always goes to stderr with [DEBUG] prefix
static inline void d(const char *fmt, ...) {
    va_list args;
    fprintf(stderr, "[DEBUG] ");
    va_start(args, fmt);
    vfprintf(stderr, fmt, args);
    va_end(args);
    fprintf(stderr, "\n");
}

static inline void dw(const char *fmt, ...) {
    va_list args;
    // fprintf(stderr, "");
    va_start(args, fmt);
    vfprintf(stderr, fmt, args);
    va_end(args);
    fprintf(stderr, "\n");
}

#endif // DEBUG_HELPER_H
