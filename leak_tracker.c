#include "leak_tracker.h"
/*
    Implementation that uses the real malloc/free internally.
    So inside this .c, you typically want to do:

    #undef malloc
    #undef free

    before you implement debug_malloc / debug_free,
    so you can call the real malloc/free inside them.
*/

#undef malloc
#undef realloc
#undef free

#include <string.h>  // for memcpy or strcpy if needed

/* 
 * We'll store info about each allocation in this struct.
 * A simple singly linked list for demonstration purposes.
 */
typedef struct Allocation {
    void           *ptr;       /* pointer returned by malloc */
    size_t         size;       /* size requested */
    const char     *file;      /* file where malloc was called */
    int            line;       /* line where malloc was called */
    struct Allocation *next;
} Allocation;

/* Head pointer to our linked list of active allocations. */
static Allocation *g_allocations = NULL;

void* debug_malloc(size_t size, const char *file, int line)
{
    /* Call the real malloc. */
    void *ptr = malloc(size);
    if (!ptr) {
        /* Real malloc failed; just return NULL. */
        return NULL;
    }

    /* Create a new Allocation node. */
    Allocation *newAlloc = (Allocation*)malloc(sizeof(Allocation));
    if (!newAlloc) {
        /* If we can't even allocate this, free original ptr & return. */
        free(ptr);
        return NULL;
    }

    /* Fill out the allocation info. */
    newAlloc->ptr  = ptr;
    newAlloc->size = size;
    newAlloc->file = file;
    newAlloc->line = line;

    /* Insert at head of linked list. */
    newAlloc->next = g_allocations;
    g_allocations = newAlloc;

    return ptr;
}

void* debug_realloc(void *oldPtr, size_t newSize, const char *file, int line)
{
    // If oldPtr is NULL, this behaves like malloc
    if (!oldPtr) {
        return debug_malloc(newSize, file, line);
    }
    // If newSize == 0, this behaves like free
    if (newSize == 0) {
        debug_free(oldPtr, file, line);
        return NULL;
    }

    // Find the old pointer in your allocations list
    Allocation *prev = NULL;
    Allocation *curr = g_allocations;
    while (curr) {
        if (curr->ptr == oldPtr) {
            // Found it; call the real realloc
            void *newPtr = realloc(oldPtr, newSize);
            if (!newPtr) {
                // If it fails, real realloc returns NULL, oldPtr is still valid
                return NULL; 
            }
            // Update our record with the new pointer/size
            curr->ptr  = newPtr;
            curr->size = newSize;
            // Return the new pointer
            return newPtr;
        }
        prev = curr;
        curr = curr->next;
    }

    // If we never found oldPtr in the list, warn or handle it.
    fprintf(stderr, "Warning: Attempt to realloc unknown pointer %p\n", oldPtr);
    return realloc(oldPtr, newSize); // fallback
}

void debug_free(void *ptr, const char *file, int line)
{
    (void)file; /* Not used here, but we could log it if we want. */
    (void)line;

    /* If ptr is NULL, just ignore. */
    if (!ptr) {
        return;
    }

    /* Find the allocation in our list to remove it. */
    Allocation **curr = &g_allocations;
    while (*curr) {
        if ((*curr)->ptr == ptr) {
            /* Found the matching allocation. */
            Allocation *toRemove = *curr;
            *curr = (*curr)->next; /* unlink from list */
            
            /* Free our Allocation metadata. */
            free(toRemove);

            /* Now free the actual data pointer. */
            free(ptr);
            return;
        }
        curr = &((*curr)->next);
    }

    /* If we get here, 'ptr' wasn't in the list.
       Possibly a double-free or free of a pointer not allocated by debug_malloc. */
    fprintf(stderr, "Warning: Attempt to free unknown pointer %p\n", ptr);
}

/* Writes info about any still-allocated pointers to the provided FILE* */
void log_memory_leaks(FILE *out)
{
    Allocation *curr = g_allocations;
    if (!curr) {
        fprintf(out, "No memory leaks detected!\n");
        return;
    }
    
    fprintf(out, "Potential memory leaks detected:\n");
    while (curr) {
        fprintf(out, 
                " Leak: %zu bytes at %p, allocated from %s:%d\n",
                curr->size,
                curr->ptr,
                curr->file,
                curr->line);
        curr = curr->next;
    }
}

