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








