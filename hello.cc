/*
 * DOS/GCC hello world tests.
 * Copyright (C) 2018 René Rebe, ExactCODE; Germany.
 * 
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; version 2.
 *
 * Alternatively, commercial licensing options are available from the
 * copyright holder ExactCODE GmbH Germany.
 */

#define DOS_WANT_ARGS

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

#include "vga.h"

extern "C"
int main(int argc, const char** argv)
{
  printf("DOS gcc tests, (c) Rene Rebe, ExactCODE; %d.\n", 2018);
  
  /*for (int i = 0; i < argc; ++i) {
    printf("%d %s\n", i, argv[i]);
    } */
  
  const char* blaster = getenv("BLASTER");
  if (blaster)
    printf("BLASTER=%s\n", blaster);
  
  printf("returning.\n");
  return 0;
}
