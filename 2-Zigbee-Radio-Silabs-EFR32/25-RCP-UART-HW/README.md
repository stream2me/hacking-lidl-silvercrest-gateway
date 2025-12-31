# RCP 802.15.4 Zigbee-Only Firmware

Minimal Radio Co-Processor (RCP) firmware for the EFR32MG1B232F256GM48.
**Zigbee-only** - stripped of OpenThread networking, Multi-PAN, and Green Power.

## Architecture

```
┌───────────────────┐    UART     ┌───────────────────┐   Ethernet   ┌─────────────────────┐
│  EFR32MG1B (RCP)  │   460800    │  RTL8196E         │    TCP/IP    │  Host (x86/ARM)     │
│                   │    baud     │  (Gateway SoC)    │              │                     │
│  802.15.4 PHY/MAC │◄───────────►│                   │◄────────────►│  cpcd               │
│  + Spinel handler │   ttyS0     │  serialgateway    │   port 8888  │    │                │
│                   │     ↔       │  (serial→TCP)     │              │    ▼                │
│  CPC Protocol v6  │   ttyS1     │                   │              │  zigbeed 8.2        │
│  Hardware Watchdog│             │                   │              │  (Zigbee stack)     │
│                   │             │                   │              │    │                │
│  ~98KB flash (38%)│             │                   │              │    ▼                │
│  ~21KB RAM (64%)  │             │                   │              │  Zigbee2MQTT / ZHA  │
└───────────────────┘             └───────────────────┘              │  Home Assistant     │
                                                                     └─────────────────────┘
```

The Lidl Silvercrest Gateway's RTL8196E SoC acts as a transparent serial-to-TCP bridge.
The EFR32's UART (ttyS0) is physically connected to the RTL8196E's ttyS1, which is
exposed over Ethernet via `serialgateway`. The actual Zigbee host stack runs on a
separate machine (e.g., Raspberry Pi, NAS, or server).

## Features

### Stripped Components (Space Savings)

| Component | Status | Savings |
|-----------|--------|---------|
| OpenThread Networking | **DISABLED** | ~30KB flash |
| Multi-PAN | **DISABLED** | ~5KB flash |
| Green Power | **NOT INCLUDED** | ~4KB flash |
| NVM3 | **DISABLED** | ~8KB flash, stateless |
| CPC Security/MbedTLS | **DISABLED** | ~40KB flash |
| Debug/RTT/Logging | **DISABLED** | ~6KB flash |
| VCOM/iostream | **NOT INCLUDED** | ~2KB flash |

**Retained:** 802.15.4 PHY/MAC + Spinel protocol + CPC transport

### Reliability Features

| Feature | Description |
|---------|-------------|
| Hardware Watchdog | WDOG0, ~2s timeout using ULFRCO (1kHz) |
| Status LED | PF4 - toggles on activity, rapid blink on CPC errors |
| HW Flow Control | RTS/CTS prevents buffer overflows at 460800 baud |

### Performance Optimizations

| Parameter | Default | Optimized | Purpose |
|-----------|---------|-----------|---------|
| CPC RX Buffers | 8 | 16 | Prevents packet loss during traffic bursts |
| CPC TX Queue | 8 | 16 | Better throughput at 460800 baud |
| OT Radio RX Buffers | 16 | 24 | High-density Zigbee network support |
| Stack Size | 1536 | 2048 | Stability margin |
| Heap Size | 512 | 1024 | Stability margin |
| Spinel Payload | 256 | 256 | Max Zigbee frame size |

### Hardware Radio Acceleration

All radio operations use hardware acceleration (no software fallback):

```c
OPENTHREAD_CONFIG_MAC_SOFTWARE_ENERGY_SCAN_ENABLE    = 0  // HW energy scan
OPENTHREAD_CONFIG_MAC_SOFTWARE_TX_TIMING_ENABLE      = 0  // HW TX timing
OPENTHREAD_CONFIG_MAC_SOFTWARE_TX_SECURITY_ENABLE    = 0  // HW AES
OPENTHREAD_CONFIG_MAC_SOFTWARE_RETRANSMIT_ENABLE     = 0  // HW retransmit
OPENTHREAD_CONFIG_MAC_SOFTWARE_CSMA_BACKOFF_ENABLE   = 0  // HW CSMA/CA
```

### MAC Filtering

RAIL 802.15.4 automatically filters frames by PAN ID in hardware.
Only frames destined for the configured PAN are processed by the CPU.

## CPC Protocol v6 Backport

