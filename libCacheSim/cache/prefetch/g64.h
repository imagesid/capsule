// Define GPOINTER_TO_INT64 if not available
#ifndef GPOINTER_TO_INT64
#define GPOINTER_TO_INT64(p) ((int64_t)(intptr_t)(p))
#endif

// Define GINT64_TO_POINTER if not available
#ifndef GINT64_TO_POINTER
#define GINT64_TO_POINTER(i) ((void*)(intptr_t)(i))
#endif