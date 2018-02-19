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

    size_t size = 12;
    void *p[512];
    void *t = memalign(0x20000, 0x500);
    printf("memalign: %p\n", t);
    free(t);
    for(int k = 0; k < 10; k++) {
        for (int i = 0; i < 512; i++) {
            p[i] = malloc(size);
        }
        for (int i = 0; i < 512; i++) {
            free(p[i]);
        }
    }
    return NULL;
}

int main(int argc, char **argv)
{
    pthread_t thid[4];
    void *ret;
    for (int i = 0; i < 4; i++) {
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
