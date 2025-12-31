#!/bin/bash
# build_zigbeed.sh - Build zigbeed from Simplicity SDK 2025.6.2
#
# Zigbeed is the Zigbee stack daemon that runs on Linux and communicates
# with the 802.15.4 RCP via CPC protocol v6.
#
# Prerequisites:
#   - slc-cli in PATH
#   - Native GCC toolchain (gcc, g++)
#   - Simplicity SDK 2025.6.2
#
# Usage:
#   ./build_zigbeed.sh              # Build for current architecture
#   ./build_zigbeed.sh arm64        # Cross-compile for ARM64
#   ./build_zigbeed.sh arm32        # Cross-compile for ARM32
#   ./build_zigbeed.sh clean        # Clean build directory
#
# Output:
#   bin/zigbeed                     # The zigbeed daemon binary
#
# J. Nilo - December 2025

set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
BUILD_DIR="${SCRIPT_DIR}/build"
OUTPUT_DIR="${SCRIPT_DIR}/bin"

# Project root (25-RCP-UART-HW -> 2-Zigbee-Radio-Silabs-EFR32 -> project root)
PROJECT_ROOT="$(cd "${SCRIPT_DIR}/../../.." && pwd)"
SILABS_TOOLS_DIR="${PROJECT_ROOT}/silabs-tools"

# SDK path - Simplicity SDK 2025.6.2 (has CPC v6)
SIMPLICITY_SDK="${SILABS_TOOLS_DIR}/simplicity_sdk_2025.6.2"
ZIGBEED_SLCP="${SIMPLICITY_SDK}/protocol/zigbee/app/projects/zigbeed/zigbeed.slcp"

# Target architecture (default: auto-detect)
TARGET_ARCH="${1:-auto}"

# Handle clean command
if [ "${TARGET_ARCH}" = "clean" ]; then
    echo "Cleaning build directory..."
    rm -rf "${BUILD_DIR}"
    rm -rf "${OUTPUT_DIR}"
    echo "Done."
    exit 0
fi

echo "========================================="
echo "  Zigbeed Builder"
echo "  SDK: Simplicity SDK 2025.6.2"
echo "  CPC Protocol: v6"
echo "========================================="
echo ""

# =========================================
# Auto-detect silabs-tools
# =========================================
if [ -d "${SILABS_TOOLS_DIR}/slc_cli" ]; then
    export PATH="${SILABS_TOOLS_DIR}/slc_cli:$PATH"
fi

# =========================================
# Check prerequisites
# =========================================

# Check slc
if ! command -v slc >/dev/null 2>&1; then
    echo "ERROR: slc (Silicon Labs CLI) not found in PATH"
    echo ""
    echo "Install slc-cli from Silicon Labs or add it to PATH"
    exit 1
fi
SLC_VERSION=$(slc --version 2>/dev/null | head -1)
echo "slc: ${SLC_VERSION}"

# Check native GCC
if ! command -v gcc >/dev/null 2>&1; then
    echo "ERROR: gcc not found in PATH"
    exit 1
fi
echo "GCC: $(gcc --version | head -1)"

# Check libcpc (required for zigbeed)
if ! ldconfig -p | grep -q libcpc; then
    echo ""
    echo "WARNING: libcpc not found in system libraries"
    echo "         Build cpcd first: cd ../cpcd && ./build_cpcd.sh install"
    echo ""
fi

# Check SDK
if [ ! -f "${ZIGBEED_SLCP}" ]; then
    echo "ERROR: Simplicity SDK 2025.6.2 not found at: ${SIMPLICITY_SDK}"
    echo "       Expected zigbeed.slcp at: ${ZIGBEED_SLCP}"
    exit 1
fi
echo "SDK: ${SIMPLICITY_SDK}"

# Trust SDK signature (required for slc generate)
echo "  - Trusting SDK signature..."
slc signature trust --sdk "${SIMPLICITY_SDK}" >/dev/null 2>&1 || true

# =========================================
# Determine target architecture
# =========================================
if [ "${TARGET_ARCH}" = "auto" ]; then
    MACHINE=$(uname -m)
    case "${MACHINE}" in
        x86_64)
            TARGET_ARCH="x86_64"
            ARCH_COMPONENT="linux_arch_64"
            ZIGBEE_ARCH="zigbee_x86_64"
            ;;
        aarch64)
            TARGET_ARCH="arm64"
            ARCH_COMPONENT="linux_arch_64"
            ZIGBEE_ARCH="zigbee_arm64v8"
            ;;
        armv7l|armhf)
            TARGET_ARCH="arm32"
            ARCH_COMPONENT="linux_arch_32"
            ZIGBEE_ARCH="zigbee_arm32v7"
            ;;
        *)
            echo "ERROR: Unknown architecture: ${MACHINE}"
            exit 1
            ;;
    esac
