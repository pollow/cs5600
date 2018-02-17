#define _GNU_SOURCE

#include <unistd.h>
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <sched.h>
#include <assert.h>
#include <pthread.h>

#include "malloc.h"

void *malloc_test(int id) {
    size_t num_cores = sysconf( _SC_NPROCESSORS_ONLN );
    pthread_t self = pthread_self();
    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    CPU_SET(id % num_cores, &cpuset);
    pthread_setaffinity_np(self, sizeof(cpu_set_t), &cpuset);

    while (1) {
        void *t = malloc(1024);
        free(t);
    }
    return NULL;
}

void fork_test(void *args) {
    system("sleep 0.01");
    pid_t pid = fork();
    if (pid == 0) {
        // child process
        write(1, "???\n", 4);
        malloc(1024);
        write(1, "hhh\n", 4);
    } else {
        // parrent process
        return;
    }
}

int main() {
    pthread_t thid[4];
    void *ret;
    if (pthread_create(&thid[0], NULL, fork_test, NULL) != 0) {
        perror("pthread_create() fork_test error");
        exit(1);
    }
    for (int i = 1; i < 4; i++) {
        if (pthread_create(&thid[i], NULL, malloc_test, i) != 0) {
            perror("pthread_create() error");
            exit(1);
        }
    }

    for (int i = 0; i < 4; i++) {
        if (pthread_join(thid[i], &ret) != 0) {
            perror("pthread_create() error");
            exit(3);
        }
    }
    malloc_stats();
    return 0;
}
