/* Copyright (c) 2010 ExactCODE GmbH. All rights reserved. */

void* memset(void* b, int c, size_t len)
{
   size_t i;
   for (i = 0; i < len; i++)
     ((char*)b)[i] = c;
   return b;
}
