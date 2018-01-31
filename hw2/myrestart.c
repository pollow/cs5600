#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <string.h>
#include <ctype.h>

#include "ckpt.h"

char filename[256];

int main(int argc, char** argv) {
    memcpy(filename, argv[1], strlen(argv[1]));
    write(1, "restoring from checkpoint image: ", 33);
    write(1, filename, strlen(filename));
    write(1, "\n", 1);

    restore(filename);
}
