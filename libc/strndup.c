/* Copyright (c) 2010 ExactCODE GmbH. All rights reserved. */

#include <stdlib.h> /* malloc */
#include <string.h> /* size_t */

char* strndup(const char* s, size_t n)
{
  char* s2 = malloc(n + 1);
  if (s2) {
    strncpy(s2, s, n);
    s2[n] = 0;
  }
  return s2;
}
