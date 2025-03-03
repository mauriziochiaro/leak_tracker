#include "leak_tracker.h"

/* Undefine so we can call real malloc/realloc/calloc/free internally. */
#undef malloc
#undef realloc
#undef calloc
#undef free

#include <string.h>

/* 
 * SENTINEL_SIZE bytes at front and back detect simple overruns.
 * We'll store a fixed pattern in these areas.
 */
#define SENTINEL_SIZE 8
static const unsigned char SENTINEL_PATTERN[8] = {0xDE, 0xAD, 0xC0, 0xDE, 0xDE, 0xAD, 0xC0, 0xDE};

/* ========== Data Structures ========== */

/* Linked list node for active allocations */
typedef struct Allocation {
    void               *realPtr;       /* Pointer from real malloc (including sentinels). */
    void               *userPtr;       /* Pointer returned to the user (start of usable data). */
    size_t             requestedSize;  /* Bytes the user requested. */
    size_t             totalSize;      /* Actual allocated size (requested + sentinel overhead). */
    const char         *file;
    int                line;
    struct Allocation  *next;
} Allocation;

/* Linked list node for freed pointers (to detect double-free) */
typedef struct Freed {
    void         *ptr;
    struct Freed *next;
} Freed;

/* Global pointers to the lists */
static Allocation *g_allocations = NULL;
static Freed      *g_freed       = NULL;

/* Memory usage counters */
static size_t g_totalAllocated   = 0; /* cumulative total bytes ever allocated */
static size_t g_currentAllocated = 0; /* currently allocated (in-use) bytes */
static size_t g_peakAllocated    = 0; /* peak in-use bytes */
static size_t g_allocationCount  = 0; /* count of active allocations */

/* Optional mutex for thread safety */
#ifndef NO_THREAD_SAFE_LEAK_TRACKER
static pthread_mutex_t g_allocMutex = PTHREAD_MUTEX_INITIALIZER;
#define LOCK_TRACKER()   pthread_mutex_lock(&g_allocMutex)
#define UNLOCK_TRACKER() pthread_mutex_unlock(&g_allocMutex)
#else
#define LOCK_TRACKER()   ((void)0)
#define UNLOCK_TRACKER() ((void)0)
#endif

/* ========== Internal Helpers ========== */

/* Add Freed record for double-free detection */
static void add_to_freed(void *ptr) {
    Freed *node = (Freed*)malloc(sizeof(Freed));
    if (!node) return;
    node->ptr = ptr;
    node->next = g_freed;
    g_freed = node;
}

static int was_pointer_freed(void *ptr) {
    Freed *cur = g_freed;
    while (cur) {
        if (cur->ptr == ptr) return 1; /* double-free */
        cur = cur->next;
    }
    return 0;
}

/* Remove a pointer from Freed list if present */
static void remove_from_freed(void *ptr) {
    Freed **pp = &g_freed;
    while (*pp) {
        Freed *cur = *pp;
        if (cur->ptr == ptr) {
            *pp = cur->next;
            free(cur);
            return;
        }
        pp = &cur->next;
    }
}

/* Find an allocation record by user pointer */
static Allocation* find_allocation(void *userPtr, Allocation **prevOut) {
    Allocation *prev = NULL;
    Allocation *cur  = g_allocations;
    while (cur) {
        if (cur->userPtr == userPtr) {
            if (prevOut) *prevOut = prev;
            return cur;
        }
        prev = cur;
        cur = cur->next;
    }
    return NULL;
}

/* Write sentinel bytes at the front and back of allocated region */
static void write_sentinels(unsigned char *base, size_t userSize) {
    /* front sentinel: base[0..SENTINEL_SIZE-1] */
    memcpy(base, SENTINEL_PATTERN, SENTINEL_SIZE);
    /* back sentinel: base[SENTINEL_SIZE+userSize .. SENTINEL_SIZE+userSize+SENTINEL_SIZE-1] */
    memcpy(base + SENTINEL_SIZE + userSize, SENTINEL_PATTERN, SENTINEL_SIZE);
}

