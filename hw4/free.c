#include <pthread.h>

#include "malloc_utils.h"
#include "malloc.h"

void free(void *ptr) {
    if (ptr == NULL)
        return;
    pthread_mutex_lock(&malloc_lock);

    page_info *pi = GET_INFO(ptr);

    // if returned by memalign
    if (pi->flags & ADDR_MEMALIGN) {
        size_t hdr = sizeof(u8ptr_t) + sizeof(page_info);
        ptr = *((void **)ptr - hdr/sizeof(u8ptr_t));
        pi = GET_INFO(ptr);
    }

    _insert(pi);
    pi->flags = 0;
    _try_merge(pi);
    #ifdef DEBUG
    _validate();
    #endif
    pthread_mutex_unlock(&malloc_lock);
}
