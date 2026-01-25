#!/bin/bash
# build_zigbeed.sh - Build and install zigbeed from Simplicity SDK (EmberZNet 8.2.2)
#
# Portable script for x86_64, ARM64 (Raspberry Pi 4/5), ARM32.
# Automatically downloads Simplicity SDK 2025.6.2 if not present.
#
# Prerequisites:
#   - slc-cli in PATH (install via 1-Build-Environment/)
#   - libcpc installed (build cpcd first)
#
# Usage:
#   ./build_zigbeed.sh         # Build and install
#   ./build_zigbeed.sh clean   # Clean build directory
#
# J. Nilo - January 2026

set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
BUILD_DIR="${SCRIPT_DIR}/build"

# Project paths
PROJECT_ROOT="$(cd "${SCRIPT_DIR}/../../.." && pwd)"
SILABS_TOOLS="${PROJECT_ROOT}/silabs-tools"
SIMPLICITY_SDK="${SILABS_TOOLS}/simplicity_sdk_2025.6.2"
ZIGBEED_SAMPLE="${SIMPLICITY_SDK}/protocol/zigbee/app/projects/zigbeed"

# Handle clean
if [ "${1:-}" = "clean" ]; then
    echo "Cleaning..."
    rm -rf "${BUILD_DIR}"
    exit 0
fi

echo "========================================="
echo "  zigbeed builder (EmberZNet 8.2.2)"
echo "  Architecture: $(uname -m)"
echo "========================================="

# =========================================
# Check prerequisites
# =========================================

# SDK version configuration
SIMPLICITY_SDK_VERSION="2025.6.2"
SIMPLICITY_SDK_TAG="v${SIMPLICITY_SDK_VERSION}"

# Check/install SDK
if [ ! -d "${SIMPLICITY_SDK}/protocol/zigbee" ]; then
    echo "Simplicity SDK not found at ${SIMPLICITY_SDK}"
    echo "Downloading from GitHub (this may take a while)..."
    echo ""
    mkdir -p "${SILABS_TOOLS}"
    git clone --depth 1 --branch "${SIMPLICITY_SDK_TAG}" \
        https://github.com/SiliconLabs/simplicity_sdk.git \
        "${SIMPLICITY_SDK}" \
        || { echo "ERROR: Failed to clone Simplicity SDK"; exit 1; }
    echo "Simplicity SDK ${SIMPLICITY_SDK_VERSION} installed"
    echo ""
fi

# Check slc
if [ -d "${SILABS_TOOLS}/slc_cli" ]; then
    export PATH="${SILABS_TOOLS}/slc_cli:$PATH"
fi

if ! command -v slc >/dev/null 2>&1; then
    echo "ERROR: slc not found in PATH"
    exit 1
fi

# Check libcpc
if ! ldconfig -p 2>/dev/null | grep -q libcpc; then
    echo "WARNING: libcpc not found. Build cpcd first:"
    echo "  cd ../cpcd && ./build_cpcd.sh"
fi

# =========================================
# Detect architecture and set slc parameters
# =========================================
case "$(uname -m)" in
    x86_64)
        ARCH="x86-64"
        ZIGBEE_COMP="zigbee_x86_64"
        LINUX_ARCH="linux_arch_64"
        ;;
    aarch64)
        ARCH="arm64v8"
        ZIGBEE_COMP="zigbee_arm64"
        LINUX_ARCH="linux_arch_64"
        ;;
    armv7l)
        ARCH="arm32v7"
        ZIGBEE_COMP="zigbee_arm32"
        LINUX_ARCH="linux_arch_32"
        ;;
    *)
        echo "ERROR: Unsupported architecture: $(uname -m)"
        exit 1
        ;;
esac

echo "Target: ${ARCH} (${ZIGBEE_COMP}, ${LINUX_ARCH})"

# =========================================
# Generate project
# =========================================
echo ""
echo "[1/2] Generating project..."

rm -rf "${BUILD_DIR}"
mkdir -p "${BUILD_DIR}"
cd "${BUILD_DIR}"

# Copy sample sources
cp "${ZIGBEED_SAMPLE}"/*.c .
cp "${ZIGBEED_SAMPLE}"/*.h .
cp "${ZIGBEED_SAMPLE}/zigbeed.slcp" .

# Trust SDK
slc signature trust --sdk "${SIMPLICITY_SDK}" 2>/dev/null || true

# Generate with correct architecture components (like Nerivec)
# --with selects architecture-specific libraries automatically
# --without prevents slc from auto-selecting wrong architecture
slc generate zigbeed.slcp \
    --sdk "${SIMPLICITY_SDK}" \
    --with="${ZIGBEE_COMP},${LINUX_ARCH}" \
    --without=zigbee_recommended_linux_arch \
    -o makefile \
    --force 2>&1 | tail -5

# Replace partial SDK copy with symlink (slc copies some files, but not all headers)
rm -rf simplicity_sdk_2025.6.2
ln -s "${SIMPLICITY_SDK}" simplicity_sdk_2025.6.2

# =========================================
# Build
# =========================================
echo ""
echo "[2/2] Building..."

make -f zigbeed.Makefile -j$(nproc)

# =========================================
# Install
# =========================================
echo ""
echo "Installing to /usr/local/bin..."

sudo cp build/debug/zigbeed /usr/local/bin/
sudo chmod +x /usr/local/bin/zigbeed

echo ""
echo "========================================="
echo "  Done! (EmberZNet 8.2.2 / EZSP 18)"
echo "========================================="
echo ""
echo "Usage:"
echo "  zigbeed -p 9999    # Listen on TCP port 9999"
echo ""
echo "Zigbee2MQTT config:"
echo "  serial:"
echo "    port: tcp://localhost:9999"
echo "    adapter: ember"
