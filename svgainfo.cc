/*
 * SVGA info
 * Copyright (C) 2018 René Rebe, ExactCODE; Germany.
 * 
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; version 2.
 *
 * Alternatively, commercial licensing options are available from the
 * copyright holder ExactCODE GmbH Germany.
 */

#define DOS_WANT_ARGS 1

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
#include "libc/string.h"

#include "vga.h"

static struct VbeInfoBlock vbeinfo;
static struct ModeInfoBlock modeinfo;

void printvbeinfo(VbeInfoBlock& vbeinfo)
{
  printf("VBE: %c%c%c%c, ver: %x, memory: %d, modelist: %x\n",
	 vbeinfo.VbeSignature[0], vbeinfo.VbeSignature[1],
	 vbeinfo.VbeSignature[2], vbeinfo.VbeSignature[3],
	 vbeinfo.VbeVersion, vbeinfo.TotalMemory * 64,
	 vbeinfo.VideoModePtr);
  
  printf("OEM: %x rev: %x vendor: %x product: %x Rev: %x\n",
	 vbeinfo.OemStringPtr, vbeinfo.OemSoftwareRev,
	 vbeinfo.OemVendorNamePtr, vbeinfo.OemProductNamePtr,
	 vbeinfo.OemProductRevPtr);

  // TODO: more generic farptr string support, ..!
  if (vbeinfo.OemStringPtr) {
    uint32_t seg = _segment(vbeinfo.OemStringPtr);
    uint16_t off = _offset(vbeinfo.OemStringPtr);
    farptr p(seg << 4);
    for (uint8_t c; (c = p.get8(off)); ++off) {
      printf("%c", (char)c);
    }
    printf("\n");
  }
}

void printmode(uint16_t mode, ModeInfoBlock& modeinfo)
{
  const uint16_t attr = modeinfo.ModeAttributes;
  printf("mode: %x: %dx%d @%d %x %x%s%s%s%s%s\n",
	 mode, modeinfo.XResolution, modeinfo.YResolution,
	 modeinfo.BitsPerPixel, modeinfo.PhysBasePtr, attr,
	 attr & (1<<0) ? "" : " UNSUPPORTED",
	 attr & (1<<2) ? " TTY_SUPPORTED" : "",
	 attr & (1<<3) ? " COLOR" : " MONO",
	 attr & (1<<4) ? " GRAPHIC" : " TEXT",
	 attr & (1<<5) ? " " : " VGA_COMPAT"
	 );
}

extern "C"
int main(int argc, const char** argv)
{
  uint16_t ret;
  printf("svgainfo, (c) Rene Rebe, ExactCODE; 2018\n");
  
  uint8_t list = 0;
  for (; argc; --argc, ++argv) {
    if (strcmp(*argv, "-l") == 0){
      list = 1;
    } else {
      printf("no option: %s\n", *argv);
      return 1;
    }
  }
  
  ret = svga_vbe_info(&vbeinfo);
  if (ret != 0x4f) {
    printf("no VBE\n");
    return 1;
  }
  
  if (vbeinfo.VideoModePtr) {
    uint32_t seg = _segment(vbeinfo.VideoModePtr);
    uint16_t off = _offset(vbeinfo.VideoModePtr);
    farptr p(seg << 4);
    
    //printf("%x %x\n", seg, off);
    uint16_t mode;
    if (list) {
      bool gotmodes = false;
      for (; (mode = p.get16(off)) != 0xffff; off += 2) {
	if (!mode) {
	  printf("not really a vbe mode list?\n");
	  break;
	}
	
	ret = svga_mode_info(mode, &modeinfo);
	if (ret != 0x4f) {
	  printf("mode: %x: get info failed: %x\n", mode, ret);
	} else {
	  gotmodes = true;
	  printmode(mode, modeinfo);
	}
      }
      
      if (!gotmodes) {
	printf("trying std modes:\n");
	for (uint16_t mode = 0x100; mode <= 0x11b; ++mode) {
	  ret = svga_mode_info(mode, &modeinfo);
	  if (ret != 0x4f) {
	    //printf("mode: %x: get info failed: %x\n", ret);
	  } else {
	    printmode(mode, modeinfo);
	  }
	}
      }
    }
  }

  printvbeinfo(vbeinfo);
  return 0;
}
