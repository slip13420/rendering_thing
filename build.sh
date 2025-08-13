#!/bin/bash

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$SCRIPT_DIR"
BUILD_DIR="$PROJECT_ROOT/build"

echo "Path Tracer Renderer - Build Script"
echo "===================================="
echo "Platform: $(uname -s)"
echo "Architecture: $(uname -m)"
echo ""

print_usage() {
    echo "Usage: $0 [options]"
    echo ""
    echo "Options:"
    echo "  -c, --clean          Clean build directory before building"
    echo "  -r, --release        Build in Release mode (default: Debug)"
    echo "  -j, --jobs N         Number of parallel jobs for building"
    echo "  --use-glfw           Use GLFW instead of SDL for windowing"
    echo "  --no-windowing       Build without windowing support (console only)"
    echo "  -h, --help           Show this help message"
    echo ""
}

CLEAN_BUILD=false
BUILD_TYPE="Debug"
NUM_JOBS=$(nproc 2>/dev/null || sysctl -n hw.ncpu 2>/dev/null || echo "4")
USE_GLFW=false
NO_WINDOWING=false

while [[ $# -gt 0 ]]; do
    case $1 in
        -c|--clean)
            CLEAN_BUILD=true
            shift
            ;;
        -r|--release)
            BUILD_TYPE="Release"
            shift
            ;;
        -j|--jobs)
            NUM_JOBS="$2"
            shift 2
            ;;
        --use-glfw)
            USE_GLFW=true
            shift
            ;;
        --no-windowing)
            NO_WINDOWING=true
            shift
            ;;
        -h|--help)
            print_usage
            exit 0
            ;;
        *)
            echo "Unknown option: $1"
            print_usage
            exit 1
            ;;
    esac
done

if [ "$CLEAN_BUILD" = true ]; then
    echo "Cleaning build directory..."
    rm -rf "$BUILD_DIR"
fi

mkdir -p "$BUILD_DIR"
cd "$BUILD_DIR"

echo "Configuring with CMake..."
CMAKE_ARGS="-DCMAKE_BUILD_TYPE=$BUILD_TYPE"

if [ "$NO_WINDOWING" = true ]; then
    CMAKE_ARGS="$CMAKE_ARGS -DUSE_SDL=OFF -DUSE_GLFW=OFF"
    echo "Building without windowing support (console only)"
elif [ "$USE_GLFW" = true ]; then
    CMAKE_ARGS="$CMAKE_ARGS -DUSE_SDL=OFF -DUSE_GLFW=ON"
    echo "Using GLFW for windowing"
else
    CMAKE_ARGS="$CMAKE_ARGS -DUSE_SDL=ON -DUSE_GLFW=OFF"
    echo "Using SDL for windowing"
fi

cmake $CMAKE_ARGS "$PROJECT_ROOT"

echo ""
echo "Building with $NUM_JOBS parallel jobs..."
cmake --build . --config "$BUILD_TYPE" -j "$NUM_JOBS"

echo ""
echo "Build completed successfully!"
echo "Executable location: $BUILD_DIR/src/path_tracer_renderer"
echo ""
echo "To run the application:"
echo "  cd $BUILD_DIR/src && ./path_tracer_renderer"