Backports CPC v6 from Simplicity SDK 2025.6.2 → GSDK 4.5.0.

**Discovery:** The API is identical between versions - only the version number changes:
```c
#define SLI_CPC_PROTOCOL_VERSION (6)  // was (5) in GSDK 4.5.0
```

This allows zigbeed 8.2 to recognize and communicate with the RCP.

## Hardware Pinout

| Signal | Pin | Location | Function |
|--------|-----|----------|----------|
| TX | PA0 | 0 | USART0 TX |
| RX | PA1 | 0 | USART0 RX |
| RTS | PA4 | 30 | Flow Control |
| CTS | PA5 | 30 | Flow Control |
| LED | PF4 | - | Status (optional) |

**UART:** 460800 baud, 8N1, Hardware Flow Control

## Build

### Prerequisites

- GSDK 4.5.0 (Gecko SDK)
- slc-cli (Silicon Labs Configurator)
- ARM GCC toolchain (12.x recommended)

### Build Commands

```bash
# Build firmware
./build_rcp.sh

# Clean and rebuild
./build_rcp.sh clean && ./build_rcp.sh
```

### Output Files

| File | Size | Use |
|------|------|-----|
| `firmware/rcp-uart-802154.gbl` | ~97 KB | Flash via UART/Xmodem |
| `firmware/rcp-uart-802154.s37` | ~276 KB | Flash via J-Link/SWD |
| `firmware/rcp-uart-802154.hex` | ~270 KB | Alternative format |
| `firmware/rcp-uart-802154.bin` | ~96 KB | Raw binary |

## Memory Usage

| Resource | Available | Used | Percentage |
|----------|-----------|------|------------|
| Flash | 256 KB | ~98 KB | 38% |
| RAM | 32 KB | ~21 KB | 64% |

### Memory Breakdown

```
Flash (text + data):  ~98 KB
  ├── RAIL 802.15.4:  ~35 KB
  ├── OpenThread RCP: ~25 KB
  ├── CPC Transport:  ~15 KB
  ├── Platform/emlib: ~12 KB
  ├── MbedTLS (AES):  ~8 KB
  └── App + misc:     ~3 KB

RAM (data + bss):     ~21 KB
  ├── CPC Buffers:    ~8 KB (16 RX + 16 TX @ 256B)
  ├── OT RX Buffers:  ~6 KB (24 @ 256B)
  ├── Stack:          ~2 KB
  ├── Heap:           ~1 KB
  └── Static vars:    ~4 KB
```

## Flashing

### Via UART/Xmodem (recommended)

```bash
# Using universal-silabs-flasher
pip install universal-silabs-flasher
universal-silabs-flasher --device /dev/ttyUSB0 --firmware firmware/rcp-uart-802154.gbl
```

### Via J-Link/SWD

```bash
commander flash firmware/rcp-uart-802154.s37 --device EFR32MG1B232F256GM48
```

## Host Software Build

The host machine needs cpcd and zigbeed. Build scripts are provided.

| Component | Source | Build Tool | Provides |
|-----------|--------|------------|----------|
| **cpcd** | GitHub (`SiliconLabs/cpc-daemon`) | cmake | cpcd + libcpc.so |
| **zigbeed** | Simplicity SDK 2025.6.2 | slc + gcc | zigbeed |

### Prerequisites

```bash
# For cpcd (from GitHub, uses cmake)
sudo apt install cmake gcc g++ libmbedtls-dev

# For zigbeed (from Simplicity SDK, uses slc)
sudo apt install build-essential
# Also needs: slc-cli + Simplicity SDK 2025.6.2
```

### Build Order

**Important:** Build cpcd first, then zigbeed.

- cpcd produces **libcpc.so** which is required to compile zigbeed
- zigbeed build script automatically trusts the SDK signature

```bash
# 1. Build and install cpcd
cd cpcd
./build_cpcd.sh install

# 2. Build zigbeed
cd ../zigbeed
./build_zigbeed.sh

# 3. Install zigbeed
sudo cp bin/zigbeed /usr/local/bin/
sudo cp bin/zigbeed.conf /usr/local/etc/
```

### Output Summary

| Component | Binary | Size | Description |
|-----------|--------|------|-------------|
| cpcd | `cpcd/bin/cpcd` | ~273 KB | CPC daemon |
| libcpc | `cpcd/bin/libcpc.so*` | ~94 KB | CPC library |
| zigbeed | `zigbeed/bin/zigbeed` | ~2.2 MB | Zigbee stack (x86_64) |

