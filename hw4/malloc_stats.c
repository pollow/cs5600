#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "malloc_utils.h"

void print_data(int arena_id, size_t system, size_t use, size_t malloc, size_t free) {
    char buf[256];
    if (arena_id == -1) {
        snprintf(buf, 256, "Total:\n");
        write(STDOUT_FILENO, buf, strlen(buf));
    } else {
        snprintf(buf, 256, "Arena %d:\n", arena_id);
        write(STDOUT_FILENO, buf, strlen(buf));
    }
    snprintf(buf, 256, "system bytes\t=\t%ld\n", system);
    write(STDOUT_FILENO, buf, strlen(buf));
    snprintf(buf, 256, "in use bytes\t=\t%ld\n", use);
    write(STDOUT_FILENO, buf, strlen(buf));
    snprintf(buf, 256, "malloc calls\t=\t%ld\n", malloc);
    write(STDOUT_FILENO, buf, strlen(buf));
    snprintf(buf, 256, "free calls\t=\t%ld\n", free);
    write(STDOUT_FILENO, buf, strlen(buf));
}

void malloc_stats() {
    int total_system = 0;
    int total_used= 0;
    int total_malloc = 0;
    int total_free = 0;

    for (size_t i = 0; i < num_cores; i++) {
        print_data(i,
                   BLOCK_SIZE << mem_zone_base[i].current,
                   mem_zone_base[i].used,
                   mem_zone_base[i].malloc,
                   mem_zone_base[i].free);
        total_system += BLOCK_SIZE << mem_zone_base[i].current;
        total_used += mem_zone_base[i].used;
        total_malloc += mem_zone_base[i].malloc;
        total_free += mem_zone_base[i].free;
    }

    print_data(-1, total_system, total_used, total_malloc, total_free);
}
