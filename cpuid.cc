/*
 * cpuid
 * Copyright (C) 2018 René Rebe, ExactCODE; Germany.
 * 
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; version 2.
 *
 * Alternatively, commercial licensing options are available from the
 * copyright holder ExactCODE GmbH Germany.
 */

// TODO: must currently come first!
asm("call main\n"
    "mov $0x4c, %ah\n"
    "int $0x21\n");

#ifndef DOS
#define DOS 1
#endif

#include "libc/inttypes.h"
typedef int size_t;
typedef unsigned char uchar;
#include "libc/strlen.c"
#include "libc/math.h"

#include <stdarg.h>

#include "dos.h"
#include "libc/stdio.h"

#define __cpuid(q, a, b, c, d) \
    __asm__("xchg %%ebx, %1\n\t" \
            "cpuid\n\t" \
            "xchg  %%ebx, %1\n\t" \
            : "=a"(a), "=r"(b), "=c"(c), "=d"(d) \
            : "a"(q))

void cpuid(uint32_t q, uint32_t& a, uint32_t& b, uint32_t& c, uint32_t& d)
{
  __cpuid(q, a, b, c, d);
}

extern "C"
int main(void)
{
  printf("cpuid, (c) Rene Rebe, ExactCODE; 2018.\n");
  
  // cpuid vendor ID string
  uint32_t a, b, c, d;
  cpuid(0, a, b, c, d);
  if (a > 0) {
    printf("max: %d: %.4s", a, &b);
    printf("%.4s", &d);
    printf("%.4s\n", &c);
  }
  
  // cpuid brand string
  uint32_t maxext;
  cpuid(0x80000000, maxext, b, c, d);
  printf("mex ext: %x\n", maxext);
  
  if (maxext > 0x80000002) {
    for (uint32_t i = 0x80000002; i <= 0x80000004; ++i) {
      cpuid(i, a, b, c, d);
      printf("%.4s", &a);
      printf("%.4s", &b);
      printf("%.4s", &c);
      printf("%.4s", &d);
    }
    printf("\n");
  }
  
  return 0;
}
