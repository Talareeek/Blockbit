#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$ROOT_DIR"

RELEASE_DIR="$ROOT_DIR/release"
BUILD_LINUX="$ROOT_DIR/build-linux"
BUILD_WINDOWS="$ROOT_DIR/build-windows"
VCPKG_ROOT="${VCPKG_ROOT:-$ROOT_DIR/.vcpkg}"
JOBS="$(nproc)"
ARCH_FLAGS="-O3 -flto -march=x86-64-v2"   # zmień na -march=native jeśli budujesz tylko dla siebie

for tool in cmake ninja x86_64-w64-mingw32-g++ makensis git; do
    command -v "$tool" >/dev/null || { echo "Brakuje narzędzia: $tool"; exit 1; }
done

echo "==> vcpkg (statyczne zależności pod Windowsa)"
if [ ! -d "$VCPKG_ROOT" ]; then
    git clone --depth 1 https://github.com/microsoft/vcpkg "$VCPKG_ROOT"
    "$VCPKG_ROOT/bootstrap-vcpkg.sh" -disableMetrics
fi
"$VCPKG_ROOT/vcpkg" install sfml tinyxml2 openssl protobuf zstd \
    --triplet x64-mingw-static

rm -rf "$RELEASE_DIR"
mkdir -p "$RELEASE_DIR/linux" "$RELEASE_DIR/windows"

# ---------------------------------------------------------------- LINUX ----
echo "==> Build Linux (Release)"
cmake -S "$ROOT_DIR" -B "$BUILD_LINUX" -G Ninja \
    -DCMAKE_BUILD_TYPE=Release \
    -DBLOCKBIT_WITH_GNS=ON \
    -DBLOCKBIT_HEADLESS_STRIP=ON \
    -DCMAKE_CXX_FLAGS="$ARCH_FLAGS" \
    -DCMAKE_EXE_LINKER_FLAGS="-flto -static-libgcc -static-libstdc++"
cmake --build "$BUILD_LINUX" -j"$JOBS"

cp "$BUILD_LINUX/bin/Blockbit"       "$RELEASE_DIR/linux/"
cp "$BUILD_LINUX/bin/BlockbitServer" "$RELEASE_DIR/linux/"
[ -d "$ROOT_DIR/assets" ] && cp -r "$ROOT_DIR/assets" "$RELEASE_DIR/linux/"
strip --strip-unneeded "$RELEASE_DIR/linux/Blockbit" "$RELEASE_DIR/linux/BlockbitServer"
tar -C "$RELEASE_DIR" -czf "$RELEASE_DIR/Blockbit-linux-x64.tar.gz" linux

# -------------------------------------------------------------- WINDOWS ----
echo "==> Build Windows (Release, static, cross z Linuksa)"
cmake -S "$ROOT_DIR" -B "$BUILD_WINDOWS" -G Ninja \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_TOOLCHAIN_FILE="$VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake" \
    -DVCPKG_CHAINLOAD_TOOLCHAIN_FILE="$ROOT_DIR/toolchain-mingw.cmake" \
    -DVCPKG_TARGET_TRIPLET=x64-mingw-static \
    -DBLOCKBIT_WITH_GNS=ON \
    -DBLOCKBIT_HEADLESS_STRIP=ON \
    -DOPENSSL_USE_STATIC_LIBS=TRUE \
    -DCMAKE_CXX_FLAGS="$ARCH_FLAGS" \
    -DCMAKE_EXE_LINKER_FLAGS="-flto -static -static-libgcc -static-libstdc++ -lbcrypt"
cmake --build "$BUILD_WINDOWS" -j"$JOBS"

cp "$BUILD_WINDOWS/bin/Blockbit.exe"       "$RELEASE_DIR/windows/"
cp "$BUILD_WINDOWS/bin/BlockbitServer.exe" "$RELEASE_DIR/windows/"
[ -d "$ROOT_DIR/assets" ] && cp -r "$ROOT_DIR/assets" "$RELEASE_DIR/windows/"
x86_64-w64-mingw32-strip "$RELEASE_DIR/windows/Blockbit.exe" "$RELEASE_DIR/windows/BlockbitServer.exe"

# -------------------------------------------------------------- INSTALLER --
echo "==> Instalator Windows (NSIS, budowany na Linuksie)"
makensis -DRELEASE_DIR="$RELEASE_DIR/windows" -DOUT_DIR="$RELEASE_DIR" \
    "$ROOT_DIR/installer/blockbit.nsi"

echo "==> Gotowe:"
ls -lh "$RELEASE_DIR"