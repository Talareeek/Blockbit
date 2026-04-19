---
name: build-cmake
description: Reconfigure CMake build files. Use when CMakeLists.txt changes or when source files (.cpp) are added or removed from src/.
allowed-tools: shell
---

# Reconfigure CMake

Regenerate the Ninja build files via CMake. Required when:

- `CMakeLists.txt` has been modified
- New `.cpp` files have been added to `src/`
- Source files have been removed from `src/`

The project uses `file(GLOB_RECURSE SOURCES "src/*.cpp")` so CMake **must** re-run to discover new or removed source files.

## Command

```bash
mkdir -p build && cd build && cmake -G Ninja -DCMAKE_BUILD_TYPE=Debug .. && cd ..
```

## After reconfiguring

Build everything:

```bash
ninja -C build
```