elif [ "${TARGET_ARCH}" = "arm64" ]; then
    ARCH_COMPONENT="linux_arch_64"
    ZIGBEE_ARCH="zigbee_arm64v8"
elif [ "${TARGET_ARCH}" = "arm32" ]; then
    ARCH_COMPONENT="linux_arch_32"
    ZIGBEE_ARCH="zigbee_arm32v7"
elif [ "${TARGET_ARCH}" = "x86_64" ]; then
    ARCH_COMPONENT="linux_arch_64"
    ZIGBEE_ARCH="zigbee_x86_64"
else
    echo "ERROR: Unknown target architecture: ${TARGET_ARCH}"
    echo "       Supported: auto, x86_64, arm64, arm32"
    exit 1
fi

echo "Target: ${TARGET_ARCH} (${ARCH_COMPONENT}, ${ZIGBEE_ARCH})"
echo ""

# =========================================
# Prepare build directory
# =========================================
echo "[1/4] Preparing build directory..."
rm -rf "${BUILD_DIR}"
mkdir -p "${BUILD_DIR}"
cd "${BUILD_DIR}"

# =========================================
# Generate project with slc
# =========================================
echo ""
echo "[2/4] Generating project with slc..."

# Generate the project targeting Linux with specific architecture
slc generate "${ZIGBEED_SLCP}" \
    --sdk "${SIMPLICITY_SDK}" \
    --with "${ARCH_COMPONENT}" \
    --with "${ZIGBEE_ARCH}" \
    --with "linux" \
    -o makefile \
    -d "${BUILD_DIR}" \
    --force 2>&1 | tail -10

echo "  - Project generated"

# =========================================
# Configure build
# =========================================
echo ""
echo "[3/4] Configuring build..."

MAKEFILE="${BUILD_DIR}/zigbeed.Makefile"
if [ -f "${MAKEFILE}" ]; then
    echo "  - Makefile found"
else
    # Try to find the makefile
    MAKEFILE=$(find "${BUILD_DIR}" -name "*.Makefile" | head -1)
    if [ -z "${MAKEFILE}" ]; then
        echo "ERROR: No Makefile generated"
        exit 1
    fi
    echo "  - Using: ${MAKEFILE}"
fi

# =========================================
# Compile
# =========================================
echo ""
echo "[4/4] Compiling zigbeed..."

make -f "${MAKEFILE}" -j$(nproc)

# =========================================
# Copy output
# =========================================
echo ""
echo "Copying output files..."
mkdir -p "${OUTPUT_DIR}"

# Find the built binary
ZIGBEED_BIN=$(find "${BUILD_DIR}" -name "zigbeed" -type f -executable 2>/dev/null | head -1)
if [ -z "${ZIGBEED_BIN}" ]; then
    # Try common locations
    for loc in "build/debug/zigbeed" "build/release/zigbeed" "zigbeed"; do
        if [ -f "${BUILD_DIR}/${loc}" ]; then
            ZIGBEED_BIN="${BUILD_DIR}/${loc}"
            break
        fi
    done
fi

if [ -n "${ZIGBEED_BIN}" ] && [ -f "${ZIGBEED_BIN}" ]; then
    cp "${ZIGBEED_BIN}" "${OUTPUT_DIR}/zigbeed"
    chmod +x "${OUTPUT_DIR}/zigbeed"
    echo "  - Copied zigbeed to ${OUTPUT_DIR}/"
else
    echo "WARNING: Could not find zigbeed binary"
    echo "         Check build output for errors"
fi

# Copy default config
CONFIG_SRC="${SIMPLICITY_SDK}/app/multiprotocol/apps/zigbeed/usr/local/etc/zigbeed.conf"
if [ -f "${CONFIG_SRC}" ]; then
    cp "${CONFIG_SRC}" "${OUTPUT_DIR}/zigbeed.conf"
    echo "  - Copied zigbeed.conf"
fi

# =========================================
# Summary
# =========================================
echo ""
echo "========================================="
echo "  BUILD COMPLETE"
echo "========================================="
echo ""
echo "Target architecture: ${TARGET_ARCH}"
echo ""

if [ -f "${OUTPUT_DIR}/zigbeed" ]; then
    echo "Binary info:"
    file "${OUTPUT_DIR}/zigbeed"
    ls -lh "${OUTPUT_DIR}/zigbeed"
fi

echo ""
echo "Output files:"
ls -lh "${OUTPUT_DIR}/"
echo ""
echo "Installation:"
echo "  sudo cp bin/zigbeed /usr/local/bin/"
echo "  sudo cp bin/zigbeed.conf /usr/local/etc/"
echo ""
echo "Usage:"
echo "  zigbeed -h                    # Show help"
echo "  zigbeed -c /etc/zigbeed.conf  # Start with config file"
echo ""
