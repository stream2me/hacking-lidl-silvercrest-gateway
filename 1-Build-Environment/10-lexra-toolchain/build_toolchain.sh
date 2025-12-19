#!/bin/bash
# build_toolchain.sh — Build Lexra MIPS toolchain with crosstool-ng and Musl 1.2.5
#
# This script automates the complete toolchain build process including:
#   - crosstool-ng installation (if needed)
#   - Patch deployment to ~/.crosstool-ng/
#   - Toolchain compilation
#
# Output: $HOME/x-tools/mips-lexra-linux-musl/
#
# J. Nilo - November 2025

set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
CT_NG_VERSION="crosstool-ng-1.26.0"
CT_NG_URL="http://crosstool-ng.org/download/crosstool-ng/${CT_NG_VERSION}.tar.bz2"
TOOLCHAIN_PREFIX="$HOME/x-tools/mips-lexra-linux-musl"

echo "========================================="
echo "  LEXRA MIPS TOOLCHAIN BUILD"
echo "========================================="
echo ""

# Check if crosstool-ng is installed
if ! command -v ct-ng >/dev/null 2>&1; then
    echo "❌ crosstool-ng not found in PATH"
    echo ""
    echo "Install crosstool-ng manually:"
    echo "  cd /tmp"
    echo "  wget ${CT_NG_URL}"
    echo "  tar xjf ${CT_NG_VERSION}.tar.bz2"
    echo "  cd ${CT_NG_VERSION}"
    echo "  ./configure --prefix=\$HOME/crosstool-ng"
    echo "  make && make install"
    echo "  export PATH=\"\$HOME/crosstool-ng/bin:\$PATH\""
    echo ""
    exit 1
fi

echo "✅ crosstool-ng found: $(ct-ng version)"
echo ""

# Check if toolchain already exists
if [ -d "$TOOLCHAIN_PREFIX" ]; then
    echo "⚠️  Toolchain already exists at: $TOOLCHAIN_PREFIX"
    echo ""
    read -p "Rebuild? This will take ~30 minutes. [y/N] " -n 1 -r
    echo ""
    if [[ ! $REPLY =~ ^[Yy]$ ]]; then
        echo "Cancelled."
        exit 0
    fi
    echo "Removing existing toolchain..."
    rm -rf "$TOOLCHAIN_PREFIX"
fi

# Deploy Lexra patches to crosstool-ng patches directory
echo "📦 Deploying Lexra patches to ~/.crosstool-ng/..."
mkdir -p ~/.crosstool-ng/
cp -f -a "${SCRIPT_DIR}/patches" ~/.crosstool-ng/
echo "✅ Patches deployed"
echo ""

# Create temporary build directory
BUILD_DIR=$(mktemp -d)
trap "rm -rf $BUILD_DIR" EXIT

cd "$BUILD_DIR"
echo "📁 Build directory: $BUILD_DIR"
echo ""

# Copy config
cp "${SCRIPT_DIR}/crosstool-ng.config" .config
echo "✅ Configuration loaded"
echo ""

# Show configuration summary
echo "========================================="
echo "  TOOLCHAIN CONFIGURATION"
echo "========================================="
ct-ng show-config | grep -E "(Target|Vendor|OS|Kernel|C library|GCC|Binutils)" || true
echo "========================================="
echo ""

read -p "Start build? This will take ~30 minutes. [Y/n] " -n 1 -r
echo ""
if [[ $REPLY =~ ^[Nn]$ ]]; then
    echo "Cancelled."
    exit 0
fi

# Build toolchain
echo ""
echo "🔨 Building toolchain..."
echo "   This will take approximately 30 minutes..."
echo ""

ct-ng build

echo ""
echo "========================================="
echo "  BUILD COMPLETE!"
echo "========================================="
echo ""
echo "✅ Toolchain installed to: $TOOLCHAIN_PREFIX"
echo ""
echo "Add to your ~/.bashrc:"
echo "  export PATH=\"$TOOLCHAIN_PREFIX/bin:\$PATH\""
echo ""
echo "Verify installation:"
echo "  mips-lexra-linux-musl-gcc --version"
echo ""
