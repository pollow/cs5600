#ifndef _MALLOC_UTILS_H_
#define _MALLOC_UTILS_H_

#include <stdlib.h>
#include <stdio.h>
#include <pthread.h>

#define MAX_ORDER 26
#define HEAD_OFFSET sizeof(page_info)

// Address status mask
#define ADDR_ALLOCATED 0x1
#define ADDR_MEMALIGN 0x2

#define GET_INFO(x) (page_info *)((u8ptr_t)(x) - HEAD_OFFSET);

typedef unsigned char * u8ptr_t;

typedef struct free_area {
    struct page_info *ptr; // last 12 bits are free for mark
    int count;
} free_area;

typedef struct mem_arena {
    u8ptr_t mem_base;
    pthread_mutex_t lock;
    struct page_info* page_info_base;
    struct free_area free_area[MAX_ORDER + 1];
    int high; // highest available blockorder
    int current; // current max order

    // stats information
    size_t used; // used block
    size_t malloc;
    size_t free;
} mem_arena;

typedef struct page_info {
    struct page_info *next;
    struct page_info *prev;
    int order;
    size_t flags;
} page_info;

void *_memset(void* ptr, int c, size_t len);
void _update_high();
void _validate();
int _valid_align(size_t x);
size_t _round(size_t size);
void _unlink(page_info *pi);
void _insert(page_info *left);
void _split(size_t order);
void *_alloc_arena(int order);
void _try_merge(page_info *pi);
void _merge_buddy(page_info *left, page_info *right);
page_info *_get_buddy(u8ptr_t pi, size_t order);
void initialize_arena();
void malloc_stats();

extern size_t PAGE_SIZE;
extern size_t BLOCK_SIZE;
extern mem_arena *mem_zone_base;
extern __thread mem_arena *arena;
extern size_t num_cores;

#endif
