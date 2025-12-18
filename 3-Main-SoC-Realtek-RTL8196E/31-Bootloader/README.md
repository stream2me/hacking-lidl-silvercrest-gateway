# Realtek Bootloader

The RTL8196E gateway uses a proprietary Realtek bootloader stored in the first 128 KB of flash memory.

## Overview

| Property | Value |
|----------|-------|
| Location | `mtd0` (0x000000 - 0x020000) |
| Size | 128 KB |
| Serial | 38400 8N1 |
| Prompt | `<RealTek>` |
| TFTP server IP | 192.168.1.6 (default) |

## Entering the Bootloader

1. Connect serial adapter (38400 8N1)
2. Power on or reboot the gateway
3. Press **ESC** repeatedly until the `<RealTek>` prompt appears

```
---RealTek(RTL8196E)at 2020.08.05-10:15+0800 version v1.5 [16bit](390MHz)
...
<RealTek>
```

## Network Configuration

The bootloader runs a TFTP server for flashing. Default configuration:

| Parameter | Default |
|-----------|---------|
| IP address | 192.168.1.6 |
| Netmask | 255.255.255.0 |

Your PC must be on the same subnet to communicate with the bootloader.

## Image Format

The bootloader only accepts images with a valid **Realtek header**. Raw binary files will be rejected.

### Header Structure (16 bytes)

```c
typedef struct {
    unsigned char signature[4];   // Image type identifier
    unsigned long startAddr;      // RAM load address (big-endian)
    unsigned long burnAddr;       // Flash destination address (big-endian)
    unsigned long len;            // Payload length (big-endian)
} IMG_HEADER_T;
```

### Complete Image Layout

```
+------------------+
|  Header (16 B)   |  signature + startAddr + burnAddr + len
+------------------+
|                  |
|  Payload         |  Raw binary data (kernel, rootfs, etc.)
|  (len bytes)     |
|                  |
+------------------+
|  Checksum (2 B)  |  16-bit one's complement
+------------------+
|  Padding         |  Alignment to block size (optional)
+------------------+
```

### Known Signatures

The bootloader recognizes these 4-character signatures:

| Signature | Type | Description |
|-----------|------|-------------|
| `cs6c` | Kernel | Linux kernel for RTL8196E |
| `cr6c` | Firmware | Kernel + rootfs combined |
| `r6cr` | Rootfs/Data | SquashFS rootfs or JFFS2 userdata |
| `sqsh` | Rootfs | SquashFS filesystem (big-endian magic) |
| `hsqs` | Rootfs | SquashFS filesystem (little-endian magic) |
| `boot` | Bootloader | Bootloader image |

### Checksum Algorithm

The bootloader validates images using a **16-bit one's complement checksum**:

