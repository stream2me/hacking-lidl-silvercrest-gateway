# RCP 802.15.4 Firmware with cpcd + zigbeed

Radio Co-Processor (RCP) firmware for the EFR32MG1B232F256GM48.
For **Zigbee** via CPC protocol with cpcd + zigbeed on Linux host.

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

| Component | Source | Description |
|-----------|--------|-------------|
| cpcd | [SiliconLabs/cpc-daemon](https://github.com/SiliconLabs/cpc-daemon) | CPC daemon |
| zigbeed | GSDK 4.5.0 | Zigbee stack daemon |

See `cpcd/README.md` and `zigbeed/README.md` for build instructions.

## Zigbee2MQTT Configuration

```yaml
serial:
  port: tcp://localhost:9999
  adapter: ember
```

zigbeed exposes a TCP socket on port 9999 for Zigbee2MQTT.

## Build

### Prerequisites

- GSDK 4.5.0 (Gecko SDK)
- slc-cli (Silicon Labs Configurator)
- ARM GCC toolchain (12.x recommended)

### Build Commands

```bash
./build_rcp.sh
```

### Output Files

| File | Use |
|------|-----|
| `firmware/rcp-uart-802154.gbl` | Flash via UART/Xmodem |
| `firmware/rcp-uart-802154.s37` | Flash via J-Link/SWD |

## Flashing

Via J-Link/SWD (tested):

```bash
commander flash firmware/rcp-uart-802154.gbl --device EFR32MG1B232F256GM48
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

## Known Limitations

**TCP/Network not supported:** The cpcd + zigbeed architecture requires a direct UART connection.
TCP bridges (via socat/serialgateway) introduce latency that causes spinel protocol timeouts.
For TCP-based access to the Lidl Gateway, use [26-OT-RCP](../26-OT-RCP/) instead.

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
├── build_rcp.sh                 # Build script
├── README.md
├── patches/
│   ├── rcp-uart-802154.slcp     # Project config (based on SDK sample)
│   ├── main.c                   # Entry point (RTL8196E boot delay)
│   ├── sl_cpc_drv_uart_usart_vcom_config.h  # UART pins
│   └── sl_cpc_security_config.h # Security disabled
├── firmware/                    # Output (generated)
├── cpcd/                        # CPC daemon build
└── zigbeed/                     # Zigbee daemon build
```

## References

- [CPC Daemon](https://github.com/SiliconLabs/cpc-daemon)
- [AN1333: Multiprotocol RCP](https://www.silabs.com/documents/public/application-notes/an1333-concurrent-protocols-with-802-15-4-rcp.pdf)
- [serialgateway](../../3-Main-SoC-Realtek-RTL8196E/34-Userdata/serialgateway/)

## License

Educational and personal use. Silicon Labs SDK components under their respective licenses.
