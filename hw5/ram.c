#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <time.h>
#include <assert.h>

const int mega = 1 << 20;

int main() {
    int *start = sbrk(0);
    printf("Start: %p\n", start);
    printf("%d MB: %lf seconds used, sum: %lld\n", 0, 0.0, 0);
    start = sbrk(0);
    printf("Start: %p\n", start);
    size_t size = 0;
    float sum_delta = 0;
    float avg_delta = 0;
    float prev_time = 0;

    int i = 0;
    while(1) {
        int *tmp = sbrk(mega);
        i++;
        size += mega / sizeof(int);
        long long sum = 0;
        clock_t sc = clock();
        for(int k = 0; k < 2; k++) {
            int *cur = start;
            for (int j = 0; j < size; j++) {
                (*cur)++;
                sum += *cur;
                cur++;
            }
        }
        clock_t ec = clock();
        float time_spent = (ec - sc) * 1.0 / CLOCKS_PER_SEC;
        float delta = time_spent - prev_time;
        if (i > 1 && delta > 3 * avg_delta) {
            printf("GOTCHA! Total available RAM size is %dMB\n", i);
            return 1;
        }
        sum_delta += delta;
        avg_delta = sum_delta / i;
        printf("%d MB: %lf seconds used, sum: %lld\n", i, time_spent, sum);
    }

    return 0;
}
