#!/bin/bash
# flash_bootloader.sh — Upload bootloader via TFTP to device in recovery mode
#
# Usage: ./flash_bootloader.sh [--force] [IP] [IMAGE]
#   --force - Override safety check (DANGEROUS)
#   IP      - Target IP (default: 192.168.1.6)
#   IMAGE   - Image file in out/ (default: boot.bin)

set -e

# =============================================================================
# WARNING: This bootloader is under development and DOES NOT WORK YET.
# Flashing it WILL BRICK your device and require hardware recovery (JTAG/SPI).
# =============================================================================
if [ "$1" != "--force" ]; then
    echo "========================================================"
    echo "  ERROR: This bootloader is NOT ready for use!"
    echo "========================================================"
    echo ""
    echo "  Flashing this bootloader WILL BRICK your device."
    echo "  You will need JTAG or SPI hardware to recover."
    echo ""
    echo "  This script is disabled for your protection."
    echo "  If you are a developer and know what you're doing,"
    echo "  use: $0 --force [IP] [IMAGE]"
    echo ""
    exit 1
fi
shift  # Remove --force from arguments

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
OUT_DIR="${SCRIPT_DIR}/out"

TARGET_IP="${1:-192.168.1.6}"
IMAGE_NAME="${2:-boot.bin}"
IMAGE_PATH="${OUT_DIR}/${IMAGE_NAME}"

if [ ! -f "$IMAGE_PATH" ]; then
    echo "Error: $IMAGE_PATH not found"
    echo "Run ./build_bootloader.sh first"
    exit 1
fi

echo "Flashing ${IMAGE_NAME} to ${TARGET_IP}..."
cd "$OUT_DIR"
tftp -m binary "$TARGET_IP" -c put "$IMAGE_NAME"
echo "Done"
