# Zigbeed - Zigbee Host Daemon

Build script for zigbeed from **Gecko SDK 4.5.0**.

Zigbeed is the Zigbee stack daemon that:
- Runs on a Linux host (x86_64, ARM64, ARM32)
- Communicates with the RCP via CPC protocol v5
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

3. **Gecko SDK 4.5.0**:
   ```bash
   # Should be at silabs-tools/gecko_sdk/
   ```

4. **Native GCC toolchain**:
   ```bash
   sudo apt install build-essential
   ```

## Build

```bash
./build_zigbeed.sh         # Build for current architecture
./build_zigbeed.sh clean   # Clean build
```

The script automatically:
- Uses `-cp` option to copy necessary library files
- Adds architecture-specific components (`zigbee_x86_64`, etc.)
- Generates the project with slc
- Compiles with native GCC

## Output

```
bin/
└── zigbeed           # Daemon binary (~2.2 MB)
```

## Installation

```bash
sudo cp bin/zigbeed /usr/local/bin/
```

## Usage

```bash
# Show help
zigbeed -h

# Start with radio URL (CPC interface)
zigbeed -r 'spinel+cpc://cpcd_0?iid=2'
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
       │ UART 115200
       ▼
┌─────────────┐
│  EFR32 RCP  │
│(rcp-uart-802154)
└─────────────┘
```

## Supported Architectures

| Target | Components |
|--------|------------|
| x86_64 | `linux_arch_64,zigbee_x86_64` |
| ARM64 | `linux_arch_64,zigbee_arm64` |
| ARM32 | `linux_arch_32,zigbee_arm32` |
