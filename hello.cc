/*
 * DOS/GCC hello world tests.
 * Copyright (C) 2018 René Rebe, ExactCODE; Germany.
 * 
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; version 2.
 *
 * Alternatively, commercial licensing options are available from the
 * copyright holder ExactCODE GmbH Germany.
 */

// TODO: png / jpeg loading ;_)
// TODO: mouse
// TODO: document segment register use
// TODO: find a nice way to adress any memory
// TODO: nice BIOS / DOS syscall wrappers
// TODO: svga
// TODO: some kind of timer
// TODO: interrupts
// TODO: DMA
// TODO: opl3
// TODO: SB
// TODO: vga off-screen blitting
// TODO: protect & unreal modes
// TODO: link .exe files!

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

extern "C"
int main(void)
{
  printf("DOS gcc tests, (c) Rene Rebe, ExactCODE; %d.\n", 2018);
  
  //vga_mode(0x03); // back to text mode
  if (false)
    {
      farptr p(0xb8000);
      for (int i = 80*2+1; i < 80 * 4; i += 2) {
	p.set8(160*4, 'a');
      }
      
      for (int i = 80*2; i < 80 * 4; i += 2) {
	p.set8(i, 'A');
      }
      
      for (int i = 0; i < 80; ++i) {
	p.set16(80*4 + i * 2, (i << 8) + 'X');
      }
      
      for (int i = 0; i < 80*25; ++i) {
	//p.set16(i * 2, (i << 8) | (i & 0xff));
	p.set8(i * 2, i);
	p.set8(i * 2 + 1, i);
      }
    
      return 0;
    }
  
    {
      // 0x12 640x480 16c
      vga_mode(0x13); // 320x200 256c
      vga_mode_x(); // 320x240, needs enable mode 0x13 first!
      
      // program gray-8-bit color palette
      vgaDacPaletteBegin(0);
      for (uint16_t i = 0; i < 0x100; ++i)
	vgaDacPaletteSet(i, i, i);

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
      
      float start = dos_gettimef();
      
      uint16_t offset = 0; // double-buffer start of frame offset
      for (uint16_t frame = 0; frame < 0xff; ++frame) {
	for (uint8_t plane = 0; plane < vga_planes; ++plane) {
	  if (vga_planes > 1)
	    vga_write_planes(1 << plane);
	  
	  for (unsigned y = 0; y < vga_h; ++y) {
	    for (unsigned x = plane; x < vga_w; x += vga_planes) {
	      float _x = (float)x / vga_w - .5;
	      float _y = (float)y / vga_h - 1;
	      float _time = .1 * frame;
	      
	      float v1 = sinf(10. * _x + _time); // 1st test
	      float v2 = sinf(10. * (_x *sinf(_time/2) + _y * cosf(_time/3)) + _time);
	      float cx = _x + .5 * sinf(.2 * _time);
	      float cy = _y + .5 * cosf(.3 * _time);
	      float v3 = sinf(sqrtf((cx*cx + cy*cy)*100 + 1) + _time);
	      
	      float v = v1 + v2 + v3;

	      uint8_t r, g, b;
	      r = (sinf(v * M_PI) + 1) * 0xff / 2;
	      g = (sinf(v * M_PI + 2 * M_PI / 3) + 1) * 0xff / 2;
	      b = (sinf(v * M_PI + 4 * M_PI / 3) + 1) * 0xff / 2;
	      
	      //uint8_t r = (sin(v * 5 * M_PI) + 1) * 0xff / 2;
	      //uint8_t r = (v + 1) * 0xff / 2;
	      
	      r >>= 5;
	      g >>= 5;
	      b >>= 6;
	      
	      p.set8((offset + y) * (vga_w / vga_planes) + (x / vga_planes),
		     r | (g<<3) | (b<<6));
	    }
	  }
	}
	
	{
	  static char text[24];
	  float now = dos_gettimef();
	  float fps = (float)frame / (now - start);
	  sprintf(text, "VGA %f fps", fps);
	  
	  vga_blit_text(p, 8, offset + 16, text, 0xff, 0);
	}
	
	if (vga_planes > 1) {
	  vga_scanout_offset(offset * vga_w / vga_planes);
	  if (offset) offset = 0;
	  else offset = vga_h;
	}
	
	if (kbhit())
	  break;
      }
      
      vga_mode(0x03); // back ot text mode
    }
    
    printf("end.\n");
    return 0;
}
