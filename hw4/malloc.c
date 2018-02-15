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
#include "malloc.h"

void *malloc(size_t size) {
    if (size == 0) {
        return NULL;
    }
    if (arena == NULL) {
        initialize_arena();
    }
    pthread_mutex_lock(&arena->lock);
    size_t alloc_size = _round(size + sizeof(page_info));
    int order = __builtin_ctz(alloc_size) - 12; // 0x1000 -> order 0
    page_info *rtnptr = NULL;
    free_area *fa = &arena->free_area[order];

    if (order > arena->high) {
        _alloc_arena(order);
    }

    if (!fa->count) {
        _split(order+1);
    }

    if (!fa->count) {
        errno = ENOMEM;
        pthread_mutex_unlock(&arena->lock);
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
    #ifdef DEBUG
    _validate();
    #endif
    pthread_mutex_unlock(&arena->lock);
    return rtn;
}
