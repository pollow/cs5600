#include <stdio.h>
#include <stdlib.h>

void quick_sort(const char *a[], size_t len);
void print_array(const char * a[], const int size);

int main(void)
{
  const char * data[] = {"Joe", "Jenny", "Jill", "John",
                         "Jeff", "Joyce", "Jerry", "Janice",
                         "Jake", "Jonna", "Jack", "Jocelyn",
                         "Jessie", "Jess", "Janet", "Jane"};
  int size = 16;

  printf("Initial array:\n");
  print_array(data, size);

  quick_sort(data, size);

  printf("Sorted array:\n");
  print_array(data, size);
  exit(0);
}


int str_lt (const char *x, const char *y) {
  for (; *x!='\0' && *y!='\0'; x++, y++) {
    if ( *x < *y ) return 1;
    if ( *y < *x ) return 0;
  }
  if ( *y == '\0' ) return 0;
  else return 1;
}

void swap_str_ptrs(const char **s1, const char **s2)
{
  const char *tmp = *s1;
  *s1 = *s2;
  *s2 = tmp;
}

void quick_sort(const char *a[], size_t len) {
  if (len <= 1) {
    return;
  }

  // Partition the array around a pivot.
  int pivot = 0;
  for (int i = 0; i < len - 1; i++) {
    if (str_lt(a[i], a[len - 1])) {
      swap_str_ptrs(&a[i], &a[pivot]);
      pivot++;
    }
  }
  swap_str_ptrs(&a[pivot], &a[len - 1]);

  quick_sort(a, pivot);
  quick_sort(a + pivot + 1, len - pivot - 1);
}

void print_array(const char * a[], const int size)
{
  printf("[");
  for (int i = 0; i < size; i++) {
    printf(" %s", a[i]);
  }
  printf(" ]\n");
}
