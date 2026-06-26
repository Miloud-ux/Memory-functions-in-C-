# Garbage Collector & Memory Functions in C

> A single-file implementation of a custom memory allocator with a conservative mark-and-sweep garbage collector for Linux.

[![C](https://img.shields.io/badge/C-99-blue.svg)](https://en.wikipedia.org/wiki/C99)
[![Linux](https://img.shields.io/badge/Linux-x86__64%20%7C%20i386-green.svg)]()
[![License](https://img.shields.io/badge/License-MIT-yellow.svg)](LICENSE)

---

## Table of Contents

- [Overview](#overview)
- [Features](#features)
- [Architecture](#architecture)
- [Build & Run](#build--run)
- [API Reference](#api-reference)
- [How It Works](#how-it-works)
- [Limitations](#limitations)
- [License](#license)

---

## Overview

This project is a **single-file C implementation** (`main.c`) that provides:

1. **A custom memory allocator** — drop-in replacements for `malloc`, `free`, and `realloc` using `sbrk()` with block metadata, free-list reuse, block splitting, and adjacent block merging.
2. **A conservative mark-and-sweep garbage collector** — automatically reclaims unreachable heap memory by scanning the stack, data segment, and heap transitively for live pointers.

It was built as an educational exercise to understand how memory allocators and garbage collectors work under the hood, without relying on the standard C library's heap management.

---

## Features

- **Custom `malloc` / `free` / `realloc`**
  - Built on `sbrk()` for raw heap expansion
  - First-fit free-list allocation with block reuse
  - Block splitting: large free blocks are split to reduce internal fragmentation
  - Block merging: adjacent free blocks are coalesced on `free()`

- **Conservative Mark-and-Sweep GC**
  - **Mark phase**: Scans the stack (via inline assembly), data segment (`.bss`/`.data`), and heap transitively to find reachable pointers
  - **Sweep phase**: Unmarked, allocated blocks are automatically freed
  - **GC roots**: Stack variables, global/static data, and pointers inside live heap blocks

- **Debugging & Diagnostics**
  - `print_gc_stats()` — live count of allocated vs. free blocks
  - `debug_heap()` — formatted heap dump showing all blocks with metadata
  - Magic numbers (`0x12345678`, `0x77777777`, etc.) for heap corruption detection

- **Single File** — Everything is in `main.c`. No build system needed.

---

## Architecture

```
┌─────────────────────────────────────────────────────────────┐
│                     Linux Process Memory                     │
│                                                              │
│  ┌──────────────┐  ┌──────────────┐  ┌──────────────────┐   │
│  │   .text      │  │  .data/.bss  │  │      Stack       │   │
│  │  (code)      │  │  (globals)   │  │   (local vars)   │   │
│  └──────────────┘  └──────┬───────┘  └────────┬─────────┘   │
│                           │                    │             │
│                           └────────────────────┘             │
│                                              │               │
│                           ┌──────────────────┼──────────┐    │
│                           │   GC Scan Roots  │          │    │
│                           │  (mark phase)    │          │    │
│                           └────────┬─────────┘          │    │
│                                    │                    │    │
│                           ┌────────▼─────────┐          │    │
│                           │   Heap (sbrk)    │          │    │
│                           │  ┌────────────┐  │          │    │
│                           │  │ block_meta │  │◄─────────┘    │
│                           │  │  (header)  │  │               │
│                           │  └────────────┘  │               │
│                           │  ┌────────────┐  │               │
│                           │  │   data     │  │               │
│                           │  └────────────┘  │               │
│                           └──────────────────┘               │
└─────────────────────────────────────────────────────────────┘
```

### Block Metadata

Each heap allocation is preceded by a `struct block_meta` header:

```c
struct block_meta {
    size_t size;           // Payload size in bytes
    struct block_meta *next; // Next block in the linked list
    int free;              // 1 = free, 0 = allocated
    int marked;            // GC mark flag (1 = reachable)
    int magic;             // Debug/corruption sentinel
};
```

---

## Build & Run

### Requirements

- Linux (x86_64 or i386)
- GCC or Clang
- `/proc/self/stat` must be readable (for stack bottom detection)

### Compile

```bash
gcc -o gc_demo main.c
```

### Run

```bash
./gc_demo
```

### Expected Output

```
===============================================
  GARBAGE COLLECTOR - DEMONSTRATION
===============================================

✓ GC Initialized (Stack bottom: 0x7ffd...

--- Test 1: Basic Allocation ---
Allocated 3 blocks
  [Allocated: 3 blocks | Free: 0 blocks]
Freed middle block
  [Allocated: 2 blocks | Free: 1 blocks]
Freed remaining blocks
  [Allocated: 0 blocks | Free: 2 blocks]
✓ Test 1 passed

--- Test 2: Garbage Collection ---
Before GC:
  [Allocated: 2 blocks | Free: 0 blocks]
Made one block unreachable
After GC:
  [Allocated: 1 blocks | Free: 1 blocks]
✓ Test 2 passed

--- Test 3: Multiple Unreachable Blocks ---
Allocated 4 blocks
  [Allocated: 4 blocks | Free: 0 blocks]
Made 3 blocks unreachable
After GC (should collect 3 blocks):
  [Allocated: 1 blocks | Free: 3 blocks]

[HEAP DUMP]
Address            Size     Free   Marked   Magic
0x...              200      0      1        0x77777777
0x...              240      1      0        0x55555555
...
----------------------------------------
✓ Test 3 passed

===============================================
  ALL TESTS COMPLETED SUCCESSFULLY!
===============================================
```

---

## API Reference

### Memory Allocation

| Function | Signature | Description |
|----------|-----------|-------------|
| `malloc` | `void *malloc(size_t size)` | Allocate memory. Returns 8-byte aligned payload. Reuses free blocks via first-fit. Splits large free blocks. |
| `free` | `void free(void *ptr)` | Deallocate memory. Sets block as free, clears mark, and merges with adjacent free neighbors. |
| `realloc` | `void *realloc(void *ptr, size_t size)` | Resize allocation. Copies data to a new block if the current one is too small. |

### Garbage Collection

| Function | Signature | Description |
|----------|-----------|-------------|
| `gc_init` | `void gc_init(void)` | One-time initialization. Reads `/proc/self/stat` to find the stack bottom address. |
| `gc` | `void gc(void)` | Run a full mark-and-sweep cycle. Unmarks all blocks, scans roots, marks reachable blocks, then frees unmarked allocated blocks. |

### Diagnostics

| Function | Signature | Description |
|----------|-----------|-------------|
| `print_gc_stats` | `void print_gc_stats(void)` | Print a one-line summary of allocated vs. free blocks. |
| `debug_heap` | `void debug_heap(void)` | Print a formatted table of all heap blocks (address, size, free, marked, magic). Stops after 20 blocks to prevent spam. |
| `count_allocated_blocks` | `int count_allocated_blocks(void)` | Return the number of currently allocated (non-free) blocks. |
| `count_free_blocks` | `int count_free_blocks(void)` | Return the number of free blocks in the heap. |

### Internal Helpers

| Function | Signature | Description |
|----------|-----------|-------------|
| `find_free_block` | `struct block_meta *find_free_block(...)` | First-fit search for a free block large enough for the requested size. |
| `request_space` | `struct block_meta *request_space(...)` | Expand the heap via `sbrk()` and initialize a new block header. |
| `merge_free_blocks` | `void merge_free_blocks(struct block_meta *head)` | Coalesce adjacent free blocks into a single larger free block. |
| `scan_region` | `static void scan_region(...)` | Conservative pointer scan over a memory region (stack or data segment). |
| `scan_heap` | `static void scan_heap(void)` | Transitively mark heap blocks that are pointed to by already-marked heap blocks. |

---

## How It Works

### 1. Custom Allocator (`malloc` / `free` / `realloc`)

- **Heap expansion**: When no suitable free block exists, `sbrk()` is called to grow the program break.
- **First-fit**: The allocator walks the linked list and returns the first free block that fits.
- **Block splitting**: If a free block is larger than needed by at least `META_SIZE + MIN_SIZE` (16 bytes), the remainder is split into a new free block.
- **Block merging**: On `free()`, adjacent free blocks are coalesced to reduce fragmentation.
- **Alignment**: All requested sizes are rounded up to the nearest 8-byte boundary.

### 2. Garbage Collection (`gc()`)

The collector performs three phases:

#### Phase 1: Clear Marks
All blocks have their `marked` flag reset to `0`.

#### Phase 2: Mark Roots
Three memory regions are scanned conservatively for values that look like pointers into the heap:

| Region | Range | Method |
|--------|-------|--------|
| **Data segment** | `&etext` → `&end` | Global/static variables (`.data` and `.bss`) |
| **Stack** | `RBP/EBP` → `stack_bottom` | Inline assembly reads the frame pointer; stack bottom from `/proc/self/stat` |
| **Heap (transitive)** | All marked blocks | Iteratively scan payload of marked blocks for pointers to other blocks |

A value is treated as a pointer if it falls within the heap range and points inside a known block's payload. When found, that block is marked.

#### Phase 3: Sweep
All blocks are scanned. Any block that is **allocated** (`free == 0`) but **unmarked** (`marked == 0`) is automatically freed.

### 3. Magic Numbers

| Value | Meaning |
|-------|---------|
| `0x12345678` | Fresh block from `sbrk()` |
| `0x77777777` | Allocated block (active) |
| `0x22222222` | Split remainder block (free) |
| `0x55555555` | Freed block (swept or manually freed) |

`debug_heap()` uses these to detect corruption.

---

## Limitations

⚠️ **Important — read before using in production**

1. **Linux only**: Uses `/proc/self/stat` and inline x86 assembly (`RBP`/`EBP`). Will not compile or run on macOS, Windows, or non-x86 architectures without modification.

2. **Not thread-safe**: No locking or atomic operations. Do not use in multi-threaded programs.

3. **Overrides standard functions**: This file defines `malloc`, `free`, and `realloc` directly. If linked with other code that also defines them (or uses the real libc versions internally), you will get symbol conflicts or undefined behavior.

4. **Conservative GC**: The collector treats any word-aligned value that looks like a heap pointer as a reference. This means:
   - Random integers may accidentally keep blocks alive (false positives).
   - Pointers that are XORed, encrypted, or stored in non-standard ways will be missed.

5. **No `calloc` or `aligned_alloc`**: Only `malloc`, `free`, and `realloc` are implemented.

6. **No `sbrk` shrink**: The heap never shrinks back to the OS; freed memory is kept in the process for reuse.

7. **Stack scanning assumptions**: Assumes the stack grows downward and that the frame pointer chain is intact. Compiler optimizations like `-fomit-frame-pointer` may affect accuracy.

---

## Project Structure

```
.
└── main.c          ← Everything is here (allocator + GC + demo)
```

No `Makefile`, no headers, no dependencies. Just compile and run.

---

## License

This project is released under the **MIT License**.

---

## Author

**Miloud-ux** — [github.com/Miloud-ux](https://github.com/Miloud-ux)

> *"Understanding memory management is the gateway to understanding systems programming."*
