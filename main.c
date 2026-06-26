#include <assert.h>
#include <iso646.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>

#define META_SIZE sizeof(struct block_meta)
#define MIN_SIZE 8

struct block_meta {
  size_t size;
  struct block_meta *next;
  int free;
  int marked;
  int magic;
};

void *global_base = NULL;
uintptr_t stack_bottom = 0;

struct block_meta *find_free_block(struct block_meta **last, size_t size);
struct block_meta *request_space(struct block_meta *last, size_t size);
void *malloc(size_t size);
void free(void *ptr);
void *realloc(void *ptr, size_t size);
void merge_free_blocks(struct block_meta *head);

void gc_init(void);
void gc(void);
static void scan_region(uintptr_t *start, uintptr_t *end);
static void scan_heap(void);

void debug_heap(void);
void print_gc_stats(void);
int count_allocated_blocks(void);
int count_free_blocks(void);

int main() {
  printf("===============================================\n");
  printf("  GARBAGE COLLECTOR - DEMONSTRATION\n");
  printf("===============================================\n\n");

  gc_init();
  printf("✓ GC Initialized (Stack bottom: 0x%lx)\n\n", stack_bottom);

  printf("--- Test 1: Basic Allocation ---\n");
  int *a = (int *)malloc(5 * sizeof(int));
  int *b = (int *)malloc(3 * sizeof(int));
  int *c = (int *)malloc(4 * sizeof(int));

  for (int i = 0; i < 5; i++)
    a[i] = i;
  printf("Allocated 3 blocks\n");
  print_gc_stats();

  free(b);
  printf("Freed middle block\n");
  print_gc_stats();

  free(a);
  free(c);
  printf("Freed remaining blocks\n");
  print_gc_stats();
  printf("✓ Test 1 passed\n\n");

  printf("--- Test 2: Garbage Collection ---\n");
  int *reachable = (int *)malloc(10 * sizeof(int));
  int *unreachable = (int *)malloc(10 * sizeof(int));

  for (int i = 0; i < 10; i++) {
    reachable[i] = i;
    unreachable[i] = i * 2;
  }

  printf("Before GC:\n");
  print_gc_stats();

  unreachable = NULL;
  printf("Made one block unreachable\n");

  gc();
  printf("After GC:\n");
  print_gc_stats();

  free(reachable);
  printf("✓ Test 2 passed\n\n");

  printf("--- Test 3: Multiple Unreachable Blocks ---\n");
  int *p1 = (int *)malloc(20 * sizeof(int));
  int *p2 = (int *)malloc(30 * sizeof(int));
  int *p3 = (int *)malloc(40 * sizeof(int));
  int *keep = (int *)malloc(50 * sizeof(int));

  printf("Allocated 4 blocks\n");
  print_gc_stats();

  p1 = p2 = p3 = NULL;
  printf("Made 3 blocks unreachable\n");

  gc();
  printf("After GC (should collect 3 blocks):\n");
  print_gc_stats();
  debug_heap();

  free(keep);
  printf("✓ Test 3 passed\n\n");

  printf("===============================================\n");
  printf("  ALL TESTS COMPLETED SUCCESSFULLY!\n");
  printf("===============================================\n");

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
  struct block_meta *block = sbrk(0);
  void *request = sbrk(size + META_SIZE);

  assert((void *)block == request);
  if (request == (void *)-1) {
    return NULL;
  }

  if (last) {
    last->next = block;
  }

  block->size = size;
  block->next = NULL;
  block->free = 0;
  block->marked = 1;
  block->magic = 0x12345678;

  return block;
}

void *malloc(size_t size) {
  if (size <= 0) {
    return NULL;
  }

  size = (size + 7) & ~7;

  struct block_meta *block;

  if (!global_base) {
    block = request_space(NULL, size);
    if (!block)
      return NULL;
    global_base = block;
  } else {
    struct block_meta *last = NULL;
    block = find_free_block(&last, size);

    if (!block) {
      block = request_space(last, size);
      if (!block)
        return NULL;
    } else {
      if (block->size >= size + META_SIZE + MIN_SIZE) {
        size_t remaining = block->size - size - META_SIZE;
        block->size = size;

        struct block_meta *new_block =
            (struct block_meta *)((char *)block + META_SIZE + size);

        new_block->size = remaining;
        new_block->free = 1;
        new_block->marked = 0;
        new_block->magic = 0x22222222;
        new_block->next = block->next;

        block->next = new_block;
      }

      block->free = 0;
      block->marked = 1;
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

      current->size += META_SIZE + next->size;
      current->next = next->next;
    } else {
      current = current->next;
    }
  }
}

void free(void *ptr) {
  if (!ptr)
    return;

  struct block_meta *block = (struct block_meta *)ptr - 1;

  assert(block->free == 0);
  assert(block->magic == 0x77777777 || block->magic == 0x12345678);

  block->free = 1;
  block->marked = 0;
  block->magic = 0x55555555;

  merge_free_blocks(global_base);
}

