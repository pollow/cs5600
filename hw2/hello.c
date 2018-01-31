#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <ucontext.h>
#include <string.h>
#include <ctype.h>
#include <assert.h>
#include <errno.h>

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

void checkpoint(int signum) {
    if (signum != SIGUSR2) {
        return;
    }

    int flag = 0;
    int pid = getpid();
    char buf[256];
    sprintf(buf, "/proc/%d/maps", pid);
    FILE* fp = fopen(buf, "r");
    struct ucontext context;

    // create checkpoint file and save memory
    int fd = open("./myckpt", O_CREAT | O_RDWR, S_IRWXU);
    if (fd == -1)
        printf("%s", strerror(errno));
    int fileMaps = 0, memMaps = 0;
    write(fd, &pid, sizeof(pid));
    write(fd, &fileMaps, sizeof(fileMaps)); // number of file mapping
    write(fd, &memMaps, sizeof(memMaps)); // number of memory region mapping
    struct MemoryRegion mr, stack, heap;
    struct MemoryRegion mmr[20]; // 20 is enough for now
    while (fgets(buf, 256, fp)) {
        char r, w, x, p, *fileName;
        sscanf(buf,
               "%p-%p %c%c%c%c %x",
               &mr.startAddr,
               &mr.endAddr,
               &r,
               &w,
               &x,
               &p,
               &mr.offset);
        mr.isReadable = r == 'r';
        mr.isWriteable = w == 'w';
        mr.isExecutabl = x == 'x';
        int len = strlen(buf);
        int i = len - 2; // last char is \n
        while (!isspace(buf[i])) {
            i--;
        }
        /* printf("%p, %p, %d, %s", mr.startAddr, mr.endAddr, mr.offset, buf+i+1);
        if (buf[i+1] != '[' && buf[i+1] != '\n' && !mr.isWriteable) {
            ssize_t wl = write(fd, &mr, sizeof(mr));
            if (wl == -1) {
                printf("%s\n", strerror(errno));
            }
            assert(wl == sizeof(mr));
            wl = write(fd, buf+i+1, len-i-1);
            if (wl == -1) {
                printf("%s\n", strerror(errno));
            }
            assert(wl == len-i-1);
            fileMaps++;
            } else */if (buf[i+2] != 'v' && mr.isReadable) {
            mmr[memMaps] = mr;
            memMaps++;
        }
    }

    for (int i = 0; i < memMaps; i++) {
        printf("%lx\n", mmr[i].startAddr);
        ssize_t wl = write(fd, &mmr[i], sizeof(mr));
        if (wl == -1) {
            printf("%s\n", strerror(errno));
        }
        assert(wl == sizeof(mr));
        wl = write(fd, mmr[i].startAddr, mmr[i].endAddr - mmr[i].startAddr);
        if (wl == -1) {
            printf("%s\n", strerror(errno));
        }
        assert(wl == mmr[i].endAddr - mmr[i].startAddr);
        wl = write(fd, "000000000\n", sizeof(char)*10);
        assert(wl == 10);
    }

    lseek(fd, sizeof(pid), SEEK_SET);
    write(fd, &fileMaps, sizeof(fileMaps));
    write(fd, &memMaps, sizeof(memMaps));
    fclose(fp);
    close(fd);

    flag = 1;
    getcontext(&context);

    write(1, "FUCK!\n", 6);
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
        write(1, "FUCK!\n", 6);
        return;
    }
}


__attribute__ ((constructor))
void init() {
    signal(SIGUSR2, checkpoint);
}

void loop() {
    write(1, "FUCK!\n", 6);
    while (1) {
        printf(".");
        fflush(stdout);
        sleep(1);
    }
}

int main() {
    loop();
    return 0;
}
