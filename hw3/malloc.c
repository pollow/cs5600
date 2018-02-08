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
    pthread_mutex_lock(&malloc_lock);
    if (size == 0) {
        return NULL;
    }
    size_t alloc_size = _round(size + sizeof(page_info));
    int order = __builtin_ctz(alloc_size) - 12; // 0x1000 -> order 0
    page_info *rtnptr = NULL;
    free_area *fa = &mem_zone.free_area[order];

    if (order > mem_zone.high) {
        _alloc_arena(order);
    }

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
    #ifdef DEBUG
    _validate();
    #endif
    pthread_mutex_unlock(&malloc_lock);
    return rtn;
}
