# OpenThread RCP Firmware with zigbee-on-host

OpenThread Radio Co-Processor (RCP) firmware for the EFR32MG1B232F256GM48.
For **Zigbee** (via zigbee-on-host) or **Thread/Matter** networks.

## zigbee-on-host

This firmware works with [**zigbee-on-host**](https://github.com/Nerivec/zigbee-on-host),
an amazing open-source Zigbee stack developed by [@Nerivec](https://github.com/Nerivec).

Unlike proprietary solutions like Silicon Labs' zigbeed, zigbee-on-host is:
- **Fully open-source** - transparent, auditable, community-driven
- **Integrated in Zigbee2MQTT 2.x** as the `zoh` adapter
- **Actively developed** - contributions welcome!

> **Note:** zigbee-on-host is still under active development. Check the
> [GitHub repository](https://github.com/Nerivec/zigbee-on-host) for the latest
> updates, known issues, and to report bugs or contribute.

## Architecture

```
+-------------------+    UART     +-------------------+   Ethernet   +---------------------+
|  EFR32MG1B (RCP)  |   115200    |  RTL8196E         |    TCP/IP    |  Host (x86/ARM)     |
|                   |    baud     |  (Gateway SoC)    |              |                     |
|  802.15.4 PHY/MAC |<----------->|                   |<------------>|  Zigbee2MQTT        |
|  + Spinel/HDLC    |   ttyS0     |  serialgateway    |   port 8888  |    + zoh adapter    |
|                   |     <->     |  (serial->TCP)    |              |                     |
|  OpenThread 2.4.7 |   ttyS1     |                   |              |  zigbee-on-host     |
|  HW Flow Control  |             |                   |              |  (Zigbee stack)     |
+-------------------+             +-------------------+              +---------------------+
```

The RTL8196E runs `serialgateway` to bridge the EFR32's UART to TCP port 8888.
See [34-Userdata](../../3-Main-SoC-Realtek-RTL8196E/34-Userdata/) for gateway setup.

## Zigbee2MQTT Configuration

Edit `configuration.yaml`:

```yaml
serial:
  port: tcp://192.168.1.xxx:8888
  adapter: zoh
  baudrate: 115200
```

**Tested devices:** Xiaomi LYWSD03MMC (temperature/humidity sensor)

## Build

### Prerequisites

- GSDK 4.5.0 (Gecko SDK)
- slc-cli (Silicon Labs Configurator)
- ARM GCC toolchain (12.x recommended)

### Build Commands

```bash
./build_ot_rcp.sh
```

### Output Files

| File | Use |
|------|-----|
| `firmware/ot-rcp.gbl` | Flash via UART/Xmodem |
| `firmware/ot-rcp.s37` | Flash via J-Link/SWD |

## Flashing

Via J-Link/SWD (tested):

```bash
commander flash firmware/ot-rcp.gbl --device EFR32MG1B232F256GM48
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
- **Hardware Radio Acceleration:** All MAC operations in hardware

## Memory Usage

| Resource | Used | Available |
|----------|------|-----------|
| Flash | ~100 KB | 256 KB |
| RAM | ~16 KB | 32 KB |

## Troubleshooting

### No response from RCP

1. Verify TCP connection: `nc -zv <gateway-ip> 8888`
2. Check baud rate matches (115200) on firmware and serialgateway
3. Verify hardware flow control is enabled

### HDLC Parsing Errors

1. Ensure baud rate is 115200 (not higher)
2. Check for Zigbee device flooding (remove battery from problematic devices)
3. Verify hardware flow control is enabled

### Device Won't Pair

1. Factory reset the device (hold button while inserting battery)
2. Ensure permit join is enabled in Z2M

## Project Structure

```
26-OT-RCP/
├── build_ot_rcp.sh              # Build script
├── README.md
├── patches/
│   ├── ot-rcp.slcp              # Project config (based on SDK sample)
│   ├── main.c                   # Entry point (RTL8196E boot delay)
│   └── sl_uartdrv_usart_vcom_config.h
└── firmware/                    # Output (generated)
```

## References

- [zigbee-on-host](https://github.com/Nerivec/zigbee-on-host) - Open-source Zigbee stack by Nerivec
- [Zigbee2MQTT](https://www.zigbee2mqtt.io/)
- [OpenThread RCP](https://openthread.io/platforms/co-processor)

## License

Educational and personal use. Silicon Labs SDK components under their respective licenses.