/* Check sentinel bytes in debug_free; log if corrupted */
static int check_sentinels(const Allocation *alloc) {
    unsigned char *base = (unsigned char*)alloc->realPtr;
    size_t userSize     = alloc->requestedSize;
    /* Front check */
    if (memcmp(base, SENTINEL_PATTERN, SENTINEL_SIZE) != 0) {
        fprintf(stderr, 
            "ERROR: Front sentinel corrupted for pointer %p (allocated at %s:%d)\n",
            alloc->userPtr, alloc->file, alloc->line);
        return 0;
    }
    /* Back check */
    if (memcmp(base + SENTINEL_SIZE + userSize, SENTINEL_PATTERN, SENTINEL_SIZE) != 0) {
        fprintf(stderr, 
            "ERROR: Back sentinel corrupted for pointer %p (allocated at %s:%d)\n",
            alloc->userPtr, alloc->file, alloc->line);
        return 0;
    }
    return 1; /* OK */
}

/* ========== Public Functions ========== */

void* debug_malloc(size_t size, const char *file, int line) {
    /* Minimal check for 0-size. Some code does malloc(0). */
    if (size == 0) size = 1;

    /* We allocate extra space for front+back sentinels. */
    size_t totalSize = size + (2 * SENTINEL_SIZE);
    /* Real memory from the system */
    void *realPtr = malloc(totalSize);
    if (!realPtr) return NULL; /* out of memory */

    /* -------------- NEW FIX: remove from Freed if re-used -------------- */
    remove_from_freed(realPtr);
    /* -------------------------------------------------------------------- */
    
    LOCK_TRACKER();

    Allocation *node = (Allocation*)malloc(sizeof(Allocation));
    if (!node) {
        free(realPtr);
        UNLOCK_TRACKER();
        return NULL;
    }
    /* Write sentinel patterns */
    write_sentinels((unsigned char*)realPtr, size);

    /* userPtr is after the front sentinel */
    void *userPtr = (unsigned char*)realPtr + SENTINEL_SIZE;

    /* Fill out allocation info */
    node->realPtr       = realPtr;
    node->userPtr       = userPtr;
    node->requestedSize = size;
    node->totalSize     = totalSize;
    node->file          = file;
    node->line          = line;

    /* Insert into linked list */
    node->next = g_allocations;
    g_allocations = node;

    /* Update stats */
    g_allocationCount++;
    g_currentAllocated += size;
    g_totalAllocated   += size;
    if (g_currentAllocated > g_peakAllocated) {
        g_peakAllocated = g_currentAllocated;
    }

    UNLOCK_TRACKER();
    return userPtr;
}

void* debug_calloc(size_t count, size_t size, const char *file, int line) {
    /* Check for overflow in multiplication: count * size */
    if (count && size > (size_t)(-1) / count) {
        fprintf(stderr, "ERROR: calloc overflow in multiplication at %s:%d\n", file, line);
        return NULL;
    }
    size_t total = count * size;
    void *ptr = debug_malloc(total, file, line); /* relies on debug_malloc's boundary checks */
    if (ptr) {
        memset(ptr, 0, total);
    }
    return ptr;
}

void* debug_realloc(void *oldPtr, size_t newSize, const char *file, int line) {
    /* If oldPtr is NULL, this is basically malloc. */
    if (!oldPtr) {
        return debug_malloc(newSize, file, line);
    }
    /* If newSize == 0, this is basically free. */
    if (newSize == 0) {
        debug_free(oldPtr, file, line);
        return NULL;
    }

    LOCK_TRACKER();
    Allocation *prev = NULL;
    Allocation *cur  = find_allocation(oldPtr, &prev);
    if (!cur) {
        /* Not an allocation we know about -> real realloc fallback. */
        fprintf(stderr, 
                "Warning: Attempt to realloc unknown pointer %p at %s:%d\n",
                oldPtr, file, line);
        UNLOCK_TRACKER();
        return realloc(oldPtr, newSize);
    }

    /* Check old sentinels before real realloc. */
    check_sentinels(cur);

    /* Store the old requested size BEFORE we overwrite it. */
    size_t oldSize = cur->requestedSize;

    /* Perform real realloc with newSize + 2*SENTINEL_SIZE. */
    size_t newTotalSize = newSize + 2 * SENTINEL_SIZE;
    void *newRealPtr    = realloc(cur->realPtr, newTotalSize);
    if (!newRealPtr) {
        /* If real realloc fails, old pointer remains valid. */
        UNLOCK_TRACKER();
        return NULL;
    }

    /* Update allocation record. */
    cur->realPtr       = newRealPtr;
    cur->userPtr       = (unsigned char*)newRealPtr + SENTINEL_SIZE;
    cur->totalSize     = newTotalSize;
    cur->requestedSize = newSize;  /* Now we overwrite with new size. */

    /* Rewrite sentinels in front/back. */
    write_sentinels((unsigned char*)cur->realPtr, newSize);

    /*
     * Correctly update usage stats:
     *   1) Subtract the old requested size
     *   2) Add the new size
     */
    g_currentAllocated -= oldSize;   
    g_currentAllocated += newSize;

    /* Update cumulative total if we expanded. */
    if (newSize > oldSize) {
        g_totalAllocated += (newSize - oldSize);
    }

    /* Possibly update peak. */
    if (g_currentAllocated > g_peakAllocated) {
        g_peakAllocated = g_currentAllocated;
    }

    UNLOCK_TRACKER();
    return cur->userPtr;
}

