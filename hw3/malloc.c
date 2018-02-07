#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <stddef.h>
#include <stdint.h>
#include <pthread.h>
#include <string.h>
#include <assert.h>
#include <errno.h>

#include "malloc.h"

#define MAX_ORDER 16
#define HEAD_OFFSET sizeof(page_info)
#define GET_INFO(x) (page_info *)((u8ptr_t)(x) - HEAD_OFFSET);

static size_t PAGE_SIZE;
static size_t BLOCK_SIZE;  // 2^0 takes a page, typically 2^12 = 4096;
pthread_mutex_t malloc_lock;

typedef unsigned char * u8ptr_t;

// Address status mask
const size_t ADDR_ALLOCATED = 0x1;
const size_t ADDR_MEMALIGN = 0x2;

typedef struct free_area {
    struct page_info *ptr; // last 12 bits are free for mark
    int count;
} free_area;

typedef struct mem_arena {
    u8ptr_t mem_base;
    struct page_info* page_info_base;
    struct free_area free_area[MAX_ORDER + 1];
} mem_arena;

mem_arena mem_zone;

typedef struct page_info {
    struct page_info *next;
    struct page_info *prev;
    int order;
    size_t flags;
} page_info;

void *_memset(void* ptr, int c, size_t len) {
    u8ptr_t p = (u8ptr_t)ptr;
    while (len--) {
        *p++ = (unsigned char)c;
    }
    return ptr;
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
        if (len != fa->count) {
            write(1, "FUCK!\n", 6);
        }
    }
}

int _valid_align(size_t x) {
    if (x && (x & (x-1)) == 0) {
        return x;
    } else return 0;
}

__attribute__ ((constructor))
void init() {
    PAGE_SIZE = sysconf(_SC_PAGESIZE);
    BLOCK_SIZE = PAGE_SIZE;
    char buf[1024];
    void *cur = sbrk(0);
    mem_zone.mem_base = sbrk(BLOCK_SIZE << MAX_ORDER);
    mem_zone.free_area[MAX_ORDER].ptr = (page_info *)cur;

    snprintf(buf, 1024, "User space memory %p:%p\n", cur, sbrk(0));
    write(STDOUT_FILENO, buf, strlen(buf));

    _memset(cur, 0, sizeof(page_info));
    mem_zone.free_area[MAX_ORDER].count = 1;
    mem_zone.free_area[MAX_ORDER].ptr->order= MAX_ORDER;

    // initalize mutex
    int rtn = pthread_mutex_init(&malloc_lock, NULL);
    if (rtn) {
        const char* info = "Cannot initialize pthread_mutex.\n";
        write(STDOUT_FILENO, info, strlen(info));
        exit(0);
    }
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
    // struct free_area *fa = &mem_zone.free_area[pi->order];
    // if (fa->count != 0 && fa->ptr == NULL) {
    //     write(1, "WTF???\n", 7);
    // }
}

void _insert(page_info *left) {
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
        int tmp = fa->count;
        page_info *left = fa->ptr;
        _unlink(left);
        assert(fa->count == tmp - 1);
        left->order--;
        page_info *right = left + ((BLOCK_SIZE / HEAD_OFFSET) << left->order);
        right->order = order - 1;
        assert(left->order == right->order);
        tmp = mem_zone.free_area[left->order].count;
        _insert(right);
        assert(mem_zone.free_area[left->order].ptr == right);
        _insert(left);
        assert(mem_zone.free_area[left->order].count == tmp + 2);
        assert(mem_zone.free_area[left->order].ptr == left);

    }
}

