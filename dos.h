/*
 * Real-mode DOS glue
 * Copyright (C) 2018 René Rebe, ExactCODE; Germany.
 * 
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; version 2.
 *
 * Alternatively, commercial licensing options are available from the
 * copyright holder ExactCODE GmbH Germany.
 */

void outb(uint16_t port, uint8_t val) {
  asm volatile ("outb %0, %1\n"
		: /* no outputs */
		: "a"(val), "Nd"(port)
		: /* no clobbers */);
}

void outw(uint16_t port, uint16_t val) {
  asm volatile ("outw %0, %1\n"
		: /* no outputs */
		: "a"(val), "Nd"(port)
		: /* no clobbers */);
}

uint8_t inb(uint16_t port) {
  uint8_t val;
  asm volatile ("inb %1, %0\n"
		: "=a"(val)
		: "Nd"(port)
		: /* no clobbers */);
  return val;
}

uint16_t inw(uint16_t port) {
  uint16_t val;
  asm volatile ("inw %1, %0\n"
		: "=a"(val)
		: "Nd"(port)
		: /* no clobbers */);
  return val;
}

static void dos_print(char* string)
{
  asm volatile ("mov $0x09, %%ah\n" // print string
		"int $0x21\n"
		: /* no output */
		: "d"(string)
		: "ah");
}

static int write(int fd, const char* buf, int len)
{
  asm volatile ("mov $0x40, %%ah\n" // write to file
		"int $0x21\n"
		: /* no output */
		: "d"(buf), "c"(len), "b"(fd)
		: "ah");
  // TODO: return
  return len;
}


static bool kbhit()
{
    bool result;
    asm volatile ("mov $1, %%ah\n" // state of keyboard buffer
                  "int $0x16\n"
                  "jnz .1\n"
                  "mov $0, %0\n"
                  "jmp .2\n"
                  ".1:\n"
                  "mov $1, %0\n"
                  ".2:\n"
                  : "=rm"(result));
    return result;
}

static int getch()
{
  int key = 0;
  if (kbhit()) {
    asm volatile ("mov $0, %%ah\n" // read key press
                  "int $0x16\n"
                  : "=a"(key));
  }
  return key;
}

// Warning: output is raw, real-mode seg:offset!
static void (*dos_getvect(uint8_t intr))()
{
  void (*ret)();
  asm volatile ("push %%es\n" // save es
		"mov $0x35, %%ah\n" // getvect, ES:BX!
		"int $0x21\n"
		"mov %%es, %%ax\n" // load far ptr pair
		"shl $16, %%eax\n"
		"add %%bx, %%ax\n"
		"pop %%es\n"
		: "=a"(ret)
		: "al"(intr)
		: /* no clobbers */);
  return ret;
}

// Warning: input is raw, real-mode seg:offset!
static void dos_setvect(uint8_t intr, void (*isr)())
{
  // DS:DX, so assuming
  asm volatile ("push %%ds\n" // save our ds
		"mov %%edx, %%ebx\n" // shuffle seg into ds
		"shr $16, %%edx\n"
		"mov %%dx, %%ds\n"
		"xor %%edx, %%edx\n" // load only offset to ax
		"mov %%bx, %%dx\n"
		"mov $0x25, %%ah\n" // setvect, DS:DX
		"int $0x21\n"
		"pop %%DS\n"
		: /* no output */
		: "al"(intr), "d"(isr)
		: "ah", "bx");  
}

static void dos_gettime(uint8_t* hour, uint8_t* min,
			uint8_t* sec, uint8_t* usec)
{
  uint16_t c, d;
  asm volatile ("mov $0x2c, %%ah\n" // get time
		"int $0x21\n"
		: "=c"(c), "=d"(d)
		: /* no input */
		: "ah");
  if (hour) *hour = c >> 8;
  if (min) *min = c & 0xf;
  if (sec) *sec = d >> 8;
  if (usec) *usec = d & 0xf;
}

static float dos_gettimef()
{
  uint8_t h, m, s, us;
  dos_gettime(&h, &m, &s, &us);
  return 0.01 * us + h * 60 * 60 + m * 60 + s;
}

static void putchar(char ch)
{
  // we can only call DOS w/ data-segment pointers, ...
  static char c[1]; c[0] = {ch};
  write(1, c, sizeof(c));
}


struct farptr {
  farptr(uint32_t base)
    : base(base / 16) {
    enable();
  }
  
  void enable() {
    asm volatile ("push %%es\n"
		  "mov %%dx, %%es\n"
		  "pop %%dx\n"
		  : "=dx"(old) /* no outputs */
		  : "dx"(base)
		  : /* no clobbers */);
  }
  
  void disable() {
    asm volatile ("mov %%dx, %%es\n"
		  : /* no outputs */
		  : "d"(old)
		  : /* no clobbers */);
  }

  uint8_t get8(uint16_t index) {
    uint8_t val;
    asm volatile ("mov %%es:(%1), %0"
		  : "=q"(val)
		  : "bSD"(index)
		  : /* no clobbers */);
    return val;
  }
  
