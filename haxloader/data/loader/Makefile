LD  := arm-none-eabi-ld
AS  := arm-none-eabi-as
OC  := arm-none-eabi-objcopy
XXD := xxd

all: loader.h

loader.o: loader.s
	$(AS) -o $@ $<

loader.elf: loader.o
	$(LD) -T stub.ld -o $@ $<

loader.bin: loader.elf
	$(OC) -O binary $< $@

loader.h: loader.bin
	$(XXD) -i $< > $@
	@rm -f *.elf *.o
	@echo built loader

.PHONY: clean
clean:
	@rm -f *.h *.bin *.elf *.o
	@echo cleaned loader
