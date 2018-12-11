/*
 * "Unreal" ptr DOS SVGA demo
 * Copyright (C) 2018 René Rebe, ExactCODE; Germany.
 * 
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; version 2.
 *
 * Alternatively, commercial licensing options are available from the
 * copyright holder ExactCODE GmbH Germany.
 */

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

struct ModeInfoBlock modeinfo;

extern "C"
int main(void)
{
  printf("DOS simple unreal-mode, (c) Rene Rebe, ExactCODE; 2018\n");
  
  unrealptr p(0xb8000);
  if (true)
    {
      for (int i = 80*2+1; i < 80 * 4; i += 2) {
	p.set8(160*4, 'a');
      }
      
      for (int i = 80*2; i < 80 * 4; i += 2) {
	p.set8(i, 'A');
      }
      
      for (int i = 0; i < 80; ++i) {
	p.set16(80*4 + i * 2, (i << 8) + 'X');
      }
      
      for (int i = 80*12; i < 80*25; ++i) {
	p.set16(i * 2, (i << 8) | (i & 0xff));
	//p.set8(i * 2, i); p.set8(i * 2 + 1, i);
      }
    }
  
  //p.disable();
  
  printf("end.\n");
  return 0;
}
