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

#ifndef ARRAY_SIZE
// TODO: maybe more somewhere nicer?
#define ARRAY_SIZE(x) (sizeof(x) / sizeof(*x))
#endif

static char* psp;

#ifndef __cplusplus
typedef enum {
  false = 0,
  true = 1,
} bool;
#endif

// has to be placed 1st in .TEXT by linker script
// "naked" attribute new since gcc-7, do not supportd C in clang
#ifndef DOS_NOSTART

#ifdef DOS_WANT_ARGS
#ifdef __cplusplus
extern "C"
#endif
int main(int, const char**);

static int __attribute__((noinline)) _main() {
  uint8_t argc = 0; // 1st count args, and tokenize
  // we rely on the first being a space, also term'ed w/ 0xd
  int8_t i, j, len = psp[0x80];
  
  for (i = 0; i < len; ++i)
    if (psp[0x81 + i] == ' ') {
      psp[0x81 + i] = 0; // zero terminate
      ++argc;
    }
  psp[0x81 + i] = 0; // zero terminate, overwrite last 0xd :-/!
  
  {
    const char* argv[argc] = {}; // 2nd allocate and assign
    // iterate again, we had to determine the argc stack size first
    for (i = 0, j = 0; i < len && j < argc; ++i) {
      if (psp[0x81 + i] == 0) {
	argv[j] = psp + 0x81 + 1 + i;
	++j;
      }
    }
    
    return main(argc, argv);
  }
}
#else
#ifdef __cplusplus
extern "C"
#endif
 int main();
#endif

void __attribute__((noreturn)) exit(int ret) {
  asm volatile ("mov $0x4c, %%ah\n"
		"int $0x21\n"
		: /* no outputs */
		: "a"(ret)
		: /* no clobbers */);
}

static void __attribute__((section(".start"), naked, used)) start() {
#ifdef DOS_WANT_ARGS
  // Program Segment Prefix in DS:BX
  asm volatile (""
		: "=b"(psp)/* ouptut */
		: /* no inputs */
		: /* no clobbers */);
  uint8_t ret = _main();
#else
  uint8_t ret = main();
#endif
  exit(ret);
}
#endif 

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

static void bios_print(const char* string)
{
  while (*string) {
    asm volatile ("mov $0x0e, %%ah\n" // bios tty display char
		  "int $0x10\n"
		  : /* no output */
		  : "a"(*string++)
		  : /* no other clobbers? */);
  }
}

static void dos_print(char* string)
{
  asm volatile ("mov $0x09, %%ah\n" // print string
		"int $0x21\n"
		: /* no output */
		: "d"(string)
		: "ah");
}

static int16_t dos_errno;

#define O_RDONLY 0
#define O_WRONLY 1
#define O_RDWR 2

static int open(const char* file, int flags, int mode)
{
  dos_errno = 0;
  int handle = 0;
  asm volatile ("mov $0x3d, %%ah\n" // open file
		"int $0x21\n"
		"jnc 1f\n"
		"mov %%ax, %[dos_errno]\n"
		"1:\n"
		: "=a"(handle), [dos_errno]"=rm"(dos_errno)
		: "a"(mode), "d"(file)
		: /* clobbers */);
  return dos_errno ? 0 : handle;
}

static int close(int fd)
{
  dos_errno = 0;
  asm volatile ("mov $0x3e, %%ah\n" // read from file
		"int $0x21\n"
		"jnc 1f\n"
		"mov %%ax, %[dos_errno]\n"
		"1:\n"
		: [dos_errno]"=rm"(dos_errno)
		: "b"(fd)
		: "ah");
  
  return dos_errno;
}

static int read(int fd, void* buf, int len)
{
  dos_errno = 0;
  uint16_t ret;
  asm volatile ("int3\n"
		"mov $0x3f, %%ah\n" // read from file
		"int $0x21\n"
		"jnc 1f\n"
		"mov %%ax, %[dos_errno]\n"
		"1:\n"
		: "=a"(ret), [dos_errno]"=rm"(dos_errno)
		: "d"(buf), "c"(len), "b"(fd)
		: /* clobbers */);
  return dos_errno ? 0 : ret;
}

static int write(int fd, const void* buf, int len)
{
  dos_errno = 0;
  uint16_t ret;
  asm volatile ("mov $0x40, %%ah\n" // write to file
		"int $0x21\n"
		"jnc 1f\n"
		"mov %%ax, %[dos_errno]\n"
		"1:\n"
		: "=a"(ret), [dos_errno]"=rm"(dos_errno)
		: "d"(buf), "c"(len), "b"(fd)
		: /* clobbers */);
  return dos_errno ? 0 : ret;
}


