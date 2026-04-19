# Blockbit Development Guidelines for Copilot

## Build System

The project uses CMake with Ninja for fast, parallel builds.

### Standard Build Commands
```bash
# Quick debug build (incremental, ~30ms if no changes)
./build.sh

# Release build (optimized with -O3)
./build.sh release

# Clean rebuild with all CPU cores
./build.sh rebuild -j$(nproc)

# Build and run
./build.sh run

# Build and load specific world
./build.sh run-world Swiat
```

### Build Artifacts
- Binary: `build/bin/Blockbit`
- All artifacts in `build/` directory (can be safely deleted)

## Debug Symbols

Both Debug and Release builds include debug symbols for stack traces:
- **Debug Mode**: `-g3 -O0` (full symbols, no optimization)
- **Release Mode**: `-O3 -g` (optimized, with symbols)

Binary includes `.debug_*` sections for accurate debugging with GDB.

## Running Games

### Load Worlds
```bash
./build.sh run-world Swiat        # Direct
./build/bin/Blockbit --load Swiat  # Manual
./run-world.sh Swiat               # Helper script
```

Available worlds: `~/Blockbit/saves/`
- **Swiat** - Main test world

## Code Structure

```
Blockbit/
├── src/          - 37 source files
├── include/      - 47 header files (.hpp)
├── build/        - Build artifacts (gitignored)
├── CMakeLists.txt - Build configuration
└── build.sh      - Build script
```

## When Making Changes

1. **Edit source files** in `src/` and `include/`
2. **Build incrementally**: `./build.sh` (fast, only recompiles changed files)
3. **Full rebuild if needed**: `./build.sh rebuild`
4. **Run and test**: `./build.sh run` or `./build.sh run-world <name>`

## Performance Notes

- Incremental builds: ~30ms (when nothing changed)
- Full rebuild: ~6 seconds (with parallel jobs)
- Binary size: 11MB (includes debug symbols)

## GitHub Integration

- `copilot-setup-steps.yml` - Sets up Copilot development environment
- `.github/skills/blockbit-build.md` - Build skill documentation
- Automatic SFML and CMake dependency installation in GitHub Actions