1. Pad payload to 2-byte alignment
2. Sum all 16-bit words (big-endian) of the payload
3. Append checksum = `~sum + 1` (two's complement of sum)
4. Verification: sum of all words including checksum must equal **0**

```c
uint16_t calculate_checksum(const uint8_t *buf, size_t len) {
    uint16_t sum = 0;
    for (size_t i = 0; i < len; i += 2) {
        sum += (buf[i] << 8) | buf[i + 1];  // Big-endian
    }
    return ~sum + 1;  // Two's complement
}
```

### Image Validation Process

When receiving a file via TFTP, the bootloader:

1. **Reads header** — Extracts signature, addresses, and length
2. **Validates signature** — Checks against known signatures table
3. **Verifies checksum** — Sums all 16-bit words of payload + checksum
4. **Rejects if invalid** — Displays `checksum error` message
5. **Flashes to burnAddr** — Writes to the address specified in header

### Creating Valid Images

Use the `cvimg` tool to create properly formatted images. Examples from build scripts:

```bash
# Kernel (from 32-Kernel/build_kernel.sh)
cvimg -i loader.bin -o kernel.img \
      -s cs6c \
      -e 0x80c00000 \
      -b 0x00020000 \
      -a 4k

# Rootfs (from 33-Rootfs/build_rootfs.sh)
cvimg -i rootfs.sqfs -o rootfs.bin \
      -s r6cr \
      -e 0x80c00000 \
      -b 0x200000

# Userdata (from 34-Userdata/build_userdata.sh)
cvimg -i userdata.jffs2 -o userdata.bin \
      -s r6cr \
      -e 0x80c00000 \
      -b 0x400000
```

| Option | Description |
|--------|-------------|
| `-i` | Input file (raw binary) |
| `-o` | Output file (with Realtek header) |
| `-s` | 4-char signature (`cs6c`, `r6cr`, etc.) |
| `-e` | Start/entry address in RAM |
| `-b` | Burn address in flash |
| `-a` | Output alignment (optional) |

See [1-Build-Environment/11-realtek-tools/cvimg](../../1-Build-Environment/11-realtek-tools/cvimg/) for the tool source.

## Commands Reference

### TFTP Operations

#### Receiving files (flash to device)

When you send a file via TFTP (`tftp put`), the bootloader automatically:
1. Receives the file into RAM
2. Detects the image type from its header
3. Flashes to the appropriate partition
4. Displays "Flash Write Successed!"

```bash
# From your PC
tftp -m binary 192.168.1.6 -c put kernel.img
tftp -m binary 192.168.1.6 -c put rootfs.bin
tftp -m binary 192.168.1.6 -c put userdata.bin
```

The bootloader identifies images by their Realtek header and writes them to the correct flash location.

#### Sending files (backup from device)

Use `FLR` to load flash content into RAM, then `tftp get` to download:

```
<RealTek>FLR <ram_addr> <flash_offset> <size>
```

Example — backup rootfs (mtd2):
```
<RealTek>FLR 80500000 00200000 00200000
```

Then from your PC:
```bash
tftp -m binary 192.168.1.6 -c get mtd2.bin
```

### Flash Operations

#### FLR — Flash Load to RAM

Load data from flash into RAM for TFTP download.

```
FLR <ram_address> <flash_offset> <length>
```

| Parameter | Description |
|-----------|-------------|
| ram_address | Destination in RAM (hex), typically `80500000` |
| flash_offset | Source offset in SPI flash (hex) |
| length | Number of bytes to read (hex) |

#### FLW — Flash Write from RAM

Write data from RAM to flash (used after TFTP upload with `LOADADDR`).

```
FLW <flash_offset> <ram_address> <length> <flags>
```

| Parameter | Description |
|-----------|-------------|
| flash_offset | Destination in SPI flash (hex) |
| ram_address | Source in RAM (hex) |
| length | Number of bytes to write (hex) |
| flags | Usually `0` |

#### LOADADDR — Set TFTP load address

Set the RAM address for incoming TFTP transfers:

```
<RealTek>LOADADDR 80500000
```

### Boot Commands

#### J — Jump to address

Start execution at a memory address:

```
<RealTek>J 80c00000
```

This boots the kernel loaded at address `0x80c00000`.

#### Automatic boot

If no ESC is pressed, the bootloader automatically loads and boots the kernel after a short delay.

## Flash Partitions

The bootloader manages these partitions:

| Partition | Offset | Size | Description |
|-----------|--------|------|-------------|
| mtd0 | 0x000000 | 0x020000 (128 KB) | Bootloader + config |
| mtd1 | 0x020000 | 0x1E0000 (1.9 MB) | Linux kernel |
| mtd2 | 0x200000 | 0x220000 (2.1 MB) | Root filesystem |
| mtd3 | 0x420000 | 0xBE0000 (11.9 MB) | User data (JFFS2) |

## Quick Reference

### Backup a partition

```
<RealTek>FLR 80500000 <offset> <size>
```
Then: `tftp -m binary 192.168.1.6 -c get backup.bin`

### Restore a partition

```
<RealTek>LOADADDR 80500000
```
Then: `tftp -m binary 192.168.1.6 -c put image.bin`
```
<RealTek>FLW <offset> 80500000 <size> 0
```

### Flash with auto-detection (recommended)

Simply send images with Realtek headers — the bootloader handles placement:

```bash
tftp -m binary 192.168.1.6 -c put rootfs.bin
tftp -m binary 192.168.1.6 -c put kernel.img  # triggers reboot
```

### Boot manually

```
<RealTek>J 80c00000
```

## Changing Bootloader IP

The bootloader IP can be modified in its configuration area. This is an advanced operation — see [30-Backup-Restore](../30-Backup-Restore/) for details on backing up mtd0 before making changes.

## Safety Notes

- **Never flash mtd0** unless you have a backup and SPI programmer
- The bootloader is the only way to recover a bricked device (without desoldering the flash chip)
- Always verify TFTP transfers completed successfully before rebooting
- Kernel flash triggers automatic reboot — flash rootfs/userdata first
