/*
 * random and PIC benchmark demo
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

#include "libc/stdlib.h" // random

#include "dos.h"
#include "libc/stdio.h"

static volatile uint16_t ticks = 0;

static void (*isr_system)();

extern "C"
// "naked" attribute new in gcc-8, not fully supported in clang
void __attribute__((naked)) isr_timer()
{
  // save all, and establish DS, assuming our CS == DS
  asm volatile (//"int3\n"
		"pusha\n"
		//"cli\n"
		"mov %%cs, %%ax\n" // not needed in dosbox?
		"mov %%ax, %%ds\n"
		: /* no output */
		: /* no input */
		: /* no clobber*/);
 
  ++ticks;
  // TODO: unsave! should check if DOS is running!
  //printf("isr %d\n", ticks);

  // call original
  asm volatile (//"mov (%0), %%eax\n"
		".code16\n"
		"pushf\n" // for iret return to us!		
		"lcall *(%0)\n" // * absolute jump
		".code16gcc\n"
		: /* no output */
		: "m"(isr_system)
		: /* no clobber*/);

  // return from interrupt
  asm volatile ("popa\n"
		".code16\n"
		"iret\n"
		".code16gcc\n"
		: /* no output */
		: /* no input */
		: /* no clobber*/);
}

extern "C"
int main(void)
{
  printf("random timer, (c) Rene Rebe, ExactCODE; 2018\n");

  const uint8_t timer_irq = 0x1c; // 8
  
  // save CS
  char* cs;
  asm volatile("mov %%cs, %%ax\n"
	       : "=a"(cs)
	       : /* no inputs */
	       : /* no other clobbers */);
  
  farptr ivt(0);
  printf("raw ivt 8 %x\n", ivt.get32(8*4));
  printf("raw ivt timer %x\n", ivt.get32(timer_irq * 4));

  printf("sizeof *void %d\n", sizeof(void*));
  printf("our isr: %p\n", isr_timer);
  isr_system = dos_getvect(timer_irq);
  printf("cur isr: %p\n", isr_system);
  dos_setvect(timer_irq, (void(*)()) (((int)cs << 16) | (int)isr_timer));
  printf("new isr: %p\n", dos_getvect(timer_irq));
  
  printf("running\n");
  
  uint32_t cycles = 0;
  uint32_t r = 0;
  while (ticks < 9) {
    ++cycles;
    register int _ = random();
    r ^= _;
    
  }
  printf("end: %d %x\n", cycles * 18 / ticks, r);

  dos_setvect(timer_irq, isr_system);
  
  return 0;
}
