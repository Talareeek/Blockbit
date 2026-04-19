name: blockbit-test-world
description: Build and test Blockbit with specific world

# Test Blockbit with World

Build the game and load a specific world for testing.

## Quick Commands

- `./build.sh run-world Swiat` - Build and load "Swiat" world
- `./build/bin/Blockbit --load Swiat` - Run already built binary with world
- `./run-world.sh Swiat` - Helper script for loading world

## Available Worlds

Worlds are stored in: `~/Blockbit/saves/`

Currently available:
- **Swiat** - Main test world

## Testing Workflow

1. Make code changes in `src/` or `include/`
2. Run: `./build.sh run-world Swiat`
3. Game builds and launches directly into the world for testing
4. Incremental builds are fast (~30ms if nothing changed)

## Manual Testing

Run already-built binary with different worlds:
```bash
./build/bin/Blockbit --load Swiat
./run-world.sh Swiat
```

Always run from: `/home/talarek/Desktop/Blockbit`
