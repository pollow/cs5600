#define _GNU_SOURCE

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <stddef.h>
#include <stdint.h>
#include <pthread.h>
#include <string.h>
#include <sched.h>
#include <assert.h>
#include <errno.h>

#include <sys/mman.h>

#include "malloc_utils.h"

size_t PAGE_SIZE;
size_t BLOCK_SIZE;  // 2^0 takes a page, typically 2^12 = 4096;
mem_arena *mem_zone_base = NULL;
__thread mem_arena *arena = NULL;

u8ptr_t arena_cur_base = (u8ptr_t) 0x10000000000;
size_t arena_offset = 0x1000000000;
pthread_mutex_t arena_lock;

void *_memset(void* ptr, int c, size_t len) {
    u8ptr_t p = (u8ptr_t)ptr;
    while (len--) {
        *p++ = (unsigned char)c;
    }
    return ptr;
}

void _update_high() {
    while (arena->free_area[arena->high].count == 0) {
        arena->high--;
        if (arena->high == -1) {
            break;
        }
    }
}

void _validate() {
    for (int i = 0; i < MAX_ORDER; i++) {
        free_area *fa = &arena->free_area[i];
        int len = 0;
        page_info *pi = fa->ptr;
        while (pi) {
            len++;
            pi = pi->next;
        }
        assert(len == fa->count);
    }
}

int _valid_align(size_t x) {
    if (x && (x & (x-1)) == 0) {
        return x;
    } else return 0;
}

size_t _round(size_t size) {
    // Align size to block size
    size_t pow = 32 - __builtin_clz(size) - 1;
    if (size != (1 << pow)) {
        size = 1 << (pow+1);
    }
    if (size < BLOCK_SIZE) {
        size = BLOCK_SIZE;
    }
    return size;
}

void _unlink(page_info *pi) {
    if (pi->prev != NULL) {
        pi->prev->next = pi->next;
    } else {
        arena->free_area[pi->order].ptr = pi->next;
    }
    if (pi->next != NULL) {
        pi->next->prev = pi->prev;
    }
    arena->free_area[pi->order].count--;
    if (pi->order == arena->high &&
        arena->free_area[pi->order].count == 0) {
        _update_high();
    }
}

void _insert(page_info *left) {
    if (left->order > arena->high) {
        arena->high = left->order;
    }
    free_area *fa = &arena->free_area[left->order];
    left->prev = NULL;
    left->next = fa->ptr;
    if (left->next != NULL) {
        left->next->prev = left;
    }
    fa->ptr = left;
    fa->count++;
}

void _split(size_t order) {
    free_area *fa = &arena->free_area[order];

    if (order > MAX_ORDER) {
        return;
    }

    if (!fa->count) {
        _split(order+1);
    }

    if (fa->count) {
        page_info *left = fa->ptr;
        _unlink(left);
        left->order--;
        page_info *right = left + ((BLOCK_SIZE / HEAD_OFFSET) << left->order);
        right->order = order - 1;
        _insert(right);
        _insert(left);
    }
}

// Increasing heap until the target order reached
void *_alloc_arena(int order) {
    void *cur = NULL;
    do {
        size_t length = BLOCK_SIZE << arena->current;
        cur = mmap(arena->mem_base + length, length,
                   PROT_READ | PROT_WRITE,
                   MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED,
                   -1, 0);
        if (cur != (void *)-1) {
            page_info *pi = (page_info *)cur;
            pi->order = arena->current;
            _insert(pi);
        } else
            return NULL;
        arena->current++;
    } while (order >= arena->current);

    return cur;
}

void _merge_buddy(page_info *left, page_info *right) {
    assert(left < right);
    _memset(right, 0, sizeof(page_info));
    left->order++;
    _insert(left);
    _try_merge(left);
}

page_info *_get_buddy(u8ptr_t pi, size_t order) {
    return (page_info *)(arena->mem_base +
                         ((size_t)(pi-arena->mem_base) ^ (BLOCK_SIZE << order)));
}

void _try_merge(page_info *pi) {
    page_info *buddy = _get_buddy((u8ptr_t)pi, pi->order);

    if (pi->order != MAX_ORDER &&
        buddy->order == pi->order &&
        (buddy->flags & ADDR_ALLOCATED) == 0) {
        // same order, buddy not allocated, merge
        free_area tfa = arena->free_area[pi->order];
        _unlink(pi);
        _unlink(buddy);
        assert(arena->free_area[pi->order].count == tfa.count-2);
        _merge_buddy(pi < buddy ? pi : buddy,
                     pi < buddy ? buddy : pi);
    }
}


__attribute__ ((constructor))
void init() {
    PAGE_SIZE = sysconf(_SC_PAGESIZE);
    BLOCK_SIZE = PAGE_SIZE;
    // initalize mutex
    int rtn = pthread_mutex_init(&arena_lock, NULL);
    if (rtn) {
        const char* info = "Cannot initialize pthread_mutex.\n";
        write(STDOUT_FILENO, info, strlen(info));
        exit(0);
    }
}

void initialize_arena() {
    char *info = "initialize_arena\n";
    write(1, info, strlen(info));
    pthread_mutex_lock(&arena_lock);
    if (mem_zone_base == NULL) {

        char *info = "init arena metadata\n";
        write(1, info, strlen(info));
        // initialize arena metadata
        size_t num_cores = sysconf( _SC_NPROCESSORS_ONLN );
        size_t size = sizeof(mem_arena) * num_cores;
        mem_zone_base = (mem_arena *)sbrk(size);
        _memset(mem_zone_base, 0, size);
    }

    pthread_t self = pthread_self();
    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    pthread_getaffinity_np(self, sizeof(cpu_set_t), &cpuset);

    for (int i = 0; ; i++) {
        if (CPU_ISSET(i, &cpuset)) {
            char buf[10];
            snprintf(buf, 10, "%d\n", i);
            write(1, buf, strlen(buf));
            arena = mem_zone_base + i;
            if (arena->mem_base == NULL) {
                // initialize arena region
                char *info = "mmap new thread arena\n";
                write(1, info, strlen(info));
                u8ptr_t cur = mmap(arena_cur_base, BLOCK_SIZE,
                                   PROT_READ | PROT_WRITE,
                                   MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED,
                                   -1, 0);
                if (cur == (void *)-1) {
                    const char* info = "Cannot initialize arena memory space.\n";
                    write(STDOUT_FILENO, info, strlen(info));
                    exit(0);
                }
                arena->mem_base = cur;// sbrk(BLOCK_SIZE << MAX_ORDER);
                arena->high = -1;
                arena->current = 0;
                arena->free_area[0].ptr = (page_info *)cur;
                arena->free_area[0].count = 1;
                _memset(cur, 0, sizeof(page_info));
                int rtn = pthread_mutex_init(&arena->lock, NULL);
                if (rtn) {
                    const char* info = "Cannot initialize pthread_mutex.\n";
                    write(STDOUT_FILENO, info, strlen(info));
                    exit(0);
                }
                arena_cur_base += arena_offset;
            }
            break;
        }
    }

    pthread_mutex_unlock(&arena_lock);
}
