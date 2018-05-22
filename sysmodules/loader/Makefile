rwildcard = $(foreach d, $(wildcard $1*), $(filter $(subst *, %, $2), $d) $(call rwildcard, $d/, $2))

ifeq ($(strip $(DEVKITARM)),)
$(error "Please set DEVKITARM in your environment. export DEVKITARM=<path to>devkitARM")
endif

include $(DEVKITARM)/3ds_rules

name := $(shell basename $(CURDIR))

dir_source := source
dir_build := build
dir_out := ../../$(dir_build)

LIBS := -lctru
LIBDIRS	:= $(CTRULIB)
LIBPATHS := $(foreach dir,$(LIBDIRS),-L$(dir)/lib)

INCLUDE	:= $(foreach dir,$(LIBDIRS),-I$(dir)/include)

ASFLAGS := -mcpu=mpcore -mfloat-abi=hard
CFLAGS := -Wall -Wextra $(ASFLAGS) -fno-builtin -std=c11 -O2 -ffast-math $(INCLUDE) -DARM11 -D_3DS
LDFLAGS := -specs=3dsx.specs $(ASFLAGS) -Wl,--section-start,.text=0x14000000

objects = $(patsubst $(dir_source)/%.c, $(dir_build)/%.o, \
          $(call rwildcard, $(dir_source), *.s *.c))

.PHONY: all
all: $(dir_out)/$(name).cxi

.PHONY: clean
clean:
	@rm -rf $(dir_build)

.PRECIOUS: $(dir_build)/%.bin

$(dir_out)/$(name).cxi: $(dir_build)/$(name).elf
	@makerom -f ncch -rsf loader.rsf -nocodepadding -o $@ -elf $<

$(dir_build)/$(name).elf: $(bundled) $(objects)
	$(LINK.o) $(OUTPUT_OPTION) $^ $(LIBPATHS) $(LIBS)

$(dir_build)/memory.o $(dir_build)/strings.o: CFLAGS += -O3

$(dir_build)/%.o: $(dir_source)/%.c
	@mkdir -p "$(@D)"
	$(COMPILE.c) $(OUTPUT_OPTION) $<

$(dir_build)/%.o: $(dir_source)/%.s
	@mkdir -p "$(@D)"
	$(COMPILE.s) $(OUTPUT_OPTION) $<
