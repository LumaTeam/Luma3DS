SUBFOLDERS	:=	loader sm pxi rosalina
CXIS		:=	$(foreach dir, $(SUBFOLDERS), $(dir)/$(dir).cxi)

.PHONY:	all clean $(SUBFOLDERS)

all:	sysmodules.bin

clean:
	@$(foreach dir, $(SUBFOLDERS), make -C $(dir) clean &&) true
	@rm -rf sysmodules.bin

sysmodules.bin:	$(SUBFOLDERS)
	@cat $(CXIS) > $@
	@echo built... $(notdir $@)

$(SUBFOLDERS):
	@$(MAKE) -C $@ all
