/*
 * S3/Virge hardware accel testing
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

/* S3/Virge mmio, the beginnign is Trio compatible
   0x000 0000 16M linear address
   0x100 0000 image data transfer 32k
   0x100 8000 pci config space
   0x100 8180 stream processor
   0x100 8200 memory port
   0x100 83b0 vga crt 3bx
   0x100 83c0 ...
   0x100 83d0 ...
   0x100 8504 subs status enhanced
   0x100 850c advaned function
   0x100 8580 dma control
   0x100 a000 color pattern
   0x100 a400 bitblt
   0x100 a800 2d line
   0x100 ac00 2d poly fill
   0x100 b000 3d line draw
   0x100 b400 3d triangle
   0x100 ff00 local bus
 */


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

// we probably could unlock all once?
void s3_lock(uint8_t val = 0)
{
  outb(CRTC_INDEX, 0x39);  // CR39
  outb(CRTC_INDEX + 1, val); // lock system control registers 
}

void s3_unlock()
{
  return s3_lock(0xa0); // unlock system control registers 
}

void s3_cursor(uint16_t addr, uint32_t fg = 0xffffff, uint32_t bg = 0)
{
  s3_unlock();
  
  // foreground color, R, G, B "stack"
  outb(CRTC_INDEX, 0x4a);
  outb(CRTC_INDEX + 1, fg);
  outb(CRTC_INDEX + 1, fg >> 8);
  outb(CRTC_INDEX + 1, fg >> 16);

  // background, ...
  outb(CRTC_INDEX, 0x4b);
  outb(CRTC_INDEX + 1, bg);
  outb(CRTC_INDEX + 1, bg >> 8);
  outb(CRTC_INDEX + 1, bg >> 16);
  
  // cursor shape memory location 1024 byte "segment"
  outb(CRTC_INDEX, 0x4c);
  outb(CRTC_INDEX + 1, addr >> 8);

  outb(CRTC_INDEX, 0x4d);
  outb(CRTC_INDEX + 1, addr);
   
  s3_lock();
}

void s3_cursor_enable(uint8_t state)
{
  outb(CRTC_INDEX, 0x45);  // CR45
  outb(CRTC_INDEX + 1, state); // cursor en/dis-able
}

void s3_cursor_pos(uint16_t x, uint16_t y)
{
  s3_unlock();
  
  outb(CRTC_INDEX, 0x46);
  outb(CRTC_INDEX + 1, x >> 8);
  outb(CRTC_INDEX, 0x47);
  outb(CRTC_INDEX + 1, x & 0xff);
  outb(CRTC_INDEX, 0x48);
  outb(CRTC_INDEX + 1, y >> 8);
  outb(CRTC_INDEX, 0x49);
  outb(CRTC_INDEX + 1, y & 0xff);
  
  outb(CRTC_INDEX, 0x4e);
  outb(CRTC_INDEX + 1, 0); // x/y-hot-offset
  outb(CRTC_INDEX, 0x4f);
  outb(CRTC_INDEX + 1, 0);
  
  s3_lock();
}

// This are old new Trio compatible registers
enum s3regs {
  ALT_CURXY = 0x8100,
  ALT_STEP = 0x8108,
  ERR_TERM = 0x8110,
  CMD = 0x8118,
  BGGD_MIX = 0x8134,
  FRGD_MIX = 0x8136,
  BGGD_COLOR = 0x8120,
  FRGD_COLOR = 0x8124,
  PIX_CNTL = 0x8140,
  ALT_PCNT = 0x8148,
  MIN_AXIS_PCNT = ALT_PCNT,
  MAJ_AXIS_PCNT = 0x814a,
};

static const uint32_t iobase = 0x1000000;

void s3_line(unrealptr& p,
	     int16_t x1, int16_t y1, int16_t x2, int16_t y2,
	     uint32_t color)
{
  // TODO: wait not busy!
  const int16_t dx = ABS(x2 - x1), dy = ABS(y2 - y1);
  const int16_t max = MAX(dx, dy);
  const int16_t min = MIN(dx, dy);
  const uint8_t xdir = x2 < x1 ? 0 : 1, ydir = y2 < y1 ? 0 : 1;
  const uint8_t ymajor = dy > dx ? 1 : 0;

  uint16_t errterm = 2 * min - max;
  uint16_t step = 2 * (min - max);
  
  p.set16(iobase + FRGD_MIX, 0x0027); // 0:3 mix-type, 5:6: clr-src
  p.set32(iobase + FRGD_COLOR, color); // color
  p.set16(iobase + PIX_CNTL, 0xa000); // FRGD_MIX as source
  
  p.set32(iobase + ALT_CURXY, (x1 << 16) | y1); // line start
  p.set16(iobase + MAJ_AXIS_PCNT, max - 1); // length on maj axis - 1
  p.set32(iobase + ALT_STEP, (step << 16) | 2 * min); // diag and axial step
  p.set16(iobase + ERR_TERM, errterm - (xdir ? 0 : 1));
  
  // 15:13 CMD 12: SWAP 10-9: BUS SIZE 8: WAIT
  // 7-5: DIR 4: DRAW 3: DIR-TYPE 2: LAST-OFF 1:  PX MD 0: 1
  p.set16(iobase + CMD, 0x2011 |
	  (xdir << 5) | (ymajor << 6) | (ydir << 7)); // LINE CMD
}
  
