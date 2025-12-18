#!/bin/bash
# install_deps.sh — Install all dependencies for building the gateway system
#
# This script installs all required packages for:
#   - Lexra MIPS toolchain (crosstool-ng) for RTL8196E
#   - Realtek tools (cvimg, lzma, lzma-loader)
#   - Linux kernel, BusyBox, Dropbear, etc.
#   - Silicon Labs slc-cli and Gecko SDK for EFR32
#
# Tested on: Ubuntu 22.04 LTS
#
# J. Nilo - December 2025

set -e

echo "========================================="
echo "  INSTALL BUILD DEPENDENCIES"
echo "========================================="
echo ""

# Check if running as root or with sudo
if [ "$EUID" -ne 0 ]; then
    echo "This script must be run with sudo:"
    echo "   sudo ./install_deps.sh"
    exit 1
fi

echo "Adding i386 architecture (for some legacy tools)..."
dpkg --add-architecture i386
apt-get update -qq

echo ""
echo "Installing packages..."
echo ""

apt-get install -y \
    build-essential \
    bison \
    flex \
    gawk \
    help2man \
    texinfo \
    libtool-bin \
    libncurses5-dev \
    unzip \
    wget \
    curl \
    bzip2 \
    lbzip2 \
    xz-utils \
    zlib1g-dev \
    libc6-i386 \
    zlib1g:i386 \
    git \
    git-lfs \
    bc \
    libssl-dev \
    libelf-dev \
    cpio \
    python3 \
    python3-pip \
    python3-venv \
    squashfs-tools \
    mtd-utils \
    fakeroot \
    tftp-hpa \
    device-tree-compiler \
    rsync \
    openjdk-21-jre-headless \
    libgl1

echo ""
echo "========================================="
echo "  ALL DEPENDENCIES INSTALLED"
echo "========================================="
echo ""
echo "Packages installed:"
echo "  - Build essentials: build-essential, bison, flex, gawk, bc"
echo "  - Crosstool-ng: help2man, texinfo, libtool-bin, libncurses5-dev"
echo "  - Kernel build: libssl-dev, libelf-dev, device-tree-compiler"
echo "  - Compression: bzip2, lbzip2, xz-utils, zlib1g-dev"
echo "  - Filesystem: squashfs-tools, mtd-utils, fakeroot, cpio"
echo "  - 32-bit support: libc6-i386, zlib1g:i386"
echo "  - Flashing: tftp-hpa"
echo "  - Silabs tools: openjdk-21-jre-headless, libgl1"
echo "  - Other: git, git-lfs, wget, curl, unzip, python3, rsync"
echo ""
echo "Next steps:"
echo ""
echo "  1. Build Lexra toolchain:"
echo "     cd 10-lexra-toolchain && ./build_toolchain.sh"
echo ""
echo "  2. Build Realtek tools:"
echo "     cd 11-realtek-tools && ./build_tools.sh"
echo ""
echo "  3. Install Silabs tools:"
echo "     cd 12-silabs-toolchain && ./install_silabs.sh"
echo ""
echo "See README.md for detailed instructions."
echo ""
