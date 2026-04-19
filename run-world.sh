#!/bin/bash

# Helper script to run Blockbit with a specific world
# Usage: ./run-world.sh world-name

if [ $# -eq 0 ]; then
    echo "Usage: $0 <world-name>"
    echo ""
    echo "Available worlds:"
    if [ -d "$HOME/Blockbit/saves" ]; then
        ls -1 "$HOME/Blockbit/saves" | grep -v "^\." || echo "  (none)"
    else
        echo "  (saves directory not found at $HOME/Blockbit/saves)"
    fi
    exit 1
fi

WORLD_NAME="$1"
WORLD_PATH="$HOME/Blockbit/saves/$WORLD_NAME"

if [ ! -d "$WORLD_PATH" ]; then
    echo "Error: World '$WORLD_NAME' not found at $WORLD_PATH"
    exit 1
fi

echo "Loading world: $WORLD_NAME"
./Blockbit --load "$WORLD_NAME"
