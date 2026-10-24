#!/bin/bash
# Release build of just the headless dedicated server (target: BlockbitServer).
# No debug flags: BLOCKBIT_HEADLESS_STRIP=ON toggles -O3 + stripped -s linking
# in CMakeLists.txt. Does not touch the interactive Blockbit target.

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BUILD_DIR="${SCRIPT_DIR}/build-headless"

RED='\033[0;31m'
GREEN='\033[0;32m'
BLUE='\033[0;34m'
NC='\033[0m'

print_status()  { echo -e "${BLUE}▶${NC} $1"; }
print_success() { echo -e "${GREEN}✓${NC} $1"; }
print_error()   { echo -e "${RED}✗${NC} $1" >&2; }

for cmd in cmake ninja; do
    if ! command -v "$cmd" &> /dev/null; then
        print_error "$cmd not found"
        echo "Install: sudo apt-get install $cmd"
        exit 1
    fi
done

mkdir -p "$BUILD_DIR"

print_status "Configuring CMake (Release, headless-only)..."
cd "$BUILD_DIR"
cmake -G Ninja \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_C_COMPILER="${CC:-clang}" \
    -DCMAKE_CXX_COMPILER="${CXX:-clang++}" \
    -DBLOCKBIT_HEADLESS_STRIP=ON \
    "$SCRIPT_DIR"

print_status "Building BlockbitServer..."
# Named target: skips compilation of client-only sources entirely.
ninja BlockbitServer

cd - > /dev/null

print_success "Headless server built: ${BUILD_DIR}/bin/BlockbitServer"
