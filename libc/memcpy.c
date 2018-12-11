/* Copyright (c) 2010 ExactCODE GmbH. All rights reserved. */

void* memcpy(void* __restrict__ s1, const void* __restrict__ s2, size_t n)
{
  size_t i;
  for (i = 0; i < n; ++i)
    ((char*)s1)[i] = ((const char*)s2)[i];

  return s1;
}
