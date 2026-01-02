# RTL8196E bootloader second stage (btcode) Makefile
#
# Copyright (C) Realtek Semiconductor Corp.
# Simplified for RTL8196E: J. Nilo - November 2025

CROSS      ?= mips-lexra-linux-musl-
CC          = $(CROSS)gcc
LD          = $(CROSS)ld
OBJCOPY     = $(CROSS)objcopy
OBJDUMP     = $(CROSS)objdump
NM          = $(CROSS)nm

# External tools (provided by build_bootloader.sh or use defaults)
LZMA_TOOL  ?= lzma
CVIMG_TOOL ?= cvimg

# Compiler flags - detect Lexra toolchain
ifeq ($(findstring lexra,$(CROSS)),)
MARCH_FLAG  = -march=4181
else
MARCH_FLAG  = -march=lx4380
endif

CFLAGS      = $(MARCH_FLAG) -Os -g -fomit-frame-pointer -fno-pic -mno-abicalls \
              -D__KERNEL__ -DLZMA_COMPRESS -D_LZMA_PROB32 -DRTL8196=1
INCLUDES    = -I../boot/include

.PHONY: all clean

all: boot.bin rescue.bin

# Compile sources
piggy.o: piggy.S
	$(CC) $(CFLAGS) $(INCLUDES) -O2 -c $< -o $@

bootload.o: bootload.c
	$(CC) $(CFLAGS) -nostdinc -c $< -o $@

LzmaDecode.o: LzmaDecode.c
	$(CC) $(CFLAGS) $(INCLUDES) -fno-inline -O2 -c $< -o $@

start.o: start.S
	$(CC) $(CFLAGS) $(INCLUDES) -O2 -c $< -o $@

# Compress boot.img and create piggy (LZMA-compressed payload)
boot.img.gz: boot.img
	$(LZMA_TOOL) e $< $@

piggy.elf: piggy.o bootload.o LzmaDecode.o boot.img.gz
	$(OBJCOPY) --add-section .initrd=boot.img.gz piggy.o
	$(LD) -X -nostdlib -Tpiggy.script --static piggy.o bootload.o LzmaDecode.o -o $@

piggy.bin: piggy.elf
	$(OBJCOPY) -Obinary $< $@
	$(OBJDUMP) -h -S $< > piggy.elf.txt

# Final boot image with start code wrapper
boot.elf: start.o piggy.bin
	$(OBJCOPY) --add-section .initrd=piggy.bin start.o
	$(LD) -X -nostdlib -Tld.script start.o -o $@

boot: boot.elf
	$(OBJCOPY) -Obinary $< $@
	$(OBJDUMP) -h -S $< > boot.elf.txt
	$(NM) $< | sort > system.map
	cp -f $@ ../boot/Output/

# Create final images with headers
# Note: For boot signature, cvimg requires both addresses to be 0
boot.bin: boot
	$(CVIMG_TOOL) -i $< -o $@ -s boot -e 0 -b 0
	cp -f $@ ../boot/Output/

rescue.bin: piggy.bin
	$(CVIMG_TOOL) -i $< -o $@ -s boot -e 0 -b 0

clean:
	rm -f *.o *.elf *.bin *.txt *.gz boot system.map
