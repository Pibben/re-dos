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

// TODO:
// * double check fractional triangle positions!
// * finish & test double-buffer on real hw
// * 8 bit DST flat triangle (lines work!)
// * full texture details & perspective correction
// * some more software engine API
// * s3d wait for free FIFO?
// * s3d ring buffer test example

#ifndef DOS
#define DOS 1
#endif

typedef int size_t;

#include "libc/inttypes.h"
typedef int size_;
typedef unsigned char uchar;
#include "libc/strlen.c"
#include "libc/math.h"

#include <stdarg.h>
#include "dos.h"
#include "libc/stdio.h"
#include "libc/stdlib.h"
#include "libc/string.h"

#include "vga.h"
#include "cursor.h"

static uint32_t vga_fb = 0; // current frame base for double-buffering

// re-define vga_blit_text for double_buffering

static void _vga_blit_text(unrealptr& p, uint16_t x, uint16_t y,
			   const char* t, uint32_t fg = 0xff, uint32_t bg = 0)
{
  // assumes one double-buffer directly follows after first
  vga_blit_text(p, x, vga_fb == 0 ? y : vga_h + y, t, fg, bg);
}
#define vga_blit_text _vga_blit_text


// RGB helpers
static inline uint8_t RED(uint32_t v) 	{ return (v >> 0) & 0xff; }
static inline uint8_t GREEN(uint32_t v)	{ return (v >> 8) & 0xff; }
static inline uint8_t BLUE(uint32_t v)	{ return (v >>16) & 0xff; }
static inline uint8_t ALPHA(uint32_t v) { return (v >>24) & 0xff; }

typedef int16_t vertex[3];
typedef float vertexf[3];

// fixed point helpers - TODO: w/ full precision by bit repetition
static inline int16_t S87(int16_t v) 	{ return (v << 7) /*| (v >> 1)*/; }
static inline int32_t S11_20(int32_t v) { return (v << 20) /*| (v >> 1)*/; }
static inline int32_t S11_20(float v) { return v * (1 << 20); }
static inline int32_t S16_15(int32_t v) { return (v << 15) /*| (v >> 1)*/; }

static uint32_t RGB2COL(uint32_t c) {
  switch (vga_bits) {
  case 8: // 232
    c = (BLUE(c) >> 6) << 6 | (GREEN(c) >> 5) << 6 | (RED(c) >> 6);
    break;
  case 15:
    c = (BLUE(c) >> 3) << 10 | (GREEN(c) >> 3) << 5 | (RED(c) >> 3);
    break;
  case 16:
    c = (BLUE(c) >> 3) << 11 | (GREEN(c) >> 2) << 5 | (RED(c) >> 3);
    break;
  }
  return c;
}

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

static uint16_t mx, my, c;

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

// we probably could unlock all once?
void s3_lock(uint8_t val = 0)
{
  vga_write_crtc(0x39, val); // lock system control registers 
}

void s3_unlock()
{
  return s3_lock(0xa0); // unlock system control registers 
}

void s3_cursor(uint32_t addr, uint32_t fg = 0xffffff, uint32_t bg = 0,
	       uint8_t ox = 0, uint8_t oy = 0)
{
  uint32_t c;
  // foreground color, R, G, B color "stack"
  // TODO: apparently not RGB, but 8: indexed and 15/16: packed! :-/
  
  c = RGB2COL(fg);
  vga_read_crtc(0x4B); // reset stack pointer
  outb(CRTC_INDEX, 0x4a); // foreground
  outb(CRTC_INDEX + 1, c);
  if (vga_bits > 8) outb(CRTC_INDEX + 1, c >> 8);
  if (vga_bits > 16) outb(CRTC_INDEX + 1, c >> 16);
  
  c = RGB2COL(bg);
  vga_read_crtc(0x4B); // reset stack pointer
  outb(CRTC_INDEX, 0x4b); // background
  outb(CRTC_INDEX + 1, bg);
  if (vga_bits > 8) outb(CRTC_INDEX + 1, c >> 8);
  if (vga_bits > 16) outb(CRTC_INDEX + 1, c >> 16);
  
  // cursor shape memory location 1024 byte "segment"
  vga_write_crtc(0x4c, addr >> 18);
  vga_write_crtc(0x4d, addr >> 10);
  
  // x/y offset, unfortuantely permanent
  vga_write_crtc(0x4e, ox);
  vga_write_crtc(0x4f, oy);
}

void s3_cursor_enable(uint8_t state)
{
  vga_write_crtc(0x45, state); // cursor en/dis-able
}

void s3_cursor_pos(uint16_t x, uint16_t y)
{
  // recommended Trio order?
  vga_write_crtc(0x47, x); // x
  vga_write_crtc(0x46, x >> 8);
  vga_write_crtc(0x49, y); // y
  vga_write_crtc(0x48, y >> 8);
}

static const uint32_t iobase = 0x1000000;

// This are old Trio compatible registers
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

// new Virge registers
enum s3vregs {
  L3dGdY_dBdY = 0xB144,
  L3dAdY_dRdY = 0xB148,
  L3GS_BS = 0xB14C,
  L3AS_RS = 0xB150,
  
  L3dZ = 0xB158,
  L3ZSTART = 0xB15C,
  
  L3XEND0_END1 = 0xB16C, // 3dline
  L3dX = 0xB170,
  L3XSTART = 0xB174,
  L3YSTART = 0xB178,
  L3YCNT = 0xB17C,

  TEX_BASE = 0xB4EC, // texture base
  
  TdGdX_dBdX = 0xB53C, // 3d triangle - color
  TdAdX_dRdX = 0xB540,
  TdGdY_dBdY = 0xB544,
  TdAdY_dRdY = 0xB548,
  TGS_BS = 0xB54C,
  TAS_RS = 0xB550,
  
  TdZdX = 0xB554, // Z
  TdZdY = 0xB558,
  TZS = 0xB55C,
  
  TBV = 0xB504, // texture
  TBU =  0xb508,
  TdWdX =  0xB50C,
  TdWdY = 0xB510,
  TWS =  0xB514,
  TdDdX = 0xB518,
  TdVdX = 0xB51C,
  TdUdX = 0xB520,
  TdDdY = 0xB524,
  TdVdY = 0xB528,
  TdUdY = 0xB52C,
  TDS = 0xB530,
  TVS = 0xB534,
  TUS = 0xB538,

  TdXdY12 = 0xB560, // geometry
  TXEND12 = 0xB564,
  TdXdY01 = 0xB568,
  TXEND01 = 0xB56C,
  TdXdY02 = 0xB570,
  TXS = 0xB574,
  TYS = 0xB578,
  TY01_Y12 = 0xB57C,
};

enum S3V_ROP {
  ROP_0 = 0, // zero
  ROP_Dn = 0x55, // invert Dest, tested
  ROP_S = 0xcc, // Source
  ROP_P = 0xf0, // Pattern
  ROP_1 = 0xff, // one
};

enum S3V_ZB_MODE {
  ZB_NORMAL = 0,
  ZB_MUX = 1,
  ZB_MUX_DRAW = 2,
  ZB_NONE = 3,
};

enum S3V_TEX_FORMAT {
  TC_ARGB8888 = 0,
  TC_ARGB4444 = 1,
  TC_ARGB1555 = 2,
  TC_PALETTE8 = 6,
  TC_YUYV16 = 7,
};

