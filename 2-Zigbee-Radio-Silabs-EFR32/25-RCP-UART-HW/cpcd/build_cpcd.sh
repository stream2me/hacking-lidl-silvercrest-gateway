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

prepare_cmake() {
    cat << 'EOF' > cmakeLists.patch
--- CMakeLists.txt
+++ CMakeLists.txt
@@ -166,12 +166,19 @@
 ###
 
 install(TARGETS cpc
+  COMPONENT development
   LIBRARY DESTINATION "${CMAKE_INSTALL_LIBDIR}"
   PUBLIC_HEADER DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}")
-install(
-  FILES "${PROJECT_BINARY_DIR}/libcpc.pc"
+
+install(FILES "${PROJECT_BINARY_DIR}/libcpc.pc"
+  COMPONENT development
   DESTINATION "${CMAKE_INSTALL_LIBDIR}/pkgconfig")
 
 install(TARGETS cpcd
+  COMPONENT runtime
   RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}")
-install(FILES "./cpcd.conf" DESTINATION "${CMAKE_INSTALL_FULL_SYSCONFDIR}")
+
+install(FILES "./cpcd.conf"
+  COMPONENT runtime
+  DESTINATION "${CMAKE_INSTALL_FULL_SYSCONFDIR}")
+include(CPack)
EOF

patch -b CMakeLists.txt < cmakeLists.patch
rm cmakeLists.patch
}

prepare_deb_files() {
    echo "Creating build helper scripts..."
    # Debian Maintainer Scripts
    cat << 'EOF' > preinst
#!/bin/sh
set -e
echo "Stopping cpc-daemon if running..."
pkill -f cpcd || true
EOF

    cat << 'EOF' > prerm
#!/bin/sh
set -e
echo "Stopping cpc-daemon before removal..."
pkill -f cpcd || true
EOF

    chmod +x preinst prerm
}

set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
CPCD_SRC="${SCRIPT_DIR}/cpc-daemon"

CPCD_REPO="https://github.com/SiliconLabs/cpc-daemon.git"
CPCD_VERSION="v4.5.3"

REPO_OWNER=$(git remote get-url origin | sed -E 's/.*[:\/](.*)\/.*\..*/\1/')

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
prepare_cmake

cmake -B build -S . \
    -DCMAKE_INSTALL_PREFIX=/usr \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_EXE_LINKER_FLAGS='-static-libgcc -static-libstdc++' \
    -DBUILD_SHARED_LIBS=OFF `#include libcpc in binary` \
    -DENABLE_ENCRYPTION=OFF `#disable encryption, we dont use it` \
    -DCMAKE_C_FLAGS="-s" \
    -DCPACK_GENERATOR=DEB \
    -DCPACK_DEBIAN_PACKAGE_MAINTAINER=$REPO_OWNER \
    -DCPACK_DEBIAN_PACKAGE_SHLIBDEPS=OFF \
    -DCPACK_PACKAGE_DESCRIPTION_SUMMARY='cpc-daemon - statically linked no mbedTLS' \
    -DCPACK_DEBIAN_PACKAGE_DEPENDS='socat' \
    -DCPACK_DEBIAN_PACKAGE_CONTROL_EXTRA="preinst;prerm" \
    -DCPACK_OUTPUT_FILE_PREFIX="${SCRIPT_DIR}/packages" \
    -DCPACK_COMPONENTS_ALL=runtime \
    -DCPACK_DEB_COMPONENT_INSTALL=ON
cmake --build build

# cleanup
mv CMakeLists.txt.orig CMakeLists.txt

# Install
read -p "Install CPC-daemon local or build DEB-File? ((l)ocal/(d)eb): " answer
cd build

case ${answer:0:1} in
    l|L )
        echo "Installing locally..."
        sudo cmake -DCOMPONENT=runtime -P cmake_install.cmake
        echo "Done."
        ;;
    d|D )
        echo "Generating DEB-Package..."
        prepare_deb_files
        cpack -G DEB
        echo "Done."
	echo "use dpkg -i cpc-daemon.deb to install"
       ;;
    * )
        echo "Installation canceled."
        ;;
esac
