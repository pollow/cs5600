#ifndef CKPT_H
#define CKPT_H

struct MemoryRegion {
    char* startAddr;
    char* endAddr;
    int isReadable;
    int isWriteable;
    int isExecutabl;
    unsigned int offset;
};

void checkpoint();

void restore();

#endif
