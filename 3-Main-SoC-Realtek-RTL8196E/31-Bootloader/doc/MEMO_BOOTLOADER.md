# RTL8196E Bootloader Memo

## Overview

Bootloader for Realtek RTL8196E SoC (Lexra MIPS 4181 big-endian architecture).
Used on routers/gateways with SPI flash and SDRAM.

---

## Code Architecture

### Two-stage structure

```
├── btcode/          # Stage 1: Initial boot code (LZMA wrapper)
│   ├── start.S      # MIPS assembly entry point
│   ├── start_c.c    # C-level startup (unused, placeholder)
│   ├── piggy.S      # Compressed payload wrapper
│   ├── bootload.c   # LZMA decompression and jump to stage 2
│   └── LzmaDecode.* # LZMA decompressor
│
└── boot/            # Stage 2: Main bootloader
    ├── head.S       # MIPS entry point (CPU init, DDR calibration)
    ├── inthandler.S # Exception/interrupt vectors
    ├── arch.c       # Architecture init (CP0, cache, exceptions)
    ├── main.c       # Boot logic: image validation, kernel loading
    ├── irq.c        # Interrupt controller setup
    ├── monitor.c    # Interactive console + CPU speed calibration
    ├── uart.c       # UART driver
    ├── libc.c       # Minimal libc (printf, string, strtol...)
    ├── calloc.c     # Heap allocator
    ├── flash.c      # SPI flash driver
    ├── swCore.c     # Ethernet switch core
    ├── swTable.c    # Ethernet switch table management
    ├── swNic.c      # Ethernet NIC polling driver
    └── net/
        ├── eth.c    # Ethernet frame TX/RX
        └── tftpd.c  # TFTP server for firmware recovery
```

### Boot process

1. **Stage 1** (`btcode/`): Loaded from SPI flash at `0x80100000`
   - Initializes UART, minimal memory
   - Decompresses stage 2 (LZMA) to `0x80400000`
   - Jumps to stage 2

2. **Stage 2** (`boot/`): Full bootloader
   - Initializes SDRAM, Ethernet switch, GPIO
   - Displays banner
   - Scans flash for a valid firmware image
   - During image copy, polls for ESC key (configurable timeout)
   - Normal mode -> loads kernel from flash
   - Download mode -> TFTP server + console

---

## Toolchain

**Required**: Custom Lexra toolchain built with crosstool-ng

```bash
export PATH=$HOME/hacking-lidl-silvercrest-gateway/x-tools/mips-lexra-linux-musl/bin:$PATH
```

Compiler: `mips-lexra-linux-musl-gcc` (MIPS big-endian, Lexra LX4380, musl libc)

The toolchain is built using crosstool-ng with Lexra patches. See `1-Build-Environment/` for build instructions.

---

## Boot Modes

### 1. Normal mode (LOCALSTART_MODE)

- Automatic boot after image scan
- Loads and executes kernel from flash

### 2. Download mode (DOWN_MODE)

Triggered by:
- **ESC key** (0x1B) on UART during image copy/checksum verification

Available services:
- TFTP server (port 2098)
- Interactive console `<RealTek>`

### ESC key detection: `main.c`

```c
int pollingDownModeKeyword(int key)
{
    if (!uart_data_ready())
        return 0;
    ch = uart_getc_nowait();
    if (ch == key) {       // key = 0x1B (ESC)
        gCHKKEY_HIT = 1;
        return 1;          // DOWN_MODE
    }
    return 0;
}

int user_interrupt(unsigned long time)
{
    return pollingDownModeKeyword(ESC);
}
```

Note: The reset button / GPIO detection has been removed in this simplified version. Only ESC key over UART triggers download mode.

---

## TFTP Server

### Configuration: `net/tftpd.c`

| Parameter | Value |
|-----------|-------|
| Server IP (bootloader) | 192.168.1.6 |
| Expected client IP | 192.168.1.116 |
| TFTP port | **2098** (non-standard) |
| Standard TFTP port | 69 (used for WRQ reception) |

### Usage

```bash
# From PC (192.168.1.116)
tftp 192.168.1.6 2098
> binary
> put firmware.bin
```

**Note**: The bootloader acts as TFTP server, not client. It waits for incoming connections.

---

## Console Output at Boot

### Banner: `main.c:61-70`

```c
void showBoardInfo(void)
{
    cpu_speed = check_cpu_speed();
    prom_printf("Realtek RTL8196E  CPU: %dMHz  RAM: 32MB  Flash: %s\n",
                cpu_speed, g_flash_chip_name);
    prom_printf("Bootloader: %s - %s - J. Nilo", B_VERSION, BOOT_CODE_TIME);
}
```

### Typical output

```
Realtek RTL8196E  CPU: 400MHz  RAM: 32MB  Flash: GD25Q128
Bootloader: V2.2 - 2026.02.10-18:30+0100 - J. Nilo
```

