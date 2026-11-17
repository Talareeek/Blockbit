#!/bin/bash

# Helper script to run Blockbit, optionally loading a specific world
# Usage: ./run.sh [world-name]

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$SCRIPT_DIR"

./build.sh

if [ ! -x ./build/bin/Blockbit ]; then
    echo "Error: build completed without producing ./build/bin/Blockbit" >&2
    exit 1
fi

if [ $# -eq 0 ]; then
    ./build/bin/Blockbit
    exit $?
fi

WORLD_NAME="$1"
WORLD_PATH="$HOME/Blockbit/saves/$WORLD_NAME"

if [ ! -d "$WORLD_PATH" ]; then
    echo "Error: World '$WORLD_NAME' not found at $WORLD_PATH"
    exit 1
fi

echo "Loading world: $WORLD_NAME"
./build/bin/Blockbit --load "$WORLD_NAME"
