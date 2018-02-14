#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <stddef.h>
#include <stdint.h>
#include <pthread.h>
#include <string.h>
#include <assert.h>
#include <errno.h>

#include "malloc_utils.h"

size_t PAGE_SIZE;
size_t BLOCK_SIZE;  // 2^0 takes a page, typically 2^12 = 4096;
pthread_mutex_t malloc_lock;
mem_arena mem_zone;

void *_memset(void* ptr, int c, size_t len) {
    u8ptr_t p = (u8ptr_t)ptr;
    while (len--) {
        *p++ = (unsigned char)c;
    }
    return ptr;
}

void _update_high() {
    while (mem_zone.free_area[mem_zone.high].count == 0) {
        mem_zone.high--;
        if (mem_zone.high == -1) {
            break;
        }
    }
}

void _validate() {
    for (int i = 0; i < MAX_ORDER; i++) {
        free_area *fa = &mem_zone.free_area[i];
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
        mem_zone.free_area[pi->order].ptr = pi->next;
    }
    if (pi->next != NULL) {
        pi->next->prev = pi->prev;
    }
    mem_zone.free_area[pi->order].count--;
    if (pi->order == mem_zone.high &&
        mem_zone.free_area[pi->order].count == 0) {
        _update_high();
    }
}

void _insert(page_info *left) {
    if (left->order > mem_zone.high) {
        mem_zone.high = left->order;
    }
    free_area *fa = &mem_zone.free_area[left->order];
    left->prev = NULL;
    left->next = fa->ptr;
    if (left->next != NULL) {
        left->next->prev = left;
    }
    fa->ptr = left;
    fa->count++;
}

void _split(size_t order) {
    free_area *fa = &mem_zone.free_area[order];

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
        cur = sbrk(BLOCK_SIZE << mem_zone.current);
        if (cur != (void *)-1) {
            page_info *pi = (page_info *)cur;
            pi->order = mem_zone.current;
            _insert(pi);
        } else
            return NULL;
        mem_zone.current++;
    } while (order >= mem_zone.current);

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
    return (page_info *)(mem_zone.mem_base +
                         ((size_t)(pi-mem_zone.mem_base) ^ (BLOCK_SIZE << order)));
}

void _try_merge(page_info *pi) {
    page_info *buddy = _get_buddy((u8ptr_t)pi, pi->order);

    if (pi->order != MAX_ORDER &&
        buddy->order == pi->order &&
        (buddy->flags & ADDR_ALLOCATED) == 0) {
        // same order, buddy not allocated, merge
        free_area tfa = mem_zone.free_area[pi->order];
        _unlink(pi);
        _unlink(buddy);
        assert(mem_zone.free_area[pi->order].count == tfa.count-2);
        _merge_buddy(pi < buddy ? pi : buddy,
                     pi < buddy ? buddy : pi);
    }
}

__attribute__ ((constructor))
void init() {
    PAGE_SIZE = sysconf(_SC_PAGESIZE);
    BLOCK_SIZE = PAGE_SIZE;
    void *cur = sbrk(BLOCK_SIZE);
    mem_zone.mem_base = cur;// sbrk(BLOCK_SIZE << MAX_ORDER);
    mem_zone.high = -1;
    mem_zone.current = 0;
    mem_zone.free_area[0].ptr = cur;
    mem_zone.free_area[0].count = 1;
    _memset(cur, 0, sizeof(page_info));

    // char buf[1024];
    // snprintf(buf, 1024, "User space memory %p:%p\n", cur, sbrk(0));
    // write(STDOUT_FILENO, buf, strlen(buf));

    // initalize mutex
    int rtn = pthread_mutex_init(&malloc_lock, NULL);
    if (rtn) {
        const char* info = "Cannot initialize pthread_mutex.\n";
        write(STDOUT_FILENO, info, strlen(info));
        exit(0);
    }
}
