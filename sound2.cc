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

static volatile uint16_t ticks = 0;

static const uint16_t freq[8] = {
  300, 239, 267, 354,
  255, 311, 295, 326,
};

static const uint16_t freq2[] = {
  2*147, 2*294, // D
  2*165, 2*329, // E
  2*185, 2*370, // F#
  2*196, 2*392, // G
};

static void (*isr_system)();

static void __attribute__((noinline)) isr_timer2() {
  ++ticks;
  
  // call original
  asm volatile (//"mov (%0), %%eax\n"
		".code16\n"
		"pushf\n" // for iret return to us!		
		"lcall *(%0)\n" // * absolute jump
		".code16gcc\n"
		: /* no output */
		: "m"(isr_system)
		: /* no clobber*/);

  // TODO: unsave! should check if DOS is running!
  //printf("isr %d\n", ticks);
  
  static const uint16_t tticks = 18.2 * 10;
  if (ticks <= tticks) {
    uint16_t a = tticks - ticks, b = ticks;
    for (uint8_t i = 0; i < sizeof(freq) / sizeof(*freq); ++i) {
      fm_play_tone(i, (a * freq[i] + b * freq2[i]) / tticks, 63);
    }
  }
}

extern "C"
// "naked" attribute new since gcc-7, do not support C in clang
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
  
  isr_timer2();
  
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
  printf("opl2/3 multi note, (c) Rene Rebe, ExactCODE; 2018\n");

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
  
  if (!fm_detect()) {
    printf("ERROR: FM not detected \n");
    return 1;
  }
  printf("FM detected \n");
  
  fm_reset();
  
  for (uint8_t i = 0; i < sizeof(freq) / sizeof(*freq); ++i)
    fm_load_patch(i, fm_get_patch_sine());
  
  for (uint8_t i = 0; i < sizeof(freq) / sizeof(*freq); ++i)
    fm_play_tone(i, freq[i], 63);
  
  printf("running\n");
  while (true) {
    if (kbhit())
      break;
  }
  printf("end.\n");

  for (uint8_t i = 0; i < sizeof(freq) / sizeof(*freq); ++i)
    fm_stop_tone(i);
  
  dos_setvect(timer_irq, isr_system);
  
  return 0;
}
