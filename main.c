#include <assert.h>
#include <iso646.h> // Parses and -> &&
#include <stdint.h> // To use uintptr_t to hold the memory address of a a pointer regardless of the architecture
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>
// of the system

// ===Macros===
#define META_SIZE sizeof(struct block_meta)
#define MIN_SIZE 8 // For splitting blocks

// === Variables and structs ===
struct block_meta {
  size_t size;
  struct block_meta *next;
  int free;
  int magic; // for debugging
};
// Head of the linked list to track all allocated blocks.
void *global_base = NULL;

// === Functions ===

// Malloc function
struct block_meta *find_free_block(struct block_meta **last, size_t size);
struct block_meta *request_space(struct block_meta *last, size_t size);
void *malloc(size_t size);

// Free function
void free(void *ptr);
struct block_meta *get_block_addr(void *ptr);
void merge_free_blocks(struct block_meta *head);

// Realloc function
void *realloc(void *ptr, size_t size);
void shrink_and_split(struct block_meta *block, size_t size);
struct block_meta *expand_in_place(void *ptr, size_t size);

// == Debugging ==
void print_heap(void);
void debug_heap(void);

int main() {

  // ===For debugging only===
  // int *memory = (int *)malloc(sizeof(int) * 4);
  // if (!memory) {
  //   printf("Error");
  //   return 1;
  // }
  //
  // for (int i = 0; i < 4; i++) {
  //   memory[i] = i;
  //   printf("%d\n", memory[i]);
  // }
  //
  // struct block_meta *block_addr = (struct block_meta *)memory - 1;
  // free(memory);
  //
  // printf("block -> free = %d\n", block_addr->free);
  // printf("block -> magic = %x", block_addr->magic);

  printf("=== Custom malloc/free/realloc Debugging ===\n");

  // === Test 1: Simple malloc ===
  printf("\n--- Test 1: Simple malloc ---\n");
  int *a = (int *)malloc(5 * sizeof(int));
  for (int i = 0; i < 5; i++)
    a[i] = i;
  debug_heap();

  // === Test 2: Allocate more blocks ===
  printf("\n--- Test 2: Allocate additional blocks ---\n");
  int *b = (int *)malloc(3 * sizeof(int));
  int *c = (int *)malloc(4 * sizeof(int));
  debug_heap();

  // === Test 3: Free middle block (b) ===
  printf("\n--- Test 3: Free middle block (b) ---\n");
  free(b);
  debug_heap();

  // === Test 4: Allocate smaller block to reuse freed space ===
  printf("\n--- Test 4: Allocate smaller block (d) ---\n");
  int *d = (int *)malloc(2 * sizeof(int));
  debug_heap();

  // === Test 5: Shrink a block with realloc (split leftover) ===
  printf("\n--- Test 5: Shrink block 'a' ---\n");
  int *tmp = (int *)realloc(a, 3 * sizeof(int)); // shrink
  if (tmp)
    a = tmp;
  debug_heap();

  // === Test 6: Expand a block with realloc (expand in place if possible) ===
  printf("\n--- Test 6: Expand block 'a' ---\n");
  tmp = (int *)realloc(a, 8 * sizeof(int)); // expand
  if (tmp)
    a = tmp;
  debug_heap();

  // === Test 7: realloc NULL pointer (acts like malloc) ===
  printf("\n--- Test 7: realloc NULL (like malloc) ---\n");
  int *e = (int *)realloc(NULL, 4 * sizeof(int));
  if (e) {
    for (int i = 0; i < 4; i++)
      e[i] = i * 2;
  }
  debug_heap();

  // === Test 8: realloc size 0 (acts like free) ===
  printf("\n--- Test 8: realloc size 0 (free 'd') ---\n");
  tmp = (int *)realloc(d, 0); // free
  if (!tmp)
    d = NULL;
  debug_heap();

  // === Test 9: Free all remaining blocks ===
  printf("\n--- Test 9: Free remaining blocks (a, c, e) ---\n");
  free(a);
  debug_heap();

  free(c);
  debug_heap();

  free(e);
  debug_heap();

  printf("\n=== Debugging complete ===\n");

  struct block_meta *curr = (struct block_meta *)global_base;
  printf("\n=== Heap State ===\n");
  while (curr) {
    printf("Block %p | size=%zu | free=%d | magic=0x%x | next=%p\n",
           (void *)curr, curr->size, curr->free, curr->magic,
           (void *)curr->next);
    curr = curr->next;
  }
}

struct block_meta *find_free_block(struct block_meta **last, size_t size) {
  struct block_meta *current = global_base;
  while (current && !(current->free && current->size >= size)) {
    *last = current;
    current = current->next;
  }
  return current;
}

struct block_meta *request_space(struct block_meta *last, size_t size) {
  struct block_meta *block;
  block = sbrk(0);

  void *request = sbrk(size + META_SIZE);
  assert((void *)block == request); // not thread safe

  if (request == (void *)-1) {
    return NULL; // sbrk failed
  }

  if (last) {
    last->next = block;
  }

  block->free = 0;
  block->size = size;
  block->next = NULL;
  block->magic = 0x12345678;

  return block;
}

