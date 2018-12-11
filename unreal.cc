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

struct ModeInfoBlock modeinfo;

extern "C"
int main(void)
{
  printf("DOS gcc tests, (c) Rene Rebe, ExactCODE; 2018\n");
  
  uint16_t ret = svga_mode_info(0x101, &modeinfo);
  printf("mode: %x, %d, %d %x\n", ret,
	 modeinfo.XResolution, modeinfo.YResolution, modeinfo.PhysBasePtr);
  ret = svga_mode(0x101 | (1 << 14) | (1 << 15)); // linear, no clear
  if (ret != 0x4f) {
    printf("mode set failed\n");
    return 1;
  }

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
    
    unrealptr p(modeinfo.PhysBasePtr);
    
    float start = dos_gettimef();
    vga_w = modeinfo.XResolution;
    vga_h = modeinfo.YResolution;
    vga_planes = 1;
    const uint8_t doublebuffer = 1;

    uint16_t offset = 0; // double-buffer start of frame offset
    for (uint16_t frame = 0; frame < 0xff; ++frame) {
      for (uint8_t plane = 0; plane < vga_planes; ++plane) {
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
	
	vga_blit_text(p, 8, offset + 16 + frame * 2, text, 0xff, 0);
      }
      
      if (doublebuffer) {
	// if you do not want to see the 1st frame draw,
	// simply move this to the beginning of the loop, ...
	svga_set_start(0, offset);
	if (offset) offset = 0;
	else offset = vga_h;
      }

      if (kbhit())
	break;
    }
  }
  
  vga_mode(0x03); // back ot text mode
  
  printf("end.\n");
  return 0;
}
