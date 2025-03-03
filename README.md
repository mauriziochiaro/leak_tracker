# Leak Tracker

A simple ~400 lines memory leak tracker for C programs.
This project provides a custom memory allocator to track memory leaks. It replaces standard `malloc`, `realloc`, and `free` functions with debug versions that log allocations and deallocations, helping identify memory leaks in C programs.
Just Include `leak_tracker.h` in your C source files to override standard memory allocation functions.

## Example Log

```bash
==== Memory Leak Check ====
Potential leaks:
----------------------------------------------------
----------------------------------------------------
  Pointer            Size     Location
----------------------------------------------------
  0000028F5BD1FF58        5   test_code.c:165
  0000028F5BD21898      256   test_code.c:99

==== Memory Statistics ====
  Current In-Use:  261 bytes
  Total Allocated: 366 bytes (cumulative)
  Peak In-Use:     357 bytes
  Active Blocks:   2
```

## Build Instructions

```bash
gcc -Wall -Wextra -o test_code test_code.c leak_tracker.c
```

## Usage

After building, run in VSCode Terminal:

```bash
./test_code
```

Or, right click on main folder, open with Terminal and
```bash
.\test_code.exe
```

## License

This project is released under the BSD 2-Clause License. 