name: blockbit-build
description: Build the Blockbit game using CMake and Ninja

# Build Blockbit game

Quick commands:
- `./build.sh` - Debug build
- `./build.sh release` - Release build (optimized)
- `./build.sh rebuild -j8` - Clean and rebuild with 8 parallel jobs
- `./build.sh rebuild -j$(nproc)` - Use all CPU cores

Always run from: `/home/talarek/Desktop/Blockbit`

Output binary: `build/bin/Blockbit`

Build takes ~6 seconds for full rebuild, ~30ms for incremental builds with no changes.
