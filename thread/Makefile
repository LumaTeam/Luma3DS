ifeq ($(strip $(DEVKITARM)),)
$(error "Please set DEVKITARM in your environment. export DEVKITARM=<path to>devkitARM")
endif

include $(DEVKITARM)/ds_rules

SFLAGS=-c -mcpu=arm946e-s -march=armv5te -mlittle-endian -fshort-wchar
CFLAGS=$(SFLAGS) -std=c99

all:
	$(CC) -g arm9/thread.c arm9/lib.c $(CFLAGS)
	$(CC) -g arm9/_start.s arm9/FS.s -I arm9 $(SFLAGS)
	$(CC) -nostdlib -T 3ds.ld _start.o thread.o lib.o FS.o
	$(OBJCOPY) -O binary a.out arm9.bin
	cp arm11/arm11.bin ./arm11.bin
	rm -f *.o *.out
	