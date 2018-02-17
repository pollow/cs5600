#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <malloc.h>

int main() {
    char *str = (char *)malloc(1024);
    malloc_stats();

    return 0;
}
