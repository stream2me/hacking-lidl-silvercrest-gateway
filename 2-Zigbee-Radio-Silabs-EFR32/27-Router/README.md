# Zigbee 3.0 Router Firmware for Lidl Silvercrest Gateway

Minimal Zigbee 3.0 Router (SoC) firmware for the EFR32MG1B232F256GM48 chip found in the Lidl Silvercrest Smart Home Gateway.

This firmware transforms the gateway into an autonomous Zigbee router that extends your mesh network coverage.

## Features

- **Zigbee 3.0 Router** - Full mesh routing capabilities
- **Auto-join** - Automatically joins open Zigbee networks via network steering
- **Child support** - Up to 16 sleepy end-devices as children
- **Source routing** - 50-entry route table for large networks
- **Minimal footprint** - ~183KB flash (70KB margin on 256KB chip)
- **NVM3 storage** - 36KB for network credentials and tokens

## Hardware

| Component | Specification |
|-----------|---------------|
| Zigbee SoC | EFR32MG1B232F256GM48 |
| Flash | 256KB |
| RAM | 32KB |
| Radio | 2.4GHz IEEE 802.15.4 |
| UART | PA0 (TX), PA1 (RX), PA4 (RTS), PA5 (CTS) @ 115200 baud |

## Building

### Prerequisites

Install Silicon Labs tools (see `1-Build-Environment/12-silabs-toolchain/`):

```bash
cd 1-Build-Environment/12-silabs-toolchain
./install_silabs.sh
```

This installs:
- `slc-cli` - Silicon Labs Configurator
- `arm-none-eabi-gcc` - ARM GCC toolchain
- `commander` - Simplicity Commander
- Gecko SDK with EmberZNet

### Build

```bash
cd 2-Zigbee-Radio-Silabs-EFR32/27-Router
./build_router.sh
```

### Output

```
firmware/
├── z3-router-7.5.1.gbl   # For UART/Xmodem flashing
├── z3-router-7.5.1.s37   # For J-Link/SWD flashing
├── z3-router-7.5.1.hex   # Intel HEX format
└── z3-router-7.5.1.bin   # Raw binary
```

### Clean

```bash
./build_router.sh clean
```

## Flashing

### Via UART (recommended)

Requires access to the gateway's serial port (see `silabs-flasher/` or use `universal-silabs-flasher`):

```bash
# Using universal-silabs-flasher
pip install universal-silabs-flasher

# Flash via network (if ser2net is running on gateway)
universal-silabs-flasher \
    --device tcp://192.168.1.X:8888 \
    --firmware firmware/z3-router-7.5.1.gbl \
    flash
```

### Via J-Link/SWD

If you have physical access to the SWD pads:

```bash
commander flash firmware/z3-router-7.5.1.s37 \
    --device EFR32MG1B232F256GM48
```

## Usage

### Joining a Network

1. **Enable permit join** on your Zigbee coordinator:
   - Zigbee2MQTT: Settings → "Permit join"
   - Home Assistant ZHA: "Add device"

2. **Power on** the gateway with the router firmware

3. **Wait** for the router to appear (usually 30-60 seconds)

The router will:
- Scan all Zigbee channels (11-26)
- Find networks with permit join enabled
- Join using the default install code
- Start routing mesh traffic

### Verification

After flashing and powering on, you should see the router join in Zigbee2MQTT logs:

```
info  Zigbee: allowing new devices to join.
info  Device '0x847127fffe422cfe' joined
info  Starting interview of '0x847127fffe422cfe'
info  Successfully interviewed '0x847127fffe422cfe', device has successfully been paired
warning  Device '0x847127fffe422cfe' with Zigbee model 'undefined' and manufacturer name
         'undefined' is NOT supported, please follow https://www.zigbee2mqtt.io/...
```

The device will appear in Z2M with these properties:

![Router in Zigbee2MQTT](images/z2m-router-joined.png)

| Property | Value |
|----------|-------|
| Device type | Router |
| Support status | Not supported |
| Interview state | Successful |
| Power | Mains |

### About the "Not Supported" Warning

The **"Not supported"** warning in Zigbee2MQTT is **expected and normal** for this device.

This happens because:
1. The router firmware only implements the **Basic cluster** (mandatory minimum)
2. It has no manufacturer name or model identifier configured
3. Z2M has no device definition file for it

**This does NOT affect functionality.** The router still:
- Routes mesh traffic between devices
- Extends network coverage
- Supports end-device children
- Appears in the network map

