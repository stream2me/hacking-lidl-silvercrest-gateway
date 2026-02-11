# Top-level Makefile -- RTL8196E bootloader
#
# Two-stage build: first compiles the stage-2 bootloader (boot/) into a
# raw binary (boot.img), then wraps it with the stage-1 LZMA loader
# (btcode/) to produce the final flash image.
#
# Three variants are built by default:
#   - noreboot: boot code TFTP flash does NOT auto-reboot (safe default)
#   - reboot:   boot code TFTP flash auto-reboots after completion
#   - ramtest:  RAM-test image with read-back verification of BSS clears
#
# boot/ must be cleaned between variants because the Makefiles do not
# track CFLAGS changes.
#
# Outputs:
#   boot_noreboot.bin     - flash image, no reboot after boot-code TFTP
#   boot_reboot.bin       - flash image, auto-reboot after boot-code TFTP
#   btcode/build/test.bin - RAM-loadable image for RAM testing
#
# Targets:
#   all   - build all three variants (default)
#   clean - remove all build outputs
#
# Variables:
#   JUMP_ADDR  - kernel entry point passed to stage-2 (default 0x80500000)
#   XTOOLS_DIR - path to the Lexra cross-toolchain sysroot
#   CROSS      - toolchain prefix (default mips-lexra-linux-musl-)

JUMP_ADDR    ?= 0x80500000
XTOOLS_DIR   ?= $(HOME)/hacking-lidl-silvercrest-gateway/x-tools/mips-lexra-linux-musl
CROSS        ?= mips-lexra-linux-musl-
PATH         := $(XTOOLS_DIR)/bin:$(PATH)

export PATH CROSS

.PHONY: all clean

all:
	# --- noreboot variant ---
	$(MAKE) -C boot clean
	$(MAKE) -C boot boot JUMP_ADDR=$(JUMP_ADDR)
	$(MAKE) -C btcode clean
	$(MAKE) -C btcode
	cp -f btcode/build/boot.bin boot_noreboot.bin
	# --- reboot variant (only boot/ changes; btcode sees new boot.out) ---
	$(MAKE) -C boot clean
	$(MAKE) -C boot boot JUMP_ADDR=$(JUMP_ADDR) BOOT_REBOOT=1
	$(MAKE) -C btcode
	cp -f btcode/build/boot.bin boot_reboot.bin
	# --- ramtest variant (btcode CFLAGS change -> clean btcode) ---
	$(MAKE) -C boot clean
	$(MAKE) -C boot boot JUMP_ADDR=$(JUMP_ADDR) RAMTEST_TRACE=1
	$(MAKE) -C btcode clean
	$(MAKE) -C btcode RAMTEST_TRACE=1

clean:
	$(MAKE) -C boot clean
	$(MAKE) -C btcode clean
	rm -f boot_noreboot.bin boot_reboot.bin
