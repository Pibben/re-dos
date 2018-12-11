/*
 * "Unreal" ptr lines DOS SVGA demo
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

#include "stdlib.h" // random

#include "vga.h"

struct ModeInfoBlock modeinfo;

template <typename T>
T abs(const T& x) {
  return x >= 0 ? x : -x;
}

template<typename T>
bool drawPixel(T& p, int x, int y, int c) {
  if (x < 0 || y < 0 ||
      x >= vga_w || y >= vga_h)
    return false;
  
  uint32_t px = y * vga_w + x;
  if (vga_planes > 1) {
    const uint8_t plane = px & 0x3;
    px /= vga_planes;
    vga_write_planes(1 << plane);
  }
  p.set8(px, c);
  return true;
}

template<typename T>
void drawBresenhamLine(T& p, int x0, int y0, int x1, int y1, int c) {
  int dx = abs(x1-x0), sx = x0<x1 ? 1 : -1;
  int dy = abs(y1-y0), sy = y0<y1 ? 1 : -1;
  int err = (dx>dy ? dx : -dy)/2, e2;

  for(;;){
    if (!drawPixel(p, x0, y0, c)) break;
    if (x0==x1 && y0==y1) break;
    e2 = err;
    if (e2 >-dx) { err -= dy; x0 += sx; }
    if (e2 < dy) { err += dx; y0 += sy; }
  }
}

template<typename T>
void drawWuLine (T& pDC, uint16_t X0, uint16_t Y0, uint16_t X1, uint16_t Y1,
		 uint8_t BaseColor, uint16_t NumLevels, uint8_t IntensityBits)
{
  // Make sure the line runs top to bottom
  if (Y0 > Y1) {
    uint16_t Temp;
    Temp = Y0; Y0 = Y1; Y1 = Temp;
    Temp = X0; X0 = X1; X1 = Temp;
  }
  /* Draw the initial pixel, which is always exactly intersected by
     the line and so needs no weighting */
  drawPixel(pDC, X0, Y0, BaseColor);

  int16_t DeltaX, DeltaY, XDir;
  if ((DeltaX = X1 - X0) >= 0) {
    XDir = 1;
  } else {
    XDir = -1;
    DeltaX = -DeltaX; // make DeltaX positive
  }
  /* Special-case horizontal, vertical, and diagonal lines, which
     require no weighting because they go right through the center of
     every pixel */
  if ((DeltaY = Y1 - Y0) == 0) {
    // Horizontal line
    while (DeltaX-- != 0) {
      X0 += XDir;
      drawPixel(pDC, X0, Y0, BaseColor);
    }
    return;
  }
  if (DeltaX == 0) {
    // Vertical line
    do {
      Y0++;
      drawPixel(pDC, X0, Y0, BaseColor);
    } while (--DeltaY != 0);
    return;
  }
  if (DeltaX == DeltaY) {
    // Diagonal line
    do {
      X0 += XDir;
      Y0++;
      drawPixel(pDC, X0, Y0, BaseColor);
    } while (--DeltaY != 0);
    return;
  }  
  // Line is not horizontal, diagonal, or vertical
  uint16_t ErrorAcc = 0;  // initialize the line error accumulator to 0
  // # of bits by which to shift ErrorAcc to get intensity level
  const uint16_t IntensityShift = 16 - IntensityBits;
  /* Mask used to flip all bits in an intensity weighting, producing the
     result (1 - intensity weighting) */
  const uint16_t WeightingComplementMask = NumLevels - 1;
   
  // Is this an X-major or Y-major line?
  if (DeltaY > DeltaX) {
    /* Y-major line; calculate 16-bit fixed-point fractional part of a
       pixel that X advances each time Y advances 1 pixel, truncating the
       result so that we won't overrun the endpoint along the X axis */
    uint16_t ErrorAdj = ((unsigned long) DeltaX << 16) / (unsigned long) DeltaY;
    // Draw all pixels other than the first and last
    while (--DeltaY) {
      uint16_t ErrorAccTemp = ErrorAcc;   // remember currrent accumulated error
      ErrorAcc += ErrorAdj;      // calculate error for next pixel
      if (ErrorAcc <= ErrorAccTemp) {
	// The error accumulator turned over, so advance the X coord
	X0 += XDir;
      }
      Y0++; // Y-major, so always advance Y
      /* The IntensityBits most significant bits of ErrorAcc give us the
	 intensity weighting for this pixel, and the complement of the
	 weighting for the paired pixel */
      uint16_t Weighting = ErrorAcc >> IntensityShift;
      drawPixel(pDC, X0, Y0, BaseColor + Weighting);
      drawPixel(pDC, X0 + XDir, Y0,
		BaseColor + (Weighting ^ WeightingComplementMask));
    }
    /* Draw the final pixel, which is always exactly intersected by
       the line and so needs no weighting */
    drawPixel(pDC, X1, Y1, BaseColor);
    return;
  }
  /* It's an X-major line; calculate 16-bit fixed-point fractional part of a
     pixel that Y advances each time X advances 1 pixel, truncating the
     result to avoid overrunning the endpoint along the X axis */
  uint16_t ErrorAdj = ((unsigned long) DeltaY << 16) / (unsigned long) DeltaX;
  // Draw all pixels other than the first and last
  while (--DeltaX) {
    uint16_t ErrorAccTemp = ErrorAcc;   // remember currrent accumulated error
    ErrorAcc += ErrorAdj;      // calculate error for next pixel
    if (ErrorAcc <= ErrorAccTemp) {
      // The error accumulator turned over, so advance the Y coord
      Y0++;
    }
    X0 += XDir; // X-major, so always advance X
    /* The IntensityBits most significant bits of ErrorAcc give us the
       intensity weighting for this pixel, and the complement of the
       weighting for the paired pixel */
    uint16_t Weighting = ErrorAcc >> IntensityShift;
    drawPixel(pDC, X0, Y0, BaseColor + Weighting);
    drawPixel(pDC, X0, Y0 + 1,
	      BaseColor + (Weighting ^ WeightingComplementMask));
  }
  /* Draw the final pixel, which is always exactly intersected by the line
     and so needs no weighting */
  drawPixel(pDC,X1, Y1, BaseColor);
}

