---
name: build-file
description: Build a single C++ source file for quick error-checking without a full rebuild. Use when you want to quickly verify a single file compiles after editing it.
allowed-tools: shell
---

# Build Single File

Compile one source file to quickly check for errors without building the whole project.

## Command

```bash
ninja -C build CMakeFiles/Blockbit.dir/{path}.o
```

Where `{path}` is relative to the repo root.

### Examples

```bash
ninja -C build CMakeFiles/Blockbit.dir/src/Game.cpp.o
ninja -C build CMakeFiles/Blockbit.dir/src/World.cpp.o
ninja -C build CMakeFiles/Blockbit.dir/src/RenderSystem.cpp.o
```

## Prerequisites

The build must be configured first. If `build/CMakeCache.txt` doesn't exist:

```bash
mkdir -p build && cd build && cmake -G Ninja -DCMAKE_BUILD_TYPE=Debug .. && cd ..
```

## Tips

- Use this after editing a single file to quickly check for compilation errors
- Much faster than a full build — only recompiles the one `.o` file
- After confirming files compile, do a full build to check linking
