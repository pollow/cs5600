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

#include "ckpt.h"

void sighandler(int signum) {
    if (signum != SIGUSR2) {
        return;
    }
    checkpoint();
}

__attribute__ ((constructor))
void init() {
    signal(SIGUSR2, sighandler);
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
