ifeq ($(strip $(DEVKITARM)),)
$(error "Please set DEVKITARM in your environment. export DEVKITARM=<path to>devkitARM")
endif

include $(DEVKITARM)/ds_rules

SFLAGS=-c -mcpu=arm946e-s -march=armv5te -mlittle-endian -fshort-wchar
CFLAGS=$(SFLAGS) -std=c99

all:
	$(CC) -g source/thread.c source/lib.c $(CFLAGS)
	$(CC) -g source/_start.s source/FS.S -I source $(SFLAGS)
	$(CC) -nostdlib -T 3ds.ld _start.o thread.o lib.o FS.o
	$(OBJCOPY) -O binary a.out arm9.bin
	rm -f *.o *.out
	