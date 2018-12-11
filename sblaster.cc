/*
 * SoundBlaster test demo
 * Copyright (C) 2018 René Rebe, ExactCODE; Germany.
 * 
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; version 2.
 *
 * Alternatively, commercial licensing options are available from the
 * copyright holder ExactCODE GmbH Germany.
 */

// sox test.flac -r 22050 -e signed -b 8 -c 1 test.raw

// TODO: test 16-bit DMA
// read .wav header
// more modes, 16-bit,
// single, non-auto, ...

#define DOS_WANT_ARGS
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

static void (*isr_system)();

#define malloc dos_malloc
#define free dos_free

#define inp inb
#define outp outb
static const uint16_t sbport = 0x220;

static void usleep(uint32_t usec)
{
  usec *= 1000;
  for (; usec > 0; --usec)
    asm("nop\n");
}

static int16_t sb_read() {
  uint8_t v;
  for (int i = 0; i < 100; ++i) {
    v = inp(sbport + 0xe); // status port
    //printf("status: %x\n", v);
    if (v & 0x80) {
      v = inp(sbport + 0xa); // read port
      //printf("read: %x\n", v);
      return v;
    }
  }
  return -1;
}

static int8_t sb_write(uint8_t cmd) {
  uint8_t v;
  for (int i = 0; i < 100; ++i) {
    v = inp(sbport + 0xc); // write port
    //printf("status: %x\n", v);
    if (v >> 7 == 1) {
      outp(sbport + 0xc, cmd); // write port
      return 0;
    }
  }
  return -1;
}

static int8_t sb_reset() {
  outp(sbport + 0x6, 1); // reset port
  usleep(3);
  outp(sbport + 0x6, 0);
  // try to read
  uint8_t v = sb_read();
  if (v != 0xaa) {
    return 0;
  }
  return v;
}

static volatile uint16_t frame = 0;
static int fd;
static void* buffer;

// load from file
int loadFrame(int fd, int offset, int len)
{
  farptr dmabuf((uint32_t)buffer);
  uint8_t buf[512];
  for (int i = offset; i < offset + len;) {
    printf("."); // reading %p %d %d - ", buf, i, sizeof(buf));
    int ret = read(fd, buf, sizeof(buf));
    //printf("ret: %d %x\n", ret, dos_errno);
    if (!ret)
      return 0;
    for (int j = 0; j < sizeof(buf); ++i, ++j)
      dmabuf.set8(i, buf[j]);
  }
  printf("\n");
  return 1;
}

static uint8_t bits = 16;

static void __attribute__((noinline)) isr_sound2() {
  uint8_t v = inp(sbport + (bits <= 8 ? 0xe : 0xf)); // irq ack, 0xf for 16-bit
  
  // TODO: potentially unsave! should check if DOS is running!
  //printf("isr: %d %x\n", frame, v);
  
  // load more data
  if (fd) {
    int offset = 0x10000 / 2;
    loadFrame(fd, frame & 1 ? offset : 0, offset);
  }
  
  ++frame;
}

extern "C"
// "naked" attribute new since gcc-7, do not support C in clang
void __attribute__((naked)) isr_sound()
{
  // save all, and establish DS, assuming our CS == DS
  asm volatile ("int3\n"
		"pusha\n"
		//"cli\n"
		"mov %%cs, %%ax\n" // not needed in dosbox?
		"mov %%ax, %%ds\n"
		: /* no output */
		: /* no input */
		: /* no clobber*/);
  
  isr_sound2();

  // call original
  if (isr_system) {
    asm volatile (//"mov (%0), %%eax\n"
		  ".code16\n"
		  "pushf\n" // for iret return to us!		
		  "lcall *(%0)\n" // * absolute jump
		  ".code16gcc\n"
		  : /* no output */
		  : "m"(isr_system)
		  : /* no clobber*/);
  } else {
    outp(0x20, 0x20); // Ack PIC
    //outp(0xa0, 0x20); // Ack PIC
  }
  
  // return from interrupt
  asm volatile ("popa\n"
		".code16\n"
		"iret\n"
		".code16gcc\n"
		: /* no output */
		: /* no input */
		: /* no clobber*/);
}

#include "dma.h"
 
