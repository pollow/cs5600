# Homework 3

## Memory Allocator

An implementation of buddy algorithm for memory allocation.

### Code Structure

`malloc_utils.h`: header file include internal helper functions and global variables
`malloc_utils.c`: definition of internal functions and variables
`malloc.h`: exposed public APIs
`malloc.c`: implementation of function `malloc`
`free.c`: implementation of function `free`
`calloc.c`: implementation of function `calloc`
`realloc.c`: implementation of function `realloc`
`memalign.c`: implementation of function `memalign`

### Design Overview

This memory management system utilized buddy algorithm. The unit block is usually 4096 bytes, which is the size of a page. The memory managed by the arena is dynamically allocated from kernel, so there is no limitation of memory usage.

#### Block Information

The meta data for a memory region allocated to use is recorded right before the address returned by `malloc`.
```
------------------------------
| meta data | user space ....|
------------------------------
```

Meta data include, the size of this memory region, flags regard the properties of this memory region, and when it is used as a free node, it's sublings.

The system maintains a free list collecting all free nodes with same order.

A special case is `memalign`, since `memalign` ask for alignment, it might skip the head of an allocated area to align to a given number. The memory structure looks like:

```
-------------------------------------------------------------------------
| meta data | paddings ... | addr of head | meta data | user space .... |
-------------------------------------------------------------------------
```

We can tell whether a address is aligned by examing the flag in it's meta data, and if iti s, load the address of header and move to the real meta data of this block.

#### Arena Management

The system maintains only one arena, and double the size everytime we run out of local memory. This help maintains the number of blocks to be power of 2.


## Usage

`make check1` will run the tests. `make libmalloc.so` will generate a shared library.

## Reference
1. https://en.wikipedia.org/wiki/Buddy_memory_allocation
2. https://embeddedartistry.com/blog/2017/2/20/implementing-aligned-malloc
