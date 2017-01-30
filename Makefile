rwildcard = $(foreach d, $(wildcard $1*), $(filter $(subst *, %, $2), $d) $(call rwildcard, $d/, $2))

ifeq ($(strip $(DEVKITARM)),)
$(error "Please set DEVKITARM in your environment. export DEVKITARM=<path to>devkitARM")
endif

include $(DEVKITARM)/base_tools

name := Luma3DS
revision := $(shell git describe --tags --match v[0-9]* --abbrev=8 | sed 's/-[0-9]*-g/-/i')
commit := $(shell git rev-parse --short=8 HEAD)

dir_source := source
dir_patches := patches
dir_loader := loader
dir_injector := injector
dir_exceptions := exceptions
dir_arm9_exceptions := $(dir_exceptions)/arm9
dir_arm11_exceptions := $(dir_exceptions)/arm11
dir_haxloader := haxloader
dir_build := build
dir_out := out

ASFLAGS := -mcpu=arm946e-s
CFLAGS := -Wall -Wextra $(ASFLAGS) -fno-builtin -std=c11 -Wno-main -O2 -flto -ffast-math
ifeq ($(FONT),ORIG)
CFLAGS	+=	-DFONT_ORIGINAL
else ifeq ($(FONT),6X10)
CFLAGS	+=	-DFONT_6X10
else ifeq ($(FONT),ACORN)
CFLAGS	+=	-DFONT_ACORN
else ifeq ($(FONT),GB)
CFLAGS	+=	-DFONT_GB
else ifeq ($(FONT),LARGEL)
CFLAGS	+=	-DFONT_LL
else ifeq ($(FONT),LARGES)
CFLAGS	+=	-DFONT_LS
else ifeq ($(FONT),PEARL)
CFLAGS	+=	-DFONT_PEARL
else
CFLAGS	+=	-DFONT_6X10
endif
LDFLAGS := -nostartfiles

objects = $(patsubst $(dir_source)/%.s, $(dir_build)/%.o, \
          $(patsubst $(dir_source)/%.c, $(dir_build)/%.o, \
          $(call rwildcard, $(dir_source), *.s *.c)))

bundled = $(dir_build)/reboot.bin.o $(dir_build)/emunand.bin.o $(dir_build)/svcGetCFWInfo.bin.o $(dir_build)/k11modules.bin.o \
          $(dir_build)/injector.bin.o $(dir_build)/loader.bin.o $(dir_build)/arm9_exceptions.bin.o $(dir_build)/arm11_exceptions.bin.o

define bin2o
	bin2s $< | $(AS) -o $(@)
	echo "extern const u8" `(echo $(<F) | sed -e 's/^\([0-9]\)/_\1/' | tr . _)`"[];" >> $(dir_build)/bundled.h
	echo "extern const u32" `(echo $(<F) | sed -e 's/^\([0-9]\)/_\1/' | tr . _)`_size";" >> $(dir_build)/bundled.h
endef

.PHONY: all
all: a9lh 

.PHONY: release
release: $(dir_out)/$(name)$(revision).7z

.PHONY: a9lh
a9lh: $(dir_out)/arm9loaderhax.bin

.PHONY: haxloader
haxloader: a9lh
	@$(MAKE) name=$(name) -C $(dir_haxloader)

.PHONY: clean
clean:
	@$(MAKE) -C $(dir_haxloader) clean
	@$(MAKE) -C $(dir_loader) clean
	@$(MAKE) -C $(dir_arm9_exceptions) clean
	@$(MAKE) -C $(dir_arm11_exceptions) clean	
	@$(MAKE) -C $(dir_injector) clean
	@rm -rf $(dir_out) $(dir_build)

.PRECIOUS: $(dir_build)/%.bin

$(dir_out) $(dir_build):
	@mkdir -p "$@"

$(dir_out)/$(name)$(revision).7z: all
	@7z a -mx $@ ./$(@D)/* ./$(dir_exceptions)/exception_dump_parser.py

$(dir_out)/arm9loaderhax.bin: $(dir_build)/main.bin $(dir_out)
	@cp -a $(dir_build)/main.bin $@

$(dir_build)/main.bin: $(dir_build)/main.elf
	$(OBJCOPY) -S -O binary $< $@

$(dir_build)/main.elf: $(bundled) $(objects)
	$(LINK.o) -T linker.ld $(OUTPUT_OPTION) $^

$(dir_build)/%.bin.o: $(dir_build)/%.bin
	@$(bin2o)

$(dir_build)/injector.bin: $(dir_injector) $(dir_build)
	@$(MAKE) -C $<

$(dir_build)/loader.bin: $(dir_loader) $(dir_build)
	@$(MAKE) -C $<

$(dir_build)/arm9_exceptions.bin: $(dir_arm9_exceptions) $(dir_build)
	@$(MAKE) -C $<

$(dir_build)/arm11_exceptions.bin: $(dir_arm11_exceptions) $(dir_build)
	@$(MAKE) -C $<

$(dir_build)/%.bin: $(dir_patches)/%.s $(dir_build)
	@armips $<

$(dir_build)/memory.o $(dir_build)/strings.o: CFLAGS += -O3
$(dir_build)/config.o: CFLAGS += -DCONFIG_TITLE="\"$(name) $(revision) configuration\""
$(dir_build)/patches.o: CFLAGS += -DREVISION=\"$(revision)\" -DCOMMIT_HASH="0x$(commit)"

$(dir_build)/%.o: $(dir_source)/%.c $(bundled)
	@mkdir -p "$(@D)"
	$(COMPILE.c) $(OUTPUT_OPTION) $<

$(dir_build)/%.o: $(dir_source)/%.s
	@mkdir -p "$(@D)"
	$(COMPILE.s) $(OUTPUT_OPTION) $<
