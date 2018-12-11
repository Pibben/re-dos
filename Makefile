X_OUTARCH := ./objdir
X_SYSTEM = DOS
X_ARCH = i386

# objdump -D -b binary -m i8086 --adjust-vma=0x100 objdir/hello.com
# dosbox -c "c:\\dos\\objdir\\hello.com"

include build/top.make

# from the linux-kernel build system:
cc-option = $(shell if $(CC) $(1) -S -o /dev/null -xc /dev/null \
            > /dev/null 2>&1; then echo "$(1)"; else echo "$(2)"; fi ;)

CFLAGS += -I libc -s -Os -m32 -march=i386 -Wall -Wno-unused-function -fomit-frame-pointer # -Wno-format-security
CXXFLAGS = $(CFLAGS) -fno-exceptions #-I stl/include
CXXFLAGS += $(call cc-option,-fno-threadsafe-statics,)

NOT_SRCS += sound.cc sound2.cc # requires FM.{h,c}

# build each .cc file as executable - if this is not desired
# in the future a list must be supplied manually ...

BINARY = $(basename $(filter-out $(NOT_SRCS), $(notdir $(wildcard $(X_MODULE)/*.cc $(X_MODULE)/*.c))) $(SRCS))
BINARY_EXT = $(X_EXEEXT)

#CPPFLAGS += -I utility
#LDFLAGS += -lGL  -L/usr/X11/lib64

include build/bottom.make
