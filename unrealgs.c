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

int main()
{
  printf("DOS simple unreal-mode, (c) Rene Rebe, ExactCODE; 2018\n");
  
  __seg_gs uint8_t* p = unrealptr(0xb8000);
  __seg_gs uint16_t* p16 = p;
  
  if (true)
    {
      for (int i = 80*2+1; i < 80 * 4; i += 2) {
	p[i] = 'a';
      }
      for (int i = 80*2; i < 80 * 4; i += 2) {
	p[i] =  'A';
      }

      
      for (int i = 0; i < 80; ++i) {
	p16[80*4 + i] = (i << 8) + 'X';
      }
      
      for (int i = 80*12; i < 80*25; ++i) {
	p16[i] = (i << 8) | (i & 0xff);
      }
    }
  
  //p.disable();
  
  printf("end.\n");
  return 0;
}
