/*
 * fast, latched VGA mode-x fills and copies
 * Copyright (C) 2018 René Rebe, ExactCODE; Germany.
 * 
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; version 2.
 *
 * Alternatively, commercial licensing options are available from the
 * copyright holder ExactCODE GmbH Germany.
 */

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

#include "libc/stdlib.h"

// TODO: WIP, needs filling uneven sides!
template <typename T>
void vgax_fill(T& p, uint16_t _x, uint16_t _y,
	       uint16_t w, uint16_t h, uint8_t color)
{
  // write to all planes at once
  vga_write_planes(0xf);
  for (int y = _y; y < _y + h; ++y) {
    uint16_t offset = y * (vga_w / 4);
    for (int x = _x / 4; x < (_x + w) / 4; ++x) {
      p.set8(offset + x, color);
    }
  }
}

// TODO: WIP, needs filling uneven sides!
// TODO: also should check if overlap -> reverse copy, ..!
template <typename T>
void vgax_copy(T& p, uint16_t _x, uint16_t _y,
	       uint16_t w, uint16_t h, uint16_t sx, uint16_t sy)
{
  // write to all planes at once
  vga_write_planes(0xf); // write to all planes
  vga_bit_mask(0x00); // no plane from CPU, all data from latches
  
  // TODO: does this reliably work wo/ write mode change?
  for (int y = _y; y < _y + h; ++y) {
    uint16_t offset = y * (vga_w / 4);
    uint16_t soffset = sy++ * (vga_w / 4) + sx / 8;
    for (int x = _x / 4; x < (_x + w) / 4; ++x) {
      // all latches are always read
      register uint8_t _=  p.get8(soffset + x);
      p.set8(offset + x, _); // write from latches
    }
  }
  
  // TODO: maybe read and reset the prevoius mask?
  vga_bit_mask(0xff); // reset latch bit mask batk to CPU
}

#include "libc/string.h"

extern "C"
int main(void)
{
  printf("fastvga test, (c) Rene Rebe, ExactCODE; 2018.\n");
  
  if (false) {
    for (int i = 0; i < 10; ++i)
      printf("rand: %x\n", random());
    return 1;
  }
  
  const float start = dos_gettimef();
  const int frames = 0x1000;
  {
    // 0x12 640x480 16c
    vga_mode(0x13); // 320x200 256c
    vga_mode_x(); // 320x240, needs enable mode 0x13 first!
    
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
    
    farptr p(0xa0000);
    //unrealptr p(0xa0000);
    const uint8_t doublebuffer = 0;
    
    uint16_t offset = 0; // double-buffer start of frame offset
    for (uint16_t frame = 0; frame < frames; ++frame) {
      
      const unsigned off = frame % 64;
      
      vgax_fill(p, off * 4, off * 4, 64, 128, off * 21 - 1);
      if (doublebuffer && vga_planes > 1) {
	vga_scanout_offset(offset * vga_w / vga_planes);
	if (offset) offset = 0;
	else offset = vga_h;
      }
      
      if (kbhit())
	break;
    }

    {
      static char text[24];
      float now = dos_gettimef();
      if (now == start) now += 1. / 18.2;
      float fps = (float)frames / (now - start);
      
      sprintf(text, "VGA %f fps", fps);
      vga_blit_text(p, 8, 8, text, 0xff, 0);
    }
    
    if (true) {
      for (uint16_t frame = 0; frame < frames; ++frame) {
	vgax_copy(p, 0, 64, 128, 64, 0, 0);
	if (doublebuffer && vga_planes > 1) {
	  vga_scanout_offset(offset * vga_w / vga_planes);
	  if (offset) offset = 0;
	  else offset = vga_h;
	}
	if (kbhit()) break;
      }
      
      for (uint16_t frame = 0; frame < frames; ++frame) {
	vgax_fill(p, 200 - frame * 4, 200 - frame * 4, 128, 64, frame * 21 - 1);
	if (doublebuffer && vga_planes > 1) {
	  vga_scanout_offset(offset * vga_w / vga_planes);
	  if (offset) offset = 0;
	  else offset = vga_h;
	}
	if (kbhit()) break;
      }
    }
    
    if (false) {
      for (uint16_t frame = 0; frame >= 0; ++frame) {
	uint16_t x, y, w, h;
	if (false) {
	  x = random() * vga_w / RAND_MAX / 2;
	  y = random() * vga_h / RAND_MAX / 2;
	  w = random() * vga_w / RAND_MAX / 2;
	  h = random() * vga_h / RAND_MAX / 2;
	  uint8_t c = random() * vga_h / RAND_MAX / 2;
	  vgax_fill(p, x, y, w, h, c);
	}
	
	uint16_t x2, y2;
	x = random() * vga_w / RAND_MAX / 2;
	y = random() * vga_h / RAND_MAX / 2;
	x2 = random() * vga_w / RAND_MAX / 2;
	y2 = random() * vga_h / RAND_MAX / 2;
	w = random() * vga_w / RAND_MAX / 2;
	h = random() * vga_h / RAND_MAX / 2;
	vgax_copy(p, x, y, w, h, x2, y2);
	
	if (doublebuffer && vga_planes > 1) {
	  vga_scanout_offset(offset * vga_w / vga_planes);
	  if (offset) offset = 0;
	  else offset = vga_h;
	}

	if (kbhit()) break;
      }
    }

    getch();
    
    vga_mode(0x03); // back ot text mode
  }
  
  printf("end.\n");
  return 0;
}
