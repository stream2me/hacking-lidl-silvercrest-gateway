# zigbeed - EmberZNet 8.2.2 (EZSP 18)

Build script for zigbeed from Simplicity SDK 2025.6.2.
Portable: works on x86_64, ARM64 (Raspberry Pi 4/5), ARM32.

## Why This Version?

The Gecko SDK 4.5.0 produces zigbeed with EmberZNet 7.5.1 (EZSP 13).
The Simplicity SDK 2025.6.2 produces zigbeed with EmberZNet 8.2.2 (EZSP 18).

| Directory | SDK | EmberZNet | EZSP |
|-----------|-----|-----------|------|
| `../zigbeed/` | Gecko SDK 4.5.0 | 7.5.1 | 13 |
| `zigbeed-8.2.2/` | Simplicity SDK 2025.6.2 | 8.2.2 | 18 |

## Prerequisites

1. **slc-cli** installed in `silabs-tools/slc_cli` or in PATH (via `1-Build-Environment/`)
2. **cpcd** installed (provides libcpc)

```bash
# Install cpcd first
cd ../cpcd && ./build_cpcd.sh
```

> **Note:** The Simplicity SDK 2025.6.2 is downloaded automatically from GitHub on first build.

## Build and Install

```bash
./build_zigbeed.sh         # Build and install to /usr/local/bin
./build_zigbeed.sh clean   # Clean build directory
```

**Note**: Stop any running zigbeed before installing:
```bash
pkill zigbeed
# or via systemd
systemctl --user stop zigbeed.service
```

## Architecture Support

| Architecture | `uname -m` | SDK libs |
|--------------|------------|----------|
| PC 64-bit | x86_64 | x86-64 |
| Raspberry Pi 4/5 64-bit | aarch64 | arm64v8 |
| Raspberry Pi 32-bit | armv7l | arm32v7 |

The script auto-detects your architecture and patches the Makefile accordingly.

## How It Works

1. **Download SDK** from GitHub if not present (shallow clone, ~1.5 GB)
2. **Generate project** using `slc generate` from the SDK sample
3. **Replace SDK copy** with symlink (slc copies partial headers)
4. **Fix architecture** in library paths (slc defaults to arm64v8)
5. **Build** using the generated Makefile
6. **Install** to `/usr/local/bin/`

### Key differences from Gecko SDK build

- Sample location: `protocol/zigbee/app/projects/zigbeed/` (not `app/zigbeed/`)
- slc generates library references but with wrong architecture
- Partial SDK copy must be replaced with full symlink for headers

## Usage

```bash
# With CPC daemon (recommended)
zigbeed -r "spinel+cpc://cpcd_bringup?iid=1&iid-list=0" -p /tmp/ttyZigbeed

# Listen on TCP port
zigbeed -p 9999
```

## Zigbee2MQTT Configuration

```yaml
serial:
  port: tcp://localhost:9999
  # or with PTY
  port: /tmp/ttyZ2M
  adapter: ember
  baudrate: 115200
```

## Expected Z2M Log

```
[2026-01-10 19:49:06] info: zh:ember: ======== EZSP started ========
[2026-01-10 19:49:06] info: zh:ember: Adapter EZSP protocol version (18)
[2026-01-10 19:49:07] info: zh:ember: [STACK STATUS] Network up.
[2026-01-10 19:49:07] info: z2m: Coordinator firmware version: '{"meta":{"ezsp":18,"major":8,"minor":2,"patch":2}}'
```

## Troubleshooting

### "Text file busy" on install
Stop running zigbeed first:
```bash
pkill zigbeed
```

### Wrong architecture libraries
The script fixes this automatically. If you still see errors about `arm64v8` on x86_64, check that the sed commands ran correctly.

### Missing headers (sl_slist.h, etc.)
The SDK symlink must point to the full SDK, not a partial copy. The script handles this by removing the slc-generated partial copy and creating a symlink.