enum S3V_TEX_FILTER {
  TF_1TPP = 4, // nearest
  TF_4TPP = 6, // linear
};

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

template <typename T>
void SWAP(T& a, T& b) {
  T t = a;
  a = b; b = t;
}

static uint8_t dbgline = 0;
void LOG(unrealptr& p, const char* text)
{
  vga_blit_text(p, 8, dbgline++ * 16, text, 0xffff, 0);
}

// pretty command logger example
void _CMD(unrealptr& p, uint32_t r, uint32_t v, const char* s = 0)
{
  char text[24];
  sprintf(text, "[%x] = %x %s  ", r, v, s ? s : "");
  LOG(p, text);
  p.set32(iobase + r, v);
}

void s3v_3dline(unrealptr& p,
		int16_t x1, int16_t y1, uint16_t z1,
		int16_t x2, int16_t y2, uint16_t z2,
		uint32_t c0, uint32_t c1, S3V_ZB_MODE zmode = ZB_NONE)
{
  // the s3virge engine always draws bottom up :-/
  if (y2 > y1) {
    SWAP(x1, x2);
    SWAP(y1, y2);
    SWAP(z1, z2);
    SWAP(c0, c1);
  }
  
  const int16_t dx = x2 - x1;
  const int16_t dy = y2 - y1; // due sort always negative!
  
  const uint8_t xdir = x1 <= x2 ? 1 : 0; // 1: left-to-right
  const uint32_t xdelta = dy == 0 ? 0 : (dx << 20) / -dy;
  
  const uint16_t stride = vga_w * vga_bytes;
  
  // avoid div0 exception
  if (dx == 0 || dy == 0)
    return;
  
  int16_t dr, dg, db, da;
  dr = RED(c0) - RED(c1);
  dg = GREEN(c0) - GREEN(c1);
  db = BLUE(c0) - BLUE(c1);
  da = ALPHA(c0) - ALPHA(c1);
  
  p.set32(iobase + L3dGdY_dBdY, ((S87(dg)/-dy) << 16) | ((S87(db)/-dy) & 0xffff)); // X color deltas
  p.set32(iobase + L3dAdY_dRdY, ((S87(da)/-dy) << 16) | ((S87(dr)/-dy) & 0xffff));
  p.set32(iobase + L3GS_BS, (S87(GREEN(c0)) << 16) | S87(BLUE(c0))); // color start
  p.set32(iobase + L3AS_RS, (S87(ALPHA(c0)) << 16) | S87(RED(c0)));
  
  int16_t dz = z2 - z1;
  p.set32(iobase + L3dZ, S16_15(dz)/-dy); // z-delta
  p.set32(iobase + L3ZSTART, S16_15(z1)); // z-start
  
  p.set32(iobase + 0xb0d8, vga_fb); // dst base
  p.set32(iobase + 0xb0e4, stride << 16 | stride); // src & dst stride

  p.set32(iobase + L3XEND0_END1, x1 << 16 | x2); // first and last x pixel coordinate
  p.set32(iobase + L3dX, xdelta); // x direction gradient S11.20
  p.set32(iobase + L3XSTART, x1 << 20); // S11.20
  p.set32(iobase + L3YSTART, y1); // starting y coord. for S3d Engine
  p.set32(iobase + L3YCNT, (xdir << 31) | (-dy + 1)); // line count

  const uint8_t cmd = 8; // 3d line
  const uint8_t tw = 0; // texture wrap
  const uint8_t zbmode = zmode & ZB_NONE; // AND our additional bit away
  const uint8_t zup = zbmode == ZB_NONE ? 0 : 1; // zbuffer update
  const uint8_t zcomp = zbmode <= ZB_NONE ? 0x6 : 0x7; // zbuffer compare < or always
  const uint8_t abc = 0; // alpha blend control
  const uint8_t fog =  0;
  const uint8_t tblend = 0; // texture blending
  const uint8_t tfilter = 0;
  const uint8_t tfmt = 0;
  const uint8_t mipmap = 0;
  const uint8_t dstfmt = vga_bytes - 1; // 8, 16, 24 bit
  const uint8_t autox = 0;

  const uint8_t clip = 1;
  if (clip) {
    p.set32(iobase + 0xb0dc, 0 << 16 | (vga_w-1)); // CLIP_L_R
    p.set32(iobase + 0xb0e0, 0 << 16 | (vga_h-1)); // CLIP_T_B
  }

  p.set32(iobase + 0xb100, (1 << 31) | // 3d
	  (cmd << 27) | (tw << 26) | (zbmode << 24) | (zup << 23) |
	  (zcomp << 20) | (abc << 18) | (fog << 17) | (tblend << 15) |
	  (tfilter << 12) | (mipmap << 8) | (tfmt << 5) |
	  (dstfmt << 2) | (clip << 1) | (autox));
}

void s3v_line(unrealptr& p,
	     int16_t x1, int16_t y1, int16_t x2, int16_t y2,
	     uint32_t color)
{
  // the s3virge engine always draws bottom up :-/
  if (y2 > y1) {
    SWAP(x1, x2);
    SWAP(y1, y2);
  }
  
  const int32_t dx = x2 - x1;
  const int16_t dy = y2 - y1; // due sorted y, always negative
  const uint8_t ymajor = -dy > ABS(dx) ? 1 : 0;
  
  const uint8_t xdir = dx >= 0 ? 1 : 0; // 1: left-to-right
  const int32_t xdelta = dy == 0 ? 0 : (dx << 20) / -dy;
  int32_t xstart = (int32_t)x1 << 20;
  { // starting x coord. for S3d engine
    if (!ymajor) {
      xstart -= xdelta / 2;
      if (xdelta < 0)
	xstart += (1 << 20) - 1;
    }
  }
  
  const uint16_t stride = vga_w * vga_bytes;
  p.set32(iobase + 0xA8d8, vga_fb); // dst base
  p.set32(iobase + 0xA8e4, stride << 16 | stride); // dst stride
  p.set32(iobase + 0xA4f4, color); // 8-bit pattern foregnd color index
  p.set32(iobase + 0xA96C, (x1 << 16) | x2); // 1st & last scanline x pixel
  p.set32(iobase + 0xA970, xdelta); // x direction gradient S11.20
  p.set32(iobase + 0xA974, xstart); // S11.20
  p.set32(iobase + 0xA978, y1); // starting y coord. for s3d Engine
  p.set32(iobase + 0xA97c, (xdir << 31) | (-dy + 1)); // line count

  // 0 0010 00S SS SS SSS0 0000 0001 0010 00S0
  const uint8_t cmd = 3; // line
  const uint8_t rop = ROP_P;
  const uint8_t mono = 1;
  const uint8_t draw = 1;
  const uint8_t dstfmt = vga_bytes - 1; // 8, 16, 24 bit
  const uint8_t clip = 0;
  const uint8_t autox = 0;
  const uint8_t _xdir = 01, _ydir= 0;
  p.set32(iobase + 0xA900, (cmd << 27) | (_ydir << 26) | (_xdir << 25) | (rop << 17)
       | (mono << 8) | (draw << 5) | (dstfmt << 2) | (clip << 1) | (autox));
}

