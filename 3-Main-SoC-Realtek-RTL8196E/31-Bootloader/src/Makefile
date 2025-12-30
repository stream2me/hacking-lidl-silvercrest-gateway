# RTL8196E bootloader top-level Makefile
#
# Copyright (C) Realtek Semiconductor Corp.
# Simplified for RTL8196E: J. Nilo - November 2025

JUMP_ADDR ?= 0x80500000

.PHONY: all boot btcode clean

all: boot btcode

boot:
	$(MAKE) -C boot boot JUMP_ADDR=$(JUMP_ADDR) RTL865X=1

btcode:
	$(MAKE) -C btcode RTL865X=1

clean:
	$(MAKE) -C boot clean
	$(MAKE) -C btcode clean
