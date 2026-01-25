# RTL8196E bootloader (boot stage) Makefile
#
# Copyright (C) Realtek Semiconductor Corp.
# Simplified for RTL8196E: J. Nilo - November 2025

CROSS   ?= mips-lexra-linux-musl-
OUTDIR   = ./Output

CC       = $(CROSS)gcc
LD       = $(CROSS)ld
OBJCOPY  = $(CROSS)objcopy
OBJDUMP  = $(CROSS)objdump
NM       = $(CROSS)nm

CFLAGS   = -g -O -G 0 \
           -fomit-frame-pointer -fno-pic -mno-abicalls -fgnu89-inline \
           -I. -I./include \
           -D__KERNEL__ -Dlinux -DRTL865X=1

ifdef JUMP_ADDR
CFLAGS  += -DJUMP_ADDR=$(JUMP_ADDR)
endif

ASFLAGS  = -D__ASSEMBLY__ -x assembler-with-cpp -G 0
LDFLAGS  = -nostdlib -T./ld.script -EB --static

# Assembly sources
ASM_SRCS = \
	arch/mips/kernel/head.S \
	init/inthandler.S

# C sources
C_SRCS = \
	init/irq.c \
	init/eth_tftpd.c \
	init/ethInt_865x.c \
	init/main.c \
	init/calloc.c \
	init/utility.c \
	rtl8196x/swCore.c \
	rtl8196x/swNic_poll.c \
	rtl8196x/swTable.c \
	rtl8196x/vlanTable.c \
	flash/spi_flash.c \
	flash/spi_common.c \
	arch/mips/kernel/setup.c \
	arch/mips/kernel/traps.c \
	io/init.c \
	io/string.c \
	io/strtol.c \
	io/strtoul.c \
	io/ctool.c \
	io/misc.c \
	monitor/monitor.c \
	monitor/power_on_led.c

# Object files
ASM_OBJS = $(addprefix $(OUTDIR)/,$(notdir $(ASM_SRCS:.S=.o)))
C_OBJS   = $(addprefix $(OUTDIR)/,$(notdir $(C_SRCS:.c=.o)))
OBJS     = $(ASM_OBJS) $(C_OBJS)

# Default target
all:
	@echo "Usage: make boot"

# Create output directory and generate build timestamp
.PHONY: banner
banner:
	@mkdir -p $(OUTDIR)
	@echo '#define BOOT_CODE_TIME "'`date "+%Y.%m.%d-%H:%M%z"`'"' > ./banner/mk_time

# Main boot target
boot: banner $(OUTDIR)/boot.out
	$(OBJCOPY) -g -Obinary $(OUTDIR)/boot.out $(OUTDIR)/boot.img
	$(NM) $(OUTDIR)/boot.out | sort > $(OUTDIR)/boot.nm
	$(OBJDUMP) -h -S $(OUTDIR)/boot.out > $(OUTDIR)/boot.text
	cp -f $(OUTDIR)/boot.img ../btcode/boot.img

# Link
$(OUTDIR)/boot.out: $(OBJS)
	$(LD) $(LDFLAGS) $(OBJS) -o $@

# Pattern rules for assembly
$(OUTDIR)/head.o: arch/mips/kernel/head.S
	$(CC) -c $(CFLAGS) $(ASFLAGS) $< -o $@

$(OUTDIR)/inthandler.o: init/inthandler.S
	$(CC) -c $(CFLAGS) $(ASFLAGS) $< -o $@

# Pattern rules for C files
$(OUTDIR)/%.o: init/%.c
	$(CC) -c $(CFLAGS) $< -o $@

$(OUTDIR)/%.o: io/%.c
	$(CC) -c $(CFLAGS) $< -o $@

$(OUTDIR)/%.o: flash/%.c
	$(CC) -c $(CFLAGS) $< -o $@

$(OUTDIR)/%.o: rtl8196x/%.c
	$(CC) -c $(CFLAGS) $< -o $@

$(OUTDIR)/%.o: monitor/%.c
	$(CC) -c $(CFLAGS) $< -o $@

$(OUTDIR)/%.o: arch/mips/kernel/%.c
	$(CC) -c $(CFLAGS) $< -o $@

# Cleanup
clean:
	rm -f $(OUTDIR)/*.o $(OUTDIR)/boot.out $(OUTDIR)/boot.img
	rm -f $(OUTDIR)/boot.nm $(OUTDIR)/boot.text $(OUTDIR)/boot

.PHONY: all boot clean banner