static int16_t _intrp2(uint16_t v0, uint16_t v1, int16_t d, uint16_t dy01, uint16_t dy, int16_t _dx) {
  uint16_t v = v0 + d * dy01 / dy;
  return v1 - v;
}

// TODO: texture, alpha, fog control
void s3v_3dtriangle(unrealptr& p,
		    vertexf v0, vertexf v1, vertexf v2,
		    uint32_t c0, uint32_t c1, uint32_t c2,
		    S3V_ZB_MODE zmode = ZB_NONE,
		    uint32_t texaddr = 0, uint8_t texlevel = 0, uint8_t texfmt = TC_ARGB8888)
{
  // sort all points by y - the s3virge engine always draws bottom up :-/
  {
    if (v1[1] > v0[1]) {
      SWAP(v0, v1);
      SWAP(c0, c1);
    }
    if (v2[1] > v0[1]) {
      SWAP(v0, v2);
      SWAP(c0, c2);
    }
    if (v2[1] > v1[1]) {
      SWAP(v1, v2);
      SWAP(c1, c2);
    }
  }
  
  // easier to read code below
  float x0 = v0[0], y0 = v0[1], z0 = v0[2],
    x1 = v1[0], y1 = v1[1], z1 = v1[2],
    x2 = v2[0], y2 = v2[1], z2 = v2[2];
  
  //   2_  or  _2
  //  /. 1    1. \
  // 0´         ` 0
  const uint16_t stride = vga_w * vga_bytes;
  const int16_t dx = x1 - x0, dx2 = x2 - x0;
  // TODO: maybe dy on rounded to integer?
  const int16_t dy = y0 - y2 + 1, dy12 = y1 - y2 + 1;
  const int16_t dy01 = dy - dy12; 
  
  // avoid div0 exception
  if (dx == 0 || dy == 0 || y0 < 0)
    return;
  
  // xdir by intersecting points at y1
  int16_t _dx = x0 + dx2 * dy01 / dy;
  _dx = x1 - _dx;
  const uint8_t xdir = _dx >= 0; // draw right to left or left to right
  _dx = _dx != 0 ? ABS(_dx) : 1; // prevent div0
  
  // interpolate, construct a _p1 for x-delta
#define intrp2(v0, v1, d) _intrp2(v0, v1, d, dy01, dy, _dx)
  
  int16_t dr, dg, db, da;
  dr = RED(c2) - RED(c0); // 1st y-deltas directly
  dg = GREEN(c2) - GREEN(c0);
  db = BLUE(c2) - BLUE(c0);
  da = ALPHA(c2) - ALPHA(c0);
  p.set32(iobase + TGS_BS, (S87(GREEN(c0)) << 16) | S87(BLUE(c0))); // color start
  p.set32(iobase + TAS_RS, (S87(ALPHA(c0)) << 16) | S87(RED(c0)));
  p.set32(iobase + TdGdY_dBdY, ((S87(dg) / dy) << 16) | ((S87(db) / dy) & 0xffff)); // Y color deltas
  p.set32(iobase + TdAdY_dRdY, ((S87(da) / dy) << 16) | ((S87(dr) / dy) & 0xffff));
  p.set32(iobase + TdGdX_dBdX, ((S87(intrp2(GREEN(c0), GREEN(c1), dg)) / _dx) << 16) | ((S87(intrp2(BLUE(c0), BLUE(c1), db)) / _dx) & 0xffff)); // X color deltas
  p.set32(iobase + TdAdX_dRdX, ((S87(intrp2(ALPHA(c0), ALPHA(c1), da)) / _dx) << 16) | ((S87(intrp2(RED(c0), RED(c1), dr)) / _dx) & 0xffff));
  
  int16_t dz = z2 - z0;
  p.set32(iobase + TZS, S16_15(z0)); // z-start
  p.set32(iobase + TdZdY, S16_15(dz) / dy); // z-y delta S16.5
  p.set32(iobase + TdZdX, S16_15(intrp2(z0, z1, dz)) / _dx); // z-x delta
  
  p.set32(iobase + 0xb4d8, vga_fb); // dst base
  p.set32(iobase + 0xb4e4, stride << 16 | (1 << texlevel)); // dst stride, src stride for flat textures
  
  p.set32(iobase + TYS, y0); // y start
  y0 = y0 - (int)y0; // Note: only leaving fractional part, for precision below: -v
  p.set32(iobase + TXS, S11_20(x0 + y0 * dx2 / dy)); // x start S11.20
  p.set32(iobase + TdXdY12, dy12 ? S11_20(x2 - x1) / dy12 : 0); // XY12 Delta S11.20
  p.set32(iobase + TXEND12, S11_20(x1)); // XEND12 S11.20 - TODO: more precise?
  p.set32(iobase + TdXdY01, dy01 ? S11_20(dx) / dy01 : 0); // XY01 Delta a S11.20
  p.set32(iobase + TXEND01, S11_20(x0 + (dy01 ? y0 * dx / dy01 : 0))); // X01 END S11.20
  p.set32(iobase + TdXdY02, S11_20(dx2) / dy); // X02 Delta S11.20
  // v- left/right bit, always direction of largest y component!
  p.set32(iobase + TY01_Y12, (xdir << 31) | (dy01 << 16) | dy12); // y scan line count 01, 12
  
  if (texaddr) {
    const uint8_t texshift = 27 - texlevel; // w/o perspective simply 8 bits less or so?
    const uint16_t texsize = 1 << texlevel;

    // texture - D is mipmap level, W perspective correction
    // +->U
    // V
    
    // TODO: more optional mip-maps details and texture location control
    // looks like we need to set the mip-map level in any case!?, src_stride not used by pcem?
    p.set32(iobase + TEX_BASE, texaddr); // texture address
    
    p.set32(iobase + TBU, 0); // Triangle Base U Register (TBU) (4 + s).(16 - s)
    p.set32(iobase + TBV, 0); // Triangle Base V Register (TBV) 
    p.set32(iobase + TUS, 0); // Triangle U Start Register (TUS) S(4 + s).(27 - s) | S12.8.11
    p.set32(iobase + TVS, (texsize - 1) << texshift); // Triangle V Start Register (TVS) …
    p.set32(iobase + TdVdY, (1 << texshift) / dy * -texsize); // Triangle UY Delta Register (TdUdY) …
    p.set32(iobase + TdUdY, 0); // Triangle VY Delta Register (TdVdY) …
    p.set32(iobase + TdVdX, 0); // Triangle UX Delta Register (TdUdX) …
    p.set32(iobase + TdUdX, (1 << texshift) / dx * texsize); // Triangle VX Delta Register (TdVdX) …
    
    // perspective correction only:
    // Triangle W Start Register (TWS) S12.19
    // Triangle WY Delta Register (TdWdY) S12.19
    // Triangle WX Delta Register (TdWdX) S12.19
    
    // mipmap level only:
    // Triangle D Start Register (TDS) S4.27
    // Triangle DX Delta Register (TdDdX) S4.27
    // Triangle DY Delta Register (TdDdY) S4.27
  }
  
  const uint8_t cmd = texaddr ? 0x2 : 0; // unlit texture : gouraud shaded triangle
  const uint8_t tw = texaddr ? 1 : 0; // texture wrap
  const uint8_t zbmode = zmode & ZB_NONE; // AND our additional bit away
  const uint8_t zup = zbmode == ZB_NONE ? 0 : 1; // zbuffer update
  const uint8_t zcomp = zmode <= ZB_NONE ? 0x6 : 0x7; // zbuffer compare < or always
  const uint8_t abc = 0; // alpha blend control, 0: no 2: texture, 3: source
  const uint8_t fog =  0;
  const uint8_t tfilter = texaddr ? TF_1TPP : 0; // texture filter mode
  const uint8_t tfmt = texaddr ? texfmt : 0; // texture color format
  const uint8_t tblend = texaddr ? 2 : 0; // texture blending mode - decal
  const uint8_t mipmap = texaddr ? texlevel : 0; // max size
  const uint8_t dstfmt = vga_bytes - 1; // 8, 16, 24 bit
  const uint8_t autox = 0;

  // clipping
  const uint8_t clip = 1;
#if 1
  p.set32(iobase + 0xb4dc, 0 << 16 | (vga_w-1)); // CLIP_L_R
  p.set32(iobase + 0xb4e0, 0 << 16 | (vga_h-1)); // CLIP_T_B
#endif
  
  p.set32(iobase + 0xb500, (1 << 31) | // 3d
	  (cmd << 27) | (tw << 26) | (zbmode << 24) | (zup << 23) |
	  (zcomp << 20) | (abc << 18) | (fog << 17) | (tblend << 15) |
	  (tfilter << 12) | (mipmap << 8) | (tfmt << 5) |
	  (dstfmt << 2) | (clip << 1) | (autox));
}

