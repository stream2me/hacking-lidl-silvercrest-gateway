# Zigbeed - Zigbee Host Daemon

Build script for zigbeed from **Simplicity SDK 2025.6.2**.

Zigbeed is the Zigbee stack daemon that:
- Runs on a Linux host (x86_64, ARM64, ARM32)
- Communicates with the RCP via CPC protocol v6
- Exposes an EZSP interface on TCP port 9999 for Zigbee2MQTT/ZHA

## Prerequisites

1. **cpcd and libcpc** - Must be built and installed first:
   ```bash
   cd ../cpcd
   ./build_cpcd.sh install
   ```

2. **slc-cli** - Silicon Labs Configurator:
   ```bash
   # Should be at silabs-tools/slc_cli/ or in PATH
   ```

3. **Simplicity SDK 2025.6.2**:
   ```bash
   # Should be at silabs-tools/simplicity_sdk_2025.6.2/
   ```

4. **Native GCC toolchain**:
   ```bash
   sudo apt install build-essential
   ```

## Build

```bash
./build_zigbeed.sh              # Build for current architecture
./build_zigbeed.sh arm64        # Cross-compile for ARM64
./build_zigbeed.sh arm32        # Cross-compile for ARM32
./build_zigbeed.sh clean        # Clean build
```

The script automatically:
- Trusts the SDK signature
- Detects the target architecture
- Generates the project with slc
- Compiles with native GCC

## Output

```
bin/
├── zigbeed           # Daemon binary (~2.2 MB)
└── zigbeed.conf      # Default configuration
```

## Installation

```bash
sudo cp bin/zigbeed /usr/local/bin/
sudo cp bin/zigbeed.conf /usr/local/etc/
```

## Configuration

Edit `/usr/local/etc/zigbeed.conf`:

```ini
# CPC socket path (created by cpcd)
cpc_instance = cpcd_0

# EZSP TCP port for Zigbee2MQTT/ZHA
ezsp_port = 9999
```

## Usage

```bash
# Show help
zigbeed -h

# Start zigbeed
zigbeed -c /usr/local/etc/zigbeed.conf

# Or with systemd
sudo systemctl start zigbeed
```

## Architecture

```
┌─────────────┐     CPC      ┌─────────────┐    TCP:9999    ┌──────────────┐
│   cpcd      │◄────────────►│   zigbeed   │◄──────────────►│ Zigbee2MQTT  │
│ (CPC daemon)│   socket     │(Zigbee stack)│    EZSP       │   or ZHA     │
└─────────────┘              └─────────────┘                └──────────────┘
       ▲
       │ TCP:8888
       ▼
┌─────────────┐
│   Gateway   │
│(serialgateway)
└─────────────┘
       ▲
       │ UART 460800
       ▼
┌─────────────┐
│  EFR32 RCP  │
│(rcp-uart-802154)
└─────────────┘
```

## Supported Architectures

| Target | Component | Libraries |
|--------|-----------|-----------|
| x86_64 | `zigbee_x86_64` | `gcc/x86-64/` |
| ARM64 | `zigbee_arm64v8` | `gcc/arm64v8/` |
| ARM32 | `zigbee_arm32v7` | `gcc/arm32v7/` |
