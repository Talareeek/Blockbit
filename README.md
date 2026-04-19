# Blockbit

## Building

Use CMake with Ninja for fast, parallel builds:

```bash
./build.sh                  # Debug build
./build.sh release         # Release build
./build.sh rebuild         # Clean and rebuild
./build.sh run             # Build and run
./build.sh run-world Swiat # Build and load world
```

**Binary**: `build/bin/Blockbit`

## Running

```bash
./build/bin/Blockbit              # Run with menu
./build/bin/Blockbit --load Swiat # Load specific world
./run-world.sh Swiat              # Helper script
```

Worlds are loaded from `~/Blockbit/saves/<world-name>`

## Debugging

Full debug symbols included: `-g3 -O0` (Debug), `-O3 -g` (Release)

```bash
gdb ./build/bin/Blockbit
(gdb) run --load Swiat
```

## Copilot MCP Server

For GitHub Copilot integration, the `mcp_build_server.py` provides these tools:

- **build** - Build entire project
- **build_file** - Build single source file (e.g. `src/Game.cpp`)
- **test_world** - Build and test with world (e.g. `Swiat`)