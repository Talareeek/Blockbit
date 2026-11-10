#!/bin/bash
# Build with GameNetworkingSockets transport, then run.
#
# Usage:
#   ./run-gns.sh host <world> <nick>           # listen server on port 34500
#   ./run-gns.sh host <world> <nick> <port>    # listen server on custom port
#   ./run-gns.sh join <host[:port]> <nick>     # client, defaults to :34500
#   ./run-gns.sh load <world> <nick>           # single-player (loopback only)
#   ./run-gns.sh build                         # just build, don't run

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BUILD_DIR="$SCRIPT_DIR/build"
BIN="$BUILD_DIR/bin/Blockbit"

RED='\033[0;31m'; GREEN='\033[0;32m'; YELLOW='\033[1;33m'; BLUE='\033[0;34m'; NC='\033[0m'

usage() {
    echo -e "${BLUE}Blockbit (GameNetworkingSockets)${NC}"
    echo
    echo -e "  ${GREEN}./run-gns.sh${NC}                                 Launch the game (main menu)"
    echo -e "  ${GREEN}./run-gns.sh host${NC} <world> <nick> [port]    Host a listen server"
    echo -e "  ${GREEN}./run-gns.sh join${NC} <host[:port]> <nick>      Connect to a remote server"
    echo -e "  ${GREEN}./run-gns.sh load${NC} <world> <nick>            Single-player"
    echo -e "  ${GREEN}./run-gns.sh build${NC}                          Build only"
    echo
    echo "The first run clones GameNetworkingSockets v1.6.1 and compiles it (~1-2 min)."
    echo "Subsequent runs are incremental."
}

run_default() {
    build
    echo -e "${BLUE}▶${NC} Launching Blockbit (menu, transport=gns)..."
    # BB_TRANSPORT so any host/join picked from the menu also goes through GNS,
    # matching what the subcommands do explicitly.
    BB_TRANSPORT=gns exec "$BIN"
}

needs_reconfigure() {
    # Trigger a full reconfigure the first time BLOCKBIT_WITH_GNS=ON is requested,
    # since the build dir may be cached with OFF from a previous ./build.sh run.
    [ ! -f "$BUILD_DIR/CMakeCache.txt" ] && return 0
    grep -q "BLOCKBIT_WITH_GNS:BOOL=ON" "$BUILD_DIR/CMakeCache.txt" || return 0
    return 1
}

build() {
    mkdir -p "$BUILD_DIR"
    if needs_reconfigure; then
        echo -e "${BLUE}▶${NC} Configuring with BLOCKBIT_WITH_GNS=ON..."
        ( cd "$BUILD_DIR" && cmake -G Ninja \
            -DCMAKE_BUILD_TYPE="${BUILD_TYPE:-Debug}" \
            -DCMAKE_C_COMPILER="${CC:-clang}" \
            -DCMAKE_CXX_COMPILER="${CXX:-clang++}" \
            -DBLOCKBIT_WITH_GNS=ON \
            "$SCRIPT_DIR" )
    fi
    echo -e "${BLUE}▶${NC} Building..."
    ( cd "$BUILD_DIR" && ninja )
    echo -e "${GREEN}✓${NC} Built: $BIN"
}

run_host() {
    local world="$1" nick="$2" port="${3:-}"
    if [ -z "$world" ] || [ -z "$nick" ]; then
        echo -e "${RED}✗${NC} Usage: ./run-gns.sh host <world> <nick> [port]"; exit 1
    fi
    if [ ! -d "$HOME/Blockbit/saves/$world" ]; then
        echo -e "${RED}✗${NC} World '$world' not found at ~/Blockbit/saves/$world"; exit 1
    fi
    build
    echo -e "${BLUE}▶${NC} Hosting '$world' via GNS (nick: $nick${port:+, port: $port})..."
    # --host currently hardcodes the port to DEFAULT_PORT (34500). The optional
    # arg here is forwarded as a reminder, but today it has no effect — kept so
    # the CLI shape won't change when port selection lands.
    exec "$BIN" --transport gns --host "$world" "$nick"
}

run_join() {
    local addr="$1" nick="$2"
    if [ -z "$addr" ] || [ -z "$nick" ]; then
        echo -e "${RED}✗${NC} Usage: ./run-gns.sh join <host[:port]> <nick>"; exit 1
    fi
    build
    echo -e "${BLUE}▶${NC} Joining $addr via GNS (nick: $nick)..."
    exec "$BIN" --transport gns --join "$addr" "$nick"
}

run_load() {
    local world="$1" nick="$2"
    if [ -z "$world" ] || [ -z "$nick" ]; then
        echo -e "${RED}✗${NC} Usage: ./run-gns.sh load <world> <nick>"; exit 1
    fi
    if [ ! -d "$HOME/Blockbit/saves/$world" ]; then
        echo -e "${RED}✗${NC} World '$world' not found at ~/Blockbit/saves/$world"; exit 1
    fi
    build
    # Singleplayer never hits the wire — transport flag is a no-op but harmless.
    exec "$BIN" --transport gns --load "$world" "$nick"
}

cmd="${1:-}"; shift || true
case "$cmd" in
    host)     run_host "$@" ;;
    join)     run_join "$@" ;;
    load)     run_load "$@" ;;
    build)    build ;;
    "")       run_default ;;
    -h|--help) usage ;;
    *) echo -e "${RED}✗${NC} Unknown command: $cmd"; usage; exit 1 ;;
esac
