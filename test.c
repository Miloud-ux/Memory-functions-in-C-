#include <assert.h>
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

int main() {
  int *memory = (int *)malloc(sizeof(int) * 4);
  if (!memory) {
    printf("Error");
    return 1;
  }

  for (int i = 0; i < 4; i++) {
    memory[i] = i;
    printf("%d\n", memory[i]);
  }

  struct block_meta *block_addr = (struct block_meta *)memory - 1;
  free(memory);

  printf("block -> free = %d\n", block_addr->free);
  printf("block -> magic = %x", block_addr->magic);

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
    struct block_meta *last = global_base;
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
}
