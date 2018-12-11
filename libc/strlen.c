/* Copyright (c) 2010 ExactCODE GmbH. All rights reserved. */

size_t strlen(const char* s)
{
  const char* se = s;
  while (*se) ++se;
  return se - s;
}