  uint16_t get16(uint16_t index) {
    uint16_t val;
    asm volatile ("mov %%es:(%1), %%cx"
		  : "=c"(val)
		  : "bSD"(index)
		  : /* no clobbers */);
    return val;
  }

  uint32_t get32(uint16_t index) {
    uint32_t val;
    asm volatile ("mov %%es:(%1), %%ecx"
		  : "=c"(val)
		  : "bSD"(index)
		  : /* no clobbers */);
    return val;
  }
  
  void set8(uint16_t index, uint8_t val) {
    asm volatile ("mov %0, %%es:(%1)"
		  : /* no outputs */
		  : "q"(val), "bSD"(index)
		  : /* no clobbers */);

  }
  
  void set16(uint16_t index, uint16_t val) {
     asm volatile ("mov %%cx, %%es:(%%bx)"
		  : /* no outputs */
		  : "b"(index), "c"(val)
		  : /* no clobbers */);
 };
  
protected:
  const uint16_t base; uint16_t old;
};

inline const uint16_t _segment(uint32_t farp) {
  return farp >> 16;
}

inline const uint32_t _offset(uint32_t farp) {
  return farp & 0xffff;
}

struct unrealptr {
  unrealptr(uint32_t linear)
    : offset(linear) {
    enable();
  }

  unrealptr(uint32_t farp, bool)
    : offset((_segment(farp) << 4) + _offset(farp) ) {
    enable();
  }
  
  void enable() {
    struct __attribute__ ((packed)) {
      uint16_t limit : 16;
      uint16_t base : 16;
      uint8_t base2 : 8;
      uint8_t access : 8;
      uint8_t limit2 : 4;
      uint8_t flags : 4;
      uint8_t baseHi : 8;
    } gdt[] = {
      // 0x00 NULL descriptor
      {},
      // 0x08 code
      {.limit=0xffff,.base=0,.base2=0,.access=0x9a,.limit2=0xf,.flags=0xc,.baseHi=0},
      // 0x10 data
      {.limit=0xffff,.base=0,.base2=0,.access=0x92,.limit2=0xf,.flags=0xc,.baseHi=0},
    };
    
    struct __attribute__ ((packed)) {
      uint16_t limit;
      void* base;
    } gdtinfo = {
      sizeof(gdt) - 1,
      &gdt,
    };
    
    // linear base address
    char* ds;
    asm volatile("mov %%ds, %%ax\n"
		 : "=a"(ds)
	       : /* no inputs */
	       : /* no other clobbers */);
    
    gdtinfo.base = (void*)((int)ds << 4) + (int)(gdt);
    
    // TODO: A20 gate, some legacy systems may have it disabled!
    
    // setup GDT, jump to protected mode, and back
    asm volatile ("cli\n"
		  "lgdt %0\n"
		  //"mov %%cr0, %%eax\n" // copy
		  "mov $1, %%eax\n" // set pmode bit
		  "mov %%eax, %%cr0\n"
		  "jmp .+2\n" // some 3/486 need to clear the prefetch queue!
		  "mov $0x10, %%ebx\n" // 2nd, d    struct __attribute__ ((packed)) {
		  "mov %%ebx, %%gs\n" // load a pmode descriptor into the %g-segment
		  "and $0xfe, %%al\n" // disable pmode, back to real mode :-/
		  "mov %%eax, %%cr0\n"
		  "jmp .+2\n" // better flush prefetch queue, again
		  "sti\n"
		  : /* outputs */
		  : "rm"(gdtinfo) /* inputs */
		  : "%eax", "%ebx");
  }
  
  uint8_t get8(uint32_t index) {
    index += offset;

    uint8_t val;
    asm volatile ("mov %%gs:(%1), %0"
		  : "=q"(val)
		  : "bSD"(index)
		  : /* no clobbers */);
    return val;
  }
  
  uint16_t get16(uint32_t index) {
    index += offset;
    uint16_t val;
    asm volatile ("mov %%gs:(%%ebx), %%cx"
		: "=c"(val)
		: "ebx"(index)
		  : /* no clobbers */);
    return val;
  }

  uint32_t get32(uint32_t index) {
    index += offset;
    uint32_t val;
    asm volatile ("mov %%gs:(%%ebx), %%ecx"
		: "=c"(val)
		: "ebx"(index)
		  : /* no clobbers */);
    return val;
  }
  
  void set8(uint32_t index, uint8_t val) {
    index += offset;
    asm volatile ("mov %1, %%gs:(%0)"
		  : /* no outputs */
		  : "bSD"(index), "q"(val)
		  : /* no clobbers */);

  }
  
  void set16(uint32_t index, uint16_t val) {
    index += offset;
     asm volatile ( "mov %%cx, %%gs:(%0)"
		    : /* no outputs */
		    : "bSD"(index), "c"(val)
		    : /* no clobbers */);
 };

protected:
  const uint32_t offset;
};
