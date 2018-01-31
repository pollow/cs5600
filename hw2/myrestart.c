#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <ucontext.h>
#include <string.h>
#include <ctype.h>
#include <assert.h>
#include <errno.h>
#include <sys/mman.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

struct MemoryRegion
{
    char* startAddr;
    char* endAddr;
    int isReadable;
    int isWriteable;
    int isExecutabl;
    unsigned int offset;
};

char filename[256];
struct ucontext context;

// assume one line will not exceed 256 bytes, ignore check
int mygetline(int fd, char buf[256]) {
    char oneByte = 0;
    int len = 0;
    do {
        read(fd, &oneByte, 1);
        *buf = oneByte;
        len++;
        buf++;
    } while (oneByte != '\n' && oneByte != 0);
    *buf = 0;

    return len;
}

void parse_maps(char* buf, struct MemoryRegion* mr) {
    char r, w, x, p;
    sscanf(buf,
           "%p-%p %c%c%c%c %x",
           &mr->startAddr,
           &mr->endAddr,
           &r,
           &w,
           &x,
           &p,
           &mr->offset);
    mr->isReadable = r == 'r';
    mr->isWriteable = w == 'w';
    mr->isExecutabl = x == 'x';
}

void restore() {
    char buf[256];
    int pid = 0, fileMaps = 0, memMaps = 0;
    struct MemoryRegion mr;
    // look for stack frame
    int fd = open("/proc/self/maps", O_RDONLY);
    while (1) {
        int len = mygetline(fd, buf);
        if (strcmp(buf+len-8, "[stack]\n") == 0) {
            parse_maps(buf, &mr);
            close(fd);
            break;
        }
    }
    // unmap old stack frame
    int rtn = munmap(mr.startAddr, mr.endAddr - mr.startAddr);
    if (rtn == -1)
        printf("%s", strerror(errno));
    getchar();

    fd = open(filename, O_RDONLY);
    if (fd == -1) {
        printf("%s", strerror(errno));
        return;
    }
    read(fd, &pid, sizeof(pid));
    read(fd, &fileMaps, sizeof(pid));
    read(fd, &memMaps, sizeof(pid));

    for (int i = 0; i < fileMaps; i++) {
        read(fd, &mr, sizeof(mr));
        int j = 0;
        char c = 0;
        do {
            read(fd, &c, 1);
            buf[j] = c;
            j++;
        } while (c != '\n');
        j--;
        buf[j] = 0;

        int new_fd = open(buf, O_RDONLY);
        int prot = PROT_NONE;
        if (mr.isExecutabl)
            prot |= PROT_EXEC;
        if (mr.isReadable)
            prot |= PROT_READ;
        if (mr.isWriteable)
            prot |= PROT_WRITE;
        mmap(mr.startAddr,
             mr.endAddr-mr.startAddr,
             prot,
             MAP_PRIVATE,
             new_fd,
             mr.offset);
        close(new_fd);
    }
    for (int i = 0; i < memMaps; i++) {
        read(fd, &mr, sizeof(mr));
        int prot = PROT_WRITE;
        if (mr.isExecutabl)
            prot |= PROT_EXEC;
        if (mr.isReadable)
            prot |= PROT_READ;
        void *addr = mmap(mr.startAddr,
                          mr.endAddr-mr.startAddr,
                          prot,
                          MAP_PRIVATE | MAP_ANONYMOUS,
                          -1,
                          mr.offset);
        if (addr == -1) {
            printf("%s", strerror(errno));
        }
        int rl = read(fd, mr.startAddr, mr.endAddr-mr.startAddr);
        assert(rl == mr.endAddr-mr.startAddr);
        if (rl == -1)
            printf("%s\n", strerror(errno));
        read(fd, buf, 10);
        write(1, buf, 10);
    }

    read(fd, buf, 10);
    write(1, buf, 10);
    read(fd, &context, sizeof(context));
    close(fd);
    setcontext(&context);
}

int main(int argc, char** argv) {
    memcpy(filename, argv[1], strlen(argv[1]));
    write(1, "restoring from checkpoint image: ", 32);
    write(1, filename, strlen(filename));
    write(1, "\n", 1);

    void *stack_ptr = 0x5300000;
    stack_ptr = mmap(stack_ptr, 0x1000, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (stack_ptr == -1) {
        printf("%s", strerror(errno));
        return;
    }
    stack_ptr += 0x1000;
    asm volatile ("mov %0,%%esp" : : "g" (stack_ptr) : "memory");
    restore();
}
