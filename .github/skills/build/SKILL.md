---
name: build
description: Build the entire Blockbit project using CMake and Ninja. Use when asked to build, compile, or make the project.
allowed-tools: shell
---

# Build Blockbit

Build the entire project using CMake and Ninja.

## Quick start

If `build/CMakeCache.txt` exists (already configured):

```bash
ninja -C build
```

If not yet configured (first build or after clean):

```bash
mkdir -p build && cd build && cmake -G Ninja -DCMAKE_BUILD_TYPE=Debug .. && cd .. && ninja -C build
```

## Output

Binary: `build/bin/Blockbit`

## If the build fails

- Check compiler errors in the output
- If new `.cpp` files were added/removed, reconfigure CMake first with the /build-cmake skill
- The project uses C++20, SFML (Graphics, Window, System, Audio)
