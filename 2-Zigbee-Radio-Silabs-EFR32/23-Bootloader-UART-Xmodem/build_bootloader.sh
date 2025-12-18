#!/bin/bash
# build_bootloader.sh — Build Bootloader-UART-Xmodem for EFR32MG1B232F256GM48
#
# Works both in Docker container and native Ubuntu 22.04 / WSL2.
#
# Prerequisites:
#   - slc (Silicon Labs CLI) in PATH
#   - arm-none-eabi-gcc in PATH
#   - GECKO_SDK environment variable set
#
# Usage:
#   ./build_bootloader.sh           # Build bootloader
#   ./build_bootloader.sh clean     # Clean build directory
#
# Output:
#   firmware/bootloader-uart-xmodem-X.Y.Z-crc.s37  (with CRC, for serial gateway)
#
# J. Nilo - December 2025

set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
BUILD_DIR="${SCRIPT_DIR}/build"
OUTPUT_DIR="${SCRIPT_DIR}/firmware"
PATCHES_DIR="${SCRIPT_DIR}/patches"

# Target chip
TARGET_DEVICE="EFR32MG1B232F256GM48"

# Handle clean command
if [ "${1:-}" = "clean" ]; then
    echo "Cleaning build directory..."
    rm -rf "${BUILD_DIR}"
    echo "Done."
    exit 0
fi

echo "========================================="
echo "  Bootloader-UART-Xmodem Builder"
echo "  Target: ${TARGET_DEVICE}"
echo "========================================="
echo ""

# =========================================
# Check prerequisites
# =========================================

# Check slc
if ! command -v slc >/dev/null 2>&1; then
    echo "slc (Silicon Labs CLI) not found in PATH"
    echo ""
    echo "Setup options:"
    echo "  1. Use Docker: docker run -it --rm -v \$(pwd):/workspace lidl-gateway-builder"
    echo "  2. Native: See 1-Build-Environment/README.md for setup instructions"
    exit 1
fi
echo "slc: $(which slc)"

# Check ARM GCC
if ! command -v arm-none-eabi-gcc >/dev/null 2>&1; then
    echo "arm-none-eabi-gcc not found in PATH"
    exit 1
fi
echo "ARM GCC: $(arm-none-eabi-gcc --version | head -1)"

# Check GECKO_SDK
if [ -z "${GECKO_SDK:-}" ]; then
    # Try common locations
    if [ -d "/home/builder/gecko_sdk" ]; then
        export GECKO_SDK="/home/builder/gecko_sdk"
    elif [ -d "$HOME/gecko_sdk" ]; then
        export GECKO_SDK="$HOME/gecko_sdk"
    elif [ -d "$HOME/SimplicityStudio/SDKs/gecko_sdk" ]; then
        export GECKO_SDK="$HOME/SimplicityStudio/SDKs/gecko_sdk"
    elif [ -d "$HOME/SimplicityStudio/SDKs/gecko-sdk" ]; then
        export GECKO_SDK="$HOME/SimplicityStudio/SDKs/gecko-sdk"
    else
        echo "GECKO_SDK environment variable not set"
        echo ""
        echo "Set it to your Gecko SDK installation path:"
        echo "  export GECKO_SDK=/path/to/gecko_sdk"
        exit 1
    fi
fi

if [ ! -d "${GECKO_SDK}/platform/bootloader" ]; then
    echo "Gecko SDK bootloader not found: ${GECKO_SDK}/platform/bootloader"
    exit 1
fi
echo "Gecko SDK: ${GECKO_SDK}"

# =========================================
# Extract SDK and Bootloader versions
# =========================================
SDK_VERSION_FILE="${GECKO_SDK}/version.txt"
if [ -f "${SDK_VERSION_FILE}" ]; then
    SDK_VERSION=$(cat "${SDK_VERSION_FILE}" | head -1)
    echo "SDK Version: ${SDK_VERSION}"
else
    SDK_VERSION="unknown"
fi

# Extract bootloader version from SDK
BTL_CONFIG="${GECKO_SDK}/platform/bootloader/config/btl_config.h"
if [ -f "${BTL_CONFIG}" ]; then
    BTL_MAJOR=$(grep "BOOTLOADER_VERSION_MAIN_MAJOR" "${BTL_CONFIG}" | head -1 | awk '{print $3}')
    BTL_MINOR=$(grep "BOOTLOADER_VERSION_MAIN_MINOR" "${BTL_CONFIG}" | head -1 | awk '{print $3}')
    BTL_CUSTOMER=$(grep "BOOTLOADER_VERSION_MAIN_CUSTOMER" "${BTL_CONFIG}" | grep -v ifndef | head -1 | awk '{print $3}')
    BTL_VERSION="${BTL_MAJOR}.${BTL_MINOR}.${BTL_CUSTOMER}"
    echo "Bootloader Version: ${BTL_VERSION}"
else
    BTL_VERSION="unknown"
fi
echo ""

# =========================================
# Prepare build directory
# =========================================
echo "[1/4] Preparing build directory..."
rm -rf "${BUILD_DIR}"
mkdir -p "${BUILD_DIR}"
cd "${BUILD_DIR}"