static bool kbhit()
{
    bool result;
    asm volatile ("mov $1, %%ah\n" // state of keyboard buffer
                  "int $0x16\n"
                  "jnz 1f\n"
                  "mov $0, %0\n"
                  "jmp 2f\n"
                  "1:\n"
                  "mov $1, %0\n"
                  "2:\n"
                  : "=rm"(result));
    return result;
}

static uint8_t getch(const bool scancode
#ifdef __cplusplus
		     = false
#endif
		     )
{
  uint16_t key = 0;
  if (kbhit()) {
    asm volatile ("mov $0, %%ah\n" // read key press
                  "int $0x16\n"
                  : "=a"(key));
  }
  return scancode ? key >> 8 : key & 0xff;
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
		"pop %%ds\n"
		: /* no output */
		: "a"(intr), "d"(isr)
		: "bx");  
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

static void* dos_malloc(uint32_t size)
{
  uint16_t addr = 0;
  dos_errno = 0;
  asm volatile ("mov $0x48, %%ah\n" // alloc
		"int $0x21\n"
		"jnc 1f\n"
		"mov %%ax, %[dos_errno]\n"
		"1:\n"
		: "=a"(addr) , [dos_errno]"=rm"(dos_errno)
		: "b"(size / 16)
		: /* clobbers*/);
  return dos_errno ? (void*)0 : (void*)(addr * 16); // linear
}

static void dos_free(void* addr)
{
  dos_errno = 0;
  asm volatile ("push %%es\n"
		"mov %%ax, %%es\n"
		"mov $0x49, %%ah\n" // free
		"int $0x21\n"
		"pop %%es\n"
		"jnc 1f\n"
		"mov %%ax, %[dos_errno]\n"
		"1:\n"
		: [dos_errno]"=rm"(dos_errno)
		: "a"((uint32_t)addr / 16)
		: /* clobbers */);
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
  char c[1] = {ch};
  write(1, c, sizeof(c));
}

inline const uint16_t _segment(uint32_t farp) {
  return farp >> 16;
}

inline const uint32_t _offset(uint32_t farp) {
  return farp & 0xffff;
}


#ifdef __cplusplus

struct farptr {
  farptr(uint32_t base)
    : base(base / 16) {
    enable();
  }
  
  void enable() {
    asm volatile ("push %%es\n"
		  "mov %%dx, %%es\n"
		  "pop %%dx\n"
		  : "=d"(old) /* no outputs */
		  : "d"(base)
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
  }
  
  void set32(uint16_t index, uint16_t val) {
     asm volatile ("mov %%ecx, %%es:(%%bx)"
		  : /* no outputs */
		  : "b"(index), "c"(val)
		  : /* no clobbers */);
  }
  
protected:
  const uint16_t base; uint16_t old;
};

struct unrealptr {
  unrealptr(uint32_t linear)
    : offset(linear) {
    enable();
  }

  unrealptr(uint32_t farp, bool)
    : offset((_segment(farp) << 4) + _offset(farp) ) {
    enable();
  }
  
