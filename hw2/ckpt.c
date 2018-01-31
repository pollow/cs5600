#include "ckpt.h"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <ucontext.h>
#include <string.h>
#include <ctype.h>
#include <assert.h>
#include <errno.h>
#include <sys/mman.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

char _filename[256];

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

// assume one line will not exceed 256 bytes, ignore check
int mygetline(int fd, char buf[256]) {
    char oneByte = 0;
    int len = 0;
    do {
        int rtn = read(fd, &oneByte, 1);
        if (rtn == -1)
            return -1;
        *buf = oneByte;
        if (rtn == 0)
            return 0;
        len++;
        buf++;
    } while (oneByte != '\n' && oneByte != 0);
    *buf = 0;

    return len;
}

void checkerror(int rtn) {
    if (rtn == -1) {
        printf("%s", strerror(errno));
        exit(0);
    }
}

void checkpoint() {
    int flag = 0;
    int pid = getpid();
    char buf[256];
    struct ucontext context;

    // open mem mapping file
    int fp = open("/proc/self/maps", O_RDONLY);
    // create checkpoint file and save memory
    int fd = open("./myckpt", O_CREAT | O_RDWR, S_IRWXU);
    checkerror(fd);
    int fileMaps = 0, memMaps = 0;
    write(fd, &pid, sizeof(pid));
    write(fd, &fileMaps, sizeof(fileMaps)); // number of file mapping
    write(fd, &memMaps, sizeof(memMaps)); // number of memory region mapping
    struct MemoryRegion mr, stack, heap;
    while (mygetline(fp, buf) > 0) {
        parse_maps(buf, &mr);
        int len = strlen(buf);
        int i = len - 2; // last char is \n
        // locate mapping name
        while (!isspace(buf[i])) {
            i--;
        }
        // avoid vsyscall, vvar and vdso
        if (buf[i+2] != 'v' && mr.isReadable) {
            ssize_t wl = write(fd, &mr, sizeof(mr));
            checkerror(wl);
            assert(wl == sizeof(mr));
            wl = write(fd, mr.startAddr, mr.endAddr - mr.startAddr);
            checkerror(wl);
            assert(wl == mr.endAddr - mr.startAddr);
            // checksum
            wl = write(fd, "000000000\n", sizeof(char)*10);
            checkerror(wl);
            assert(wl == 10);
            memMaps++;
        }
    }

    lseek(fd, sizeof(pid), SEEK_SET);
    write(fd, &fileMaps, sizeof(fileMaps));
    write(fd, &memMaps, sizeof(memMaps));
    close(fp);
    close(fd);

    // set flag to 1 after stack is dumped
    flag = 1;
    getcontext(&context);

    // following code will be executed after restore
    // int checked_pid = 0;
    // read(fd, &checked_pid, sizeof(pid));
    if (flag) {
        // checkpoint
        fd = open("./myckpt", O_RDWR);
        if (fd == -1)
            printf("%s", strerror(errno));
        lseek(fd, 0, SEEK_END);
        write(fd, "000000000\n", sizeof(char)*10);
        int wl = write(fd, &context, sizeof(context));
        close(fd);
        exit(0);
    } else {
        // restore
        return;
    }
}

void _restore(char* filename) {
    struct ucontext context;
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

    fd = open(filename, O_RDONLY);
    if (fd == -1) {
        printf("%s", strerror(errno));
        return;
    }
    read(fd, &pid, sizeof(pid));
    read(fd, &fileMaps, sizeof(pid));
    read(fd, &memMaps, sizeof(pid));

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
        // write(1, buf, 10);
    }

    read(fd, buf, 10);
    // write(1, buf, 10);
    read(fd, &context, sizeof(context));
    close(fd);
    setcontext(&context);
}

void restore(char* filename) {
    void *stack_ptr = 0x5300000;
    memcpy(_filename, filename, strlen(filename));
    stack_ptr = mmap(stack_ptr, 0x1000, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (stack_ptr == -1) {
        printf("Stack allocation failed: %s", strerror(errno));
        return;
    }
    stack_ptr += 0x1000;

    asm volatile ("mov %0,%%rsp" : : "g" (stack_ptr) : "memory");
    _restore(_filename);
}
