#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <stddef.h>
#include <stdint.h>
#include <pthread.h>
#include <string.h>
#include <assert.h>

#include "malloc.h"

#define MAX_ORDER 16
static size_t PAGE_SIZE;
static size_t BLOCK_SIZE;  // 2^0 takes a page, typically 2^12 = 4096;
pthread_mutex_t malloc_lock;

// Address status mask
const size_t ADDR_ALLOCATED = 0x1;
const size_t ADDR_MEMALIGN = 0x2;

struct free_area {
    struct page_info *ptr; // last 12 bits are free for mark
    int count;
};

struct mem_zone {
    unsigned char *mem_base;
    struct page_info* page_info_base;
    struct free_area free_area[MAX_ORDER + 1];
} mem_zone;

struct page_info {
    struct page_info *next;
    struct page_info *prev;
    int order;
    size_t flags;
};

void *_memset(void* ptr, int c, size_t len) {
    unsigned char *p = ptr;
    while (len--) {
        *p++ = (unsigned char)c;
    }
    return ptr;
}

void _validate() {
    for (int i = 0; i < MAX_ORDER; i++) {
        struct free_area *fa = &mem_zone.free_area[i];
        int len = 0;
        struct page_info *pi = fa->ptr;
        while (pi) {
            len++;
            pi = pi->next;
        }
        if (len != fa->count) {
            write(1, "FUCK!\n", 6);
        }
    }
}

int valid_align(size_t x) {
    if (x && (x & (x-1))) {
        return x;
    } else return 0;
}