# Copy project files from patches
cp "${PATCHES_DIR}/bootloader-uart-xmodem.slcp" .
echo "  - Copied project files from patches"

# =========================================
# Generate project with slc
# =========================================
echo ""
echo "[2/4] Generating project with slc..."
slc generate bootloader-uart-xmodem.slcp --with ${TARGET_DEVICE} --force 2>&1 | tail -3

# =========================================
# Copy config files and patch Makefile
# =========================================
echo ""
echo "[3/4] Applying configuration..."
cp "${PATCHES_DIR}/btl_uart_driver_cfg.h" config/
cp "${PATCHES_DIR}/btl_gpio_activation_cfg.h" config/
echo "  - Copied UART and GPIO config from patches"

echo "  Patching Makefile..."
ARM_GCC_DIR=$(dirname $(dirname $(which arm-none-eabi-gcc)))
echo "  - Setting ARM_GCC_DIR to ${ARM_GCC_DIR}"
sed -i "s|^ARM_GCC_DIR_LINUX\s*=.*|ARM_GCC_DIR_LINUX = ${ARM_GCC_DIR}|" bootloader-uart-xmodem.Makefile

# Add -Oz optimization
if ! grep -q 'subst -Os,-Oz' bootloader-uart-xmodem.Makefile; then
    echo "  - Adding -Oz optimization to Makefile"
    sed -i '/-include bootloader-uart-xmodem.project.mak/a\
\
# Override optimization flags for maximum size reduction\
C_FLAGS := $(subst -Os,-Oz,$(C_FLAGS))\
CXX_FLAGS := $(subst -Os,-Oz,$(CXX_FLAGS))' bootloader-uart-xmodem.Makefile
fi

# =========================================
# Compile
# =========================================
echo ""
echo "[4/4] Compiling bootloader..."

# Set STUDIO_ADAPTER_PACK_PATH for post-build if commander is available
if command -v commander >/dev/null 2>&1; then
    COMMANDER_DIR=$(dirname $(which commander))
    export STUDIO_ADAPTER_PACK_PATH="${COMMANDER_DIR}"
    export POST_BUILD_EXE="${COMMANDER_DIR}/commander"
    echo "  Using commander for post-build: ${COMMANDER_DIR}"
fi

make -f bootloader-uart-xmodem.Makefile -j$(nproc)

# =========================================
# Generate output files with commander
# =========================================
echo ""
echo "Generating output files..."
mkdir -p "${OUTPUT_DIR}"

OUTPUT_NAME="bootloader-uart-xmodem-${BTL_VERSION}"
SRC_S37="build/debug/bootloader-uart-xmodem.s37"

if [ ! -f "${SRC_S37}" ]; then
    echo "  Error: No .s37 file found!"
    exit 1
fi

# Check if commander is available
if command -v commander >/dev/null 2>&1; then
    echo "  Using commander to generate files with CRC..."

    # Convert to signed bootloader with CRC
    commander convert "${SRC_S37}" --crc --outfile "${OUTPUT_DIR}/${OUTPUT_NAME}.s37"

    # Create .gbl file for OTA updates
    commander gbl create "${OUTPUT_DIR}/${OUTPUT_NAME}.gbl" \
        --bootloader "${OUTPUT_DIR}/${OUTPUT_NAME}.s37"

    echo "  - ${OUTPUT_NAME}.s37 (Stage 2 with CRC)"
    echo "  - ${OUTPUT_NAME}.gbl (Stage 2 for XMODEM upload)"

    # Copy first stage bootloader
    if [ -f "autogen/first_stage.s37" ]; then
        cp "autogen/first_stage.s37" "${OUTPUT_DIR}/first_stage.s37"
        echo "  - first_stage.s37 (Stage 1, requires J-Link)"
    fi
else
    # Fallback: copy raw files without CRC
    echo "  Warning: commander not found, copying raw files..."
    cp "${SRC_S37}" "${OUTPUT_DIR}/${OUTPUT_NAME}.s37"
    cp "build/debug/bootloader-uart-xmodem.hex" "${OUTPUT_DIR}/${OUTPUT_NAME}.hex"
    cp "build/debug/bootloader-uart-xmodem.bin" "${OUTPUT_DIR}/${OUTPUT_NAME}.bin"
    echo "  - ${OUTPUT_NAME}.s37 (without CRC)"
    echo "  Note: Install commander for .gbl generation"
fi

# =========================================
# Summary
# =========================================
echo ""
echo "========================================="
echo "  BUILD COMPLETE"
echo "========================================="
echo ""
echo "SDK Version: ${SDK_VERSION}"
echo "Bootloader Version: ${BTL_VERSION}"
echo ""
echo "Bootloader size:"
arm-none-eabi-size build/debug/bootloader-uart-xmodem.out
echo ""
echo "Output files:"
ls -lh "${OUTPUT_DIR}/${OUTPUT_NAME}".*
echo ""
echo "Flash command (via J-Link):"
echo "  commander flash firmware/${OUTPUT_NAME}.s37 --device ${TARGET_DEVICE}"
echo ""
