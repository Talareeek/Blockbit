---
name: test-world
description: Build and run Blockbit loading a specific world for testing. Use when asked to test, run, or load a world (e.g. 'Swiat').
allowed-tools: shell
---

# Test World

Build the project and launch it with a specific world loaded.

## Steps

1. Ensure the project is built:

```bash
ninja -C build
```

2. Run with the world:

```bash
./run.sh <world-name>
```

### Example: test with world "Swiat"

```bash
ninja -C build && ./run.sh Swiat
```

## World locations

Worlds are stored in `~/Blockbit/saves/<world-name>`.

## Prerequisites

- Build directory must be configured — if `build/CMakeCache.txt` doesn't exist:
  ```bash
  mkdir -p build && cd build && cmake -G Ninja -DCMAKE_BUILD_TYPE=Debug .. && cd ..
  ```
- The world must exist in `~/Blockbit/saves/`