  void rebase(uint32_t linear) {
    offset = linear;
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
    
    gdtinfo.base = (void*)(((int)ds << 4) + (int)(gdt));
    
    // setup GDT, jump to protected mode, and back
    asm volatile ("cli\n"
		  "lgdt %0\n"
		  //"mov %%cr0, %%eax\n" // copy
		  "mov $1, %%eax\n" // set pmode bit
		  "mov %%eax, %%cr0\n"
		  "jmp .+2\n" // some 3/486 need to clear the prefetch queue!
		  "mov $0x10, %%ebx\n" // 2nd descriptor, 1st is NULL
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
    asm volatile ("mov %%gs:(%1), %0"
		  : "=q"(val)
		  : "bSD"(index)
		  : /* no clobbers */);
    return val;
  }

  uint32_t get32(uint32_t index) {
    index += offset;
    uint32_t val;
    asm volatile ("mov %%gs:(%1), %0"
		  : "=q"(val)
		  : "bSD"(index)
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
    asm volatile ("mov %1, %%gs:(%0)"
		  : /* no outputs */
		  : "bSD"(index), "q"(val)
		  : /* no clobbers */);
  }
  
  void set32(uint32_t index, uint32_t val) {
    index += offset;
    asm volatile ("mov %1, %%gs:(%0)"
		  : /* no outputs */
		  : "bSD"(index), "q"(val)
		  : /* no clobbers */);
  }

protected:
  uint32_t offset;
};

const char* getenv(const char* name)
{
  // environment segment from PSP
  uint32_t envseg = *(uint16_t*)(psp + 0x2c);
  
  // farptr abstraction
  farptr environ(envseg * 16);
  uint16_t i = 0, beg, end;
  do {
    // parse each zero terminated line
    beg = i;
    char v;
    do {
      v = environ.get8(i++);
    } while (v && i < 400);
    end = i - 1;
    
    // compare variable name
    uint16_t j;
    for (j = 0; j <= end - beg; ++j) {
      v = environ.get8(beg + j);
      if (name[j] != v)
	break;
    }
    
    if (v == '=' && name[j] == 0) {
      //printf("%d %d %d\n", beg, end, j);
      // we need a "local" near pointer copy :-/
      static char buffer[16];
      char* b = buffer;
      // TODO: check buffer size overflow!
      for (++j; j <= end - beg; ++j) {
	*b++ = environ.get8(beg + j);
      }
      return buffer;
    }
  } while (beg != end);
  
  return 0;
}

// A20 gate, BIOS / DOS systems may come up disabled

bool a20_test() {
  const uint16_t test_addr = 0x80 * 4;
  const uint16_t test_addr2 = test_addr + 0x10;
  farptr p1(0x00000);
  const uint32_t vorig = p1.get32(test_addr);
  farptr p2(0xffff0); // last wrapping segment
  
  // test A20 gate - using farptr, and some high INT vector!
  
  // p2 should be last enabled
  uint32_t v1, v2;
  /*p2.enable();*/ v1 = ~p2.get32(test_addr2);
  p1.enable(); p1.set32(test_addr, v1);
  p2.enable(); v2 = p2.get32(test_addr2);
  p1.enable(); p1.set32(test_addr, vorig); // restore just in case
  //printf("%x %x\n", v1, v2);
  
  return v1 != v2;
}

static void kbd_wait_empty() {
  uint8_t status;
  while(status = inb(0x64) & 2) {
    //printf("kbd: %x", status);// wait buffer empty
  }
}

static bool a20_enable() {
  if (a20_test()) return true;
  
  // 1st try bios
  uint16_t status = 0x8600;
  asm volatile ("mov $0x2401, %%ax\n" // A20 activate
		"int $0x15\n"
		: "=a"(status)
		:
		:);
  if (a20_test()) return true;
  
  // 2nd try fast, works in dosbox
  uint8_t v = inb(0x92); // PS/2
  outb(0x92, (v | 2) & 0xfe); // not reset!
  
  if (a20_test()) return true;
  
  //v = inb(0xee); // some alt systems?
  
  // 3rd try keyboard - works in pcem/430fx
  asm volatile("cli\n");
  kbd_wait_empty();
  outb(0x64, 0xd1); // kbd ctrl command
  kbd_wait_empty();
  outb(0x60, 0xdf); // a20 on
  kbd_wait_empty();
  outb(0x60, 0xff); // null command for some UHCI emulation?
  kbd_wait_empty();
  asm volatile("sti\n");
  
  return a20_test();
}

#else

__seg_gs void* unrealptr(uint32_t base) {
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
  
  gdtinfo.base = (void*)(((int)ds << 4) + (int)(gdt));
  
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
  return (__seg_gs void*)(0xb8000);
}

#endif


// Warning: output is raw, real-mode seg:offset!
static int mouse_init()
{
  int ret, buttons;
  asm volatile ("mov $0x0, %%ax\n" // reset/init
		"int $0x33\n"
 		: "=a"(ret), "=b"(buttons)
		: /* no inputs */
		: /* no clobbers */);
  return ret ? buttons : ret;
}

// Warning: output is raw, real-mode seg:offset!
static void mouse_set_max(uint16_t w, uint16_t h)
{
  asm volatile ("mov $0x07, %%ax\n" // set horiz min/max
		"int $0x33\n"
 		: /* no outputs */
		: "c"(0), "d"(w)
		: /* no clobbers */);
  asm volatile ("mov $0x08, %%ax\n" // set vert min/max
		"int $0x33\n"
 		: /* no outputs */
		: "c"(0), "d"(h)
		: /* no clobbers */);
}


// Warning: output is raw, real-mode seg:offset!
static void (*mouse_swapvect(uint32_t* eventmask, void (*isr)()))()
{
  void (*ret)();
  asm volatile (//"int3\n"
		"push %%es\n" // save es
		"mov %%edx, %%ebx\n" // shuffle seg into ds
		"shr $16, %%ebx\n"
		"mov %%bx, %%es\n"
		"mov $0x14, %%ax\n" // swap mouse int vector ES:DX
		"int $0x33\n"
		"mov %%es, %%ax\n" // load far ptr pair
		"shl $16, %%eax\n"
		"add %%dx, %%ax\n"
		"pop %%es\n"
		: "=a"(ret), "=c"(*eventmask)
		: "d"(isr), "c"(*eventmask)
		: "bx");
  return ret;
}
