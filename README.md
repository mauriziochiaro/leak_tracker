# Memory Leak Debug Tests

A small C project demonstrating:
1. A **dynamic string list** (with basic CRUD operations).
2. A **custom memory-leak tracker** that replaces `malloc`/`realloc`/`free` with debug versions.  
3. A **test program** (`test_code.c`) that exercises the string list and triggers the debug logic.

## Table of Contents
- [Overview](#overview)
- [Project Structure](#project-structure)
- [Build Instructions](#build-instructions)
- [Usage](#usage)
- [Memory Tracking Logic](#memory-tracking-logic)
- [Known Limitations](#known-limitations)
- [License](#license)

## Overview

This project shows how to:
- Manage a simple dynamically resizable array of strings.
- Detect and log potential memory leaks at runtime.
- Demonstrate best practices in avoiding common pitfalls (like returning local buffers, or out-of-bounds array access).

**Goals**:
- Provide a working example for debugging memory usage in C.
- Offer a “drop-in” debug memory-tracking approach when you can’t or don’t want to use external tools (e.g., Valgrind, AddressSanitizer).

## Project Structure

```
.
├── README.md               # This readme file
├── leak_tracker.h          # Declarations & macros for debug_malloc/free/realloc
├── leak_tracker.c          # Implementation of debug_malloc/free/realloc, logging
├── test_code.c             # Main test program with a dynamic StringList
└── Makefile                # (Optional) For building the project
```

### Files

1. **`test_code.c`**  
   - Demonstrates usage of a `StringList` (init, add, remove, find, print).
   - Uses the function `getUserInput` to grab user-entered strings.
   - Calls `log_memory_leaks(stdout)` at the end to show any unfreed allocations.

2. **`leak_tracker.h`**  
   - Includes standard headers first, then defines macros:
     ```c
     #define malloc(size)  debug_malloc(size, __FILE__, __LINE__)
     #define realloc(ptr, size) debug_realloc((ptr), (size), __FILE__, __LINE__)
     #define free(ptr)     debug_free(ptr, __FILE__, __LINE__)
     // optionally #define realloc ...
     ```
   - Declares `debug_malloc`, `debug_realloc`, `debug_free`, and `log_memory_leaks`.

3. **`leak_tracker.c`**  
   - Implements `debug_malloc`, `debug_realloc`, `debug_free`, and an internal singly linked list to track allocations.
   - At the end of the program, `log_memory_leaks(...)` prints any pointers still tracked.

## Build Instructions

You can compile this with a **single** command if you prefer:

```bash
gcc -Wall -Wextra -o test_code test_code.c leak_tracker.c
```

Or you can use a **Makefile** (if provided) by running:

```bash
make
```

This produces an executable named `test_code`.

> **Important**: The order of compilation/linking must include both `test_code.c` and `leak_tracker.c` to satisfy references to `debug_malloc`, `debug_realloc` and `debug_free`.

## Usage

After building:

```bash
./test_code
```

You’ll see:
1. The initial list of strings (`Alice`, `Bob`, etc.).
2. Removal of index 1.
3. The location of “Charlie” in the list (if found).
4. A prompt for a user-entered string – that string gets added to the list.
5. A final removal (index 0).
6. A summary of any memory leaks, printed to `stdout`.

If you see something like:
```
Warning: Attempt to free unknown pointer 0x... 
Potential memory leaks detected:
 Leak: 32 bytes at 0x..., allocated from test_code.c:...
```
It indicates a pointer was allocated under the debug system but freed in a way that bypassed the debug tracker, or not freed.

## Memory Tracking Logic

1. **Macro Overrides**  
   - The line `#define malloc(size) debug_malloc(size, __FILE__, __LINE__)` replaces every `malloc` call in your code *after* the header is included.  
   - `debug_malloc` calls the real `malloc` internally, but also stores metadata (pointer address, size, file, and line) in a global linked list.

2. **Linked List of Allocations**  
   - Each `debug_malloc` adds an entry for the new pointer.  
   - `debug_free` searches for that pointer in the list, removes the record, and calls the real `free`.  
   - If a pointer is missing from the list, you get a `Warning: Attempt to free unknown pointer ...`.

3. **Logging Leaks**  
   - At program exit, `log_memory_leaks(stdout)` traverses the linked list. Any pointer not freed yet gets printed as a potential leak.  
   - You can optionally log to a file by calling:
     ```c
     FILE *fp = fopen("leaks.log", "w");
     if (fp) {
         log_memory_leaks(fp);
         fclose(fp);
     }
     ```

## Known Limitations

- **Not Thread-Safe**: The tracking code does not lock any mutexes when modifying the global list of allocations. In multi-threaded code, you’d need synchronization.  
- **System Headers**: If you include `<stdlib.h>` or `<malloc.h>` *after* you define these macros, the compiler can expand the system prototypes as macros and cause build errors. That’s why `leak_tracker.h` includes system headers first.  
- **No Bound Checking**: This only tracks allocated pointers, not out-of-bounds writes. For deeper memory debugging, try Valgrind or AddressSanitizer.  

## License

*(If you want to specify a license, include it here. For a private repo, you may simply mark “All Rights Reserved” or any other licensing terms.)*