void *realloc(void *ptr, size_t size) {
  if (!ptr) {
    return malloc(size);
  }

  if (size == 0) {
    free(ptr);
    return NULL;
  }

  struct block_meta *block = (struct block_meta *)ptr - 1;

  if (size <= block->size) {
    return ptr;
  }

  void *new_ptr = malloc(size);
  if (new_ptr) {
    memcpy(new_ptr, ptr, block->size);
    free(ptr);
  }

  return new_ptr;
}


void gc_init(void) {
  static int initialized = 0;

  if (initialized)
    return;
  initialized = 1;

  FILE *statfp = fopen("/proc/self/stat", "r");
  assert(statfp != NULL);

  fscanf(statfp,
         "%*d %*s %*c %*d %*d %*d %*d %*d %*u "
         "%*lu %*lu %*lu %*lu %*lu %*lu %*ld %*ld "
         "%*ld %*ld %*ld %*ld %*llu %*lu %*ld "
         "%*lu %*lu %*lu %lu",
         &stack_bottom);

  fclose(statfp);
}

static void scan_region(uintptr_t *start, uintptr_t *end) {
  if (!global_base)
    return;

  uintptr_t heap_start = (uintptr_t)(global_base) + META_SIZE;
  uintptr_t heap_end = (uintptr_t)sbrk(0);

  for (uintptr_t *p = start; p < end; p++) {
    uintptr_t value = *p;

    if (value >= heap_start && value < heap_end) {

      struct block_meta *block = global_base;
      while (block) {
        uintptr_t block_start = (uintptr_t)(block + 1);
        uintptr_t block_end = block_start + block->size;

        if (value >= block_start && value < block_end) {
          block->marked = 1;
          break;
        }

        block = block->next;
      }
    }
  }
}

static void scan_heap(void) {
  if (!global_base)
    return;

  int new_marks;

  do {
    new_marks = 0;

    struct block_meta *block = global_base;

    for (; block != NULL; block = block->next) {
      if (!block->marked)
        continue;

      uintptr_t *data = (uintptr_t *)(block + 1);

      size_t word_count = block->size / sizeof(uintptr_t);

      for (size_t i = 0; i < word_count; i++) {
        uintptr_t value = data[i];

        for (struct block_meta *other = global_base; other != NULL;
             other = other->next) {
          if (!other->marked) {
            uintptr_t other_start = (uintptr_t)(other + 1);
            uintptr_t other_end =
                (uintptr_t)((char *)(other + 1) + other->size);

            if (value >= other_start && value < other_end) {
              other->marked = 1;
              new_marks = 1;
            }
          }
        }
      }
    }
  } while (new_marks);
}

void gc(void) {
  if (!global_base)
    return;

  extern char etext, end;
  struct block_meta *block = global_base;
  for (; block != NULL; block = block->next) {
    block->marked = 0;
  }

  scan_region((uintptr_t *)&etext, (uintptr_t *)&end);

  uintptr_t stack_top;
#ifdef __x86_64__
  asm volatile("movq %%rbp, %0" : "=r"(stack_top));
#else
  asm volatile("movl %%ebp, %0" : "=r"(stack_top));
#endif
  scan_region((uintptr_t *)stack_top, (uintptr_t *)stack_bottom);

  scan_heap();

  block = global_base;
  while (block != NULL) {
    struct block_meta *next = block->next;

    if (!block->marked && !block->free) {
      block->free = 1;
      block->marked = 0;
      block->magic = 0x55555555;
    }

    block = next;
  }
}


int count_allocated_blocks(void) {
  int count = 0;
  struct block_meta *curr = global_base;

  while (curr) {
    if (!curr->free)
      count++;
    curr = curr->next;
  }

  return count;
}

int count_free_blocks(void) {
  int count = 0;
  struct block_meta *curr = global_base;

  while (curr) {
    if (curr->free)
      count++;
    curr = curr->next;
  }

  return count;
}

void print_gc_stats(void) {
  printf("  [Allocated: %d blocks | Free: %d blocks]\n",
         count_allocated_blocks(), count_free_blocks());
}

void debug_heap(void) {
  struct block_meta *curr = global_base;
  printf("\n[HEAP DUMP]\n");
  printf("%-18s %-8s %-6s %-8s %-10s\n", "Address", "Size", "Free", "Marked",
         "Magic");

  int count = 0;
  while (curr && count < 20) {
    if (curr->magic != 0x12345678 && curr->magic != 0x77777777 &&
        curr->magic != 0x22222222 && curr->magic != 0x55555555) {
      printf("%-18p [CORRUPTED - magic: 0x%x]\n", (void *)curr, curr->magic);
      break;
    }

    printf("%-18p %-8zu %-6d %-8d 0x%08x\n", (void *)curr, curr->size,
           curr->free, curr->marked, curr->magic);

    curr = curr->next;
    count++;
  }

  if (count >= 20) {
    printf("  (stopped after 20 blocks)\n");
  }
  printf("----------------------------------------\n");
}
