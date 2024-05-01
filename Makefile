ifneq ($(strip $(shell firmtool -v 2>&1 | grep usage)),)
$(error "Please install firmtool v1.1 or greater")
endif

# Disable kext and firmlaunch patches, all custom sysmodules except Loader, enable PASLR.
# Dangerous. Don't enable this unless you know what you're doing!
export BUILD_FOR_EXPLOIT_DEV ?= 0

# Build with O0 & frame pointer information for use with GDB
export BUILD_FOR_GDB ?= 0

# Default 3DSX TitleID for hb:ldr
export HBLDR_DEFAULT_3DSX_TID ?= 000400000D921E00

# What to call the title corresponding to HBLDR_DEFAULT_3DSX_TID
export HBLDR_DEFAULT_3DSX_TITLE_NAME ?= "hblauncher_loader"

NAME		:=	$(notdir $(CURDIR))
REVISION	:=	$(shell git describe --tags --match v[0-9]* --abbrev=8 | sed 's/-[0-9]*-g/-/')

SUBFOLDERS	:=	sysmodules arm11 arm9 k11_extension

.PHONY:	all release clean $(SUBFOLDERS)

all:		boot.firm

release:	$(NAME)$(REVISION).zip

clean:
	@$(foreach dir, $(SUBFOLDERS), $(MAKE) -C $(dir) clean &&) true
	@rm -rf *.firm *.zip *.3dsx

# boot.3dsx comes from https://github.com/fincs/new-hbmenu/releases
$(NAME)$(REVISION).zip:	hbmenu.zip boot.firm
	@cp $< $@
	@zip $@ boot.firm -x "*.DS_Store*" "*__MACOSX*"

boot.firm:	$(SUBFOLDERS)
	@firmtool build $@ -D sysmodules/sysmodules.bin arm11/arm11.elf arm9/arm9.elf k11_extension/k11_extension.elf \
	-A 0x18180000 -C XDMA XDMA NDMA XDMA
	@echo built... $(notdir $@)

hbmenu.zip:
	@curl -sSfL $(shell curl -s https://api.github.com/repos/devkitPro/3ds-hbmenu/releases/latest | grep 'browser_' | cut -d\" -f4) -o $@
	@echo downloaded... $(notdir $@)

$(SUBFOLDERS):
	@$(MAKE) -C $@ all
