/* Copyright (c) 2010 ExactCODE GmbH. All rights reserved. */

void* memmove(void *s1, const void *s2, size_t n)
{
  size_t i;
  if (s1 == s2) {
    // nothing, optimizatino for equal pointers
  } else if (s1 < s2) {
    for (i = 0; i < n; ++i)
      ((char*)s1)[i] = ((const char*)s2)[i];
  } else {
    for (i = n-1; i >= 0; --i)
      ((char*)s1)[i] = ((const char*)s2)[i];
  }

  return s1;
}