void *malloc(size_t size) {
  struct block_meta *block;

  if (size <= 0) {
    return NULL;
  }

  // To fix allignment
  size = (size + 7) & ~7;

  if (!global_base) {
    block = request_space(NULL, size);
    if (!block) {
      return NULL;
    }
    global_base = block;
  } else {
    struct block_meta *last = NULL;
    block = find_free_block(&last, size);
    if (!block) {
      block = request_space(last, size);
      if (!block) {
        return NULL;
      }
    } else {
      // We found the block
      // Split it if it's large enough
      if (block->size >= size + META_SIZE + MIN_SIZE) {
        size_t remaining_size = block->size - (size + META_SIZE);
        block->size = size;

        struct block_meta *new_block =
            (struct block_meta *)((char *)block + META_SIZE + size);

        new_block->size = remaining_size;
        new_block->free = 1;
        new_block->magic = 0x22222222;

        new_block->next = block->next;
        block->next = new_block;
      }
      block->free = 0;
      block->magic = 0x77777777;
    }
  }
  return (block + 1);
}

void merge_free_blocks(struct block_meta *head) {
  struct block_meta *current = head;

  while (current && current->next) {
    struct block_meta *next = current->next;

    if (current->free && next->free &&
        ((char *)current + META_SIZE + current->size == (char *)next)) {

      // merge next into current
      current->size += META_SIZE + next->size;
      current->next = next->next; // skip over merged block

      // do NOT advance current here; the merged block might merge again
    } else {
      // move forward only if no merge happened
      current = current->next;
    }
  }
}

struct block_meta *get_block_addr(void *ptr) {
  return (struct block_meta *)ptr - 1;
}

void free(void *ptr) {
  struct block_meta *block = get_block_addr(ptr);

  if (!block) {
    fprintf(stderr, "Error : block not found");
    return;
  }

  assert(block->free == 0);
  assert(block->magic == 0x77777777 || block->magic == 0x12345678);

  block->free = 1;
  block->magic = 0x55555555;

  // After freeing we merge free blocks
  merge_free_blocks(global_base);
}

void print_heap(void) {
  struct block_meta *cur = (struct block_meta *)global_base;
  printf("\n[HEAP DUMP]\n");
  while (cur) {
    printf("meta=%p | size=%6zu | free=%d | magic=0x%x | next=%p\n",
           (void *)cur, cur->size, cur->free, cur->magic, (void *)cur->next);
    cur = cur->next;
  }
  printf("------------\n");
}

void shrink_and_split(struct block_meta *block, size_t size) {
  size_t old_size = block->size;
  block->size = size;

  size_t leftover = old_size - size - META_SIZE;

  if (leftover >= MIN_SIZE) {
    struct block_meta *new_block =
        (struct block_meta *)((char *)block + META_SIZE + block->size);
    new_block->size = leftover;
    new_block->free = 1;
    new_block->magic = 0x55555555;

    // insert right after the curent block
    new_block->next = block->next;
    block->next = new_block;
  }
}

struct block_meta *expand_in_place(void *ptr, size_t size) {
  struct block_meta *block = get_block_addr(ptr);
  void *start_address = (char *)block + META_SIZE + block->size;
  // traverse the linked list and find free adjacent blocks

  struct block_meta *target = global_base;
  while (target) {
    if ((void *)target == start_address) {
      break;
    }
    target = target->next;
  }

  if (!target || target->free == 0) {
    return NULL;
  }

  if (block->size + META_SIZE + target->size >= size) {

    // int new_size = size - META_SIZE;
    // if(new_size >= 0) block -> size = new_size;
    //  This was an optimization attempt but i guess it's better to
    //  give a bit more than the user wanted so let's move on

    target->free = 0;
    target->magic = 0x12345678;

    size_t remaining =
        (block->size + target->size + META_SIZE) - (size + META_SIZE);
    block->size = size;

    struct block_meta *next_target = target->next;

    // Check if there's remaining space
    if (remaining > MIN_SIZE + META_SIZE) {
      struct block_meta *new_target_block =
          (struct block_meta *)((char *)block + META_SIZE + block->size);
      new_target_block->size = remaining;
      new_target_block->free = 1;
      new_target_block->next = next_target;
      new_target_block->magic = 0x5555555;
      block->next = new_target_block;
    } else {
      block->next = target->next;
    }
  }
  return block + 1;
}

void *realloc(void *ptr, size_t size) {
  if (!ptr) {
    return malloc(size);
  }

  if (size == 0) {
    free(ptr);
    return NULL;
  }

  struct block_meta *myblock = (struct block_meta *)ptr - 1;

  // if size == block -> size : do nothing
  if (size == myblock->size) {
    return myblock + 1;
  }

  // if size < block -> size : shrink and split

  /* if size > block -> size
   * 1. Check if there's a free adjacent block and expand in place if
   * possible. Otherwise:
   * 1. free this block
   * 2. allocate new block with the new size
   * 3. copy the content to the new block
   */

  if (myblock->size > size + META_SIZE + MIN_SIZE) {
    shrink_and_split(myblock, size);
    return myblock + 1;
  } else if (size > myblock->size) { // equivalent to elif
    void *new_block = expand_in_place(ptr, size);
    if (!new_block) {
      void *new_block = malloc(size);
      memcpy(new_block, ptr, myblock->size);
      free(ptr);
      return new_block;
    } else {
      return new_block;
    }
  }
  return ptr;
}

void debug_heap(void) {
  struct block_meta *curr = (struct block_meta *)global_base;
  printf("\n[HEAP DEBUG DUMP]\n");
  printf("%-20s %-8s %-6s %-10s %-20s %-10s\n", "Block Addr", "Size", "Free",
         "Magic", "Next", "End Addr");

  while (curr) {
    void *user_start = (void *)(curr + 1);
    void *user_end = (char *)user_start + curr->size;

    printf("%-20p %-8zu %-6d 0x%-8x %-20p %-10p\n", (void *)curr, curr->size,
           curr->free, curr->magic, (void *)curr->next, user_end);

    curr = curr->next;
  }
  printf("-------------------------------------------------------------\n");
}
