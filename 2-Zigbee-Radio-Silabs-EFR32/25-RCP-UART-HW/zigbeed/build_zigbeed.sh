#!/bin/bash
# build_zigbeed.sh - Build zigbeed from Gecko SDK 4.5.0
#
# Zigbeed is the Zigbee stack daemon that runs on Linux and communicates
# with the 802.15.4 RCP via CPC protocol.
#
# Prerequisites:
#   - slc-cli in PATH
#   - Native GCC toolchain (gcc, g++)
#   - Gecko SDK 4.5.0
#   - libcpc installed (build cpcd first)
#
# Usage:
#   ./build_zigbeed.sh              # Build for current architecture
#   ./build_zigbeed.sh clean        # Clean build directory
#
# Output:
#   bin/zigbeed                     # The zigbeed daemon binary
#
# J. Nilo - January 2026

set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
BUILD_DIR="${SCRIPT_DIR}/build"
OUTPUT_DIR="${SCRIPT_DIR}/bin"

# Project root (25-RCP-UART-HW -> 2-Zigbee-Radio-Silabs-EFR32 -> project root)
PROJECT_ROOT="$(cd "${SCRIPT_DIR}/../../.." && pwd)"
SILABS_TOOLS_DIR="${PROJECT_ROOT}/silabs-tools"

# SDK path - Gecko SDK 4.5.0 (CPC v5)
GECKO_SDK="${SILABS_TOOLS_DIR}/gecko_sdk"
ZIGBEED_SAMPLE_DIR="${GECKO_SDK}/protocol/zigbee/app/zigbeed"
ZIGBEED_SLCP="${ZIGBEED_SAMPLE_DIR}/zigbeed.slcp"

# Handle clean command
if [ "${1:-}" = "clean" ]; then
    echo "Cleaning build directory..."
    rm -rf "${BUILD_DIR}"
    rm -rf "${OUTPUT_DIR}"
    echo "Done."
    exit 0
fi

echo "========================================="
echo "  Zigbeed Builder"
echo "  SDK: Gecko SDK 4.5.0"
echo "  CPC Protocol: v5"
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
    echo "ERROR: Gecko SDK 4.5.0 not found"
    echo "       Expected: ${ZIGBEED_SLCP}"
    exit 1
fi
echo "SDK: ${GECKO_SDK}"

# Trust SDK signature (required for slc generate)
echo "  - Trusting SDK signature..."
slc signature trust --sdk "${GECKO_SDK}" >/dev/null 2>&1 || true

# =========================================
# Determine target architecture
# =========================================
MACHINE=$(uname -m)
case "${MACHINE}" in
    x86_64)
        TARGET_ARCH="x86_64"
        ARCH_COMPONENTS="linux_arch_64,zigbee_x86_64"
        ;;
    aarch64)
        TARGET_ARCH="arm64"
        ARCH_COMPONENTS="linux_arch_64,zigbee_arm64"
        ;;
    armv7l|armhf)
        TARGET_ARCH="arm32"
        ARCH_COMPONENTS="linux_arch_32,zigbee_arm32"
        ;;
    *)
        echo "ERROR: Unknown architecture: ${MACHINE}"
        exit 1
        ;;
esac

echo "Target: ${TARGET_ARCH}"
echo ""

# =========================================
# Prepare build directory
# =========================================
echo "[1/4] Preparing build directory..."
rm -rf "${BUILD_DIR}"
mkdir -p "${BUILD_DIR}"
cd "${BUILD_DIR}"

# Copy source files from SDK sample
cp "${ZIGBEED_SAMPLE_DIR}"/*.c .
cp "${ZIGBEED_SAMPLE_DIR}"/*.h .
cp "${ZIGBEED_SAMPLE_DIR}/zigbeed.slcp" .
echo "  - Copied sources from SDK sample"

# =========================================
# Generate project with slc
# =========================================
echo ""
echo "[2/4] Generating project with slc..."

# -cp: Copy necessary library files (required for cross-compilation)
# --with: Architecture components (linux_arch_XX + zigbee_archXX)
slc generate zigbeed.slcp \
    -cp \
    --sdk "${GECKO_SDK}" \
    --with "${ARCH_COMPONENTS}" \
    -o makefile \
    --force 2>&1 | tail -10

echo "  - Project generated"

# =========================================
# Configure build
# =========================================
echo ""
echo "[3/4] Configuring build..."

MAKEFILE="${BUILD_DIR}/zigbeed.Makefile"
if [ ! -f "${MAKEFILE}" ]; then
    MAKEFILE=$(find "${BUILD_DIR}" -name "*.Makefile" | head -1)
    if [ -z "${MAKEFILE}" ]; then
        echo "ERROR: No Makefile generated"
        exit 1
    fi
fi
echo "  - Using: $(basename ${MAKEFILE})"

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

# =========================================
# Summary
# =========================================
echo ""
echo "========================================="
echo "  BUILD COMPLETE"
echo "========================================="
echo ""
echo "CPC Protocol Version: 5 (GSDK 4.5.0)"
echo "Target architecture: ${TARGET_ARCH}"
echo ""

if [ -f "${OUTPUT_DIR}/zigbeed" ]; then
    echo "Binary info:"
    file "${OUTPUT_DIR}/zigbeed"
    ls -lh "${OUTPUT_DIR}/zigbeed"
fi

echo ""
echo "Installation:"
echo "  sudo cp bin/zigbeed /usr/local/bin/"
echo ""
echo "Usage:"
echo "  zigbeed -h                    # Show help"
echo "  zigbeed -c /etc/zigbeed.conf  # Start with config file"
echo ""
