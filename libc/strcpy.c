/* Copyright (c) 2010 ExactCODE GmbH. All rights reserved. */

char* strcpy(char* restrict s1, const char* restrict s2)
{
  size_t i;
  for (i = 0; s2[i]; ++i)
    s1[i] = s2[i];
  s1[i] = 0;

  return s1;
}
