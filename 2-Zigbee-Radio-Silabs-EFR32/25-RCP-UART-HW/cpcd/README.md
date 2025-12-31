# cpcd - CPC Daemon

Build script for cpcd (Co-Processor Communication Daemon) from **Silicon Labs GitHub**.

cpcd provides the CPC transport layer between the Linux host and the RCP firmware.
It also provides **libcpc**, the library required by zigbeed.

## Prerequisites

```bash
sudo apt install cmake gcc g++ libmbedtls-dev
```

Note: `libmbedtls-dev` is optional (for CPC security/encryption).

## Build

```bash
./build_cpcd.sh              # Build cpcd and libcpc
./build_cpcd.sh install      # Build and install to /usr/local
./build_cpcd.sh clean        # Clean build directory
```

## Output

```
bin/
├── cpcd              # CPC daemon binary (~273 KB)
├── cpcd.conf         # Default configuration
├── libcpc.so         # Symlink → libcpc.so.3
├── libcpc.so.3       # Symlink → libcpc.so.4.5.2.0
├── libcpc.so.4.5.2.0 # CPC library (~94 KB)
└── include/          # Header files
    └── sl_cpc.h
```

## Installation

### Option 1: Using install target (recommended)

```bash
./build_cpcd.sh install
```

This runs `sudo make install` and `sudo ldconfig`.

### Option 2: Manual installation

```bash
# Binary
sudo cp bin/cpcd /usr/local/bin/

# Library
sudo cp bin/libcpc.so* /usr/local/lib/
sudo ldconfig

# Headers (for zigbeed build)
sudo cp bin/include/*.h /usr/local/include/

# Configuration
sudo cp bin/cpcd.conf /usr/local/etc/
```

## Configuration

Edit `/usr/local/etc/cpcd.conf` for your setup:

### TCP Connection (via serialgateway)

```yaml
# Connect to Lidl Gateway via TCP
socket_file_path: /var/run/cpcd/cpcd.sock
tcp_server_port: 8888
tcp_client_address: 192.168.1.xxx  # Gateway IP
tcp_client_port: 8888
```

### Direct Serial Connection (USB adapter)

```yaml
socket_file_path: /var/run/cpcd/cpcd.sock
uart_device_file: /dev/ttyUSB0
uart_device_baud: 460800
uart_hardflow: true
```

## Usage

```bash
# Start cpcd
cpcd -c /usr/local/etc/cpcd.conf

# Or with systemd
sudo systemctl start cpcd
```

## Version

- **cpcd v4.5.2** - Compatible with CPC Protocol v6
- Source: https://github.com/SiliconLabs/cpc-daemon

## Architecture

```
┌───────────────┐
│   zigbeed     │
│ (Zigbee stack)│
└───────┬───────┘
        │ libcpc (Unix socket)
        ▼
┌───────────────┐
│     cpcd      │
│  (CPC daemon) │
└───────┬───────┘
        │ TCP:8888 (or UART)
        ▼
┌───────────────┐
│   Gateway     │
│(serialgateway)│
└───────┬───────┘
        │ UART 460800
        ▼
┌───────────────┐
│  EFR32 RCP    │
│ (CPC v6)      │
└───────────────┘
```
