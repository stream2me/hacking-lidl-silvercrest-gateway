# RCP 802.15.4 Firmware with cpcd + zigbeed

Radio Co-Processor (RCP) firmware for the EFR32MG1B232F256GM48.

## About this firmware

This is a **multiprotocol RCP** (rcp-uart-802154) that supports running OpenThread and Zigbee
stacks simultaneously on a host processor. It uses concurrent multiprotocol (CMP) / multi-PAN
functionality to run 802.15.4 networks on the same channel. The host stacks communicate with
the RCP using the Co-Processor Communication protocol (CPC), which acts as a protocol
multiplexer and serial transport layer.

**In this project, we use it for Zigbee only** (via cpcd + zigbeed), running the Zigbee stack
on an external Linux host rather than on the RTL8196E gateway. This "Zigbee-on-host" approach
offloads the resource-intensive EmberZNet stack to a more capable machine, while the gateway
simply bridges the EFR32 UART to TCP.

> For more information on multiprotocol capabilities, see
> [AN1333: Running Zigbee, OpenThread, and Bluetooth Concurrently on a Linux Host](https://www.silabs.com/documents/public/application-notes/an1333-concurrent-protocols-with-802-15-4-rcp.pdf)

## Architecture

```
+-------------------+    UART     +-------------------+   Ethernet   +---------------------+
|  EFR32MG1B (RCP)  |   115200    |  RTL8196E         |    TCP/IP    |  Host (x86/ARM)     |
|                   |    baud     |  (Gateway SoC)    |              |                     |
|  802.15.4 PHY/MAC |<----------->|                   |<------------>|  cpcd               |
|  + CPC Protocol   |   ttyS0     |  serialgateway    |   port 8888  |    |                |
|                   |     <->     |  (serial->TCP)    |              |    v                |
|  CPC Protocol v5  |   ttyS1     |                   |              |  zigbeed            |
|  HW Flow Control  |             |                   |              |  (Zigbee stack)     |
|                   |             |                   |              |    |                |
+-------------------+             +-------------------+              |    v                |
                                                                     |  Zigbee2MQTT        |
                                                                     +---------------------+
```

The RTL8196E runs `serialgateway` to bridge the EFR32's UART to TCP port 8888.
See [34-Userdata](../../3-Main-SoC-Realtek-RTL8196E/34-Userdata/) for gateway setup.

## Host Software

This RCP requires **cpcd** and **zigbeed** on the host:

| Component | Version | Source | Description |
|-----------|---------|--------|-------------|
| cpcd | v4.5.3 | [SiliconLabs/cpc-daemon](https://github.com/SiliconLabs/cpc-daemon) | CPC daemon |
| zigbeed | EmberZNet 8.2.2 | Simplicity SDK 2025.6.2 | Zigbee stack daemon (recommended) |
| zigbeed | EmberZNet 7.5.1 | Gecko SDK 4.5.0 | Zigbee stack daemon (legacy) |

See subdirectories for build instructions:
- `cpcd/` - CPC daemon (for host)
- `zigbeed-8.2.2/` - zigbeed EmberZNet 8.2.2 (recommended)
- `zigbeed-7.5.1/` - zigbeed EmberZNet 7.5.1 (legacy)
- `rcp-stack/` - Systemd service manager for the complete chain
- `cpcd-rtl8196e/` - Cross-compile cpcd for gateway (experimental)

## Zigbee2MQTT Configuration

With rcp-stack (recommended):
```yaml
serial:
  port: /tmp/ttyZ2M
  adapter: ember
```
## Build

### Prerequisites

- GSDK 4.5.0 (Gecko SDK)
- slc-cli (Silicon Labs Configurator)
- ARM GCC toolchain (12.x recommended)

### Build Commands

```bash
./build_rcp.sh
```

### Output File

| File | Use |
|------|-----|
| `firmware/rcp-uart-802154.gbl` | Flash via UART or J-Link |

## Flashing

Via J-Link/SWD:
```bash
commander flash firmware/rcp-uart-802154.gbl --device EFR32MG1B232F256GM48
```

Via TCP (universal-silabs-flasher through serialgateway):
```bash
universal-silabs-flasher --device socket://GATEWAY_IP:8888 flash --firmware firmware/rcp-uart-802154.gbl
```

## Hardware Pinout

| Signal | Pin | Function |
|--------|-----|----------|
| TX | PA0 | USART0 TX |
| RX | PA1 | USART0 RX |
| RTS | PA4 | Flow Control |
| CTS | PA5 | Flow Control |

**UART:** 115200 baud, 8N1, Hardware Flow Control (required)

## Features

- **RTL8196E Boot Delay:** 1-second delay for host UART initialization
- **Hardware Flow Control:** RTS/CTS required for reliable TCP operation
- **CPC Security Disabled:** Saves ~45KB flash (not needed for local network)

## Memory Usage

| Resource | Used | Available |
|----------|------|-----------|
| Flash | ~116 KB | 256 KB |
| RAM | ~22 KB | 32 KB |

## TCP Support

**TCP via serialgateway is supported.** The cpcd + zigbeed architecture works over TCP when
using the RTL8196E's serialgateway to bridge the UART to Ethernet.

Configuration that works:
- EFR32 RCP ↔ UART 115200 (HW flow control) ↔ RTL8196E serialgateway ↔ TCP:8888 ↔ cpcd ↔ zigbeed

**Key requirements for TCP stability:**
- Hardware flow control (RTS/CTS) must be enabled on both RCP and serialgateway
- cpcd configured for TCP client mode: `tcp_client_address: <gateway-ip>`, `tcp_client_port: 8888`
- CPC Protocol v5 (native GSDK 4.5.0)
- **Direct Ethernet cable** between host and gateway (recommended)

> **Important:** The CPC protocol is sensitive to network latency and packet loss.
> For best reliability, connect the gateway directly to the host with an Ethernet cable,
> avoiding WiFi bridges, congested switches, or multiple network hops.

## Baudrate Options

| Baudrate | Status | Notes |
|----------|--------|-------|
| **115200** | **Default** | Conservative, reliable |
| 230400 | Supported | Tested, works reliably |
| 460800+ | **Not supported** | Causes UART overruns |

### Network size considerations

| Liaison | Throughput |
|---------|------------|
| 802.15.4 radio | ~25 KB/s |
| UART 115200 | ~11 KB/s |
| UART 230400 | ~23 KB/s |

At 115200, the UART is ~2x slower than the Zigbee radio. Practical impact:

| Network size | 115200 | 230400 |
|--------------|--------|--------|
| < 50 devices | OK | OK |
| 50-100 devices | OK* | Recommended |
| > 100 devices | May bottleneck | Recommended |

*OK for normal use; possible latency during traffic spikes (OTA updates, large groups).

For most home installations, 115200 is sufficient.

### Why 460800 doesn't work

The RTL8196E UART has hardware limitations that prevent reliable operation above 230400 baud:

- **16-byte FIFO** (16550A standard) with RX trigger at 8 bytes
- At 460800 baud, the CPU has only **170 µs** to respond before overrun
- `serialgateway` runs in userspace, adding context switch latency
- Competing interrupts (Ethernet, timers) delay UART servicing

**Diagnostic:** Check UART errors on the gateway:
```bash
cat /proc/tty/driver/serial
# fe:xxx = framing errors, oe:xxx = overrun errors
```

To use 230400, modify the baudrate in:
1. `patches/sl_cpc_drv_uart_usart_vcom_config.h`
2. `patches/rcp-uart-802154.slcp`
3. Rebuild firmware and flash
4. Update `serialgateway -b 230400` on gateway
5. Update `cpcd.conf` with `uart_device_baud: 230400`

## Troubleshooting

### No response from RCP

1. Verify TCP connection: `nc -zv <gateway-ip> 8888`
2. Check baud rate matches (115200) on firmware and serialgateway
3. Verify hardware flow control is enabled

### cpcd connection issues

1. Check cpcd config points to gateway: `tcp_client_address: 192.168.1.xxx`
2. Verify CPC protocol version matches (v5 for GSDK 4.5.0)

## Project Structure

```
25-RCP-UART-HW/
├── build_rcp.sh                 # RCP firmware build script
├── README.md
├── patches/                     # RCP firmware patches
│   ├── rcp-uart-802154.slcp                 # Project config (based on SDK sample)
│   ├── main.c                               # Entry point (1s RTL8196E boot delay)
│   ├── sl_cpc_drv_uart_usart_vcom_config.h  # UART pins (PA0/PA1/PA4/PA5)
│   └── sl_cpc_security_config.h             # CPC security disabled
├── firmware/                    # Output (rcp-uart-802154.gbl)
├── cpcd/                        # CPC daemon build scripts (for host)
├── zigbeed-7.5.1/               # zigbeed EmberZNet 7.5.1 (legacy)
├── zigbeed-8.2.2/               # zigbeed EmberZNet 8.2.2 (recommended)
└── rcp-stack/                   # Systemd --user service manager
    ├── bin/rcp-stack            # Main script (up/down/status/doctor)
    ├── scripts/                 # Helper scripts
    ├── systemd/user/            # Service unit files
    └── examples/                # Config file examples
```

## References

- [CPC Daemon](https://github.com/SiliconLabs/cpc-daemon)
- [AN1333: Multiprotocol RCP](https://www.silabs.com/documents/public/application-notes/an1333-concurrent-protocols-with-802-15-4-rcp.pdf)
- [serialgateway](../../3-Main-SoC-Realtek-RTL8196E/34-Userdata/serialgateway/)

## License

Educational and personal use. Silicon Labs SDK components under their respective licenses.
