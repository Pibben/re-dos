/*
 * OPL2/3 DOS test demo
 * Copyright (C) 2018 René Rebe, ExactCODE; Germany.
 * 
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; version 2.
 *
 * Alternatively, commercial licensing options are available from the
 * copyright holder ExactCODE GmbH Germany.
 */

// TODO: full tracker
// TODO: opl3
// TODO: SB

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

#include "vga.h"

// OPL3 (SB Pro II) Adlib Gold, etc.
#define inp inb
#define outp outb
#include "FM.C"

#define PIC_CHANNEL0 0x40
#define PIC_MODE_COMMAND 0x43
void pic_read() {
  
}

static uint16_t freq = 200;
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
  
  freq = freq + 2;
  fm_play_tone(1, freq, 63);
  // TODO: unsave! should check if DOS is running!
  printf("isr\n");

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
  const uint8_t timer_irq = 0x1c; // 8
  
  // save CS
  char* cs;
  asm volatile("mov %%cs, %%ax\n"
	       : "=a"(cs)
	       : /* no inputs */
	       : /* no other clobbers */);
  
  printf("opl2/3 test, (c) Rene Rebe, ExactCODE; 2018. cs: %x\n", cs);
  
  farptr ivt(0);
  printf("raw ivt 8 %x\n", ivt.get32(8*4));
  printf("raw ivt timer %x\n", ivt.get32(timer_irq * 4));

  printf("sizeof *void %d\n", sizeof(void*));
  printf("our isr: %p\n", isr_timer);
  isr_system = dos_getvect(timer_irq);
  printf("cur isr: %p\n", isr_system);
  dos_setvect(timer_irq, (void(*)()) (((unsigned)cs << 16) | (int)isr_timer));
  printf("new isr: %p\n", dos_getvect(timer_irq));
  
  if (!fm_detect()) {
    printf("ERROR: FM not detected \n");
    //return 1;
  }
  else
    printf("FM detected \n");
  
  fm_reset();
  fm_load_patch(0, fm_get_patch_sine());
  fm_load_patch(1, fm_get_patch_sine());
  fm_load_patch(2, fm_get_patch_sine());
  fm_load_patch(3, fm_get_patch_sine());
  fm_load_patch(4, fm_get_patch_sine());
  fm_load_patch(5, fm_get_patch_sine());
  fm_load_patch(6, fm_get_patch_sine());
  fm_load_patch(7, fm_get_patch_sine());
  
  fm_play_tone(0, freq, 63);
  
  printf("running\n");
  while (freq < 220)
    ;
  printf("end.\n");
  
  dos_setvect(timer_irq, isr_system);

  fm_stop_tone(0);
  fm_stop_tone(1);
  
  return 0;
}
