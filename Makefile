ifneq ($(strip $(shell firmtool -v 2>&1 | grep usage)),)
$(error "Please install firmtool v1.1 or greater")
endif

NAME		:=	$(notdir $(CURDIR))
REVISION	:=	$(shell git describe --tags --match v[0-9]* --abbrev=8 | sed 's/-[0-9]*-g/-/')

SUBFOLDERS	:=	sysmodules arm11 arm9 k11_extension

.PHONY:	all release clean $(SUBFOLDERS)

all:		boot.firm

release:	$(NAME)$(REVISION).zip

clean:
	@$(foreach dir, $(SUBFOLDERS), $(MAKE) -C $(dir) clean &&) true
	@rm -rf *.firm *.zip

$(NAME)$(REVISION).zip:	boot.firm exception_dump_parser
	@zip -r $@ $^ -x "*.DS_Store*" "*__MACOSX*"

boot.firm:	$(SUBFOLDERS)
	@firmtool build $@ -D sysmodules/sysmodules.bin arm11/arm11.elf arm9/arm9.elf k11_extension/k11_extension.elf \
	-A 0x18180000 0x18000000 -C XDMA XDMA NDMA XDMA
	@echo built... $(notdir $@)

$(SUBFOLDERS):
	@$(MAKE) -C $@ all
