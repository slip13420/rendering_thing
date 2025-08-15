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
    echo "Build Options:"
    echo "  -c, --clean          Clean build directory before building"
    echo "  -r, --release        Build in Release mode (default: Debug)"
    echo "  -j, --jobs N         Number of parallel jobs for building (default: auto-detect)"
    echo ""
    echo "Windowing Options:"
    echo "  --use-glfw           Use GLFW instead of SDL for windowing"
    echo "  --no-windowing       Build without windowing support (console only)"
    echo ""
    echo "GPU Acceleration Options:"
    echo "  --gpu                Enable GPU acceleration (requires OpenGL 4.3+)"
    echo "  --no-gpu             Disable GPU acceleration (CPU-only build)"
    echo "  --gpu-debug          Enable GPU debugging output"
    echo ""
    echo "Other Options:"
    echo "  -h, --help           Show this help message"
    echo ""
    echo "Examples:"
    echo "  $0                   Build with default settings (Debug, auto-detect windowing, no GPU)"
    echo "  $0 -r --gpu          Build Release with GPU acceleration"
    echo "  $0 --clean --gpu     Clean build with GPU acceleration"
    echo "  $0 --no-windowing    Build console-only version"
    echo ""
}

CLEAN_BUILD=false
BUILD_TYPE="Debug"
NUM_JOBS=$(nproc 2>/dev/null || sysctl -n hw.ncpu 2>/dev/null || echo "4")
USE_GLFW=false
NO_WINDOWING=false
USE_GPU=""
GPU_DEBUG=false

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
        --gpu)
            USE_GPU="ON"
            shift
            ;;
        --no-gpu)
            USE_GPU="OFF"
            shift
            ;;
        --gpu-debug)
            GPU_DEBUG=true
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

# Windowing configuration
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

# GPU acceleration configuration
if [ -n "$USE_GPU" ]; then
    CMAKE_ARGS="$CMAKE_ARGS -DUSE_GPU=$USE_GPU"
    if [ "$USE_GPU" = "ON" ]; then
        echo "GPU acceleration enabled (requires OpenGL 4.3+)"
        if [ "$GPU_DEBUG" = true ]; then
            CMAKE_ARGS="$CMAKE_ARGS -DGPU_DEBUG=ON"
            echo "GPU debugging enabled"
        fi
    else
        echo "GPU acceleration disabled (CPU-only build)"
    fi
else
    echo "GPU acceleration auto-detect (based on OpenGL availability)"
fi

echo "CMake configuration: $CMAKE_ARGS"
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