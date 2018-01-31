#include <stdio.h>
#include <stdlib.h>

#include "ckpt.h"

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
