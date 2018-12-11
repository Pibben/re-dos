/*
 * VGA and SVGA VBE defines
 * Copyright (C) 2018 René Rebe, ExactCODE; Germany.
 * 
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; version 2.
 *
 * Alternatively, commercial licensing options are available from the
 * copyright holder ExactCODE GmbH Germany.
 */

uint16_t vga_w = 320, vga_h = 240;
uint8_t vga_planes = 4;

static const uint16_t MISC_OUTPUT = 0x3c2; // Miscellaneous Output register
static const uint16_t SC_INDEX = 0x3c4; // Sequence Controller Index
static const uint16_t GC_INDEX = 0x3ce; // Graphics Controller Index
static const uint16_t CRTC_INDEX = 0x3d4; // CRT Controller Index
static const uint16_t INPUT_STATUS_1 = 0x3da; // Input Status 1 register

// Switch to VGA mode.
static void vga_mode(uint16_t mode)
{
  asm volatile ("int   $0x10\n"
		: /* no outputs */
		: "a"(mode) /* inputs */
		: /* no clobbers */);
}

struct __attribute__ ((packed)) VbeInfoBlock {
  char VbeSignature[4]; // VBE Signature
  uint16_t VbeVersion;  // VBE Version
  uint32_t OemStringPtr; // Pointer to OEM String
  uint8_t Capabilities[4]; // Capabilities of graphics controller
  uint32_t VideoModePtr; // Pointer to VideoModeList
  uint16_t TotalMemory; // Number of 64kb memory blocks
  // Added for VBE 2.0
  uint16_t OemSoftwareRev; // VBE implementation Software revision
  uint32_t OemVendorNamePtr; // Pointer to Vendor Name String
  uint32_t OemProductNamePtr; // Pointer to Product Name String
  uint32_t OemProductRevPtr; // Pointer to Product Revision String
  uint8_t Reserved[222]; // Reserved for VBE implementation scratch area
  uint8_t OemData [256]; // Data Area for OEM Strings
};

// Get SVGA VGA info
static uint16_t svga_vbe_info(struct VbeInfoBlock* info)
{
  uint16_t ret;
  asm volatile ("push %%es\n"
		"mov %%bx, %%di\n"
		"mov %%ds, %%bx\n"
		"mov %%bx, %%es\n"
		"mov $0x4f00, %%ax\n"
		"int $0x10\n"
		"pop %%es\n"
		: "=ax"(ret) /* outputs */
		: "bx"(info) /* inputs */
		: "di" );
  return ret;
}

struct ModeInfoBlock {
  uint16_t ModeAttributes; // mode attributes
  uint8_t WinAAttributes; // window A attributes
  uint8_t WinBAttributes; // window B attributes
  uint16_t WinGranularity; // window granularity
  uint16_t WinSize; // window size
  uint16_t WinASegment; //window A start segment
  uint16_t  WinBSegment; // window B start segment
  uint32_t WinFuncPtr; // pointer to window function
  uint16_t  BytesPerScanLine; // bytes per scan line
  
  // Mandatory information for VBE 1.2 and above
  uint16_t XResolution; // horizontal resolution in pixels or characters
  uint16_t YResolution; // vertical resolution in pixels or characters
  uint8_t  XCharSize; // character cell width in pixels
  uint8_t YCharSize; // character cell height in pixels
  uint8_t NumberOfPlanes; // number of memory planes
  uint8_t BitsPerPixel; // bits per pixel
  uint8_t NumberOfBanks; // number of banks
  uint8_t  MemoryModel; // memory model type
  uint8_t  BankSize; // bank size in KB
  uint8_t NumberOfImagePages; // number of images
  uint8_t Reserved; // reserved for page function
  
  // Direct Color fields (required for direct/6 and YUV/7 memory models)
  uint8_t RedMaskSize; // size of direct color red mask
  uint8_t RedFieldPosition; // in bits RedFieldPosition
  uint8_t GreenMaskSize; // size of direct color green mask i
  uint8_t GreenFieldPosition; // bit position of lsb of green
  uint8_t BlueMaskSize; // size of direct color blue mask in 
  uint8_t BlueFieldPosition; // bit position of lsb of blue m
  uint8_t RsvdMaskSize; // size of direct color reserved mask in bits
  uint8_t RsvdFieldPosition; // bit position of lsb of reserved mask
  uint8_t DirectColorModeInfo; // direct color mode attributes
  