The warning simply means Z2M cannot expose any controllable features (switches, sensors, etc.) because a pure router has none to expose. You can safely ignore this warning.

### Verify Routing

Check routing is working:
- Other devices should show routes through this router
- Network map shows the router with connections to neighbors

## Technical Details

### ZCL Configuration

| Endpoint | Profile | Clusters |
|----------|---------|----------|
| 1 | Home Automation (0x0104) | Basic (server) |

**Basic Cluster Attributes:**
- ZCL Version: 0x08
- Power Source: 0x01 (Mains)
- Cluster Revision: 3

### Network Parameters

| Parameter | Value |
|-----------|-------|
| Device Type | Router |
| Security | Zigbee 3.0 |
| Max Children | 16 |
| Packet Buffers | 64 |
| Neighbor Table | 16 |
| Source Route Table | 50 |
| Binding Table | 10 |
| Key Table | 4 |

### Memory Layout

```
Flash (256KB):
├── Application     ~183KB
├── NVM3 Storage     36KB
└── Free            ~37KB

RAM (32KB):
├── Stack + Heap    ~16KB
└── Application     ~16KB
```

## Removed Features (Flash Savings)

The following components were excluded to minimize flash usage:

| Component | Savings | Reason |
|-----------|---------|--------|
| CLI | ~28KB | No serial console needed |
| Debug Print | ~10KB | No debug output |
| Green Power | ~50KB | Not used |
| Zigbee Light Link | ~40KB | Not a lighting device |
| Identify Cluster | ~4KB | No LED for feedback |
| Find-and-Bind | ~8KB | Router doesn't initiate bindings |

### Impact of Removed Features

- **No Identify**: The "Identify" button in Z2M/ZHA won't trigger any visual feedback (the gateway has no accessible LED anyway)
- **No Find-and-Bind**: Cannot do direct device-to-device binding (not needed for a pure router)
- **No CLI**: Cannot interact via serial commands (reduces attack surface)

All core routing functionality is preserved.

## Files

```
27-Router/
├── build_router.sh                    # Build script
├── README.md                          # This file
├── firmware/                          # Output directory
├── images/
│   └── z2m-router-joined.png          # Z2M screenshot
└── patches/
    ├── z3-router.slcp                 # Project configuration
    ├── main.c                         # Entry point + RTL8196E delay
    ├── app.c                          # Application callbacks
    ├── zap-config.h                   # ZCL endpoint configuration
    ├── zap-*.h                        # ZCL type definitions
    ├── sl_iostream_usart_vcom_config.h  # UART pin mapping
    └── sl_rail_util_pti_config.h      # PTI disabled
```

## Boot Sequence

```
Power On
    │
    ▼
1-second delay (RTL8196E boot sync)
    │
    ▼
Silicon Labs system init
    │
    ▼
Zigbee stack init
    │
    ▼
emberAfMainInitCallback()
    │
    ▼
Stack status: NETWORK_DOWN
    │
    ▼
startNetworkSteering()
    │
    ▼
Scan channels, find open network
    │
    ▼
Join network
    │
    ▼
Stack status: NETWORK_UP
    │
    ▼
[Router active - routing mesh traffic]
```

## Troubleshooting

### Router doesn't join

1. **Check permit join** is enabled on coordinator
2. **Verify channel** - Some coordinators only use specific channels
3. **Check distance** - Move closer to coordinator for initial join
4. **Reset NVM** - Flash firmware again to clear stored network data

### Router joins but doesn't route

1. **Wait** - Route discovery takes time (minutes)
2. **Check coordinator** - Some need "interview" to complete
3. **Verify in network map** - Router should show connections

### Build fails with "zap-config.h not found"

The ZAP files are pre-generated in `patches/`. Ensure they're copied:
```bash
ls patches/zap-*.h
```

## Related Projects

- `24-NCP-UART-HW/` - NCP firmware (host-controlled via EZSP)
- `25-RCP-UART-HW/` - RCP firmware (for OpenThread/zigbeed)

## License

This project uses Silicon Labs Gecko SDK which is subject to the Silicon Labs Master Software License Agreement.

## References

- [Silicon Labs Zigbee Documentation](https://docs.silabs.com/zigbee/latest/)
- [EmberZNet API Reference](https://docs.silabs.com/zigbee/latest/af-api/)
- [EFR32MG1 Datasheet](https://www.silabs.com/documents/public/data-sheets/efr32mg1-datasheet.pdf)
