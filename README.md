# Custom Memory Allocator in C

A **simple memory allocator** implemented in C that mimics `malloc`, `free`, and `realloc`.
It demonstrates **heap management**, **linked-list-based free block tracking**, **splitting**, **merging**, and **in-place expansion**.

---

## Table of Contents

* [Overview](#overview)
* [Features](#features)
* [Implementation Details](#implementation-details)
* [Usage](#usage)
* [Testing](#testing)
* [Debugging](#debugging)
* [Limitations](#limitations)
* [Contributing](#contributing)

---

## Overview

This project is a **learning exercise** in low-level memory management.
It replaces standard library allocation functions (`malloc`, `free`, `realloc`) with a **custom allocator** that manages a contiguous heap using `sbrk` and a linked list of metadata blocks.

---

## Features

* `malloc(size_t size)` – allocate memory blocks of given size
* `free(void *ptr)` – free allocated memory and merge adjacent free blocks
* `realloc(void *ptr, size_t size)` – resize a previously allocated block
* Block **splitting** for smaller allocations
* **In-place expansion** if an adjacent free block exists
* Debugging functions to **print the heap state**

---

## Implementation Details

* **Data Structure**:
  Each allocated block has a `struct block_meta` storing:

  ```c
  struct block_meta {
      size_t size;          // Size of the data section
      struct block_meta *next; // Next block in linked list
      int free;             // 1 if block is free, 0 if allocated
      int magic;            // Debugging value
  };
  ```
* **Heap Management**:

  * `sbrk` is used to request memory from the OS.
  * Blocks are tracked in a **linked list** starting from `global_base`.
* **Allocation Strategy**:

  * First, search for a free block big enough.
  * If none found, request space from the OS.
  * Split blocks if excess memory exists.
* **Reallocation**:

  * Shrinks or splits blocks if the new size is smaller.
  * Expands in place if adjacent free block exists.
  * Otherwise, allocates a new block, copies data, and frees the old block.

---

## Usage

Include your custom allocator header and call functions like standard library:

```c
int *arr = (int *)malloc(10 * sizeof(int));
arr = (int *)realloc(arr, 20 * sizeof(int));
free(arr);
```

---

## Testing

A full **debugging main** is included that tests:

1. Simple allocation
2. Multiple allocations
3. Freeing middle blocks
4. Reusing free blocks
5. Shrinking blocks
6. Expanding blocks in place
7. Realloc with `NULL` pointer (like malloc)
8. Realloc with size 0 (like free)
9. Merging of adjacent free blocks

Run the program and it prints a **heap dump** after each test to visualize memory layout.

---

## Debugging

Use `print_heap()` to print:

```
Block Addr | Size | Free | Magic | Next
```

* `Addr` – metadata start address
* `Size` – data section size
* `Free` – 1 if free, 0 if allocated
* `Magic` – for debugging block allocation state
* `Next` – pointer to next block

---

## Limitations

* **Single-threaded only**: not safe for concurrent allocations.
* **Alignment issues**: assumes 8-byte alignment.
* **No best-fit or segregated lists**: simple first-fit search only.
* **No memory release back to OS**: uses sbrk only to grow heap.

---

## Contributing

* Fork the repository and submit PRs for improvements:

  * Thread safety (mutexes)
  * Best-fit or segregated free lists
  * Memory release to OS (`brk`)
  * Advanced debugging visualization

---

## License

This project is released under the **MIT License**.

## Next Steps / TODOs

While the current implementation is fully functional, there are several planned improvements and optimizations:

- **Implement `mmap` support**:  
  Allocate large blocks of memory directly from the OS using `mmap` instead of `sbrk` for better performance and flexibility.

- **Best-fit allocation strategy**:  
  Currently, the allocator uses a first-fit strategy. Implementing best-fit can reduce fragmentation by choosing the smallest free block that fits the requested size.

- **Segregated free lists**:  
  Maintain multiple free lists for different block sizes to speed up allocation and reduce search time.

- **Thread safety / concurrency**:  
  Add mutexes or other synchronization mechanisms to make the allocator safe for multi-threaded programs.

- **New functions**:
  Implement the calloc function
- **Possibly implement a garbage collector with reference counting**
These improvements are planned for future versions, but the current project already demonstrates a functional custom memory allocator with malloc, free, realloc, splitting, and merging.