// TODO: texture, alpha, fog control
void s3v_3drect(unrealptr& p, vertexf v0, uint32_t c0,
		S3V_ZB_MODE zmode = ZB_NONE)
{
  
  //   2_  or  _2
  //  /. 1    1. \
  // 0´         ` 0
  const uint16_t stride = vga_w * vga_bytes;
  
  // xdir by intersecting points at y1
  const uint8_t xdir = 1; // draw right to left or left to right
  const int16_t x0 = v0[0], y0 = v0[1],  z0 = v0[2];
  const uint16_t dx = 1, dy = 1;
  
  const int16_t dr = 0, dg = 0, db = 0, da = 0;
  p.set32(iobase + TGS_BS, (S87(GREEN(c0)) << 16) | S87(BLUE(c0))); // color start
  p.set32(iobase + TAS_RS, (S87(ALPHA(c0)) << 16) | S87(RED(c0)));
  p.set32(iobase + TdGdX_dBdX, ((S87(dg)/dx) << 16) | ((S87(db)/dx) & 0xffff)); // X color deltas
  p.set32(iobase + TdGdY_dBdY, ((S87(dg)/dy) << 16) | ((S87(db)/dy) & 0xffff)); // Y color deltas
  p.set32(iobase + TdAdX_dRdX, ((S87(da)/dx) << 16) | ((S87(dr)/dx) & 0xffff));
  p.set32(iobase + TdAdY_dRdY, ((S87(da)/dy) << 16) | ((S87(dr)/dy) & 0xffff));
  
  const int16_t dzx = 0, dzy = 0;
  //printf("z: %d %d %d - %d\n", z0, z1, z2, _z2);
  //printf("dz: %d %d %d\n", z0, dzx, dzy);
  //printf("dz: %x %x %x\n", S16_15(z0), S16_15(dzx)/dx, S16_15(dzy)/dy);
  p.set32(iobase + TZS, S16_15(z0)); // z-start
  p.set32(iobase + TdZdX, S16_15(dzx)/dx); // z-x delta S16.5
  p.set32(iobase + TdZdY, S16_15(dzy)/dy); // z-y delta
  
  p.set32(iobase + 0xb4d8, vga_fb); // dst base
  p.set32(iobase + 0xb4e4, stride << 16); // dst stride, lower src only for flat textures
  
  p.set32(iobase + TXS, S11_20(0)); // x start S11.20
  p.set32(iobase + TYS, y0); // y start
  p.set32(iobase + TdXdY12, 0); // XY12 Delta S11.20
  p.set32(iobase + TXEND01, S11_20(x0)); // X01 END S11.20
  p.set32(iobase + TXEND12, S11_20(0)); // XEND12 S11.20
  p.set32(iobase + TdXdY01, S11_20(0)); // XY01 Delta S11.20
  p.set32(iobase + TdXdY02, S11_20(0)); // X02 Delta S11.20
  // v- left/right bit, always direction of largest y component!
  p.set32(iobase + TY01_Y12, (xdir << 31) | ((y0 + 1) << 16) | 0); // y scan line count 01, 12
  
  const uint32_t texaddr = 0;
  const uint8_t cmd = texaddr ? 0x2 : 0; // unlit texture : gouraud shaded triangle
  const uint8_t tw = 0; // texture wrap
  const uint8_t zbmode = zmode & ZB_NONE; // AND our additional bit away
  const uint8_t zup = zbmode == ZB_NONE ? 0 : 1; // zbuffer update
  const uint8_t zcomp = zmode <= ZB_NONE ? 0x6 : 0x7; // zbuffer compare < or always
  const uint8_t abc = 0; // alpha blend control, 2: texture, 3: source
  const uint8_t fog =  0;
  const uint8_t tblend = 0; // texture blending mode
  const uint8_t tfilter = texaddr ? TF_1TPP : 0; // texture filter mode - 1TPP
  const uint8_t tfmt = texaddr ? 0x2 : 0; // texture format
  const uint8_t mipmap = texaddr ? 1 : 0; // max size
  const uint8_t dstfmt = vga_bytes - 1; // 8, 16, 24 bit
  const uint8_t autox = 0;
  
  // clipping
  const uint8_t clip = 1;
#if 1
  p.set32(iobase + 0xb4dc, 0 << 16 | (vga_w-1)); // CLIP_L_R
  p.set32(iobase + 0xb4e0, 0 << 16 | (vga_h-1)); // CLIP_T_B
#endif
  
  p.set32(iobase + 0xb500, (1 << 31) | // 3d
	  (cmd << 27) | (tw << 26) | (zbmode << 24) | (zup << 23) |
	  (zcomp << 20) | (abc << 18) | (fog << 17) | (tblend << 15) |
	  (tfilter << 12) | (mipmap << 8) | (tfmt << 5) |
	  (dstfmt << 2) | (clip << 1) | (autox));
}
  
void s3_fill(unrealptr& p,
	     uint16_t x, uint16_t y, uint16_t w, uint16_t h,
	     uint32_t color)
{
  // TODO: wait not busy!
  --w; --h;
  
  p.set16(iobase + FRGD_MIX, 0x0027); // 0:3 mix-type, 5:6: clr-src
  p.set32(iobase + FRGD_COLOR, color); // color
  p.set16(iobase + PIX_CNTL, 0xa000); // FRGD_MIX as source
  
  p.set32(iobase + ALT_CURXY, (x << 16) | y); // src start
  p.set32(iobase + ALT_PCNT, (w << 16) | h); // rect size
  
  // 15:13 CMD 12: SWAP 10-9: BUS SIZE 8: WAIT
  // 7-5: DIR 4: DRAW 3: DIR-TYPE 2: LAST-OFF 1:  PX MD 0: 1
  p.set16(iobase + CMD, 0x40b1); // FILL CMD
}

