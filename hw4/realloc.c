#include <string.h>

#include "malloc_utils.h"
#include "malloc.h"

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
