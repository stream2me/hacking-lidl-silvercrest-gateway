# Hacking the Lidl Silvercrest Gateway

> **If you find this project useful, please consider giving it a star!** It helps others discover it and motivates continued development.
>
> Questions? Use [Discussions](https://github.com/jnilo1/hacking-lidl-silvercrest-gateway/discussions). Found a bug? Open an [Issue](https://github.com/jnilo1/hacking-lidl-silvercrest-gateway/issues).

## What Is This?

The **Lidl Silvercrest Zigbee Gateway** is an inexpensive device sold as part of a closed ecosystem. Out of the box, it only works with the Tuya cloud and the Silvercrest app.

This project lets you **take full control** of this gateway:

- **Use it with Home Assistant or Zigbee2MQTT** — No cloud, no app, fully local
- **Modern system** — Linux 5.10 kernel with up-to-date tools
- **Secure** — No Tuya cloud connection, SSH access only

______________________________________________________________________

## Quick Start

### What You Need

- A Lidl Silvercrest Zigbee Gateway
- USB-to-serial adapter (3.3V UART)
- Ethernet connection

### Two Chips to Update

The gateway contains **two independent processors**:

| Chip | Role | What to flash |
|------|------|---------------|
| **Realtek RTL8196E** | Main Linux system | Kernel, rootfs, userdata |
| **Silabs EFR32MG1B** | Zigbee radio | NCP firmware |

### 1. Flash the Linux System (RTL8196E)

```bash
git clone https://github.com/jnilo1/hacking-lidl-silvercrest-gateway.git
cd hacking-lidl-silvercrest-gateway/3-Main-SoC-Realtek-RTL8196E
./build_rtl8196e.sh
./flash_rtl8196e.sh
```

See [35-Migration](./3-Main-SoC-Realtek-RTL8196E/35-Migration/) for detailed instructions.

### 2. Update the Zigbee Firmware (EFR32MG1B)

The original firmware (6.5.0) is not compatible with the modern `ember` driver in Zigbee2MQTT. Update to NCP firmware **7.4+** for full compatibility.

See [2-Zigbee-Radio-Silabs-EFR32](./2-Zigbee-Radio-Silabs-EFR32/) for instructions.

### 3. Connect to Zigbee2MQTT

```yaml
serial:
  port: tcp://<GATEWAY_IP>:8888
```

______________________________________________________________________

## Repository Structure

### [0-Hardware](./0-Hardware/)

Hardware documentation: pinouts, debug interfaces, chip specifications.

### [1-Build-Environment](./1-Build-Environment/)

Build environment with Docker or native Ubuntu 22.04/WSL2. Includes all required toolchains.

### [2-Zigbee-Radio-Silabs-EFR32](./2-Zigbee-Radio-Silabs-EFR32/)

Zigbee coprocessor (Silabs EFR32MG1B):

- Backup and restore the original bootloader
- Flash NCP firmware for Zigbee2MQTT/ZHA

### [3-Main-SoC-Realtek-RTL8196E](./3-Main-SoC-Realtek-RTL8196E/)

Main Linux system (Realtek RTL8196E):

- Linux 5.10 kernel
- Root filesystem and user partition
- Flash scripts and migration guide

______________________________________________________________________

## Credits

This project builds upon the initial research by [Paul Banks](https://paulbanks.org/projects/lidl-zigbee/).

## License

MIT License — See [LICENSE](./LICENSE) for details.
