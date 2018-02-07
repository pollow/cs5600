#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include "malloc.h"

int main(int argc, char **argv)
{
  size_t size = 12;
  void *p[512];
  void *t = memalign(0x20000, 0x500);
  printf("memalign: %p\n", t);
  free(t);
  for(int k = 0; k < 10; k++) {
      for (int i = 0; i < 51; i++) {
          p[i] = malloc(size);
      }
      for (int i = 0; i < 51; i++) {
          free(p[i]);
      }
  }
  return 0;
}
