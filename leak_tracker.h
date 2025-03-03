#ifndef LEAK_TRACKER_H
#define LEAK_TRACKER_H

#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>

/* 
 * If you don't want thread safety, define NO_THREAD_SAFE_LEAK_TRACKER 
 * before including this header. 
 */
#define NO_THREAD_SAFE_LEAK_TRACKER
#ifndef NO_THREAD_SAFE_LEAK_TRACKER
  #include <pthread.h>
#endif

/* 
 * Macros to override standard allocation calls with debug versions.
 * Ensure this header is included AFTER system headers, 
 * so we don't collide with system prototypes.
 */
#define malloc(size)              debug_malloc((size), __FILE__, __LINE__)
#define realloc(ptr, size)        debug_realloc((ptr), (size), __FILE__, __LINE__)
#define calloc(count, size)       debug_calloc((count), (size), __FILE__, __LINE__)
#define free(ptr)                 debug_free((ptr), __FILE__, __LINE__)

/* Memory usage statistics. */
typedef struct {
    size_t totalAllocated;    /* Cumulative total bytes ever allocated */
    size_t currentAllocated;  /* Currently in-use bytes */
    size_t peakAllocated;     /* Peak in-use bytes observed */
    size_t allocationCount;   /* Number of active (not yet freed) allocations */
} MemStats;

/* Debug allocation function declarations */
void* debug_malloc (size_t size, const char *file, int line);
void* debug_realloc(void *ptr, size_t size, const char *file, int line);
void* debug_calloc (size_t count, size_t size, const char *file, int line);
void  debug_free   (void *ptr, const char *file, int line);

/* Logging and stats */
void  log_memory_leaks(FILE *out);
void  log_memory_stats(FILE *out);
void  get_memory_stats(MemStats *statsOut);

/* Force-free everything currently tracked (be cautious!) */
void  free_all_tracked(void);

#endif /* LEAK_TRACKER_H */