void s3v_fill(unrealptr& p,
	     uint16_t x, uint16_t y, uint16_t w, uint16_t h,
	     uint32_t color)
{
  --w; --h;
  uint8_t xdir = 1, ydir = 1;
  p.set32(iobase + 0xA4f4, color); // 8-bit pattern foregnd color index
  const uint16_t stride = vga_w * vga_bytes;
  p.set32(iobase + 0xA4d8, vga_fb); // dst base
  p.set32(iobase + 0xA4e4, stride << 16 | stride); // src & dst stride
  p.set32(iobase + 0xA504, w << 16 | h); // rectangle width and height
  p.set32(iobase + 0xA50C, x << 16 | y); // destination x and y start coord.
  const uint8_t cmd = 2; // fill
  const uint8_t rop = ROP_P;
  const uint8_t mono = 1;
  const uint8_t draw = 1;
  const uint8_t dstfmt = vga_bytes - 1; // 8, 16, 24 bit
  const uint8_t clip = 0;
  const uint8_t autox = 0;
  p.set32(iobase + 0xA500, (cmd << 27)| (ydir << 26) | (xdir << 25)
	  | (rop << 17) | (mono << 8) | (draw << 5) | (dstfmt << 2) | (clip << 1) | (autox));
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

void s3v_blit(unrealptr& p,
	uint16_t sx, uint16_t sy, uint16_t w, uint16_t h,
	uint16_t dx, uint16_t dy)
{
  --w; --h;
  uint8_t xdir = 1, ydir = 1;
  const uint16_t stride = vga_w * vga_bytes;
  p.set32(iobase + 0xA4d4, vga_fb); // src base
  p.set32(iobase + 0xA4d8, vga_fb); // dst base
  p.set32(iobase + 0xA4e4, stride << 16 | stride); // src & dst stride
  
  p.set32(iobase + 0xA504, w << 16 | h); // rectangle width and height
  p.set32(iobase + 0xA508, sx << 16 | sy); // source  x and y start coord.
  p.set32(iobase + 0xA50C, dx << 16 | dy); // destination x and y start coord.
  const uint8_t cmd = 0; // bitblt
  const uint8_t rop = ROP_S;
  const uint8_t mono = 1;
  const uint8_t draw = 1;
  const uint8_t dstfmt = vga_bytes - 1; // 8, 16, 24 bit
  const uint8_t clip = 0;
  const uint8_t autox = 0;
  p.set32(iobase + 0xA500, (cmd << 27) | (ydir << 26) | (xdir << 25)
	  | (rop << 17) | (mono << 8) | (draw << 5) | (dstfmt << 2) | (clip << 1) | (autox));
}

template<typename T>
void rotate(T& x, T& y, float angle) {
  const float _sin = sinf(angle);
  const float _cos = cosf(angle);
  const T dx = _cos * x -  _sin * y;
  const T dy = _sin * x + _cos * y;
  x = dx; y = dy;
}

static void (*isr_system)() = 0;
static volatile int ticks = 0;

static void __attribute__((noinline)) isr_timer2() {
  ++ticks;
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

void setpx(unrealptr& p, int16_t x, int16_t y, uint32_t c)
{
  if (x < 0 || x >= vga_w)
    return;
  if (y < 0 || y >= vga_h)
    return;
      
  uint32_t px = y * vga_w + x;
  switch (vga_bits) {
  case 8:
    p.set8(px, c); break;
  case 15:
  case 16:
    p.set16(px * 2, c); break;
  case 24:
    p.set8(px * 3 + 0, c >> 0);
    p.set8(px * 3 + 1, c >> 8);
    p.set8(px * 3 + 2, c >>16); break;
  case 32:
    p.set32(px * 4, c); break;
  }
}

void mark(unrealptr& p, int16_t _x, int16_t _y, uint32_t c = 0xffffff)
{
  const int px = 3;
  for (int16_t y = _y - px; y <= _y + px; ++y)
    setpx(p, _x, y, c);
  
  for (int16_t x = _x - px; x <= _x + px; ++x)
    setpx(p, x, _y, c);
}

void project(vertexf v)
{
  // project 3d model to 2d screen plane
  int16_t z = v[2] == 0 ? 1 : v[2];
  v[0] = v[0] * 256 / z;
  v[1] = v[1] * 256 / z;
  //v[2] = v[2] * 256 / z;
}

void transform(const vertex v, vertexf f, const float mtx[4][4], const bool proj = false)
{
  // via temporaries, just to allow v == vo, ...
  f[0] = mtx[0][0] * v[0] + mtx[0][1] * v[1] + mtx[0][2] * v[2] + mtx[0][3] * 1;
  f[1] = mtx[1][0] * v[0] + mtx[1][1] * v[1] + mtx[1][2] * v[2] + mtx[1][3] * 1;
  f[2] = mtx[2][0] * v[0] + mtx[2][1] * v[1] + mtx[2][2] * v[2] + mtx[2][3] * 1;
  
  //printf("> %d %d %d -> ", v[0], v[1], v[2]);
  //printf("> %d %d %d\n", vo[0], vo[1], vo[2]);
  //exit(1);
  if (proj)
    project(f);
}


void mtxPrint(float mtx[4][4])
{
  for (int i = 0; i < 4; ++i) {
    printf("%f %f %f %f\n",
	   mtx[i][0], mtx[i][1], mtx[i][2], mtx[i][3]);
  }
}

void mtxIdentity(float mtx[4][4])
{
  for (int i = 0; i < 4; ++i)
    for (int j = 0; j < 4; ++j)
      mtx[i][j] = i == j ? 1 : 0;
}

void mtxTranslate(float mtx[4][4], float tx, float ty, float tz)
{
  mtxIdentity(mtx);
  mtx[0][3] = tx;
  mtx[1][3] = ty;
  mtx[2][3] = tz;
}  

void mtxScale(float mtx[4][4], float sx, float sy, float sz)
{
  mtxIdentity(mtx);
  mtx[0][0] = sx;
  mtx[1][1] = sy;
  mtx[2][2] = sz;
}

void mtxRotateX(float mtx[4][4], float angle)
{
  mtxIdentity(mtx);
  mtx[1][1] = cosf(angle);
  mtx[1][2] =-sinf(angle);
  mtx[2][1] = sinf(angle);
  mtx[2][2] = cosf(angle);
}

void mtxRotateY(float mtx[4][4], float angle)
{
  mtxIdentity(mtx);
  mtx[0][0] = cosf(angle);
  mtx[0][2] = sinf(angle);
  mtx[2][0] =-sinf(angle);
  mtx[2][2] = cosf(angle);
}

void mtxRotateZ(float mtx[4][4], float angle)
{
  mtxIdentity(mtx);
  mtx[0][0] = cosf(angle);
  mtx[0][1] =-sinf(angle);
  mtx[1][0] = sinf(angle);
  mtx[1][1] = cosf(angle);
}

void mtxConcat(float a[4][4], float b[4][4])
{
  float m[4][4];
  for (int i = 0; i < 4; ++i) {
    for (int j = 0; j < 4; ++j) {
      register float v = 0;
      for (int k = 0; k < 4; ++k) {
	v += a[i][k] * b[k][j];
      }
      m[i][j] = v;
    }
  }
  for (int i = 0; i < 4; ++i) {
    for (int j = 0; j < 4; ++j) {
      a[i][j] = m[i][j];
    }
  }
}

static float mtx2[4][4], mtx[4][4];

static uint16_t vi = 0;
static vertex vertices[6*3*2] = {
  // currently space for one cube of quads (2x triangle)
};

static uint32_t colors[6*2] = {
  0x333333, // bottom
  0x333333,
  0xaaaaaa, // top
  0xaaaaaa,
  
  0x0000ff, // left
  0x0000ff,
  0x00ff00, // right
  0x00ff00,
  
  0xff0000, // front
  0xff0000,
  0xff00ff, // back
  0xff00ff,
};

void addVertex(int16_t x, int16_t y, int16_t z)
{
  if (vi >= ARRAY_SIZE(vertices))
    return;
  vertices[vi][0] = x;
  vertices[vi][1] = y;
  vertices[vi][2] = z;
  ++vi;
}

void triangle(vertex& p1, vertex& p2, vertex& p3)
{
  addVertex(p1[0], p1[1], p1[2]);
  addVertex(p2[0], p2[1], p2[2]);
  addVertex(p3[0], p3[1], p3[2]);
}

void quad(vertex& p1, vertex& p2, vertex& p3, vertex& p4)
{
  triangle(p1, p2, p3);
  triangle(p4, p2, p3);
}

void cube(int16_t x1, int16_t y1, int16_t z1, int16_t x2, int16_t y2, int16_t z2)
{
  vertex
    p1 = {x1, y1, z1},
    p2 = {x2, y1, z1},
    p3 = {x1, y2, z1},
    p4 = {x2, y2, z1};
  
  quad(p1, p2, p3, p4); // base
  p1[2] = p2[2] = p3[2] = p4[2] = z2;
  quad(p1, p2, p3, p4); // top

  p1[2] = p3[2] = z1;
  p2[0] = p4[0] = x1;
  quad(p1, p2, p3, p4); // left
  
  p1[0] = p2[0] = p3[0] = p4[0] = x2;
  quad(p1, p2, p3, p4); // right
  
  p3[0] = p4[0] = x1;
  p3[1] = p4[1] = y1;
  quad(p1, p2, p3, p4); // front

  p1[1] = p2[1] = p3[1] = p4[1] = y2;
  quad(p1, p2, p3, p4); // back
}

void setV(vertexf& v, int16_t x, int16_t y, int16_t z)
{
  v[0] = x;
  v[1] = y;
  v[2] = z;
}

static uint32_t texaddr = 0, texsize;

void drawWorld(unrealptr& p, int tick) {
  //if (angle > 2) return;
  float angle = M_PI * tick / 180;
  
  // clear z buffer w/ 2 forced triangles
  vertex v0, v1, v2;
  vertexf f0 = {0, vga_h - 1, 0x7fff}, f1 = {vga_w - 1, 0, 0x7fff}, f2 = {0, 0, 0x7fff};
#if 0
  s3v_3dtriangle(p,
		 f0, f1, f2,
		 0, 0, 0,
		 (S3V_ZB_MODE)(ZB_NORMAL | 0x4)); // cmp always
  f2[0] = vga_w, f2[1] = vga_h;
  s3v_3dtriangle(p,
		 f0, f1, f2,
		 0, 0, 0,
		 (S3V_ZB_MODE)(ZB_NORMAL | 0x4)); // cmp always
#else
  // our "tricked" straight rect fill ;-)
  f2[0] = vga_w - 1, f2[1] = vga_h - 1;
  s3v_3drect(p, f2, 0,
	     (S3V_ZB_MODE)(ZB_NORMAL | 0x4)); // cmp always
  
#endif
  
#if 1
  // synthetic, basic z-buffer test
  setV(f0, 200, 0, 200);
  setV(f1, 100, 200, 200);
  setV(f2, 0, 0, 100);
  s3v_3dtriangle(p, // xyz
		 f0, f1, f2,
		 0xff, 0xff, 0xff, ZB_NORMAL);
  setV(f0, 0, 200, 100);
  setV(f1, 100, 0, 200);
  setV(f2, 200, 200, 200);
  s3v_3dtriangle(p, // xyz
		 f0, f1, f2,
		 0xff0000, 0xff0000, 0xff0000, ZB_NORMAL);
#endif
  
  // in reverse order:
  mtxTranslate(mtx, 300, 200, 0);
  //mtxRotateX(mtx2, angle);
  //mtxRotateY(mtx2, angle);
  mtxRotateZ(mtx2, angle);
  mtxConcat(mtx, mtx2);
  mtxTranslate(mtx2, -150, -150, 0);
  mtxConcat(mtx, mtx2);
  
  for (uint16_t i = 0, j = 0; i < vi; ++j) {
    transform(vertices[i++], f0, mtx, true);
    transform(vertices[i++], f1, mtx, true);
    transform(vertices[i++], f2, mtx, true);

    s3v_3dtriangle(p, // xyz
		   f0, f1, f2,
		   colors[j], // ABGR
		   colors[j],
		   colors[j],
		   ZB_NORMAL);
    //mark(p, v0[0], v0[1]);
  }
  
  if (true) {
    // rotating triangle coloring test
    mtxTranslate(mtx, 100, 350, 0);
    mtxRotateZ(mtx2, angle);
    mtxConcat(mtx, mtx2);
    mtxTranslate(mtx2, -50, -50, 0);
    mtxConcat(mtx, mtx2);
    
    v0[0] = 0; v1[0] = 100; v2[0] = 50;
    v0[1] = v1[1] = 100; v2[1] = 0;
    v0[2] = v1[2] = v2[2] = 256;
    transform(v0, f0, mtx, true);
    transform(v1, f1, mtx, true);
    transform(v2, f2, mtx, true);
    s3v_3dtriangle(p, // xyz
		   f0, f1, f2,
		   0xff, 0xff00, 0xff0000,
		   ZB_NORMAL);
  }

  if (true) {
    // texture test
    int16_t s = tick % 100 - 50; // -50 .. 50
    s = s >= 0 ? s : -s; // 50 .. 0 .. 50
    const int16_t x = 400 - s, y = 250 - s;
    const int16_t w = 150 + 2 * s - 1, h = 150 + 2 * s - 1;
    f0[0] = f2[0] = x; f1[0] = x + w;
    f0[1] = f1[1] = y; f2[1] = y + h;
    f0[2] = f1[2] = f2[2] = 0;
    
    s3v_3dtriangle(p,
		   f0, f1, f2,
		   0x000000ff, 0x0000ff00,  0x00ff0000, // ARGB
		   ZB_NORMAL, texaddr, texsize /* 2^s*, */, TC_ARGB8888);
    f0[0] = f1[0]; f0[1] = f2[1];
    s3v_3dtriangle(p,
		   f0, f1, f2,
		   0x000000ff, 0x0000ff00,  0x00ff0000, // ARGB
		   ZB_NORMAL, texaddr, texsize /* 2^s*, */, TC_ARGB8888);
  }
}

static void s3v_flip_frame(unrealptr& p)
{
  // program next, finished drawn frame
  p.set32(iobase + 0x81cc, vga_fb ? 1 : 0); // Double Buffer/LPB Support
  
  // update next to be drawn frame buffer start
  vga_fb = vga_fb ? 0 : vga_w * vga_bytes * vga_h;
}

extern "C"
int main()
{
  uint16_t ret;
  printf("S3/Trio/Virge accel tests, (c) Rene Rebe, ExactCODE; 2018\n");

  unrealptr p(0);


  uint8_t virge = false;
  // unlock s3 registers and try to detect
  {
    vga_write_crtc(0x38, 0x48); // unlock CR2D-CR3F
    vga_write_crtc(0x39, 0xa5); // unlock CR40-CRFF & CR36,37,68
    
    ret = vga_read_crtc(0x2d) << 8; // extended (high) chip id
    ret |= vga_read_crtc(0x2e); // new chip id
    switch (ret) {
    case 0x8810: // Trio32
    case 0x8811: // Trio64
    case 0x8880:
    case 0x8890:
    case 0x88b0:
      break;
      
    case 0x5631: // orig, 1st gen
    case 0x8a01: // /DX
      virge = true;
      break;
    
    default:
      printf("Not supported S3(%x)?\n", ret);
      return 1;
    }
  }
  
  ret = mouse_init();
  if (!ret) {
    printf("mouse init failed\n");
    return 1;
  }
  
  // 320x200 15: 0x10d, 16: 0x10e, 24: 0x10f
  // 640x480 8: 101, 15: 0x110, 16: 0x111, 32: 0x112
  const uint16_t mode = 0x110;
  ret = svga_mode_info(mode, &modeinfo);
  printf("mode: %x, %d, %d %x\n", ret,
	 modeinfo.XResolution, modeinfo.YResolution, modeinfo.PhysBasePtr);
  ret = svga_mode(mode | (1 << 14) | (1 << 15)); // linear, no clear
  if (ret != 0x4f) {
    printf("mode set failed: %x\n", ret);
    ret = svga_mode(mode | (1 << 15)); // no clear
    if (ret != 0x4f) {
      printf("mode set banked failed, too\n");
      return 1;
    }
  }

  if (!modeinfo.PhysBasePtr) {
    printf("no linear PhysBase, trying anyway\n");
    
    vga_write_crtc(0x40, 0x1, 0xfe); // unlock enhanced programming
    vga_write_crtc(0x53, 0x08, ~0x08); // enable new MMIO
    vga_write_crtc(0x58, 0x13, 0x13); // enable linear, 4MB window
    
    modeinfo.PhysBasePtr = vga_read_crtc(0x59) << 24;
    modeinfo.PhysBasePtr |= vga_read_crtc(0x5a) << 16;
    printf("%x\n", modeinfo.PhysBasePtr);
  }
  
  p.rebase(modeinfo.PhysBasePtr);
  
  vga_w = modeinfo.XResolution;
  vga_h = modeinfo.YResolution;
  vga_planes = 1;
  vga_bits = modeinfo.BitsPerPixel;
  vga_bytes = (modeinfo.BitsPerPixel + 7) / 8; // rounding for 15
  
  // keep track of used graphic memory
  uint32_t fb_addr = vga_w * vga_bytes * vga_h; // reserve at least one frame
  
  if (virge) { // && modeinfo.BitsPerPixel == 32) {
    uint8_t psidf = 0; // Primary Stream Input Data Format
    uint8_t cmode = 0;
    switch (vga_bits) {
    case 8: break;
    case 15: psidf = 3; cmode = 3; break;
    case 16: psidf = 5; cmode = 5; break; // but no 3d accel dst frmt either!
      //case 32: psidf = 7; break; // but no accel 3d dst frmt, so fall thru:
    default: psidf = 6; cmode = 0xd;
      vga_bits = modeinfo.BitsPerPixel = 24; // update
      vga_bytes = (modeinfo.BitsPerPixel + 7) / 8; // rounding for 15
      fb_addr = vga_w * vga_bytes * vga_h;
    }
    // the virge apparently does not have 32 bit dst format accel?
    // so reconfigure for 24 bits, saves memory anyway, ...
    
    vga_write_crtc(0x67, (cmode << 4) | 0xc, 0x3); // enable w/ X bits
    
    p.set32(iobase + 0x8180, psidf << 24); // Primary Stream Control
    p.set32(iobase + 0x8184, 0x00000000); // Color/Chroma Key Control 
    p.set32(iobase + 0x8190, 0x03000000); // Secondary Stream Control 
    p.set32(iobase + 0x8194, 0x00000000); // Chroma Key Upper Bound
    p.set32(iobase + 0x8198, 0x00000000); // Secondary Stream Stretch/Filter Constants 
    p.set32(iobase + 0x81A0, 0x01000000); // Blend Control
    p.set32(iobase + 0x81C0, 0); // Primary Stream Frame Buffer Address 0
    p.set32(iobase + 0x81C4, fb_addr); // Primary Stream Frame Buffer Address 1 
    p.set32(iobase + 0x81C8, vga_w * vga_bytes); // Primary Stream Stride
    p.set32(iobase + 0x81CC, 0x00000000); // Double Buffer/LPB Support
    p.set32(iobase + 0x81D0, 0x00000000); // Secondary Stream Frame Buffer Address 0
    p.set32(iobase + 0x81D4, 0x00000000); // Secondary Stream Frame Buffer Address 1
    p.set32(iobase + 0x81D8, 0x00000001); // Secondary Stream Stride
    p.set32(iobase + 0x81DC, 0x40000000); // Opaque Overlay Control
    p.set32(iobase + 0x81E0, 0x00000000); // K1 Vertical Scale Factor
    p.set32(iobase + 0x81E4, 0x00000000); // K2 Vertical Scale Factor
    p.set32(iobase + 0x81E8, 0x00000000); // DDA Vertical Accumulator Initial Value
    p.set32(iobase + 0x81F0, 0x00010001); // Primary Stream Window Start Coordinates 
    p.set32(iobase + 0x81F4, (vga_w - 1) << 16 | vga_h); // Primary Stream Window Size 
    p.set32(iobase + 0x81F8, 0x07FF07FF); // Secondary Stream Window Start Coordinates
    p.set32(iobase + 0x81FC, 0x00010001); // Secondary Stream Window Size
    p.set32(iobase + 0x8200, 0x00006088); // FIFO Control - Enable
    
    fb_addr += fb_addr; // double buffered
  }
  
  mouse_set_max(vga_w, vga_h);
  
  char* cs; // save CS
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
    if (vga_bits == 8) {
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

  // w/o a20 we can only access every other MB, ...
  if (true) {
    // enable A20 gate, if necessary
    if (!a20_enable()) {
      printf("a20 error\n");
      return 1;
    }
  }
  
  // convert our cursor image to the HW defined layout
  {
    // cursor after frame buffer(s)
    uint32_t cur_addr = (fb_addr + 1023) / 1024 * 1024;
    // 2x 64x64 bit: AND and OR masks image
    fb_addr = cur_addr + 64 * 64 / 8;
    // win-compat, X11 is CR55 bit 4: MS/X11
    uint16_t i = 0;
    for (uint8_t y = 0; y < 64; ++y) {
      uint16_t mask = 0, color = 0;
      for (uint8_t x = 0, bit = 15; x < 64; ++x, --bit) {
	uint8_t m, c;
	if (x >= ARRAY_SIZE(*cursor) || y >= ARRAY_SIZE(cursor)) {
	  m = 1; c = 0;
	} else {
	  m = cursor[y][x] == 0 ? 1 : 0;
	  c = cursor[y][x] == 2 ? 1 : 0;
	}
	
	mask |= m << bit;
	color |= c << bit;
	// mask ^= 0xffff; color ^= 0xffff;
	if (bit == 0) {
	  p.set8(cur_addr + i++, mask >> 8);
	  p.set8(cur_addr + i++, mask);
	  p.set8(cur_addr + i++, color >> 8);
	  p.set8(cur_addr + i++, color);
	  mask = color = 0;
	  bit = 16;
	}
      }
    }
    //printf("%d %x\n", cur_addr, cur_addr);
    
    s3_cursor(cur_addr, 0xffffff, 0x000000); // addr, fg-, bg-color
    s3_cursor_pos(vga_w / 2, vga_h / 2);
    s3_cursor_enable(1);
  }
  
  if (virge) {
    // z buffer follows frame buffer(s) and cursor
    const uint32_t zaddr = fb_addr;
    fb_addr += vga_w * vga_h * 2; // 16-bit z-buffer
    p.set32(iobase + 0xB0D4, zaddr); // Z_BASE 3dlines
    p.set32(iobase + 0xB0e8, vga_w * 2); // Z_STRIDE
    p.set32(iobase + 0xB4D4, zaddr); // Z_BASE 3dtriangles
    p.set32(iobase + 0xB4e8, vga_w * 2); // Z_STRIDE
    
    // last but not least a texture, at next free vram/gpu address
    // after frame(s), z-buffer, etc!
    // our demo uses A1B5G5R5, valid sizes: 2^3 ... 2^9?
    texaddr = fb_addr;
    texsize = 1; // 2^1 = 2x2
    p.set16(texaddr + 0, 0b11111); // R
    p.set16(texaddr + 2, 0b1111100000); // G
    p.set16(texaddr + 4, 0b111110000000000); //B
    p.set16(texaddr + 6, 0b111111111111111); // white
    
    // try to load a fancy texture ;-), RGB888 for now
    int fd = open("lena.tex", 0, O_RDONLY);
    if (fd) {
      const unsigned bufsize = 512 / 3 * 3; // align to one texel size
      uint8_t buf[bufsize];
      int i = 0;
      while (true) {
	int ret = read(fd, buf, sizeof(buf));
	if (!ret)
	  break;
	for (int j = 0; j < ret; i += 4, j += 3) {
	  // expand RGB888 to ARGB8888
	  p.set8(texaddr + i + 0, buf[j + 2]);
	  p.set8(texaddr + i + 1, buf[j + 1]);
	  p.set8(texaddr + i + 2, buf[j + 0]);
	  p.set8(texaddr + i + 3, 0xff);
	}
      }
      texsize = 7;
      close(fd);
    }
  }
  
  if (!virge)
    s3_fill(p, vga_w/4, vga_h/4, 200, 100, 0xffffff);
  else
    s3v_fill(p, vga_w/4, vga_h/4, 200, 100, 0xffffff);

  if (virge) {
    // generate 3d world (or load file etc)
    cube(100, 100, 256,  // xyz, from p1 to p2
	 200, 200, 356);
    
    drawWorld(p, 0);
  }

  // test all quadrants ;-)
  for (int i = 0; i < 360; i += 15) {
    // rotate a line vector
    int dx = 64, dy = 0;
    rotate(dx, dy, M_PI * i / 180);
    const int cx = 128, cy = 128; // center
    if (!virge)
      s3_line(p, cx + dx/10, cy + dy/10, cx+dx, cy+dy, i * vga_bytes); // color from angle
    else
      s3v_line(p, cx + dx/10, cy + dy/10, cx+dx, cy+dy, i * vga_bytes); // color from angle
  }
  
  if (!virge)
    s3_blit(p, 0, 0, vga_w/2, vga_h/2, vga_w/2, vga_h/2);
  else
    s3v_blit(p, 0, 0, vga_w/2, vga_h/2, vga_w/2, vga_h/2);
  
  const uint8_t timer_irq = 0x1c; // 8
  isr_system = dos_getvect(timer_irq);
  dos_setvect(timer_irq, (void(*)()) (((int)cs << 16) | (int)isr_timer));

  uint16_t lx = 0, ly = 0; // last
  int lticks = 0; uint8_t update = 1;
  while (true) {
    asm("hlt\n"); // sleep until next interrupt
    // if we are here, we (probably) got an interrupt

    if (ticks != lticks) {
      if (update)
	lticks = ticks;
      else
	ticks = lticks;
      
      // draw virge world 1st, as it clears the screen
      if (virge) {
	//s3v_fill(p, 100, 100, 200, 200, 0); // test fill, now zbuffer!
	drawWorld(p, ticks);
	
	// wait until engine idle TODO: use interrupt
	for (uint32_t status = 0; !((status >> 13) & 1);) {
	  status = p.get32(iobase + 0x8504); // Subsystem Status Register
#if 0
	  _CMD(p, 0x8504, status, "status");
	  _CMD(p, 0x8504, (status >> 8) & 0x1f, "fifo free");
	  _CMD(p, 0x8504, (status >> 13) & 1, "idle");
	  if (dbgline > 25) dbgline = 0;
#endif
	}
      }
      
      {
	char text[24];
	sprintf(text, "%d ", ticks);
	vga_blit_text(p, 0, 0, text, 0xffff, 0);
      }
    }
    
    // only re-draw if it really was the mouse
    if (mx != lx || my != ly) {
      {
	char text[24];
	sprintf(text, "%d %d   ", mx, my);
	vga_blit_text(p, 8, 8, text, 0xffff, 0);
      }
      
      uint32_t color = ticks ? random() : 0x828282;
      // draw test line
      if (false) {
	// clear previous
	//s3v_fill(p, 0, 0, vga_w, vga_h, 0);
	//s3v_line(p, vga_w / 2, vga_h / 2, lx, ly, 0); // clear previous
	
	mark(p, vga_w / 2, vga_h / 2, 0);
	mark(p, lx, ly, 0);
	mark(p, vga_w / 2, vga_h / 2);
	mark(p, mx, my);
      }
      if (!virge)
	s3_line(p, vga_w / 2, vga_h / 2, mx, my, color);
      else
	s3v_line(p, vga_w / 2, vga_h / 2, mx, my, color);
      
      //asm("int3\n");
      lx = mx, ly = my;
      s3_cursor_pos(lx, ly);
    }
    
    if (virge) {
      s3v_flip_frame(p);
    }
    
    if (kbhit()) {
      uint8_t c = getch();
      if (c == 'q') {
	break;
      } else if (c == ' ') {
	update = !update;
      }
    }
  }
  
  if (virge) { // && modeinfo.BitsPerPixel == 24) {
    // before  changing  the  display mode, disable the Streams Processor
    p.set32(iobase + 0x8200, 0x0000C000);
    vga_write_crtc(0x67, 0, 0xf3);
  }
  
  vga_mode(0x03); // back to text mode
  if (isr_system)
    dos_setvect(timer_irq, isr_system);
  mouse_swapvect(&orig_mask, orig_isr);
  
  printf("end.\n");
  return 0;
}