__attribute__ ((constructor))
void init() {
    PAGE_SIZE = sysconf(_SC_PAGESIZE);
    BLOCK_SIZE = PAGE_SIZE;
    char buf[1024];
    mem_zone.free_area[MAX_ORDER].ptr =
        mem_zone.page_info_base = sbrk(sizeof(struct page_info) << MAX_ORDER);
    void *cur = sbrk(0);
    mem_zone.mem_base = sbrk(BLOCK_SIZE << MAX_ORDER);

    snprintf(buf, 1024, "User space memory %p:%p\n", cur, sbrk(0));
    write(STDOUT_FILENO, buf, strlen(buf));

    _memset(mem_zone.page_info_base, 0, sizeof(struct page_info));
    mem_zone.page_info_base->order = MAX_ORDER;
    mem_zone.free_area[MAX_ORDER].count = 1;

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

void _unlink(struct page_info *pi) {
    if (pi->prev != NULL) {
        pi->prev->next = pi->next;
    } else {
        mem_zone.free_area[pi->order].ptr = pi->next;
    }
    if (pi->next != NULL) {
        pi->next->prev = pi->prev;
    }
    mem_zone.free_area[pi->order].count--;
    struct free_area *fa = &mem_zone.free_area[pi->order];
    if (fa->count != 0 && fa->ptr == NULL) {
        write(1, "WTF???\n", 7);
    }
}

void _insert(struct page_info *left) {
    struct free_area *fa = &mem_zone.free_area[left->order];
    left->prev = NULL;
    left->next = fa->ptr;
    if (left->next != NULL) {
        left->next->prev = left;
    }
    fa->ptr = left;
    fa->count++;
}

void _split(size_t order) {
    struct free_area *fa = &mem_zone.free_area[order];

    if (order > MAX_ORDER) {
        return;
    }

    if (!fa->count) {
        _split(order+1);
    }

    if (fa->count) {
        int tmp = fa->count;
        struct page_info *left = fa->ptr;
        _unlink(left);
        assert(fa->count == tmp - 1);
        left->order--;
        struct page_info *right = left + (1 << (order - 1));
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
    if (size == 2605) {
        size++;
    }
    if (size == 0) {
        // set errno
        return NULL;
    }
    size_t alloc_size = _round(size);
    int order = __builtin_ctz(alloc_size) - 12; // 0x1000 -> order 0
    struct page_info *rtnptr = NULL;
    struct free_area *fa = &mem_zone.free_area[order];

    if (!fa->count) {
        _split(order+1);
    }

    if (!fa->count) {
        rtnptr = NULL;
        return NULL;
        // set errno
    } else {
        if (fa->ptr == NULL) {
            const char *info = "Assertion fail.\n";
            write(STDOUT_FILENO, info, strlen(info));
            exit(1);
        }
        rtnptr = fa->ptr;
        _unlink(fa->ptr);
        // fa->count--;
        // fa->ptr = fa->ptr->next;
    }

    rtnptr->flags |= ADDR_ALLOCATED;
    void *rtn = (rtnptr - mem_zone.page_info_base) *
        PAGE_SIZE + (unsigned char *)mem_zone.mem_base;
    // We can't use printf here because printf internally calls `malloc` and thus
    // we'll get into an infinite recursion leading to a segfault.
    // Instead, we first write the message into a string and then use the `write`
    // system call to display it on the console.
    char buf[1024];
    snprintf(buf, 1024, "%s:%d malloc(%zu): Allocated %zu bytes at %p\n",
             __FILE__, __LINE__, size, alloc_size, rtn);
    // write(STDOUT_FILENO, buf, strlen(buf));

    _validate();
    if ((size_t)rtn > 0x10804000) {
        write(1, "FUCK???\n", 8);
    }
    pthread_mutex_unlock(&malloc_lock);
    return rtn;
}

void *calloc(size_t nmemb, size_t size) {
    void* ptr = malloc(nmemb * size);
    _memset(ptr, 0, nmemb * size);
    return ptr;
}

void _try_merge(size_t page_num);
void _merge_buddy(size_t a, size_t b) {
    assert(a < b);
    struct page_info *left = mem_zone.page_info_base + a;
    struct page_info *right = mem_zone.page_info_base + b;
    _memset(right, 0, sizeof(struct page_info));
    left->order++;
    _insert(left);
    _try_merge(a);
}

void _try_merge(size_t page_num) {
    struct page_info *pi =  page_num + mem_zone.page_info_base;
    struct page_info *buddy = mem_zone.page_info_base +
        (page_num ^ (1 << pi->order));

    if (pi->order != MAX_ORDER &&
        buddy->order == pi->order &&
        (buddy->flags & ADDR_ALLOCATED) == 0) {
        // same order, buddy not allocated, merge
        struct free_area tfa = mem_zone.free_area[pi->order];
        _unlink(pi);
        _unlink(buddy);
        assert(mem_zone.free_area[pi->order].count == tfa.count-2);
        _merge_buddy(page_num & ~(1 << pi->order),
                     page_num | (1 << pi->order));
    }
}

void free(void *ptr) {
    if (ptr == NULL)
        return;
    pthread_mutex_lock(&malloc_lock);
    unsigned char *p = ptr;
    size_t page_num = (p - mem_zone.mem_base) / PAGE_SIZE;
    struct page_info *pi =  page_num + mem_zone.page_info_base;

    // We can't use printf here because printf internally calls `malloc` and thus
    // we'll get into an infinite recursion leading to a segfault.
    // Instead, we first write the message into a string and then use the `write`
    // system call to display it on the console.
    char buf[1024];
    snprintf(buf, 1024, "%s:%d free(%p): Freeing %zu bytes from %p\n",
             __FILE__, __LINE__, ptr, (1 << pi->order) * PAGE_SIZE, ptr);
    // write(STDOUT_FILENO, buf, strlen(buf));

    // if returned by memalign
    if (pi->flags | ADDR_MEMALIGN) {
        ptr = *((void **)ptr - 1);
        pi->flags -= ADDR_MEMALIGN;
    }

    struct free_area tfa = mem_zone.free_area[pi->order];
    _insert(pi);
    assert(tfa.count + 1 == mem_zone.free_area[pi->order].count);
    assert(pi == mem_zone.free_area[pi->order].ptr);
    pi->flags -= ADDR_ALLOCATED;
    _try_merge(page_num);
    _validate();
    pthread_mutex_unlock(&malloc_lock);
}


void *realloc(void *ptr, size_t size) {
    pthread_mutex_lock(&malloc_lock);

    if (ptr == NULL) {
        return malloc(size);
    }
    unsigned char *p = ptr;
    size_t page_num = (p - mem_zone.mem_base) / PAGE_SIZE;
    struct page_info *pi =  page_num + mem_zone.page_info_base;
    size_t mem_size = (1 << pi->order) * PAGE_SIZE;

    if (size < mem_size) {
        return ptr;
    }

    unsigned char *rtn = (unsigned char *)malloc(size);
    memcpy(rtn, ptr, mem_size);
    free(ptr);
    return rtn;

    pthread_mutex_unlock(&malloc_lock);
}

void *reallocarray(void *ptr, size_t nmemb, size_t size) {
    return realloc(ptr, nmemb * size);
}

void *_align_up(unsigned char *p, size_t align) {
    return (void *)(((size_t)p + (align - 1)) & ~(align - 1));
}

void *memalign(size_t align, size_t size) {
    if (!valid_align(align)) {
        return NULL;
    }
    assert((align & (align - 1)) == 0);
    if (align <= BLOCK_SIZE) {
        // buddy system will always align to page_size;
        return malloc(size);
    }

    void *rtn = NULL;
    if (align && size) {
        size_t extra = sizeof(void *) + (align - 1);
        void *p= malloc(size + extra);

        if (p) {
            if (((size_t)p & (align - 1)) == 0) {
                // already aligned.
                return p;
            }
            // align up and store start pointer
            rtn = _align_up(p + sizeof(void *), align);
            *((void **)rtn - 1) = p;
            // flag page_info
            size_t page_num = ((unsigned char *)rtn - mem_zone.mem_base) / PAGE_SIZE;
            struct page_info *pi =  page_num + mem_zone.page_info_base;
            pi->flags |= ADDR_MEMALIGN;
        }
    }

    return rtn;

}
