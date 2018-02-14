#include <assert.h>
#include <errno.h>

#include "malloc_utils.h"
#include "malloc.h"

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
