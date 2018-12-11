/* Copyright (c) 2010-2011 ExactCODE GmbH. All rights reserved. */

#include <unistd.h> // size_t

static inline void swap(char* a, char* b, int n)
{
  int i;
  for (i = 0; i < n; ++i) {
    char t = a[i];
    a[i] = b[i];
    b[i] = t;
  }
}

void qsort(void* base, size_t nel, size_t width, int (*compar)(const void *, const void *))
{
  if (nel <= 1) return; // anything to sort?

  // optimize worst case performance, start from the centre
  int pivot = nel / 2;

  // move pivot to the end and sort
  int i;
  swap(base + pivot * width, base + (nel - 1) * width, width);
  for (i = 0, pivot = 0; i < nel - 1; ++i) {
    if (compar(base + i * width, base + (nel - 1) * width) < 0)
    {
      swap(base + i * width, base + pivot * width, width);
      ++pivot;
    }
  }
  // swap into final pos
  swap(base + (nel - 1) * width, base + pivot * width, width);

  // optimize: tail call the bigger partition
  if (pivot < nel - pivot - 1) {
    qsort(base, pivot, width, compar);
    return qsort(base + (pivot+1) * width, nel - pivot - 1,  width, compar);
  } else {
    qsort(base + (pivot+1) * width, nel - pivot - 1,  width, compar);
    return qsort(base, pivot, width, compar);
  }
}

#ifdef TEST
#include <stdio.h>

int comp(const void* a, const void* b)
{
  int ret = *(const int*)a - *(const int*)b;
  return ret;
}

int main()
{
  int array[] = { 100, 2, 87, 700, -1, 66, 23, 42 };

  qsort(array, sizeof(array) / sizeof(*array), sizeof(*array), &comp);

  int i;
  for (i = 0; i < sizeof(array) / sizeof(*array); ++i) {
    printf("%d ", array[i]);
  }
  printf("\n");

  return 0;
}
#endif