  // Mandatory information for VBE 2.0 and above
  uint32_t PhysBasePtr; // physical address for flat memory frame buffer
  uint32_t OffScreenMemOffset; // pointer to start of off screen memory
  uint32_t OffScreenMemSize; // amount of off screen memory in 1k units
  uint8_t Reserved2 [206]; // remainder of ModeInfoBlockModeInfoBlock struc
};

// Get SVGA mode info
static uint16_t svga_mode_info(uint16_t mode, struct ModeInfoBlock* info)
{
  uint16_t ret;
  asm volatile ("push %%es\n"
		"mov %%bx, %%di\n"
		"mov %%ds, %%bx\n"
		"mov %%bx, %%es\n"
		"mov $0x4f01, %%ax\n"
		"int $0x10\n"
		"pop %%es\n"
		: "=ax"(ret) /* outputs */
		: "cx"(mode), "bx"(info) /* inputs */
		: "di" );
  return ret;
}

// Switch to SVGA mode.
static uint16_t svga_mode(uint16_t mode)
{
  uint16_t ret;
  asm volatile ("mov $0x4f02, %%ax\n"
		"int $0x10\n"
		: "=ax"(ret) /* outputs */
		: "bx"(mode) /* inputs */
		: /* no clobbers */);
  return ret;
}

static uint16_t svga_set_start(uint16_t x, uint16_t y)
{
  uint16_t ret;
  asm volatile ("mov $0x4f07, %%ax\n"
		"mov $0, %%bx\n"
		"int $0x10\n"
		: "=ax"(ret) /* outputs */
		: "cx"(x), "dx"(y) /* inputs */
		: /* no clobbers */);
  return ret;
}


static void vga_mode_x()
{
  // X (320x240, 256 colors) mode set routine. Works on all VGAs.
  // Revised 6/19/91 to select correct clock; fixes vertical roll
  // problems on fixed-frequency (IBM 851X-type) monitors.
  // Modified from public-domain mode set code by John Bridges.

  // Index/data pairs for CRT Controller registers that differ between
  // mode 13h and mode X.
  const uint16_t CRTParms[] = {
    0x0d06, // vertical total
    0x3e07, // overflow (bit 8 of vertical counts)
    0x4109, // cell height (2 to double-scan)
    0xea10, // v sync start
    0xac11, // v sync end and protect cr0-cr7
    0xdf12, // vertical displayed
    0x0014, // turn off dword mode
    0xe715, // v blank start
    0x0616, // v blank end
    0xe317, // turn on byte mode
  };
  
  outw(SC_INDEX, 0x604); // disable chain4 mode
  outw(SC_INDEX, 0x100); // synchronous reset while setting Misc Output
  // for safety, even though clock unchanged
  outb(MISC_OUTPUT, 0xe3); // select 25 MHz dot clock & 60 Hz scanning rate
  outw(SC_INDEX, 0x300); // undo reset (restart sequencer)
  
  // reprogram the CRT Controller
  outb(CRTC_INDEX, 0x11);  // VSync End reg contains register write
  
  // CRT Controller Data register
  uint8_t cur = inb(CRTC_INDEX + 1); // get current VSync End register setting
  outb(CRTC_INDEX + 1, cur & 0x7f); // remove write protect on various CRTC registers
    
  for (unsigned i = 0; i < sizeof(CRTParms) / sizeof(*CRTParms); ++i) {
    outw(CRTC_INDEX, CRTParms[i]);
  }
}

static void vga_write_planes(uint8_t planes)
{
  // enable write map mask to "planes"
  outw(SC_INDEX, (planes << 8) | 0x02); // memory plane write enable
}

static void vga_read_planes(uint8_t planes)
{
  // enable read map mask to "planes"
  outw(SC_INDEX, (planes << 8) | 0x04); // READ_MAP
}

static void vga_bit_mask(uint8_t mask)
{
  // enable bit mask for latch vs. cpu writes
  outw(GC_INDEX, (mask << 8) | 0x08); // BIT_MASK
}