In download mode:
```
Realtek RTL8196E  CPU: 400MHz  RAM: 32MB  Flash: GD25Q128
Bootloader: V2.2 - 2026.02.10-18:30+0100 - J. Nilo
---Escape booting by user
---Ethernet init Okay!
<RealTek>
```

### Build timestamp

Generated automatically at compile time via `boot/Makefile`:
```makefile
BOOT_CODE_TIME ?= $(shell date "+%Y.%m.%d-%H:%M%z")
```

### Version

Defined in `boot/include/ver.h`:
```c
static char B_VERSION[] = "V2.2";
```

---

## Key Memory Addresses

| Address | Usage |
|---------|-------|
| `0x80000000` | SDRAM base (kseg0) |
| `0x80100000` | Compressed btcode load address |
| `0x80300000` | LZMA state buffer |
| `0x80400000` | Bootloader decompression target |
| `0x80500000` | Kernel entry point (JUMP_ADDR) |
| `0xB8000000` | I/O registers (kseg1) |
| `0xB8002000` | UART base |
| `0xB8003500` | GPIO registers |
| `0xBFC00000` | SPI flash (boot ROM) |

---

## Building

### Using the build script

```bash
./build_bootloader.sh          # build all variants
./build_bootloader.sh clean    # clean all build outputs
```

### Using make directly

```bash
make            # build all variants
make clean      # clean all build outputs
```

### Build variants

Three variants are built:

| Variant | Output file | Description |
|---------|-------------|-------------|
| noreboot | `boot_noreboot.bin` | Boot code TFTP flash does NOT auto-reboot (safe) |
| reboot | `boot_reboot.bin` | Boot code TFTP flash auto-reboots after completion |
| ramtest | `btcode/build/test.bin` | RAM-test image with read-back verification |

The `BOOT_REBOOT` define controls whether flashing a new boot code image via TFTP triggers an automatic reboot. In the `noreboot` variant (default), the device stays in download mode after flashing boot code.

### Build pipeline

```
boot.out  (ELF from boot/)
  |  objcopy to raw binary + LZMA compress
  v
boot.img.gz  +  piggy.S  +  bootload.o  +  LzmaDecode.o
  |  link with piggy.script
  v
piggy.elf  ->  piggy.bin  (raw stage-2 payload)
  |  embed in start.o via .initrd section
  v
start.o  +  start_c.o
  |  link with ld.script
  v
boot.elf  ->  boot  (raw binary)
  |  cvimg (add Realtek flash header)
  v
boot.bin        (final flash image)
```

### Variables

| Variable | Default | Description |
|----------|---------|-------------|
| `JUMP_ADDR` | `0x80500000` | Kernel entry point passed to stage-2 |
| `CROSS` | `mips-lexra-linux-musl-` | Toolchain prefix |

---

## Configuration Files

| File | Purpose |
|------|---------|
| `Makefile` | Top-level build: orchestrates three variants |
| `boot/Makefile` | Stage-2 build: compiler flags, source list |
| `btcode/Makefile` | Stage-1 build: LZMA compression, flash header |
| `boot/include/flash_layout.h` | Flash partition offsets and scan ranges |
| `boot/include/ver.h` | Bootloader version string |

---

## Summary of Changes Made

1. **Toolchain migration**: From RSDK to crosstool-ng (mips-lexra-linux-musl)
2. **Flat source layout**: Removed `src/` wrapper; `boot/` and `btcode/` are now top-level
3. **Flat boot/ structure**: Removed subdirectories (`arch/`, `init/`, `io/`, `monitor/`, `flash/`, `rtl8196x/`); all C sources at top level with `net/` for networking
4. **Code consolidation**: `utility.c` merged into `main.c`; `power_on_led.c` removed; `string.c`/`strtol.c`/`strtoul.c`/`misc.c`/`ctool.c`/`init.c` merged into `libc.c`; `spi_common.c`/`spi_flash.c` merged into `flash.c`; `setup.c`/`traps.c`/`head.S` simplified into `arch.c`/`head.S`
5. **Reset button removed**: GPIO-based reset button detection removed; ESC key only
6. **Boot LED removed**: `power_on_led.c` and all LED-related code removed
7. **Build system simplification**: Removed menuconfig, hardcoded RTL8196E config; three-variant build (noreboot/reboot/ramtest)
8. **Header cleanup**: Replaced Linux 2.4 headers with minimal standalone headers
9. **UART driver extracted**: Dedicated `uart.c` for serial I/O

---

## Technical Notes

- **Lexra architecture**: Modified MIPS without unaligned memory access
- **Big-endian**: Watch out for conversions (`le32_to_cpu` macros)
- **SPI flash**: No NAND in this configuration
- **No ASCII logo**: Text-only boot display
- **No rootfs scan**: `SKIP_ROOTFS_SCAN` is set to 1; Linux locates rootfs itself
