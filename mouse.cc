/*
 * mouse test / demo
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


uint16_t mx, my, c;

static void __attribute__((noinline)) isr_mouse2() {
  // TODO: maybe unsave! should check if DOS is running!
  //printf("isr %d %d %x\n", mx, my, c);
}

extern "C"
// "naked" attribute new since gcc-7, do not support C in clang
void __attribute__((naked)) isr_mouse()
{
  // ax: cause
  // cx: x pos
  // dx: y pos
  // di: x count
  // si: y count
  // bx: button state
  
  // save all, and establish DS, assuming our CS == DS
  asm volatile (//"int3\n"
		"pusha\n"
		"push %%ds\n"
		"mov %%cs, %%ax\n" // not needed in dosbox?
		"mov %%ax, %%ds\n"
		: "=c"(mx), "=d"(my), "=a"(c)
		: /* no input */
		: /* no clobber*/);
  
  isr_mouse2();
  
  // return from interrupt 
  asm volatile (//"add $0x10, %%esp\n" // TODO: gross HACK!
		"pop %%ds\n"
		"popa\n"
		".code16\n"
		"retf\n" // NOT iret!
		".code16gcc\n"
		: /* no output */
		: /* no input */
		: /* no clobber*/);
}

uint8_t cursor[16][8] = {
  {2, 0, 0, 0, 0, 0, 0, 0},
  {2, 2, 0, 0, 0, 0, 0, 0},
  {2, 1, 2, 0, 0, 0, 0, 0},
  {2, 1, 1, 2, 0, 0, 0, 0},
  {2, 1, 1, 1, 2, 0, 0, 0},
  {2, 1, 1, 1, 1, 2, 0, 0},
  {2, 1, 1, 1, 1, 1, 2, 0},
  {2, 1, 1, 1, 1, 1, 1, 2},
  {2, 1, 1, 1, 1, 1, 2, 0},
  {2, 1, 1, 1, 1, 2, 0, 0},
  {2, 1, 2, 2, 2, 2, 0, 0},
  {2, 2, 0, 0, 2, 2, 0, 0},
  {0, 0, 0, 0, 0, 2, 2, 0},
  {0, 0, 0, 0, 0, 2, 2, 0},
  {0, 0, 0, 0, 0, 0, 2, 2},
  {0, 0, 0, 0, 0, 0, 2, 0},
};

void draw_cursor(unrealptr& p, uint16_t mx, uint16_t my)
{
  // clip to vga_w
  uint16_t xmax = vga_w - mx;
  if (xmax > 8) xmax = 8;
  else if (xmax <= 0) return;
  
  for (uint8_t _y = 0; _y < 16; ++_y) {
    uint32_t offset = (my + _y) * vga_w + mx;
    for (uint8_t _x = 0; _x < xmax; ++_x) {
      uint8_t c = cursor[_y][_x];
      if (c) {
	c = c == 2 ? 0xff : 0x49; // forground or background?
	uint8_t _ = p.get8(offset + _x);
	p.set8(offset + _x, _ ^ c);
      }
    }
  }
}

extern "C"
int main()
{
  uint16_t ret;
  
  printf("DOS mouse tests, (c) Rene Rebe, ExactCODE; 2018\n");
  
  ret = svga_mode_info(0x101, &modeinfo);
  printf("mode: %x, %d, %d %x\n", ret,
	 modeinfo.XResolution, modeinfo.YResolution, modeinfo.PhysBasePtr);
  ret = svga_mode(0x101 | (1 << 14) | (1 << 15)); // linear, no clear
  if (ret != 0x4f) {
    printf("mode set failed\n");
    return 1;
  }

  unrealptr p(modeinfo.PhysBasePtr);
  vga_w = modeinfo.XResolution;
  vga_h = modeinfo.YResolution;
  vga_planes = 1;
  
  ret = mouse_init();
  if (!ret) {
    printf("mouse init failed\n");
    return 1;
  }
  
  mouse_set_max(vga_w, vga_h);

  // save CS
  char* cs;
  asm volatile("mov %%cs, %%ax\n"
	       : "=a"(cs)
	       : /* no inputs */
	       : /* no other clobbers */);
  
  uint32_t orig_mask = 0x1f;
  void (*orig_isr)() =
    mouse_swapvect(&orig_mask, (void(*)()) (((unsigned)cs << 16) | (int)isr_mouse));
  if (!orig_isr) {
    printf("no orig mouse int vect?\n");
    return 1;
  }
  printf("orig: %p %x\n", orig_isr, orig_mask);
  
  
  {
    const uint8_t rbits = 3, gbits = 3, bbits = 2; 
    const uint8_t rmask = (1 << rbits) - 1,
      gmask = (1 << gbits) - 1,
      bmask = (1 << bbits) - 1;
    
    // program rgb-332-bit color palette
    if (true) {
      vgaDacPaletteBegin(0);
      for (uint16_t i = 0; i < 0x100; ++i) {
	uint8_t 
	  r = 0xff * ((i >> 0) & rmask) / rmask,
	  g = 0xff * ((i >> rbits) & gmask) / gmask,
	  b = 0xff * ((i >> (rbits + gbits)) & bmask) / bmask;
	
	//printf("%d 0x%x 0x%x 0x%x\n", i, r, g, b);
	vgaDacPaletteSet(r, g, b);
      }
    }
  }

  static uint16_t oldx = 0, oldy = 0;
  draw_cursor(p, oldx, oldy);
  while (true) {
    asm("hlt\n"); // sleep until next interrupt
    
    // if we are here, we (probably) got an interrupt
    
    // only re-draw if it really was the mouse
    if (mx != oldx || my != oldy) {
      // xor old cusor away
      draw_cursor(p, oldx, oldy);
      
      {
	static char text[24];
	sprintf(text, "%d %d   ", mx, my);
	
	vga_blit_text(p, 8, 8, text, 0xff, 0);
      }
      
      // draw new cursor
      //asm("int3\n");
      oldx = mx, oldy = my;
      draw_cursor(p, oldx, oldy);
    }
    
    if (kbhit())
      break;
  }
  
  vga_mode(0x03); // back ot text mode
  
  mouse_swapvect(&orig_mask, orig_isr);
  
  printf("end.\n");
  return 0;
}
