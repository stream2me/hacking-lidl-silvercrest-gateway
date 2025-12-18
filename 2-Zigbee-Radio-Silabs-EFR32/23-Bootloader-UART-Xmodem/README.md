# Bootloader-UART-Xmodem for Lidl Gateway

UART XMODEM bootloader for EFR32MG1B232F256GM48 (Lidl Silvercrest Gateway).

This bootloader enables firmware updates via UART using the XMODEM-CRC protocol, without requiring SWD/JTAG access.

## Quick Start

```bash
# Build the bootloader
./build_bootloader.sh

# Flash via J-Link
commander flash firmware/bootloader-uart-xmodem-2.4.2-crc.s37 --device EFR32MG1B232F256GM48
```

## Prerequisites

- **slc** (Silicon Labs CLI) in PATH
- **arm-none-eabi-gcc** in PATH
- **GECKO_SDK** environment variable set

Or use Docker:
```bash
docker run -it --rm -v $(pwd)/..:/workspace lidl-gateway-builder \
    /workspace/2-Zigbee-Radio-Silabs-EFR32/23-Bootloader-UART-Xmodem/build_bootloader.sh
```

## Build Process

```
1. Copy slcp from patches/
        ↓
2. slc generate
        ↓
3. Copy config headers from patches/
        ↓
4. make -Oz
```

## Hardware Configuration

### UART Pinout (USART0)

| Signal | Port | Pin | Description |
|--------|------|-----|-------------|
| TX | PA0 | 0 | Transmit to RTL8196E |
| RX | PA1 | 1 | Receive from RTL8196E |
| RTS | PA4 | 4 | Request to Send |
| CTS | PA5 | 5 | Clear to Send |

### GPIO Activation

| Signal | Port | Pin | Description |
|--------|------|-----|-------------|
| Button | PB11 | 11 | Hold during reset to enter bootloader |

### Debug

| Signal | Port | Pin | Description |
|--------|------|-----|-------------|
| SWV | PF2 | 2 | Serial Wire Viewer |

## Output File

| File | Description |
|------|-------------|
| `bootloader-uart-xmodem-X.Y.Z-crc.s37` | Main bootloader with CRC (e.g., `bootloader-uart-xmodem-2.4.2-crc.s37`) |

Only the CRC version is generated, as it's the only one accessible when using serialgateway with hardware flow control.

## Using the Bootloader

Once flashed, the bootloader responds to serial commands at 115200 baud:

| Command | Action |
|---------|--------|
| `1` | Start XMODEM transfer (upload new firmware) |
| `2` | Start application |

To enter the bootloader:
- Hold PB11 button during reset, OR
- Send serial break, OR
- No valid application present

______________________________________________________________________

## Understanding the 2-Stage Bootloader Architecture (Series 1)

EFR32MG1B (Gecko Series 1) devices use a **two-stage bootloader system**:

### Stage 1 – First-stage bootloader (BSL)

- Resides in main flash memory starting at address **0x0000**
- Minimal: verifies and launches Stage 2
- Cannot be updated via UART or OTA
- Can only be overwritten using **SWD and a debugger**

### Stage 2 – Main bootloader

- Resides in main flash memory starting at address **0x0800**
- Contains UART XMODEM functionality
- Can be updated in the field via `.gbl` packages (*not recommended*)

### Application

- Resides in flash memory starting at address **0x4000**
- Updated via XMODEM using `.gbl` files

### Memory Map

```
0x00000000 ┌─────────────────────────┐
           │  First Stage (2 KB)     │ ← Can only be updated via SWD
0x00000800 ├─────────────────────────┤
           │  Main Bootloader (14 KB)│ ← UART XMODEM logic
0x00004000 ├─────────────────────────┤
           │  Application            │ ← NCP-UART-HW or Router firmware
           │  (~200 KB)              │
0x0003E000 ├─────────────────────────┤
           │  NVM3 Storage (36 KB)   │ ← Network keys, tokens
0x00040000 └─────────────────────────┘
```

______________________________________________________________________

## Updating the Bootloader via SWD

When you have SWD access, flash the bootloader:

```bash
commander flash firmware/bootloader-uart-xmodem-2.4.2-crc.s37 --device EFR32MG1B232F256GM48
```

Expected output:
```
Parsing file bootloader-uart-xmodem-2.4.2-crc.s37...
Writing 16384 bytes starting at address 0x00000000
Comparing range 0x00000000 - 0x00003FFF (16 KiB)
Programming range 0x00000000 - 0x00001FFF (8 KiB)
Programming range 0x00002000 - 0x00003FFF (8 KiB)
DONE
```

______________________________________________________________________

## After Bootloader Update: Restore Application

After a bootloader update, restore your application:

```bash
commander flash ncp-uart-hw.s37 --device EFR32MG1B232F256GM48
```

Or via UART XMODEM (if bootloader is working):
```bash
# Convert .s37 to .gbl first
commander gbl create app.gbl --app ncp-uart-hw.s37

# Then upload via XMODEM
./flash_xmodem.sh /dev/ttyUSB0 app.gbl
```

______________________________________________________________________

## Creating Combined Images (Bootloader + Application)

To update both bootloader (stage 2) and application in one UART transfer:

```bash
commander gbl create upgrade.gbl \
    --app ncp-uart-hw.s37 \
    --bootloader bootloader-uart-xmodem-2.4.2-crc.s37
```

> **Note**: This only updates the main bootloader (stage 2), not the first stage. First stage always requires SWD access.

______________________________________________________________________

## patches/ Directory

| File | Purpose |
|------|---------|
| `bootloader-uart-xmodem.slcp` | Project config with components |
| `btl_uart_driver_cfg.h` | UART pin configuration (USART0 PA0/PA1, RTS/CTS PA4/PA5) |
| `btl_gpio_activation_cfg.h` | Button pin configuration (PB11) |

## Pre-built Firmware


After running `./build_bootloader.sh`, output files are in [`firmware/`](firmware/):

| File | Content | Address | Usage |
|------|---------|---------|-------|
| `bootloader-uart-xmodem-X.Y.Z.s37` | Stage 2 only | 0x0800-0x3FFF | J-Link or combined GBL |
| `bootloader-uart-xmodem-X.Y.Z.gbl` | Stage 2 in GBL | - | XMODEM upgrade (requires working Stage 1) |

> **Important**: These files contain only the **Stage 2 (Main Bootloader)**, not Stage 1.
> - If Stage 1 is already present and working → use `.gbl` via XMODEM
> - If Stage 1 is missing or corrupted → use J-Link to flash `.s37` + restore Stage 1 from `build/autogen/first_stage.s37`
