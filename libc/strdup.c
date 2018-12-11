/* Copyright (c) 2010 ExactCODE GmbH. All rights reserved. */

char* strdup(const char* s)
{
  char* s2;
  int n = strlen(s) + 1;
  s2 = malloc(n);

  memcpy(s2, s, n);

  return s2;
}
