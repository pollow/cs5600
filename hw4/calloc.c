#include "malloc_utils.h"
#include "malloc.h"

void *calloc(size_t nmemb, size_t size) {
    void* ptr = malloc(nmemb * size);
    _memset(ptr, 0, nmemb * size);
    return ptr;
}