void *malloc(size_t size) {
    pthread_mutex_lock(&malloc_lock);
    if (size == 0) {
        return NULL;
    }
    size_t alloc_size = _round(size + sizeof(page_info));
    int order = __builtin_ctz(alloc_size) - 12; // 0x1000 -> order 0
    page_info *rtnptr = NULL;
    free_area *fa = &mem_zone.free_area[order];

    if (!fa->count) {
        _split(order+1);
    }

    if (!fa->count) {
        errno = ENOMEM;
        return NULL;
    } else {
        if (fa->ptr == NULL) {
            const char *info = "Assertion fail.\n";
            write(STDOUT_FILENO, info, strlen(info));
            exit(1);
        }
        rtnptr = fa->ptr;
        _unlink(fa->ptr);
    }

    rtnptr->flags |= ADDR_ALLOCATED;
    void *rtn = (u8ptr_t)rtnptr + sizeof(page_info);
    // We can't use printf here because printf internally calls `malloc` and thus
    // we'll get into an infinite recursion leading to a segfault.
    // Instead, we first write the message into a string and then use the `write`
    // system call to display it on the console.
    char buf[1024];
    snprintf(buf, 1024, "%s:%d malloc(%zu): Allocated %zu bytes at %p\n",
             __FILE__, __LINE__, size, alloc_size, rtn);
    // write(STDOUT_FILENO, buf, strlen(buf));

    _validate();
    pthread_mutex_unlock(&malloc_lock);
    return rtn;
}

void *calloc(size_t nmemb, size_t size) {
    void* ptr = malloc(nmemb * size);
    _memset(ptr, 0, nmemb * size);
    return ptr;
}

void _try_merge(page_info *pi);
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

void free(void *ptr) {
    if (ptr == NULL)
        return;
    pthread_mutex_lock(&malloc_lock);
    // if returned by memalign

    page_info *pi = GET_INFO(ptr);

    if (pi->flags & ADDR_MEMALIGN) {
        size_t hdr = sizeof(u8ptr_t) + sizeof(page_info);
        ptr = *((void **)ptr - hdr/sizeof(u8ptr_t));
        pi = GET_INFO(ptr);
    }

    // We can't use printf here because printf internally calls `malloc` and thus
    // we'll get into an infinite recursion leading to a segfault.
    // Instead, we first write the message into a string and then use the `write`
    // system call to display it on the console.
    char buf[1024];
    snprintf(buf, 1024, "%s:%d free(%p): Freeing %zu bytes from %p\n",
             __FILE__, __LINE__, ptr, (1 << pi->order) * PAGE_SIZE, ptr);
    // write(STDOUT_FILENO, buf, strlen(buf));

    free_area tfa = mem_zone.free_area[pi->order];
    _insert(pi);
    assert(tfa.count + 1 == mem_zone.free_area[pi->order].count);
    assert(pi == mem_zone.free_area[pi->order].ptr);
    pi->flags = 0;
    _try_merge(pi);
    _validate();
    pthread_mutex_unlock(&malloc_lock);
}


void *realloc(void *ptr, size_t size) {
    if (ptr == NULL) {
        return malloc(size);
    }

    page_info *pi = GET_INFO(ptr);
    size_t mem_size = (1 << pi->order) * PAGE_SIZE - HEAD_OFFSET;

    if (size < mem_size) {
        // already larger than requested size.
        return ptr;
    }

    unsigned char *rtn = (unsigned char *)malloc(size);
    memcpy(rtn, ptr, mem_size);
    free(ptr);

    return rtn;
}

void *reallocarray(void *ptr, size_t nmemb, size_t size) {
    return realloc(ptr, nmemb * size);
}

void *_align_up(unsigned char *p, size_t align) {
    return (void *)(((size_t)p + (align - 1)) & ~(align - 1));
}

void *memalign(size_t align, size_t size) {
    if (!_valid_align(align)) {
        errno = EINVAL;
        return NULL;
    }
    assert((align & (align - 1)) == 0);

    void *rtn = NULL;
    if (align && size) {
        size_t hdr = sizeof(u8ptr_t) + sizeof(page_info);
        size_t extra = (align - 1) + hdr;
        void *p = malloc(size + extra);

        if (p) {
            if (((size_t)p & (align - 1)) == 0) {
                // already aligned.
                return p;
            }
            // align up and store start pointer, store a new page_info
            rtn = _align_up((u8ptr_t)p + hdr, align);
            *((void **)rtn - hdr/sizeof(u8ptr_t)) = p;
            // flag page_info
            page_info *pi = GET_INFO(rtn);
            pi->flags |= ADDR_MEMALIGN;
        }
    }
    return rtn;
}

int posix_memalign(void **memptr, size_t alignment, size_t size) {
    void *rtn = memalign(alignment, size);
    if (rtn == NULL) {
        return errno;
    } else {
        *memptr = rtn;
        return 0;
    }
}
