#!/bin/bash
# build_cpcd.sh - Build cpcd (CPC Daemon) from Silicon Labs GitHub
#
# cpcd is required by zigbeed - it provides libcpc for CPC communication.
#
# Prerequisites:
#   - cmake, gcc, g++
#   - libmbedtls-dev (optional, for security)
#
# Usage:
#   ./build_cpcd.sh              # Build cpcd
#   ./build_cpcd.sh install      # Build and install to /usr/local
#   ./build_cpcd.sh clean        # Clean build directory
#
# Output:
#   build/cpcd/cpcd              # The cpcd daemon
#   build/cpcd/lib/libcpc.so     # CPC library (needed by zigbeed)
#
# J. Nilo - December 2025

set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
BUILD_DIR="${SCRIPT_DIR}/build"
CPCD_DIR="${BUILD_DIR}/cpc-daemon"
CPCD_BUILD="${CPCD_DIR}/build"
OUTPUT_DIR="${SCRIPT_DIR}/bin"

# cpcd version compatible with Simplicity SDK 2025.6.2
CPCD_REPO="https://github.com/SiliconLabs/cpc-daemon.git"
CPCD_VERSION="v4.5.2"  # Compatible with CPC protocol v6

# Handle arguments
ACTION="${1:-build}"

if [ "${ACTION}" = "clean" ]; then
    echo "Cleaning cpcd build..."
    rm -rf "${CPCD_DIR}"
    rm -rf "${OUTPUT_DIR}/cpcd"
    rm -rf "${OUTPUT_DIR}/libcpc"*
    echo "Done."
    exit 0
fi

echo "========================================="
echo "  cpcd Builder"
echo "  Version: ${CPCD_VERSION}"
echo "========================================="
echo ""

# =========================================
# Check prerequisites
# =========================================
echo "Checking prerequisites..."

if ! command -v cmake >/dev/null 2>&1; then
    echo "ERROR: cmake not found"
    echo "  Install with: sudo apt install cmake"
    exit 1
fi

if ! command -v gcc >/dev/null 2>&1; then
    echo "ERROR: gcc not found"
    exit 1
fi

echo "  cmake: $(cmake --version | head -1)"
echo "  gcc: $(gcc --version | head -1)"

# Check for optional mbedtls
if pkg-config --exists mbedtls 2>/dev/null; then
    echo "  mbedtls: found (security enabled)"
    SECURITY_OPT="-DENABLE_ENCRYPTION=ON"
else
    echo "  mbedtls: not found (security disabled)"
    SECURITY_OPT="-DENABLE_ENCRYPTION=OFF"
fi
echo ""

# =========================================
# Clone cpcd
# =========================================
echo "[1/3] Getting cpcd source..."

mkdir -p "${BUILD_DIR}"

if [ -d "${CPCD_DIR}" ]; then
    echo "  - Using existing source at ${CPCD_DIR}"
    cd "${CPCD_DIR}"
    git fetch --tags 2>/dev/null || true
else
    echo "  - Cloning from ${CPCD_REPO}"
    git clone "${CPCD_REPO}" "${CPCD_DIR}"
    cd "${CPCD_DIR}"
fi

echo "  - Checking out ${CPCD_VERSION}"
git checkout "${CPCD_VERSION}" 2>/dev/null || git checkout "tags/${CPCD_VERSION}"
echo ""

# =========================================
# Build cpcd
# =========================================
echo "[2/3] Building cpcd..."

mkdir -p "${CPCD_BUILD}"
cd "${CPCD_BUILD}"

cmake .. \
    -DCMAKE_BUILD_TYPE=Release \
    ${SECURITY_OPT} \
    -DCMAKE_INSTALL_PREFIX="${OUTPUT_DIR}/cpcd-install"

make -j$(nproc)
echo ""

# =========================================
# Install/Copy output
# =========================================
echo "[3/3] Copying output..."

mkdir -p "${OUTPUT_DIR}"

# Copy cpcd binary
if [ -f "${CPCD_BUILD}/cpcd" ]; then
    cp "${CPCD_BUILD}/cpcd" "${OUTPUT_DIR}/"
    echo "  - Copied cpcd"
fi

# Copy libcpc
LIBCPC=$(find "${CPCD_BUILD}" -name "libcpc.so*" | head -1)
if [ -n "${LIBCPC}" ]; then
    cp -P "${CPCD_BUILD}"/lib/libcpc.so* "${OUTPUT_DIR}/" 2>/dev/null || \
    cp -P "${CPCD_BUILD}"/libcpc.so* "${OUTPUT_DIR}/" 2>/dev/null || true
    echo "  - Copied libcpc"
fi

# Copy headers for zigbeed build
mkdir -p "${OUTPUT_DIR}/include"
cp "${CPCD_DIR}/lib/sl_cpc.h" "${OUTPUT_DIR}/include/" 2>/dev/null || true
cp "${CPCD_DIR}/lib/"*.h "${OUTPUT_DIR}/include/" 2>/dev/null || true
echo "  - Copied headers"

# Copy default config
if [ -f "${CPCD_DIR}/cpcd.conf" ]; then
    cp "${CPCD_DIR}/cpcd.conf" "${OUTPUT_DIR}/"
    echo "  - Copied cpcd.conf"
fi

# Install system-wide if requested
if [ "${ACTION}" = "install" ]; then
    echo ""
    echo "Installing to system..."
    cd "${CPCD_BUILD}"
    sudo make install
    sudo ldconfig
    echo "  - Installed to /usr/local"
fi

# =========================================
# Summary
# =========================================
echo ""
echo "========================================="
echo "  BUILD COMPLETE"
echo "========================================="
echo ""
echo "Output files:"
ls -lh "${OUTPUT_DIR}/"
echo ""

if [ "${ACTION}" != "install" ]; then
    echo "To install system-wide (required for zigbeed):"
    echo "  ./build_cpcd.sh install"
    echo ""
    echo "Or manually:"
    echo "  sudo cp bin/cpcd /usr/local/bin/"
    echo "  sudo cp bin/libcpc.so* /usr/local/lib/"
    echo "  sudo cp bin/include/*.h /usr/local/include/"
    echo "  sudo ldconfig"
fi
echo ""
echo "Configuration:"
echo "  Edit bin/cpcd.conf for your setup"
echo ""
