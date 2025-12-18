# NCP-UART-HW Firmware

Network Co-Processor (NCP) firmware for the **Silabs EFR32MG1B232F256GM48** Zigbee chip in the Lidl Silvercrest Gateway.

This firmware enables communication with Zigbee coordinators like **Zigbee2MQTT** and **Home Assistant ZHA** via EZSP (EmberZNet Serial Protocol).

---

## Quick Start

### Choose Your Path

| | Option 1: Flash Pre-built | Option 2: Build from Source |
|---|---|---|
| **For** | Most users | Developers / Hackers |
| **Time** | ~5 minutes | ~10 minutes |
| **Requires** | Gateway with bootloader | Docker or Ubuntu 22.04 |
| **Use case** | Just want a working Zigbee bridge | Customize network parameters |

---

## Option 1: Flash Pre-built Firmware

**Pre-built firmware files are ready to flash.** No compilation needed.

### Pre-built Firmware Files

Located in the [`firmware/`](./firmware/) directory:

| File | EZSP Version | EmberZNet | Description |
|------|--------------|-----------|-------------|
| `ncp-uart-hw-7.5.1.gbl` | 13 | 7.5.1 | For UART/Xmodem flashing |
| `ncp-uart-hw-7.5.1.s37` | 13 | 7.5.1 | For J-Link/SWD flashing |
| `ncp-uart-hw-7.5.1.hex` | 13 | 7.5.1 | Intel HEX format |
| `ncp-uart-hw-7.5.1.bin` | 13 | 7.5.1 | Raw binary |

### Flashing Instructions

For detailed flashing methods (UART/Xmodem, scripts, prerequisites), see [23-Bootloader-UART-Xmodem](../23-Bootloader-UART-Xmodem/).

---

## Option 2: Build from Source

**For developers who want to customize network parameters or add features.**

### Prerequisites

First, set up the build environment. See [1-Build-Environment](../../1-Build-Environment/) for detailed instructions.

### Build with Docker (slc-cli)

From the **project root** (e.g. ./hacking-lidl-silvercrest-gateway):

```bash
docker run --rm -v $(pwd):/workspace lidl-gateway-builder \
    /workspace/2-Zigbee-Radio-Silabs-EFR32/24-NCP-UART-HW/build_ncp.sh
```

Or interactively:

```bash
docker run -it --rm -v $(pwd):/workspace lidl-gateway-builder
cd /workspace/2-Zigbee-Radio-Silabs-EFR32/24-NCP-UART-HW
./build_ncp.sh
```

### Build Natively (slc-cli, Ubuntu 22.04 / WSL2)

```bash
# Ensure tools are in PATH
export PATH="$HOME/arm-gnu-toolchain/bin:$HOME/slc_cli:$PATH"
export GECKO_SDK="$HOME/gecko_sdk"

# Build
cd 2-Zigbee-Radio-Silabs-EFR32/24-NCP-UART-HW
./build_ncp.sh
```

### Build Output (in both cases)

```
firmware/
├── ncp-uart-hw.gbl   # For UART/Xmodem flashing
├── ncp-uart-hw.s37   # For J-Link/SWD flashing
├── ncp-uart-hw.hex   # Intel HEX format
└── ncp-uart-hw.bin   # Raw binary
```

### Clean Build

```bash
./build_ncp.sh clean
```

---

## Customization

The build process applies patches to optimize the firmware for the Lidl Gateway. See [patches/README.md](./patches/README.md) for details.

### Network Parameters

Edit `patches/apply_config.sh` to modify. For a **complete guide** on network sizing, see the [Zigbee Network Sizing Guide](https://github.com/jnilo1/slc-projects/blob/main/ncp-uart-hw/ZIGBEE_NETWORK_SIZING_GUIDE.md).

#### Quick Reference

| Parameter | File | Default | Range | RAM/Entry |
|-----------|------|---------|-------|-----------|
| `EMBER_MAX_END_DEVICE_CHILDREN` | sl_zigbee_pro_stack_config.h | 32 | 0-64 | ~40 bytes |
| `EMBER_PACKET_BUFFER_COUNT` | sl_zigbee_pro_stack_config.h | 255 | 20-255 | ~36 bytes |
| `EMBER_SOURCE_ROUTE_TABLE_SIZE` | sl_zigbee_source_route_config.h | 100 | 2-255 | ~12 bytes |
| `EMBER_BINDING_TABLE_SIZE` | sl_zigbee_pro_stack_config.h | 32 | 1-127 | ~20 bytes |
| `EMBER_ADDRESS_TABLE_SIZE` | sl_zigbee_pro_stack_config.h | 12 | 1-256 | ~16 bytes |
| `EMBER_NEIGHBOR_TABLE_SIZE` | sl_zigbee_pro_stack_config.h | 26 | 16/26 | ~32 bytes |
| `EMBER_KEY_TABLE_SIZE` | sl_zigbee_security_link_keys_config.h | 12 | 1-127 | ~32 bytes |
| `EMBER_APS_UNICAST_MESSAGE_COUNT` | sl_zigbee_pro_stack_config.h | 32 | 1-255 | ~24 bytes |
| `NVM3_DEFAULT_NVM_SIZE` | nvm3_default_config.h | 36864 | — | N/A |

#### Network Size Presets

| Preset | Devices | Children | Buffers | Routes | Bindings | NVM3 |
|--------|---------|----------|---------|--------|----------|------|
| **Small** | <20 | 10 | 75 | 20 | 10 | 20KB |
| **Medium** | 20-50 | 20 | 150 | 50 | 20 | 28KB |
| **Large** (default) | 50-100 | 32 | 255 | 100 | 32 | 36KB |
| **Very Large** | 100-150 | 48 | 255 | 150 | 48 | 40KB |

#### RAM Budget

The EFR32MG1B has only **32KB RAM**. Current configuration uses ~27KB, leaving ~5KB headroom.

```
RAM ≈ Base (~12KB) + Children×40 + Buffers×36 + Routes×12 + Bindings×20 + ...
```

> **Warning**: Increasing parameters beyond presets may cause instability due to RAM overflow.

### UART Configuration

The firmware is configured for the Lidl Gateway pinout:

| Signal | Pin | Description |
|--------|-----|-------------|
| TX | PA0 | Transmit |
| RX | PA1 | Receive |
| RTS | PA4 | Ready to Send |
| CTS | PA5 | Clear to Send |

Baudrate: 115200, Hardware flow control (RTS/CTS)

---

## Technical Details

### Memory Usage

| Region | Size | Usage |
|--------|------|-------|
| Flash | 256 KB | ~200 KB (78%) |
| RAM | 32 KB | ~24 KB (75%) |
| NVM3 | 36 KB | Network data storage |

### Features

- **Zigbee PRO R22** stack
- **Green Power** support
- **Source routing** for large networks
- **Hardware flow control** (RTS/CTS)
- **1s boot delay** for RTL8196E compatibility

### Optimizations Applied

To fit in 256KB flash, the following were removed:
- Debug print components (~12 KB)
- ZigBee Light Link / touchlink (~4 KB)
- Virtual UART (~1 KB)
- PTI (Packet Trace Interface)

---


## References

- [EZSP Protocol Reference (UG100)](https://www.silabs.com/documents/public/user-guides/ug100-ezsp-reference-guide.pdf)
- [EmberZNet NCP Guide (UG115)](https://www.silabs.com/documents/public/user-guides/ug115-ncp-user-guide.pdf)
- [AN1233: Zigbee Stack Configuration](https://www.silabs.com/documents/public/application-notes/an1233-zigbee-stack-config.pdf)
