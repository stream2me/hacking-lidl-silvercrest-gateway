#!/bin/bash
# flash_bootloader.sh — Upload bootloader image via TFTP to device in boot mode.
# Uses bootloader-rtl8196e/out/boot.img (or wboot.img if present).

set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
OUT_DIR="${SCRIPT_DIR}/out"

IMAGE_DEFAULT="boot.img"

if [ -f "${OUT_DIR}/wboot.img" ]; then
    IMAGE_DEFAULT="wboot.img"
fi

echo -n "TFTP server IP (default 192.168.1.6): "
read -r TARGET_IP
TARGET_IP=${TARGET_IP:-192.168.1.6}

echo -n "Image to upload [boot.img/wboot.img] (default ${IMAGE_DEFAULT}): "
read -r IMAGE_NAME
IMAGE_NAME=${IMAGE_NAME:-${IMAGE_DEFAULT}}

IMAGE_PATH="${OUT_DIR}/${IMAGE_NAME}"

if [ ! -f "$IMAGE_PATH" ]; then
    echo "❌ Image not found: $IMAGE_PATH"
    exit 1
fi

cd "$OUT_DIR"
echo "📤 Uploading ${IMAGE_NAME} to ${TARGET_IP} via TFTP..."
if tftp -m binary "$TARGET_IP" -c put "$IMAGE_NAME"; then
    echo "✅ Upload completed"
    echo "ℹ️  Ensure the device is in bootloader/TFTP mode before running this."
else
    echo "❌ TFTP upload failed"
    exit 1
fi
cd "$SCRIPT_DIR"