void s3_fill(unrealptr& p,
	     uint16_t sx, uint16_t sy, uint16_t w, uint16_t h,
	     uint32_t color)
{
  // TODO: wait not busy!
  --w; --h;
  
  p.set16(iobase + FRGD_MIX, 0x0027); // 0:3 mix-type, 5:6: clr-src
  p.set32(iobase + FRGD_COLOR, color); // color
  p.set16(iobase + PIX_CNTL, 0xa000); // FRGD_MIX as source
  
  p.set32(iobase + ALT_CURXY, (sx << 16) | sy); // src start
  p.set32(iobase + ALT_PCNT, (w << 16) | h); // rect size
  
  // 15:13 CMD 12: SWAP 10-9: BUS SIZE 8: WAIT
  // 7-5: DIR 4: DRAW 3: DIR-TYPE 2: LAST-OFF 1:  PX MD 0: 1
  p.set16(iobase + CMD, 0x40b1); // FILL CMD
}

void s3_blit(unrealptr& p,
	uint16_t sx, uint16_t sy, uint16_t w, uint16_t h,
	uint16_t dx, uint16_t dy)
{
  // TODO: wait not busy!
  --w; --h;
  uint8_t xdir = 1, ydir = 1;
  // TODO: check and swap coordinates, and direction bits
  p.set16(iobase + PIX_CNTL, 0xa000); // FRGD_MIX as source
  p.set16(iobase + FRGD_MIX, 0x0067); // 0:3 mix-type, 5:6: clr-src
  p.set32(iobase + ALT_CURXY, (sx << 16) | sy); // src start
  p.set32(iobase + ALT_STEP, (dx << 16) | dy); // dest start
  p.set32(iobase + ALT_PCNT, (w << 16) | h); // dest start
  // 15:13 CMD 12: SWAP 10-9: BUS SIZE 8: WAIT
  // 7-5: DIR 4: DRAW 3: DIR-TYPE 2: LAST-OFF 1:  PX MD 0: 1
  p.set16(iobase + CMD, 0xc011 | (xdir << 5) | (ydir << 7)); // BLIT CMD
}

extern "C"
int main()
{
  uint16_t ret;
  
  printf("S3 Virge accel tests, (c) Rene Rebe, ExactCODE; 2018\n");
  
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
  
  // convert our cursor image to the HW defined layout
  {
    uint32_t cur_addr = 1024 * 1024;
    // 2x 64x64 bit: AND and OR masks image
    // win-compat, X11 is CR55 bit 4: MS/X11
    uint16_t i = 0;
    for (uint8_t y = 0; y < 64; ++y) {
      uint16_t mask = 0, color = 0;
      for (uint8_t x = 0; x < 64; ++x) {
	uint8_t m, c;
	if (x >= 8 || y >= 16) {
	  m = 1; c = 0;
	} else {
	  m = cursor[y][x] == 0 ? 1 : 0;
	  c = cursor[y][x] == 2 ? 1 : 0;
	}
	
	mask = mask << 1 | m;
	color = color << 1 | c;
	if (x % 16 == 15) {
	  p.set16(cur_addr + i, mask); i += 2;
	  p.set16(cur_addr + i, color); i += 2;
	  mask = color = 0;
	}
      }
    }
    
    s3_cursor(cur_addr / 1024, 0xffffff, 0x000000);
    s3_cursor_enable(1);
  }
  
  s3_fill(p, vga_w/4, vga_h/4, 200, 100, 0x82);
  
  // test all quadrants ;-)
  for (int i = 0; i < 360; i += 15) {
    // rotate a line vector
    const float angle = M_PI * i / 180;
    const float _sin = sinf(angle);
    const float _cos = cosf(angle);
    const int x = 64, y = 0;
    const int dx = x * _cos + y * _sin;
    const int dy = x * _sin + y * _cos;
    const int cx = 128, cy = 128; // center
    s3_line(p, cx + dx/10, cy + dy/10, cx+dx, cy+dy, i); // "random" color from angle
  }
  
  s3_blit(p, 0, 0, vga_w/2, vga_w/2, vga_w/2, vga_h/2);

  static uint16_t oldx = 0, oldy = 0;
  while (true) {
    asm("hlt\n"); // sleep until next interrupt
    
    // if we are here, we (probably) got an interrupt
    
    // only re-draw if it really was the mouse
    if (mx != oldx || my != oldy) {
      
      {
	static char text[24];
	sprintf(text, "%d %d   ", mx, my);
	vga_blit_text(p, 8, 8, text, 0xff, 0);
      }

      // draw test line
      s3_line(p, vga_w / 2, vga_h / 2, mx, my, 0x82);
	
      //asm("int3\n");
      oldx = mx, oldy = my;
      s3_cursor_pos(oldx, oldy);
    }
    
    if (kbhit())
      break;
  }
  
  vga_mode(0x03); // back ot text mode
  
  mouse_swapvect(&orig_mask, orig_isr);
  
  printf("end.\n");
  return 0;
}
