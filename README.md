# Blockbit

## Building

### 🚀 Modern CMake Build System (Recommended)

Uses CMake with Ninja for fast, parallel multi-threaded builds. All artifacts output to `build/` directory.

```bash
./build.sh                  # Debug build (default)
./build.sh release         # Release build with optimizations (-O3)
./build.sh rebuild         # Clean and full rebuild
./build.sh rebuild -j8     # Build with 8 parallel jobs
./build.sh -h              # Show all options
```

**Build Artifacts:**
- **Binary**: `build/bin/Blockbit`
- **Build Files**: `build/` (can be safely deleted and rebuilt)
- **CMake Cache**: `build/CMakeCache.txt`

### Legacy Build Scripts (Deprecated)

The old scripts still work but are superseded by CMake:

```bash
bash build-linux-g++.sh   # Linux only - direct g++ compilation
build.bat                 # Windows only
```

## Running

### With New CMake Build System

```bash
# Build and run (Debug mode)
./build.sh run

# Build and run with specific world
./build.sh run-world Swiat

# Manual run
./build/bin/Blockbit
./build/bin/Blockbit --load Swiat
```

### With Compiled Binary

```bash
# Default (show intro menu)
./build/bin/Blockbit

# Load specific world
./build/bin/Blockbit --load <world-name>

# Helper script
./run-world.sh <world-name>
```

The world must exist in `~/Blockbit/saves/<world-name>`

**Example:**
```bash
./build/bin/Blockbit --load Swiat
./run-world.sh Swiat
```

## Debugging

The binary is built with **full debug symbols** for proper stack traces with function names and line numbers:

- **Debug Mode**: `-g3 -O0` (maximum debug info, no optimization)
- **Release Mode**: `-O3 -g` (optimized, with debug symbols)

```bash
# Debug with GDB
gdb ./build/bin/Blockbit
# Then in gdb:
(gdb) run --load Swiat

# Or directly:
gdb -ex "run --load Swiat" ./build/bin/Blockbit
```

## Build Features

✅ **Fast Parallel Build** - Uses Ninja with multi-threaded compilation  
✅ **Debug Symbols** - Full debug info for stack traces  
✅ **CMake 3.24+** - Modern C++20 support  
✅ **Organized Output** - All artifacts in `build/` directory  
✅ **Clean Structure** - Source files remain untouched  
✅ **Smart Caching** - Incremental rebuilds only recompile changed files  

## Build System Details

### CMakeLists.txt Features
- Automatic source file discovery
- SFML library integration
- Separate Debug/Release configurations
- Proper include directory setup
- Color-coded build output

### build.sh Script Features
- Dependency checking (cmake, ninja)
- Automatic directory creation
- Parallel job control (`-j` flag)
- Verbose output option (`-v` flag)
- Multiple build targets (debug, release, rebuild, run, run-world)
- Helpful error messages and status indicators