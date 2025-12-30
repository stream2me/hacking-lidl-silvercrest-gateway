# RTL8196E Bootloader Memo

## Overview

Bootloader for Realtek RTL8196E SoC (Lexra MIPS 4181 big-endian architecture).
Used on routers/gateways with SPI flash and SDRAM.

---

## Code Architecture

### Two-stage structure

```
src/
├── btcode/          # Stage 1: Initial boot code (LZMA wrapper)
│   ├── start.S      # MIPS assembly entry point
│   ├── piggy.S      # Compressed payload wrapper
│   ├── bootload.c   # LZMA decompression and jump to stage 2
│   └── LzmaDecode.* # LZMA decompressor
│
└── boot/            # Stage 2: Main bootloader
    ├── arch/        # MIPS architecture (head.S, setup.c, traps.c)
    ├── init/        # System initialization
    │   ├── main.c   # Main entry point
    │   ├── utility.c # Utility functions, key/button detection
    │   └── eth_tftpd.c # TFTP server
    ├── io/          # String, console I/O functions
    ├── monitor/     # Interactive console
    │   ├── monitor.c # Command shell
    │   └── power_on_led.c # GPIO, LEDs, reset button
    ├── flash/       # SPI flash drivers
    └── rtl8196x/    # Ethernet switch drivers
```

### Boot process

1. **Stage 1** (`btcode/`): Loaded from SPI flash at `0x80100000`
   - Initializes UART, minimal memory
   - Decompresses stage 2 (LZMA) to `0x80400000`
   - Jumps to stage 2

2. **Stage 2** (`boot/`): Full bootloader
   - Initializes SDRAM, Ethernet switch, GPIO
   - Displays banner
   - Waits for ESC or reset button (configurable timeout)
   - Normal mode → loads kernel from flash
   - Download mode → TFTP server + console

---

## Toolchain

**Required**: Custom Lexra toolchain built with crosstool-ng

```bash
export PATH=$HOME/hacking-lidl-silvercrest-gateway/x-tools/mips-lexra-linux-musl/bin:$PATH
```

Compiler: `mips-lexra-linux-musl-gcc` (MIPS big-endian, Lexra LX4380, musl libc)

The toolchain is built using crosstool-ng with Lexra patches. See `1-Build-Environment/` for build instructions.

---

## Current Hardware Configuration

Hardcoded in Makefiles (no menuconfig):

| Define | Value | Description |
|--------|-------|-------------|
| `RTL8196E` | 1 | Target SoC |
| `DDR1_SDRAM` | 1 | DDR1 memory type (not DDR2) |
| `RTL865X` | 1 | Ethernet switch family |
| `LZMA_COMPRESS` | 1 | LZMA compression enabled |

Memory: 32 MB DDR1 SDRAM (16-bit bus)

---

## Boot Modes

### 1. Normal mode (LOCALSTART_MODE)

- Automatic boot after timeout
- Loads and executes kernel from flash

### 2. Download mode (DOWN_MODE)

Triggered by:
- **ESC key** (0x1B) on UART during boot
- **Reset button** GPIO held during boot

Available services:
- TFTP server (port 2098)
- Interactive console `<RealTek>`

---

## ESC / Reset Button Detection

### Code: `init/utility.c`

#### ESC key detection (lines 209-226)
```c
int pollingDownModeKeyword(int key)
{
    if (Check_UART_DataReady()) {
        i = Get_UART_Data();
        if (i == key)    // key = 0x1B (ESC)
            return 1;    // DOWN_MODE
    }
    return 0;
}
```

#### Reset button detection (lines 229-264)
```c
int pollingPressedButton(int pressedFlag)
{
    if (Get_GPIO_SW_IN())   // Instantaneous GPIO read
        return 1;
    return pressedFlag;
}
```

#### Main function (lines 268-302)
```c
int user_interrupt(unsigned long time)
{
    ret = pollingDownModeKeyword(ESC);
    if (ret == 1) return 1;
    ret = pollingPressedButton(button_press_detected);
    if (ret > 0) return ret;
    return 0;
}
```