extern "C"
int main(int argc, const char** argv)
{
  printf("SoundBlaster DMA (c) Rene Rebe, ExactCODE; 2018\n");

  const uint8_t sb_irq = 5; // TODO: getenv
  const uint8_t sb_int = 0x8 + sb_irq;

  uint16_t rate = 44100;
  uint8_t channels = 2;
  
  uint8_t list = 0;
  for (; argc; --argc, ++argv) {
    fd = open(*argv, 0, O_RDONLY);
    printf("fd: %d %x\n", fd, dos_errno);
    if (!fd) {
      printf("failed to open: %s\n", *argv);
      return 1;
    }
#if 0
    if (strcmp(*argv, "-l") == 0){
      list = 1;
    } else {
      printf("no option: %s\n", *argv);
      return 1;
    }
#endif
  }


  // reset
  if (!sb_reset()) {
    printf("No blaster\n");
    return 1;
  }
  printf("SB @ %x %x\n", sb_irq, sb_int);
  
  
  isr_system = dos_getvect(sb_int);
  printf("cur isr: %p\n", isr_system);
  {
    // save CS
    char* cs;
    asm volatile("mov %%cs, %%ax\n"
		 : "=a"(cs)
		 : /* no inputs */
		 : /* no other clobbers */);
    dos_setvect(sb_int, (void(*)()) (((int)cs << 16) | (int)isr_sound));
  }
  printf("new isr: %p\n", dos_getvect(sb_int));
  
  // try to write
  sb_write(0xe1); // DSP version
  uint8_t v = sb_read();
  uint8_t v2 = sb_read();
  printf("DSP v:%d.%d\n", v, v2);
  {
    char t[64];
    // try to write
    sb_write(0xe3); // DSP (c)
    uint8_t i;
    for (i = 0; i < sizeof(t) - 1; ++i) {
      v = sb_read();
      if (v < 0) break;
      t[i] = v;
    }
    t[i] = 0;
    printf("%s\n", t);
  }
  
  // allocate huge buffer - TODO: fails?
#if 0
  = malloc(0x10);
  printf("alloc: %p %x\n", buffer, dos_errno);
  if (buffer) {
    free(buffer);
    printf("free: %x\n", dos_errno);
  }
#endif

  buffer = (void*)(256*1024); // TODO: hope this is free for now
  farptr dmabuf((uint32_t)buffer);
  
  // test fill memory
  if (true) {
    const float hz = 512;
    for (int i = 0; i < 0x10000; ++i)
      dmabuf.set8(i, sinf((float(i) * M_PI * 2 * hz / 0x10000)) * 127);
  }
  
  if (fd)
    loadFrame(fd, 0, 0x10000);

  // unmask irq
  if (true) {
    //printf("IRQ mask: %x %x\n", inb(0x20 + 1), inb(0xa0 + 1));
    outb(0x20 + 1, inb(0x20 + 1) & ~(1 << sb_irq));
    //printf("IRQ mask: %x %x\n", inb(0x20 + 1), inb(0xa0 + 1));
  }
  
  // test irq
  //sb_write(0xf2); // trigger 8-bit IRQ, 0xf3 for 16-bit
  
  if (true) {
    // program SB
    sb_write(0xd1); // enable "speaker"
    
    sb_write(0x41); // output rate
    sb_write(rate >> 8);
    sb_write(rate & 0xff);
    
    // program DMA
    dma_setup(bits <= 8 ? 1 : 5, (uint32_t)buffer, 0x10000);

    // start SB
    uint32_t samples = 0x10000 / (bits <= 8 ? 1 : 2) / 2; // half frame
    sb_write(bits <= 8 ? 0xc6 : 0xb6); // 0xb6 for 16-bit output
    sb_write(channels < 2 ? 0x10 : 0x30); // mono, signed, or stereo 0x30
    sb_write((samples - 1) & 0xff);	// # of samples - 1
    sb_write((samples - 1) >> 8);
  }
  
  printf("running\n");
  uint16_t lframe = 0;
  while (true) {
    asm("hlt\n");
    if (frame != lframe) {
      lframe = frame;
      printf("frame: %d\n", frame);
    }
    if (kbhit())
      break;
  }
  printf("end.\n");

  if (true) {
    // program SB
    sb_write(0xd3); // disable "speaker"
    
    dma_disable(bits <= 8 ? 1 : 5);
    sb_reset();
  }
  
  dos_setvect(sb_int, isr_system);
  if (fd)
    close(fd);
  fd = 0;
  
  return 0;
}
