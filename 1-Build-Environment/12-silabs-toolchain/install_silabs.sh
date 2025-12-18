#!/bin/bash
# install_silabs.sh — Install Silicon Labs slc-cli and Gecko SDK
#
# This script downloads and installs:
#   - slc-cli (Simplicity Commander CLI)
#   - Gecko SDK
#   - Required SDK extensions for Zigbee development
#
# Usage:
#   ./install_silabs.sh [install_dir]
#
# Default install directory: ~/silabs

set -e

# Configuration
SLC_VERSION="5.9.3.0"
GECKO_SDK_VERSION="4.4.0"
INSTALL_DIR="${1:-$HOME/silabs}"

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

info() { echo -e "${GREEN}[INFO]${NC} $1"; }
warn() { echo -e "${YELLOW}[WARN]${NC} $1"; }
error() { echo -e "${RED}[ERROR]${NC} $1"; exit 1; }

# Detect architecture
ARCH=$(uname -m)
case "$ARCH" in
    x86_64)  SLC_ARCH="amd64" ;;
    aarch64) SLC_ARCH="arm64" ;;
    *)       error "Unsupported architecture: $ARCH" ;;
esac

# Create install directory
mkdir -p "$INSTALL_DIR"
cd "$INSTALL_DIR"

info "Installing Silicon Labs tools to: $INSTALL_DIR"

# ============================================================================
# Step 1: Download and install slc-cli
# ============================================================================
info "Downloading slc-cli v${SLC_VERSION} for ${SLC_ARCH}..."

SLC_URL="https://www.silabs.com/documents/login/software/slc_cli_linux_${SLC_ARCH}.zip"
SLC_ZIP="slc_cli_linux_${SLC_ARCH}.zip"

if [ ! -f "$INSTALL_DIR/slc_cli/slc" ]; then
    wget -q --show-progress -O "$SLC_ZIP" "$SLC_URL" || error "Failed to download slc-cli"
    unzip -q -o "$SLC_ZIP" -d slc_cli
    rm "$SLC_ZIP"
    chmod +x slc_cli/slc
    info "slc-cli installed"
else
    info "slc-cli already installed, skipping"
fi

# Add to PATH for this session
export PATH="$INSTALL_DIR/slc_cli:$PATH"

# Verify installation
slc --version || error "slc-cli installation failed"

# ============================================================================
# Step 2: Trust Silicon Labs signature
# ============================================================================
info "Trusting Silicon Labs signature..."
slc signature trust --sdk || warn "Signature trust may have failed (non-critical)"

# ============================================================================
# Step 3: Install Gecko SDK
# ============================================================================
info "Installing Gecko SDK v${GECKO_SDK_VERSION}..."

if [ ! -d "$INSTALL_DIR/gecko_sdk" ]; then
    slc sdk install \
        --sdk-id "com.silabs.sdk.gecko:${GECKO_SDK_VERSION}" \
        --dest "$INSTALL_DIR/gecko_sdk" \
        || error "Failed to install Gecko SDK"
    info "Gecko SDK installed"
else
    info "Gecko SDK already installed, skipping"
fi

# ============================================================================
# Step 4: Install SDK extensions
# ============================================================================
info "Installing SDK extensions..."

EXTENSIONS=(
    "com.silabs.extension.zigbee"
    "com.silabs.extension.bluetooth"
)

for ext in "${EXTENSIONS[@]}"; do
    info "  Installing extension: $ext"
    slc sdk add-extension --extension "$ext" --sdk "$INSTALL_DIR/gecko_sdk" 2>/dev/null || true
done

# ============================================================================
# Step 5: Install ARM GCC toolchain
# ============================================================================
info "Installing ARM GCC toolchain..."

ARM_GCC_VERSION="12.2.rel1"
ARM_GCC_URL="https://developer.arm.com/-/media/Files/downloads/gnu/${ARM_GCC_VERSION}/binrel/arm-gnu-toolchain-${ARM_GCC_VERSION}-${ARCH}-arm-none-eabi.tar.xz"

if [ ! -d "$INSTALL_DIR/arm-gnu-toolchain" ]; then
    wget -q --show-progress -O arm-gcc.tar.xz "$ARM_GCC_URL" || error "Failed to download ARM GCC"
    tar xf arm-gcc.tar.xz
    mv arm-gnu-toolchain-* arm-gnu-toolchain
    rm arm-gcc.tar.xz
    info "ARM GCC toolchain installed"
else
    info "ARM GCC toolchain already installed, skipping"
fi

# ============================================================================
# Step 6: Install Simplicity Commander
# ============================================================================
info "Installing Simplicity Commander..."

if [ ! -d "$INSTALL_DIR/commander" ]; then
    wget -q --show-progress -O commander.zip "https://www.silabs.com/documents/public/software/SimplicityCommander-Linux.zip" || error "Failed to download Commander"
    unzip -q commander.zip
    tar xf SimplicityCommander-Linux/Commander_linux_x86_64_*.tar.bz -C .
    rm -rf commander.zip SimplicityCommander-Linux
    info "Simplicity Commander installed"
else
    info "Simplicity Commander already installed, skipping"
fi

# ============================================================================
# Step 7: Create environment setup script
# ============================================================================
info "Creating environment setup script..."

cat > "$INSTALL_DIR/env.sh" << 'EOF'
# Silicon Labs build environment
# Source this file: source ~/silabs/env.sh

export SILABS_HOME="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
export PATH="$SILABS_HOME/slc_cli:$PATH"
export PATH="$SILABS_HOME/arm-gnu-toolchain/bin:$PATH"
export PATH="$SILABS_HOME/commander:$PATH"
export GECKO_SDK="$SILABS_HOME/gecko_sdk"
export ARM_GCC_DIR="$SILABS_HOME/arm-gnu-toolchain"

echo "Silicon Labs environment loaded:"
echo "  GECKO_SDK=$GECKO_SDK"
echo "  ARM_GCC_DIR=$ARM_GCC_DIR"
echo "  slc version: $(slc --version 2>/dev/null | head -1)"
echo "  commander version: $(commander --version 2>/dev/null | head -1)"
EOF

# ============================================================================
# Done
# ============================================================================
info "Installation complete!"
echo ""
echo "To use the Silicon Labs tools, add to your shell profile:"
echo "  source $INSTALL_DIR/env.sh"
echo ""
echo "Or add these to PATH:"
echo "  export PATH=\"$INSTALL_DIR/slc_cli:\$PATH\""
echo "  export PATH=\"$INSTALL_DIR/arm-gnu-toolchain/bin:\$PATH\""
echo "  export PATH=\"$INSTALL_DIR/commander:\$PATH\""
