# RTL8196E Bootloader Memo

## Overview

Bootloader for Realtek RTL8196E SoC (Lexra MIPS 4181 big-endian architecture).
Used on routers/gateways with SPI flash and SDRAM.

---

## Code Architecture

### Two-stage structure

```
src/
├── btcode/          # Stage 1: Initial boot code (LZMA compressed)
│   ├── start.S      # MIPS assembly entry point
│   ├── start_c.c    # Early C initialization
│   ├── bootload.c   # LZMA decompression and jump to stage 2
│   └── LzmaDecode.* # LZMA decompressor
│
└── boot/            # Stage 2: Main bootloader
    ├── init/        # System initialization
    │   ├── main.c   # Main entry point
    │   ├── utility.c # Utility functions, key/button detection
    │   └── eth_tftpd.c # TFTP server
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

**Required**: Realtek RSDK (supports `-march=4181`)

```bash
export PATH=/path/to/rsdk-4.4.7-4181-EB-2.6.30-0.9.30-m32u-140129/bin:$PATH
```

Compiler: `rsdk-linux-gcc` (MIPS big-endian, Lexra 4181)

---

## Current Hardware Configuration

From `src/.config`:

| Option | Value | Description |
|--------|-------|-------------|
| `CONFIG_RTL8196E` | y | Target SoC |
| `CONFIG_SPI_FLASH` | y | SPI flash |
| `CONFIG_SDRAM` | y | Memory type |
| `CONFIG_D32_16` | y | 32 MB SDRAM |
| `CONFIG_BOOT_RESET_ENABLE` | y | Reset button enabled |
| `CONFIG_LZMA_ENABLE` | y | LZMA compression |

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

#### ESC key detection (lines 998-1015)
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

#### Reset button detection (lines 1019-1063)
```c
int pollingPressedButton(int pressedFlag)
{
    if (Get_GPIO_SW_IN())   // Instantaneous GPIO read
        return 1;
    return pressedFlag;
}
```

#### Main function (lines 1068-1116)
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

Configured in `monitor/power_on_led.c:424-437`:

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

### Main banner: `init/main.c:176-232`

```c
void showBoardInfo(void)
{
    prom_printf("SDRAM:");
    prom_printf("32MB\n");
    cpu_speed = check_cpu_speed();
    prom_printf("\n---RealTek(RTL8196E)at %s %s [%s](%dMHz)\n",
                BOOT_CODE_TIME, B_VERSION, "16bit", cpu_speed);
}
```

### Typical output

```
SDRAM:32MB
---RealTek(RTL8196E)at 2025.11.25-12:31+0100 v1.0 [16bit](400MHz)
```

In download mode:
```
SDRAM:32MB
---RealTek(RTL8196E)at 2025.11.25-12:31+0100 v1.0 [16bit](400MHz)
---Ethernet init Okay!
<RealTek>
```

### Date generation

Automatic at compile time via `boot/Makefile:121`:
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

```
# CONFIG_RTL8196E_ULINKER_BOOT_LED is not set
```

### Available code: `monitor/power_on_led.c:454-476`

```c
#ifdef CONFIG_RTL8196E_ULINKER_BOOT_LED
void power_on_led(void)
{
    // WLAN LED (via PCIE GPIO)
    reg = (REG32(PCIE0_EP_CFG_BASE + 0x18) & 0xffff0000) | 0xb0000000;
    (*(volatile u32 *)(reg + 0x44)) = 0x30300000;

    // Ethernet LED (GPIO B6, bit 14)
    (*(volatile u32 *)0xb800350c) &= ~(0x00004000);  // LED ON
}
#endif
```

### To enable

In `src/.config` or `src/autoconf.h`:
```c
#define CONFIG_RTL8196E_ULINKER_BOOT_LED 1
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
| `src/.config` | Main configuration (menuconfig) |
| `src/autoconf.h` | C defines generated from .config |
| `src/boot/init/ver.h` | Bootloader version |
| `src/boot/banner/mk_time` | Compile timestamp (auto-generated) |

---

## Summary of Changes Made

1. **Compilation warnings fixed**: ~80 warnings resolved
2. **Unused code cleanup**: Removed non-RTL8196E files
3. **Code reformatting**: Linux kernel style (`indent -linux`)
4. **Dependency fixes**: `ver.h`, PCIE defines, `DBG_PRINT`

---

## Technical Notes

- **Lexra architecture**: Modified MIPS without unaligned memory access
- **Big-endian**: Watch out for conversions (`le32_to_cpu` macros)
- **SPI flash**: No NAND in this configuration
- **No ASCII logo**: Text-only boot display
