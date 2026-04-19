name: blockbit-build
description: >-
    Skill for building Blockbit game using CMake and Ninja.
    Supports Debug/Release builds, running the game, and loading specific worlds.
user-invocable: false

---

# Blockbit Build Skill

This skill provides commands to build the Blockbit game with full control over build types and options.

## Quick Build Commands

### Standard Builds
```bash
# Debug build (default)
./build.sh

# Release build (optimized)
./build.sh release

# Clean and rebuild
./build.sh rebuild
```

### With Parallel Jobs
```bash
# Auto-detect CPU cores
./build.sh rebuild -j$(nproc)

# Specific number of jobs
./build.sh rebuild -j8
```

### Run Commands
```bash
# Build and run with intro menu
./build.sh run

# Build and load specific world
./build.sh run-world Swiat

# Build and load any world
./build.sh run-world <world-name>
```

### Maintenance
```bash
# Show all available commands
./build.sh -h

# Clean build artifacts
./build.sh clean

# Rebuild from scratch
./build.sh rebuild
```

## Build Artifacts

- **Binary**: `build/bin/Blockbit` (11MB with debug symbols)
- **Build Cache**: `build/` directory (can be deleted, safe to rebuild)
- **Build Config**: `CMakeLists.txt` (defines build configuration)

## Build Modes

| Mode | Flags | Use Case |
|------|-------|----------|
| **Debug** | `-g3 -O0` | Development, full debug info, slower |
| **Release** | `-O3 -g` | Production, optimized, still debuggable |

## Performance

- **Full build**: ~6 seconds (parallel with all CPU cores)
- **Incremental**: ~30ms (no changes needed)
- **Rebuild**: ~7 seconds

## Features

✅ CMake 3.24+ with modern C++20  
✅ Ninja for fast multi-threaded builds  
✅ Automatic source discovery  
✅ SFML library integration  
✅ Full debug symbols in both modes  
✅ Incremental compilation support  
✅ Color-coded build output  
✅ Dependency validation  

## Inside Copilot Context

When working in Copilot sessions, the `copilot-setup-steps.yml` automatically:
1. Installs CMake, Ninja, GCC
2. Installs SFML development libraries
3. Builds project in Debug mode
4. Verifies debug symbols are present

This enables seamless code exploration, testing, and debugging within Copilot's ephemeral environment.
