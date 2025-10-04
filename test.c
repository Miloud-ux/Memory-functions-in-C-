#include <assert.h>
#include <iso646.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>

// ===Macros===
#define META_SIZE sizeof(struct block_meta)
#define MIN_SIZE 8

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

// == Debugging ==
void print_heap(void);

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

  printf("=== Test 1: Simple allocation ===\n");
  int *a = (int *)malloc(10 * sizeof(int));
  for (int i = 0; i < 10; i++)
    a[i] = i;
  for (int i = 0; i < 10; i++)
    printf("%d ", a[i]);
  printf("\n");

  printf("\n=== Test 2: Allocate more blocks ===\n");
  int *b = (int *)malloc(5 * sizeof(int));
  int *c = (int *)malloc(8 * sizeof(int));
  printf("Allocated three blocks: a=%p, b=%p, c=%p\n", (void *)a, (void *)b,
         (void *)c);

  printf("\n=== Test 3: Free the middle block (b) ===\n");
  free(b);
  print_heap();
  printf("Freed block b\n");

  printf(
      "\n=== Test 4: Allocate a smaller block (should reuse or split b) ===\n");
  int *d = (int *)malloc(3 * sizeof(int));
  printf("Allocated d=%p\n", (void *)d);

  printf("\n=== Test 5: Free all blocks (a, c, d) ===\n");

  free(a);
  print_heap();

  free(c);
  print_heap();

  free(d);
  print_heap();

  printf("Freed all\n");

  printf("\n=== Test 6: Trigger merge ===\n");
  // This happens inside free() automatically in your code.
  // But if you want to check manually:
  // merge_free_blocks(global_base);

  // Print the linked list of blocks to visualize
  struct block_meta *curr = (struct block_meta *)global_base;
  printf("\n=== Heap State ===\n");
  while (curr) {
    printf("Block %p | size=%zu | free=%d | magic=0x%x | next=%p\n",
           (void *)curr, curr->size, curr->free, curr->magic,
           (void *)curr->next);
    curr = curr->next;
  }

  return 0;
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
  if (myblock->size > size + META_SIZE + MIN_SIZE) {
    shrink_and_split(myblock, size);
    return myblock + 1;
  }

  /* if size > block -> size
   * 1. free this block
   * 2. allocate new block with the new size
   * 3. copy the content to the new block
   */

  if (size > myblock->size) {
    void *new_block = malloc(size);
    memcpy(new_block, ptr, myblock->size);
    free(ptr);
    return new_block;
  }
}
