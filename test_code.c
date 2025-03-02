#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "leak_tracker.h"
/* 
 * Because of the macros in leak_tracker.h,
 * any call to malloc/realloc/free in this file actually
 * calls debug_malloc/debug_realloc/debug_free behind the scenes.
 */
#define INITIAL_CAPACITY 4

/* A simple resizable list of C-strings. */
typedef struct {
    char **data;
    size_t size;
    size_t capacity;
} StringList;

/* Initializes a StringList structure. */
void initStringList(StringList *list) {
    list->data = malloc(INITIAL_CAPACITY * sizeof(char*));
    if (!list->data) {
        fprintf(stderr, "Allocation failed\n");
        return;
    }
    list->size = 0;
    list->capacity = INITIAL_CAPACITY;
}

/* Frees all memory used by the list (and the list's strings). */
void freeStringList(StringList *list) {
    if (list->data) {
        for (size_t i = 0; i < list->size; i++) {
            free(list->data[i]);
        }
        free(list->data);
    }
    list->data = NULL;
    list->size = 0;
    list->capacity = 0;
}

/* Adds a copy of 'str' to the end of the list, resizing if necessary. */
void addString(StringList *list, const char *str) {
    if (list->size >= list->capacity) {
        size_t newCapacity = list->capacity * 2;
        char **newData = realloc(list->data, newCapacity * sizeof(char*));
        if (!newData) {
            fprintf(stderr, "Reallocation failed\n");
            return;
        }
        list->data = newData;
        list->capacity = newCapacity;
    }

    size_t len = strlen(str);
    char *copy = malloc(len + 1);
    if (copy) {
        strcpy(copy, str);
        list->data[list->size] = copy;
        list->size++;
    }
    else {
        fprintf(stderr, "Allocation failed\n");
    }
}

/* Removes the string at the specified index, shifting subsequent elements. */
void removeString(StringList *list, size_t index) {
    if (index < list->size) {
        free(list->data[index]);
        for (size_t i = index; i < list->size - 1; i++) {
            list->data[i] = list->data[i + 1];
        }
        list->size--;
    }
}

/* Finds the first occurrence of 'str' in the list; returns its index or -1 if not found. */
int findString(const StringList *list, const char *str) {
    for (size_t i = 0; i < list->size; i++) {
        if (strcmp(list->data[i], str) == 0) {
            return (int)i;
        }
    }
    return -1;
}

/* Prints all strings in the list with their indices. */
void printStringList(const StringList *list) {
    for (size_t i = 0; i < list->size; i++) { // this was a BUG!
        printf("%zu: %s\n", i, list->data[i]);
    }
}

char* getUserInput(void) {
    size_t bufSize = 256;
    char *buffer = malloc(bufSize);
    if (!buffer) {
        return NULL;  // Memory allocation failed
    }
    
    printf("Enter a string: ");
    if (fgets(buffer, bufSize, stdin)) {
        size_t len = strlen(buffer);
        if (len > 0 && buffer[len - 1] == '\n') {
            buffer[len - 1] = '\0';
        }
        return buffer; 
    }
    
    free(buffer);  // Clean up if fgets fails
    return NULL;
}

int main(void) {
    StringList list;
    initStringList(&list);

    addString(&list, "Alice");
    addString(&list, "Bob");
    addString(&list, "Charlie");
    addString(&list, "Dennis");
    addString(&list, "Eve");
    addString(&list, "Frank");

    printf("Initial list:\n");
    printStringList(&list);

    removeString(&list, 1);
    printf("\nAfter removing index 1:\n");
    printStringList(&list);

    int idx = findString(&list, "Charlie");
    if (idx >= 0) {
        printf("\nFound 'Charlie' at index %d\n", idx);
    }

    char *userStr = getUserInput();
    if (userStr) {
        addString(&list, userStr);
        printf("\nList after adding user input:\n");
        printStringList(&list);
        free(userStr);  // Clean up the allocated memory
    }

    // find "Mauri" in the list
    idx = findString(&list, "Mauri");
    if (idx >= 0) {
        printf("\nFound 'Mauri' at index %d\n", idx);
    }

    // remove index 0
    removeString(&list, 0);
    printf("\nAfter removing index 0:\n");
    printStringList(&list);

    freeStringList(&list);

    log_memory_leaks(stdout);

    return 0;
}

// run main
// gcc -o test_code test_code.c
// gcc -o test_code test_code.c leak_tracker.c
// gcc -Wall -Wextra -o test_code test_code.c leak_tracker.c