See `cpcd/README.md` and `zigbeed/README.md` for details.

## Host Setup (Linux)

The host machine runs cpcd, zigbeed, and your Zigbee coordinator software.
It connects to the gateway's serialgateway over TCP.

### 1. cpcd Configuration

Edit `/etc/cpcd.conf`:
```yaml
# Connect to Lidl Gateway via TCP (serialgateway)
socket_file_path: /var/run/cpcd/cpcd.sock
tcp_server_port: 8888
tcp_client_address: 192.168.1.xxx  # Gateway IP address
tcp_client_port: 8888

# Alternative: Direct serial connection (if using USB adapter)
# uart_device_file: /dev/ttyUSB0
# uart_device_baud: 460800
# uart_hardflow: true
```

### 2. Gateway Configuration (RTL8196E)

On the Lidl Gateway, ensure serialgateway is running:
```bash
# serialgateway bridges ttyS1 to TCP port 8888
serialgateway -p 8888 -s /dev/ttyS1 -b 460800
```

### 3. Start Services on Host

```bash
systemctl start cpcd
systemctl start zigbeed
```

### 4. Zigbee2MQTT Configuration

Edit `configuration.yaml`:
```yaml
serial:
  port: tcp://localhost:9999
  adapter: ember
```

### 5. Verify Connection

```bash
# Check cpcd status
journalctl -u cpcd -f

# Check zigbeed status
journalctl -u zigbeed -f

# Test TCP connection to gateway
nc -zv 192.168.1.xxx 8888
```

## Troubleshooting

### TCP Connection Issues

If cpcd cannot connect to the gateway:
1. Verify gateway IP address and port 8888 is reachable: `nc -zv <gateway-ip> 8888`
2. Check serialgateway is running on the RTL8196E
3. Verify no firewall blocking port 8888
4. Check gateway hasn't rebooted (IP may have changed)

### Watchdog Reset

If the firmware resets unexpectedly, check:
1. CPC communication is active (LED should toggle)
2. cpcd is running and connected
3. TCP connection to gateway is stable

The watchdog timeout is ~2 seconds. If `app_process_action()` is not called
within this time, the system will reset.

### CPC Communication Errors

The status LED (PF4) will show a rapid blink pattern (3 fast blinks) when
CPC errors occur. Check:
1. Baud rate matches (460800) on both EFR32 and serialgateway
2. TCP connection latency (should be <10ms on LAN)
3. cpcd version is compatible with CPC protocol v6

### High Packet Loss

If experiencing packet loss during high traffic:
1. Check network latency between host and gateway
2. Verify serialgateway isn't dropping bytes (check with `netstat -s`)
3. Consider wired Ethernet instead of WiFi for the gateway
4. Check buffer counts in build output

## Project Structure

```
25-RCP-UART-HW/
├── build_rcp.sh                 # RCP firmware build script
├── README.md                    # This file
├── patches/                     # RCP firmware customizations
│   ├── rcp-uart-802154.slcp     # Project configuration
│   ├── main.c                   # Entry point
│   ├── app.c                    # App logic, watchdog, GPIO
│   ├── sl_cpc_drv_uart_usart_vcom_config.h  # UART config
│   └── sl_cpc_security_config.h # Security stub config
├── firmware/                    # RCP firmware output
│   ├── rcp-uart-802154.gbl
│   ├── rcp-uart-802154.s37
│   └── ...
├── cpcd/                        # CPC daemon (host)
│   ├── build_cpcd.sh            # Build script
│   ├── README.md
│   └── bin/                     # Build output
├── zigbeed/                     # Zigbee daemon (host)
│   ├── build_zigbeed.sh         # Build script
│   ├── README.md
│   └── bin/                     # Build output
└── build/                       # Build directory (generated)
```

## References

- [AN1333: Running Zigbee, OpenThread, and Bluetooth Concurrently on a Linux Host with a Multiprotocol Co-Processor](https://www.silabs.com/documents/public/application-notes/an1333-concurrent-protocols-with-802-15-4-rcp.pdf)
- [Silicon Labs zigbeed Documentation](https://docs.silabs.com/zigbee/8.2.0/multiprotocol-solution-linux/)
- [CPC Protocol Documentation](https://docs.silabs.com/co-processor-communication/latest/)
- [GSDK 4.5.0 Release Notes](https://www.silabs.com/developers/gecko-software-development-kit)

## License

This firmware configuration is provided for educational and personal use.
Silicon Labs SDK components are subject to their respective licenses.