void debug_free(void *ptr, const char *file, int line) {
    if (!ptr) return; /* free(NULL) no-op */

    LOCK_TRACKER();

    /* Double-free check */
    if (was_pointer_freed(ptr)) {
        fprintf(stderr, 
                "ERROR: Double free detected for pointer %p at %s:%d\n",
                ptr, file, line);
        UNLOCK_TRACKER();
        return;
    }

    Allocation *prev = NULL;
    Allocation *cur  = find_allocation(ptr, &prev);
    if (!cur) {
        /* Unknown pointer -> free it anyway, but can't track stats. */
        fprintf(stderr, 
                "Warning: Attempt to free unknown pointer %p at %s:%d\n",
                ptr, file, line);
        free(ptr);
        UNLOCK_TRACKER();
        return;
    }

    /* Check if sentinels are intact. */
    check_sentinels(cur);

    /* Remove from active list */
    if (!prev) {
        g_allocations = cur->next;
    } else {
        prev->next = cur->next;
    }

    /* Update usage stats. */
    g_allocationCount--;
    g_currentAllocated -= cur->requestedSize;

    /* Mark pointer as freed to detect double-frees. */
    add_to_freed(ptr);

    /* Free the metadata and the real pointer. */
    free(cur->realPtr);
    free(cur);

    UNLOCK_TRACKER();
}

void log_memory_leaks(FILE *out) {
    LOCK_TRACKER();
    if (!g_allocations) {
        fprintf(out, "\n==== Memory Leak Check ====\nNo memory leaks detected.\n");
        UNLOCK_TRACKER();
        return;
    }
    fprintf(out, "\n==== Memory Leak Check ====\nPotential leaks:\n");
    fprintf(out, "----------------------------------------------------\n");
    fprintf(out, "  Pointer            Size     Location\n");
    fprintf(out, "----------------------------------------------------\n");

    Allocation *cur = g_allocations;
    while (cur) {
        fprintf(out, 
            "  %p   %6zu   %s:%d\n",
            cur->userPtr, cur->requestedSize, cur->file, cur->line);
        cur = cur->next;
    }
    UNLOCK_TRACKER();
}

void log_memory_stats(FILE *out) {
    LOCK_TRACKER();
    fprintf(out, "\n==== Memory Statistics ====\n");
    fprintf(out, "  Current In-Use:  %zu bytes\n", g_currentAllocated);
    fprintf(out, "  Total Allocated: %zu bytes (cumulative)\n", g_totalAllocated);
    fprintf(out, "  Peak In-Use:     %zu bytes\n", g_peakAllocated);
    fprintf(out, "  Active Blocks:   %zu\n", g_allocationCount);
    UNLOCK_TRACKER();
}

void get_memory_stats(MemStats *statsOut) {
    if (!statsOut) return;
    LOCK_TRACKER();
    statsOut->currentAllocated = g_currentAllocated;
    statsOut->totalAllocated   = g_totalAllocated;
    statsOut->peakAllocated    = g_peakAllocated;
    statsOut->allocationCount  = g_allocationCount;
    UNLOCK_TRACKER();
}

void free_all_tracked(void) {
    LOCK_TRACKER();
    /* Free everything in the allocation list */
    Allocation *cur = g_allocations;
    while (cur) {
        Allocation *next = cur->next;
        free(cur->realPtr);
        free(cur);
        cur = next;
    }
    g_allocations = NULL;
    g_allocationCount  = 0;
    g_currentAllocated = 0;
    /* Freed list can be cleared as well */
    Freed *fc = g_freed;
    while (fc) {
        Freed *fn = fc->next;
        free(fc);
        fc = fn;
    }
    g_freed = NULL;
    UNLOCK_TRACKER();
}
