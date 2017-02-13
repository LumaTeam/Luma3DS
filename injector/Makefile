rwildcard = $(foreach d, $(wildcard $1*), $(filter $(subst *, %, $2), $d) $(call rwildcard, $d/, $2))

ifeq ($(strip $(DEVKITARM)),)
$(error "Please set DEVKITARM in your environment. export DEVKITARM=<path to>devkitARM")
endif

include $(DEVKITARM)/3ds_rules

name := $(shell basename $(CURDIR))

dir_source := source
dir_patches := patches
dir_build := build
dir_out := ../$(dir_build)

LIBS := -lctru
LIBDIRS	:= $(CTRULIB)
LIBPATHS := $(foreach dir,$(LIBDIRS),-L$(dir)/lib)

INCLUDE	:= $(foreach dir,$(LIBDIRS),-I$(dir)/include)

ASFLAGS := -mcpu=mpcore -mfloat-abi=hard
CFLAGS := -Wall -Wextra $(ASFLAGS) -fno-builtin -std=c11 -O2 -flto -ffast-math $(INCLUDE) -DARM11 -D_3DS
LDFLAGS := -specs=3dsx.specs $(ASFLAGS) -Wl,--section-start,.text=0x14000000

objects = $(patsubst $(dir_source)/%.c, $(dir_build)/%.o, \
          $(call rwildcard, $(dir_source), *.s *.c))

bundled = $(dir_build)/romfsredir.bin.o

define bin2o
	bin2s $< | $(AS) -o $(@)
	echo "extern const u8" `(echo $(<F) | sed -e 's/^\([0-9]\)/_\1/' | tr . _)`"[];" >> $(dir_build)/bundled.h
	echo "extern const u32" `(echo $(<F) | sed -e 's/^\([0-9]\)/_\1/' | tr . _)`_size";" >> $(dir_build)/bundled.h
endef

.PHONY: all
all: $(dir_out)/$(name).bin

.PHONY: clean
clean:
	@rm -rf $(dir_build)

.PRECIOUS: $(dir_build)/%.bin

$(dir_build):
	@mkdir -p "$@"

$(dir_out)/$(name).bin: $(dir_build)/$(name).elf
	@makerom -f ncch -rsf loader.rsf -nocodepadding -o $@ -elf $<

$(dir_build)/$(name).elf: $(bundled) $(objects)
	$(LINK.o) $(OUTPUT_OPTION) $^ $(LIBPATHS) $(LIBS)

$(dir_build)/%.bin.o: $(dir_build)/%.bin
	@$(bin2o)

$(dir_build)/%.bin: $(dir_patches)/%.s $(dir_build)
	@armips $<

$(dir_build)/memory.o $(dir_build)/strings.o: CFLAGS += -O3

$(dir_build)/%.o: $(dir_source)/%.c
	@mkdir -p "$(@D)"
	$(COMPILE.c) $(OUTPUT_OPTION) $<

$(dir_build)/%.o: $(dir_source)/%.s
	@mkdir -p "$(@D)"
	$(COMPILE.s) $(OUTPUT_OPTION) $<
