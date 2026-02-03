#!/bin/bash
# kbuild_dbg.sh — Debug build for Linux 5.10.246 (RTL8196E)
#
# This script NEVER downloads, patches, or copies kernel files.
# It only builds the existing kernel tree and optionally packages it.
#
# Usage:
#   ./kbuild_dbg.sh                 # build + package (if tools exist)
#   ./kbuild_dbg.sh vmlinux         # build vmlinux only
#   ./kbuild_dbg.sh no-package      # build vmlinux only
#   ./kbuild_dbg.sh olddefconfig    # update config non-interactively
#   ./kbuild_dbg.sh menuconfig      # open menuconfig
#   ./kbuild_dbg.sh clean           # make clean (keeps .config)
#   ./kbuild_dbg.sh --help
#
# J. Nilo - Debug build helper

set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_ROOT="$(cd "${SCRIPT_DIR}/../.." && pwd)"

KERNEL_VERSION="5.10.246"
KERNEL_DIR="${SCRIPT_DIR}/linux-${KERNEL_VERSION}-rtl8196e"
KERNEL_CMDLINE="console=ttyS0,115200"

TOOLCHAIN_DIR="${PROJECT_ROOT}/x-tools/mips-lexra-linux-musl"
export PATH="${TOOLCHAIN_DIR}/bin:$PATH"
export ARCH=mips
export CROSS_COMPILE=mips-lexra-linux-musl-

BUILD_VMLINUX_ONLY=false
DO_MENUCONFIG=false
DO_CLEAN=false
DO_OLDDEFCONFIG=false

case "${1:-}" in
    vmlinux|no-package)
        BUILD_VMLINUX_ONLY=true
        ;;
    menuconfig)
        DO_MENUCONFIG=true
        ;;
    clean)
        DO_CLEAN=true
        ;;
    olddefconfig)
        DO_OLDDEFCONFIG=true
        ;;
    --help|-h)
        echo "Usage: $0 [vmlinux|no-package|menuconfig|clean|olddefconfig]"
        exit 0
        ;;
    "")
        ;;
    *)
        echo "Unknown option: $1"
        echo "Use --help for usage."
        exit 1
        ;;
 esac

if [ ! -d "$KERNEL_DIR" ]; then
    echo "ERROR: kernel dir not found: $KERNEL_DIR"
    echo "Run build_kernel.sh once to create the tree."
    exit 1
fi

cd "$KERNEL_DIR"

if [ ! -f .config ]; then
    echo "ERROR: .config missing in $KERNEL_DIR"
    echo "Run menuconfig or copy a known config before debug builds."
    exit 1
fi

if ! command -v ${CROSS_COMPILE}gcc >/dev/null 2>&1; then
    echo "ERROR: Lexra toolchain not found: ${CROSS_COMPILE}gcc"
    exit 1
fi

if [ "$DO_CLEAN" = true ]; then
    make ARCH=$ARCH CROSS_COMPILE=$CROSS_COMPILE clean
fi

if [ "$DO_OLDDEFCONFIG" = true ]; then
    make ARCH=$ARCH CROSS_COMPILE=$CROSS_COMPILE olddefconfig
fi

if [ "$DO_MENUCONFIG" = true ]; then
    make ARCH=$ARCH CROSS_COMPILE=$CROSS_COMPILE menuconfig
fi

JOBS=$(nproc)
if [ "$BUILD_VMLINUX_ONLY" = true ]; then
    make ARCH=$ARCH CROSS_COMPILE=$CROSS_COMPILE -j$JOBS vmlinux
    exit 0
fi

make ARCH=$ARCH CROSS_COMPILE=$CROSS_COMPILE -j$JOBS

# Packaging (optional)
BUILD_ENV="${PROJECT_ROOT}/1-Build-Environment/11-realtek-tools"
DOCKER_TOOLS="/home/builder/realtek-tools"

if [ -x "${BUILD_ENV}/bin/cvimg" ]; then
    CVIMG="${BUILD_ENV}/bin/cvimg"
elif [ -x "${DOCKER_TOOLS}/bin/cvimg" ]; then
    CVIMG="${DOCKER_TOOLS}/bin/cvimg"
else
    CVIMG=""
fi

if [ -x "${BUILD_ENV}/bin/lzma" ]; then
    LZMA="${BUILD_ENV}/bin/lzma"
elif [ -x "${DOCKER_TOOLS}/bin/lzma" ]; then
    LZMA=""
else
    LZMA=""
fi

if [ -d "${BUILD_ENV}/lzma-loader" ]; then
    LOADER_DIR="${BUILD_ENV}/lzma-loader"
elif [ -d "${DOCKER_TOOLS}/lzma-loader" ]; then
    LOADER_DIR="${DOCKER_TOOLS}/lzma-loader"
else
    LOADER_DIR=""
fi

if [ -z "$CVIMG" ] || [ -z "$LZMA" ] || [ -z "$LOADER_DIR" ]; then
    echo "WARNING: packaging tools not found; skipping image packaging."
    exit 0
fi

CVIMG_START_ADDR="0x80c00000"
CVIMG_BURN_ADDR="0x00020000"
SIGNATURE="cs6c"

rm -f "${SCRIPT_DIR}/kernel.img"

${CROSS_COMPILE}objcopy -O binary \
    -R .reginfo -R .note -R .comment \
    -R .mdebug -R .MIPS.abiflags -S \
    vmlinux vmlinux.bin

$LZMA e vmlinux.bin vmlinux.bin.lzma -lc1 -lp2 -pb2 >/dev/null 2>&1

PATH="${TOOLCHAIN_DIR}/bin:$PATH" \
KERNEL_DIR="$KERNEL_DIR" \
VMLINUX_DIR="$KERNEL_DIR" \
VMLINUX_INCLUDE="$KERNEL_DIR/include" \
make -C "$LOADER_DIR" \
    CROSS_COMPILE=$CROSS_COMPILE \
    LOADER_DATA="$KERNEL_DIR/vmlinux.bin.lzma" \
    KERNEL_DIR="$KERNEL_DIR" \
    KERNEL_CMDLINE="$KERNEL_CMDLINE" \
    clean all

$CVIMG \
    -i "$LOADER_DIR/loader.bin" \
    -o "${SCRIPT_DIR}/kernel.img" \
    -s "$SIGNATURE" \
    -e "$CVIMG_START_ADDR" \
    -b "$CVIMG_BURN_ADDR" \
    -a 4k >/dev/null

echo "✅ Debug kernel image ready: ${SCRIPT_DIR}/kernel.img"
