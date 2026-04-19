#!/bin/bash

# Elegant Blockbit build script using CMake and Ninja
# Supports: build, clean, rebuild, release, debug

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BUILD_DIR="${SCRIPT_DIR}/build"
BUILD_TYPE="${BUILD_TYPE:-Debug}"

# Color codes
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Functions
print_help() {
    cat << EOF
${BLUE}Blockbit Build Script${NC}

Usage: ./build.sh [command] [options]

Commands:
    ${GREEN}build${NC}       Build the project (default)
    ${GREEN}clean${NC}       Remove build directory
    ${GREEN}rebuild${NC}     Clean and build
    ${GREEN}release${NC}     Build in Release mode
    ${GREEN}debug${NC}       Build in Debug mode (default)
    ${GREEN}run${NC}        Build and run the game
    ${GREEN}run-world${NC}  Build and run with world: ./build.sh run-world <world-name>

Options:
    ${GREEN}-j <jobs>${NC}  Number of parallel jobs (default: auto-detect)
    ${GREEN}-v${NC}        Verbose build output
    ${GREEN}-h${NC}        Show this help

Examples:
    ./build.sh                  # Debug build
    ./build.sh release          # Release build
    ./build.sh rebuild -j8      # Clean and build with 8 jobs
    ./build.sh run              # Build and run
    ./build.sh run-world Swiat  # Build and load world

EOF
}

print_status() {
    echo -e "${BLUE}▶${NC} $1"
}

print_success() {
    echo -e "${GREEN}✓${NC} $1"
}

print_error() {
    echo -e "${RED}✗${NC} $1" >&2
}

# Check dependencies
check_dependencies() {
    print_status "Checking dependencies..."
    
    for cmd in cmake ninja; do
        if ! command -v "$cmd" &> /dev/null; then
            print_error "$cmd not found"
            echo "Install: sudo apt-get install $cmd"
            exit 1
        fi
    done
    
    print_success "Dependencies OK"
}

# Create build directory
setup_build_dir() {
    if [ ! -d "$BUILD_DIR" ]; then
        print_status "Creating build directory..."
        mkdir -p "$BUILD_DIR"
    fi
}

# Configure CMake
configure() {
    print_status "Configuring CMake (${BUILD_TYPE})..."
    
    cd "$BUILD_DIR"
    cmake -G Ninja \
        -DCMAKE_BUILD_TYPE="$BUILD_TYPE" \
        -DCMAKE_INSTALL_PREFIX="$BUILD_DIR/install" \
        ${VERBOSE:+-DCMAKE_VERBOSE_MAKEFILE=ON} \
        "$SCRIPT_DIR"
    cd - > /dev/null
}

# Build project
build() {
    local jobs="${JOBS:-}"
    
    print_status "Building Blockbit (${BUILD_TYPE})..."
    
    cd "$BUILD_DIR"
    if [ -z "$jobs" ]; then
        ninja
    else
        ninja -j "$jobs"
    fi
    cd - > /dev/null
}

# Main script logic
main() {
    # Check for help flag first
    if [[ "$1" == "-h" || "$1" == "--help" ]]; then
        print_help
        exit 0
    fi
    
    local command="${1:-build}"
    shift || true
    
    # Parse options
    while [[ $# -gt 0 ]]; do
        case $1 in
            -j)
                JOBS="$2"
                shift 2
                ;;
            -v|--verbose)
                VERBOSE=1
                shift
                ;;
            -h|--help)
                print_help
                exit 0
                ;;
            *)
                WORLD_NAME="$1"
                shift
                ;;
        esac
    done
    
    check_dependencies
    setup_build_dir
    
    case "$command" in
        build)
            configure
            build
            print_success "Build complete! Binary: ${BUILD_DIR}/bin/Blockbit"
            ;;
        clean)
            print_status "Cleaning build directory..."
            rm -rf "$BUILD_DIR"
            print_success "Cleaned"
            ;;
        rebuild)
            "$0" clean
            "$0" build ${JOBS:+-j "$JOBS"} ${VERBOSE:+-v}
            ;;
        debug)
            BUILD_TYPE="Debug" "$0" build ${JOBS:+-j "$JOBS"} ${VERBOSE:+-v}
            ;;
        release)
            BUILD_TYPE="Release" "$0" build ${JOBS:+-j "$JOBS"} ${VERBOSE:+-v}
            ;;
        run)
            configure
            build
            print_status "Running Blockbit..."
            echo ""
            cd "$SCRIPT_DIR"
            "$BUILD_DIR/bin/Blockbit"
            ;;
        run-world)
            if [ -z "$WORLD_NAME" ]; then
                print_error "World name required"
                echo "Usage: $0 run-world <world-name>"
                exit 1
            fi
            configure
            build
            print_status "Running Blockbit with world: $WORLD_NAME"
            echo ""
            cd "$SCRIPT_DIR"
            "$BUILD_DIR/bin/Blockbit" --load "$WORLD_NAME"
            ;;
        *)
            print_error "Unknown command: $command"
            print_help
            exit 1
            ;;
    esac
}

# Run main function
main "$@"