extern "C"
int main(void)
{
  printf("DOS gcc tests, (c) Rene Rebe, ExactCODE; 2018\n");
  
  uint16_t ret = svga_mode_info(0x101, &modeinfo);
  printf("mode: %x, %d, %d %x\n", ret,
	 modeinfo.XResolution, modeinfo.YResolution, modeinfo.PhysBasePtr);
  //  ret = svga_mode(0x101 | (1 << 14) | (1 << 15)); // linear, no clear
  if (ret != 0x4f) {
    printf("mode set failed\n");
    return 1;
  }
  
  vga_mode(0x13); // 320x200 256c
  vga_mode_x(); // 320x240, needs enable mode 0x13 first!
  
  const bool antialias = true;
  {
    const uint8_t rbits = 3, gbits = 3, bbits = 2; 
    const uint8_t rmask = (1 << rbits) - 1,
      gmask = (1 << gbits) - 1,
      bmask = (1 << bbits) - 1;
    
    // program rgb-332-bit color palette
    if (!antialias) {
      vgaDacPaletteBegin(0);
      for (uint16_t i = 0; i < 0x100; ++i) {
	uint8_t 
	  r = 0xff * ((i >> 0) & rmask) / rmask,
	  g = 0xff * ((i >> rbits) & gmask) / gmask,
	  b = 0xff * ((i >> (rbits + gbits)) & bmask) / bmask;
	
	//printf("%d 0x%x 0x%x 0x%x\n", i, r, g, b);
	vgaDacPaletteSet(r, g, b);
      }
    } else {
      // program gray-8-bit color palette
      vgaDacPaletteBegin(0);
      for (uint16_t i = 0; i < 0x100; ++i)
	vgaDacPaletteSet(i, i, i);
    }
    
    
    /*
    unrealptr p(modeinfo.PhysBasePtr);
    vga_w = modeinfo.XResolution;
    vga_h = modeinfo.YResolution;
    vga_planes = 1;*/
    unrealptr p(0xa0000);
    
    int i = 0;
    while (!kbhit() && ++i < 100 ) {
      int x = random() * vga_w / RAND_MAX;
      int y = random() * vga_h / RAND_MAX;
      int x2 = random() * vga_w / RAND_MAX;
      int y2 = random() * vga_h / RAND_MAX;
      uint8_t c=  random() * 0xff / RAND_MAX;
      
      if (antialias)
	drawWuLine(p, 0, 0, x2, y2, c, 256, 8);
      else
	drawBresenhamLine(p, x, y, x2, y2, c);
    }

    while (!kbhit()) {
    }
  }
  
  vga_mode(0x03); // back ot text mode
  
  printf("end.\n");
  return 0;
}
