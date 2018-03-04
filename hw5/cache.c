#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>

// This snippet of code is referenced from:
// https://stackoverflow.com/questions/13772567/get-cpu-cycle-count
//  Windows
#ifdef _WIN32

#include <intrin.h>
uint64_t rdtsc(){
    return __rdtsc();
}

//  Linux/GCC
#else

uint64_t rdtsc(){
    unsigned int lo,hi;
    __asm__ __volatile__ ("rdtsc" : "=a" (lo), "=d" (hi));
    return ((uint64_t)hi << 32) | lo;
}

#endif

int main() {
    // 64 KB
    int M2 = 2 << 20;
    int s = 4;

    for (int cap = 1 << 19; cap <= (8 << 22); cap *= 2) {
        for(int s = 4; s <= cap / 4; s *= 2) {
            int *p = (int *)malloc(cap);
            long sum = 0;
            for (int i = 0; i < cap / sizeof(int); i+=s) {
                p[i] = 0;
            }

            uint64_t start = rdtsc();
            for (int k = 0; k < 100; k++) {
                int index = 0;
                for (int i = 0; i < cap / sizeof(int); i+=s) {
                    sum += (p[i]++);
                }
            }
            uint64_t stop = rdtsc();

            double iters = cap / sizeof(int) / s * 100;
            double clock_per_it = (double)(stop - start) / iters;
            // printf("cap: %dKB, strip: %d, sum: %ld, clock_per_it: %lf\n", cap >> 10, s, sum, clock_per_it);
            printf("%d, %d, %ld, %lf\n", cap >> 10, s, sum, clock_per_it);
            free(p);
        }
    }

    return 0;
}