static void vga_wait_vsync()
{
  // TODO: also optional wait for "disabled" bit?
  uint8_t status;
  do {
    status = inb(INPUT_STATUS_1);
  } while (!(status & 8));
}

static void vga_wait_enable()
{
  uint8_t status;
  do {
    status = inb(INPUT_STATUS_1);
  } while ((status & 1));
}

static void vga_scanout_offset(uint16_t offset)
{
  // Shows the page at the specified offset in the bitmap. Page is displayed when
  // this routine returns.
  
  const uint8_t START_ADDRESS_HIGH = 0xc; // bitmap start address high byte
  const uint8_t START_ADDRESS_LOW = 0xd; // bitmap start address low byte

  // Wait for display enable to be active (status is active low), to be
  // sure both halves of the start address will take in the same frame.
  vga_wait_enable();
  
  // Set the start offset in display memory of the page to display.
  outw(CRTC_INDEX, START_ADDRESS_LOW | (offset << 8)); // start address low
  outw(CRTC_INDEX, START_ADDRESS_HIGH | (offset & 0xff00)); // start address low
  
  // Now wait for vertical sync, so the other page will be invisible when
  // we start drawing to it.
  return vga_wait_vsync();
}

void vgaDacPaletteBegin(uint8_t Color = 0)
{
  // mask all registers even though we only need 1 of them
  outb(0x03C6, 0xff);
  outb(0x03C8, Color);
}

void vgaDacPaletteSet(uint8_t r, uint8_t g, uint8_t b)
{
  // set one color, requires ...Begin
  outb(0x03C9, r >> 2);
  outb(0x03C9, g >> 2);
  outb(0x03C9, b >> 2);
}

//#include "../unity/Fonts/Bitmap/efont.h"
#ifndef __EFONT_H
static uint32_t fontaddr = 0;

// to save memory this is not re-entrant, and only copies one char at a time
static uint8_t* getFontGlyph(uint16_t i) {
  // initialize with VGA ROM font
  if (!fontaddr) {
    uint32_t height;
    uint16_t mode = 0x0200; // 8x16, 0x600: 8x16
    uint32_t addr;
    asm volatile ("int3\n"
		  "push %%es\n"
		  "push %%bp\n"
		  "mov $0x1130, %%ax\n"
		  "int $0x10\n"
		  "mov %%es, %%ax\n"
		  "shl $16, %%eax\n"
		  "add %%bp, %%ax\n"
		  "pop %%bp\n"
		  "pop %%es\n"
		  : "=eax"(addr), "=ecx"(height) /* outputs */
		  : "b"(mode) /* inputs */
		  : "dx" );
    fontaddr = addr;
  }
  
  if (i > 0xff) {
    fprintf(stderr, "glyph out-of-bounds: %x\n", i);
    i = 0;
  }
  
  // copy from BIOS
  static uint8_t font[14];
  uint32_t seg = _segment(fontaddr);
  uint16_t off = _offset(fontaddr);
  farptr p(seg << 4);
  const uint8_t height = 14; // TODO: ?
  
  for (int j = 0; j < 14; ++j)
    font[j] = p.get8(off + i * height + j);
  p.disable(); // explicitly restore previous segment register
  
  return font;
}
#endif

template<typename T>
void vga_blit_text(T& p, uint16_t _startx, uint16_t _starty,
		   const char* text, uint8_t fg = 0xff, uint8_t bg = 0)
{
  for (uint8_t plane = 0; plane < vga_planes; ++plane) {
    if (vga_planes > 1)
      vga_write_planes(1 << plane);
    
    for (uint16_t i = 0; text[i]; ++i) {
      uint16_t startx = _startx + i * 8;
      uint8_t* glyph = getFontGlyph(((uint8_t*)text)[i]);
      
      // blit the glyph
      for (uint16_t y = 0; y < 14; ++y) {
	uint32_t starty = (_starty + y) * (vga_w / vga_planes);
	uint8_t grow = glyph[y];
	for (uint16_t x = plane; x < 8; x += vga_planes) {
	  const uint8_t bit = grow & (0x80 >> x);
	  p.set8(starty + ((startx + x) / vga_planes), bit ? fg : bg);
	}
      }
    }
  }
}
