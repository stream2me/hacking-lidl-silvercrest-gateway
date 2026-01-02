#!/bin/bash
# flash_bootloader.sh — Upload bootloader via TFTP to device in recovery mode
#
# Usage: ./flash_bootloader.sh [IP] [IMAGE]
#   IP     - Target IP (default: 192.168.1.6)
#   IMAGE  - Image file in out/ (default: boot.bin)

set -e

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
tftp -m binary "$TARGET_IP" -c put "$IMAGE_PATH"
echo "Done"