### Reset button GPIO

Configured in `monitor/power_on_led.c:414-421`:

| Config | GPIO | Register bit |
|--------|------|--------------|
| Standard | GPIO A5 | `PABCDDAT_REG` bit 5 |
| `CONFIG_RTL8196E_GPIOB5_RESET` | GPIO B5 | `PABCDDAT_REG` bit 13 |
| RTL8196ES (bond option) | Via PCIE GPIO | External register |

**Press type**: Instantaneous (no minimum duration required)

---

## TFTP Server

### Configuration: `init/eth_tftpd.c`

| Parameter | Value |
|-----------|-------|
| Server IP (bootloader) | 192.168.1.6 |
| Expected client IP | 192.168.1.116 |
| TFTP port | **2098** (non-standard) |

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

### Main banner: `init/main.c:80-98`

```c
void showBoardInfo(void)
{
    cpu_speed = check_cpu_speed();
    prom_printf("RTL8196E Bootloader %s (%s)\n", B_VERSION, BOOT_CODE_TIME);
    prom_printf("DDR1 32MB | CPU %dMHz\n", cpu_speed);
}
```

### Typical output

```
RTL8196E Bootloader v1.0 (2025.12.27-12:37+0100)
DDR1 32MB | CPU 400MHz
```

In download mode:
```
RTL8196E Bootloader v1.0 (2025.12.27-12:37+0100)
DDR1 32MB | CPU 400MHz
---Ethernet init Okay!
<RealTek>
```

### Date generation

Automatic at compile time via `boot/Makefile:70`:
```makefile
@echo #define BOOT_CODE_TIME "`date "+%Y.%m.%d-%H:%M%z"`" > ./banner/mk_time
```

### Version

Defined in `init/ver.h`:
```c
#define B_VERSION "v1.0"
```

---

## Boot LEDs

### Status: DISABLED by default

Boot LED code exists in `monitor/power_on_led.c` but is not compiled by default.

### To enable

Add to `src/boot/Makefile` CFLAGS:
```makefile
-DCONFIG_RTL8196E_ULINKER_BOOT_LED
```

---

## Key Memory Addresses

| Address | Usage |
|---------|-------|
| `0x80000000` | SDRAM base (kseg0) |
| `0x80100000` | Compressed btcode load address |
| `0x80300000` | LZMA state buffer |
| `0x80400000` | Bootloader decompression target |
| `0xB8000000` | I/O registers (kseg1) |
| `0xB8002000` | UART base |
| `0xB8003500` | GPIO registers |
| `0xBFC00000` | SPI flash (boot ROM) |

---

## Building

```bash
./build_bootloader.sh
```

Produces:
- `src/boot/boot.bin` - Stage 2 bootloader
- `src/btcode/btcode.bin` - Stage 1 bootloader (compressed)
- Combined final image for flashing

---

## Configuration Files

| File | Purpose |
|------|---------|
| `src/boot/Makefile` | Main build configuration (defines) |
| `src/boot/include/boot_config.h` | Hardware-specific defines |
| `src/boot/init/ver.h` | Bootloader version |
| `src/boot/banner/mk_time` | Compile timestamp (auto-generated) |

---

## Summary of Changes Made

1. **Toolchain migration**: From RSDK to crosstool-ng (mips-lexra-linux-musl)
2. **Build system simplification**: Removed menuconfig, hardcoded RTL8196E config
3. **Unused code cleanup**: Removed timer/, scanf.c, vfscanf.c, non-RTL8196E code
4. **Header cleanup**: Replaced Linux 2.4 headers with minimal standalone headers
5. **Compilation warnings fixed**: All warnings resolved

---

## Technical Notes

- **Lexra architecture**: Modified MIPS without unaligned memory access
- **Big-endian**: Watch out for conversions (`le32_to_cpu` macros)
- **SPI flash**: No NAND in this configuration
- **No ASCII logo**: Text-only boot display
