#!/bin/bash
# build_bootloader.sh — Build the RTL8196E bootloader from src/
#
# Original bootloader sources and build flow:
#   Copyright (C) Realtek Semiconductor Corp.
#
# Adapted and simplified for RTL8196E-only builds:
#   J. Nilo - November 2025
#
# Uses the Lexra/musl toolchain from the project x-tools directory.
# Usage:
#   ./build_bootloader.sh

set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
# Project root is 2 levels up: 31-Bootloader -> 3-Main-SoC -> project root
PROJECT_ROOT="$(cd "${SCRIPT_DIR}/../.." && pwd)"
BOOTLOADER_DIR="${SCRIPT_DIR}/src"
OUT_DIR="${SCRIPT_DIR}/out"

# Toolchain - auto-detect by walking up the repo tree
find_toolchain() {
    local dir="$SCRIPT_DIR"
    while [ "$dir" != "/" ]; do
        if [ -d "$dir/x-tools/mips-lexra-linux-musl/bin" ]; then
            echo "$dir/x-tools/mips-lexra-linux-musl"
            return 0
        fi
        dir="$(cd "$dir/.." && pwd)"
    done
    return 1
}

TOOLCHAIN_DIR="$(find_toolchain || true)"
CROSS_PREFIX="mips-lexra-linux-musl-"

if [ -n "$TOOLCHAIN_DIR" ]; then
    export PATH="${TOOLCHAIN_DIR}/bin:$PATH"
fi
export CROSS_COMPILE="${CROSS_PREFIX}"

# Realtek tools (lzma/cvimg) from build environment
REALTEK_TOOLS_DIR="${PROJECT_ROOT}/1-Build-Environment/11-realtek-tools/bin"
if [ -x "${REALTEK_TOOLS_DIR}/lzma" ]; then
    export LZMA_TOOL="${REALTEK_TOOLS_DIR}/lzma"
fi
if [ -x "${REALTEK_TOOLS_DIR}/cvimg" ]; then
    export CVIMG_TOOL="${REALTEK_TOOLS_DIR}/cvimg"
fi

echo "========================================="
echo "  BUILDING RTL8196E BOOTLOADER"
echo "========================================="
echo ""

# Checks
if [ ! -d "$BOOTLOADER_DIR" ]; then
    echo "Bootloader sources not found at $BOOTLOADER_DIR"
    exit 1
fi

if [ -z "$TOOLCHAIN_DIR" ]; then
    echo "Toolchain not found in parent directories (expected x-tools/mips-lexra-linux-musl)"
    echo ""
    echo "Build the toolchain first:"
    echo "  cd ${PROJECT_ROOT}/1-Build-Environment/10-lexra-toolchain"
    echo "  ./build_toolchain.sh"
    exit 1
fi

if ! command -v "${CROSS_PREFIX}gcc" >/dev/null 2>&1; then
    echo "Compiler not found: ${CROSS_PREFIX}gcc"
    exit 1
fi

echo "Toolchain: $TOOLCHAIN_DIR"
echo "Compiler:  $(${CROSS_PREFIX}gcc --version | head -1)"
echo ""

mkdir -p "$OUT_DIR"

pushd "$BOOTLOADER_DIR" >/dev/null

echo "Cleaning previous build..."
make -C boot CROSS="${CROSS_PREFIX}" clean >/dev/null 2>&1 || true
make -C btcode CROSS="${CROSS_PREFIX}" clean >/dev/null 2>&1 || true

echo "Building bootloader..."
make -C boot CROSS="${CROSS_PREFIX}" RTL865X=1 JUMP_ADDR=0x80500000 boot
make -C btcode CROSS="${CROSS_PREFIX}" RTL865X=1

echo ""
echo "Collecting artifacts..."
if [ -f boot/Output/boot.img ]; then
    cp -f boot/Output/boot.img "$OUT_DIR/boot.img"
fi
if [ -f boot/Output/boot.bin ]; then
    cp -f boot/Output/boot.bin "$OUT_DIR/boot.bin"
fi
if [ -f btcode/boot.bin ]; then
    cp -f btcode/boot.bin "$OUT_DIR/boot.bin"
fi
if [ -f boot/Output/wboot.img ]; then
    cp -f boot/Output/wboot.img "$OUT_DIR/wboot.img"
fi

popd >/dev/null

echo ""
echo "========================================="
echo "  BUILD SUMMARY"
echo "========================================="
ls -lh "$OUT_DIR"
echo ""
echo "Bootloader images ready in $OUT_DIR"
