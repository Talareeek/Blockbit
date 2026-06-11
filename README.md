# Blockbit

A 2D sandbox game written in **C++20** with **SFML**, inspired by classic block-based games. Explore a procedurally generated world, place and destroy blocks, and build whatever you can imagine.

![C++](https://img.shields.io/badge/C++-20-blue?logo=c%2B%2B)
![SFML](https://img.shields.io/badge/SFML-3.x-green?logo=sfml)
![License](https://img.shields.io/badge/license-MIT-brightgreen)
![Platform](https://img.shields.io/badge/platform-Windows%20%7C%20Linux-lightgrey)

---

## Features

- **Procedurally generated world** — every playthrough is unique
- **Place and destroy blocks** — full sandbox interaction
- **World persistence** — save and load named worlds
- **Fast CMake + Ninja builds** — quick iteration cycle
- **Debug-ready** — full debug symbols for GDB

---

## Requirements

- C++20 compatible compiler (GCC / MSVC)
- [CMake](https://cmake.org/) >= 3.16
- [Ninja](https://ninja-build.org/)
- [SFML](https://www.sfml-dev.org/) 3.x

---

## Building

The project uses CMake with Ninja for fast, parallel builds. A convenience script `build.sh` wraps the common workflows:

```bash
./build.sh              # Debug build
./build.sh release      # Release build (optimized)
./build.sh rebuild      # Clean and rebuild from scratch
./build.sh run          # Build and run immediately
./build.sh run-world Swiat  # Build and load a named world
```

On **Windows**, use the provided batch script:

```bat
build-windows.bat
```

The compiled binary lands at `build/bin/Blockbit`.

---

## Running

```bash
# Launch with the main menu
./build/bin/Blockbit

# Load a specific saved world directly
./build/bin/Blockbit --load Swiat

# Or use the helper script
./run.sh Swiat
```

Worlds are stored in `~/Blockbit/saves/<world-name>/`.

---

## Project Structure

```
Blockbit/
├── src/              # Game source files
├── include/          # Header files
├── resources/        # Assets (textures, fonts, etc.)
├── inspector/        # Debug/inspector tools
├── CMakeLists.txt    # Build configuration
├── build.sh          # Linux/macOS build script
├── build-linux-g++.sh
├── build-windows.bat
└── run.sh            # Run helper script
```

---

## Debugging

Builds include full debug symbols:
- **Debug**: `-g3 -O0` (maximum debug info, no optimization)
- **Release**: `-O3 -g` (optimized with symbols)

Launch under GDB:

```bash
gdb ./build/bin/Blockbit
(gdb) run --load Swiat
```

---

## License

This project is licensed under the [MIT License](LICENSE).
