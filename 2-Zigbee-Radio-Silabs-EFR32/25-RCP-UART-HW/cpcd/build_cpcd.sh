#!/bin/bash
# build_cpcd.sh - Build and install cpcd from Silicon Labs GitHub
#
# Portable script for x86_64, ARM64 (Raspberry Pi 4/5), etc.
#
# Prerequisites:
#   sudo apt install cmake gcc g++ libmbedtls-dev
#
# Usage:
#   ./build_cpcd.sh         # Clone, build and install
#   ./build_cpcd.sh clean   # Remove source directory
#
# J. Nilo - January 2026

set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
CPCD_SRC="${SCRIPT_DIR}/cpc-daemon"

CPCD_REPO="https://github.com/SiliconLabs/cpc-daemon.git"
CPCD_VERSION="v4.5.3"

if [ "${1:-}" = "clean" ]; then
    echo "Cleaning..."
    rm -rf "${CPCD_SRC}"
    exit 0
fi

echo "========================================="
echo "  cpcd ${CPCD_VERSION}"
echo "  Architecture: $(uname -m)"
echo "========================================="

# Clone or update
if [ -d "${CPCD_SRC}" ]; then
    echo "Using existing source"
else
    echo "Cloning ${CPCD_REPO}..."
    git clone --branch "${CPCD_VERSION}" --depth 1 "${CPCD_REPO}" "${CPCD_SRC}"
fi

# Build
cd "${CPCD_SRC}"
mkdir -p build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
make -j$(nproc)

# Install
sudo make install
sudo ldconfig

echo ""
echo "Done. Installed to /usr/local"
echo "Config: /usr/local/etc/cpcd.conf"
