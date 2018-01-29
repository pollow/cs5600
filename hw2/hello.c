#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <ucontext.h>

struct MemoryRegion
{
    void* startAddr;
    void* endAddr;
    int isReadable;
    int isWriteable;
    int isExecutabl;
    char* fileName;
};

void checkpoint(int signum) {
    if (signum != SIGUSR2) {
        return;
    }

    int pid = getpid();
    char[100] buf;
    sscanf(buf, "/proc/%d/maps", pid);
    FILE* fopen(buf, "r");
    printf("WTF?");
}


__attribute__ ((constructor))
void init() {
    signal(SIGUSR2, checkpoint);
}

void loop() {
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
