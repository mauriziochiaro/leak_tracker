/* leak_tracker.h */

#ifndef LEAK_TRACKER_H
#define LEAK_TRACKER_H

/* 
   1) Include system headers here (or have them included 
      in files that #include "leak_tracker.h") 
*/
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h> 

/*
   2) Now define your macros AFTER the system headers 
   so that only your code sees the redefinitions.
*/

#define malloc(size)    debug_malloc((size), __FILE__, __LINE__)
#define realloc(ptr, size) debug_realloc((ptr), (size), __FILE__, __LINE__)
#define free(ptr)       debug_free((ptr), __FILE__, __LINE__)

/* Declarations for debug_malloc, debug_free, etc. */
void* debug_malloc(size_t size, const char *file, int line);
void* debug_realloc(void *ptr, size_t size, const char *file, int line);
void  debug_free(void *ptr, const char *file, int line);
void  log_memory_leaks(FILE *out);

#endif
