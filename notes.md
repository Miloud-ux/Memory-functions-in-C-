# The heap 

Think of your simple heap as a stack for allocations: you push memory at the top using `sbrk()`
You can access middle memory, but you cannot allocate there.
Real malloc allows middle allocation by keeping track of free blocks by using **Free lists** or **metadata**.

# Building malloc
## SBRK fucntion
We use `sbrk()` to allocate memroy (extend the heap from the top).

- `sbrk(0)` : returns the address of the top of the heap
- `sbrk(size_t num_bytes)` : Allocates memroy and returns the starting address of the allocated block
(top of the heap.)

**Error handling**: 
- If `sbrk()` fails it returns `(void*)-1` (the number -1 casted to void which stands for an address full
of ones aka the representation of -1 as an int) and compares it with the address of the returned pointer.
- If it succeeds it returns a pointer to the top of the heap

**Note** : `assert()` in my project context is not thread safe due to the modifications on the heap across the function calls
 throughout the threads. Ex : first thread calls it and when second thread calls it the address of the top
of the heap changes to the return result of the previous call of it thus the `assert()` function `abort()`
the program


# Building free()

The OS sees the heap as one contiguous block of memory so freeing and returning the memory to the os is wrong
because if we free from the middle we would need to copy all the memory above to that freed space and that's
not possible that's why we mark the memory as free inside **the Process address space** = the virtual memory
the process can access. The OS cannot use that memory for other programs until the end of the heap 
(Program break) is released (rare) or large blocks are returned with `munmap`.

we can mark that the block has been freed without returning it to the OS, so that future calls to malloc can
use re-use the block. But to do that we'll need be able to access the meta information for each block. There
are a lot of possible solutions to that. We'll arbitrarily choose to use a single linked list for simplicity.

We also need to store some additional metadata to know the size of the allocated memory so that when we malloc
and the retuned pointed would be shifted by that allocated meta data before actual use.

**Steps**
1. We track the last block examined (visited)  actually it's predecessor or the tail of the list if no block
found so if we want to append more blocks we do it in $O(n)$ instead of traversing the list all over again 
or keeping a variable to track the state of the list.
2. We set that block to free.

 - - - 
# Project Extension 
The original implementation didn't include **splitting ** and **Merging (coalescing)** blocks.
> [!NOTE] Idea for later
>  Set a limit eg : 100kb and use malloc when size < limit else we use mmapSet a limit eg : 100kb and use malloc when size < limit else we use mmap
>  Switch the strategy from first fit to best fit.


## Splitting blocks (Malloc)
When we malloc space and create new blocks we are gonna check `if block_size > size + META_SIZE + MIN_SIZE`
where `MIN_SIZE` is an arbitrary minimum value to determine where the splitting happens. After that we 
split the block into two blocks and relink the blocks pointers in our  linked list.

**Splitting** means we write to the end of the first block memory region and a new meta block for the next 
region(second block).

Also we need to consider **padding** or **Address allignment** with this formula `size = (size+(N-1)) & ~(N-1)`
where `N = 7` because on most systems 8 bytes is enough for most types and universal.

## Merging blocks (free)
We can apply a sliding window algorithm to merge contiguous free blocks in the linked list.

```
void merge_free_blocks(struct meta_block* head){
  struct meta_block* curr = global_base;
  struct meta_block* prev = NULL;

  if(!curr){
    return;
  }
  
  while(curr && curr -> next){
    prev = curr;
    curr = curr -> next;

    if(curr -> free and prev -> free && ((char*)prev + META_SIZE + prev->size == (char*)curr)){
      prev -> size += META_SIZE + curr -> size;
      prev -> next = curr -> next;
      curr = prev;
   }
  }
}
```

 or : This is the implemented version to avoid edge cases bugs
```

void merge_free_blocks(struct block_meta *head) {
  struct block_meta *current = head;

  while (current && current->next) {
    struct block_meta *next = current->next;

    // Check if both blocks are free and contiguous in memory
    if (current->free && next->free &&
        ((char *)current + META_SIZE + current->size == (char *)next)) {

      // Merge: extend current block to include the next block + its metadata
      current->size += META_SIZE + next->size;
      current->next = next->next; // Skip over the merged block

      // Note: we do NOT move 'current' forward here, because the new
      // merged block might still be adjacent to another free one.
    } else {
      // Only move forward if we didn't merge
      current = current->next;
    }
  }
}
```

We also did init `last = NULL` to avoid skipping blocks when the first blocks is already free.

**Note About how sbrk works**:
A memory page = the smallest unit of memory the OS manages, This means when calling `sbkr(SIZE)` if SIZE
is small enough the OS may not extend the heap repeatedly. Instead it may extend the heap by a whole page
silently (internally) without modifying the meta blocks so when `sbrk()` is called again we don't need to extend the heap
to avoid costly system calls.






