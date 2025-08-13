# Build Instructions - Path Tracer Renderer

## Overview

This document provides comprehensive instructions for building the Path Tracer Renderer on different operating systems. The project uses CMake as the build system and supports C++17 or newer.

## Prerequisites

### All Platforms
- C++17 compatible compiler (GCC 7+, Clang 5+, MSVC 2017+)
- CMake 3.16 or newer
- Git (for cloning the repository and automatic dependency download)
- Internet connection (for initial dependency download)

**🎉 No Manual Dependency Installation Required!**

This project uses automatic dependency management. External libraries (SDL2, GLFW) are automatically downloaded and built during the CMake configuration process. You no longer need to manually install SDL2 or GLFW on your system.

📋 **See [DEPENDENCIES.md](DEPENDENCIES.md) for detailed dependency version information and troubleshooting.**

### Optional: System Dependencies (Fallback)
If you prefer to use system-installed libraries, they will be detected automatically:

**Linux:**
```bash
# Optional - system SDL2 (will fallback to automatic download if not found)
sudo apt install libsdl2-dev    # Ubuntu/Debian
sudo dnf install SDL2-devel     # Fedora/RHEL
sudo pacman -S sdl2             # Arch Linux
```

**macOS:**
```bash
# Optional - system SDL2 (will fallback to automatic download if not found)
brew install sdl2
```

**Windows:**
- No manual installation required - dependencies are automatically managed

## Build Options

The project supports several build configurations:

### Windowing Libraries
- **SDL** (default): Cross-platform windowing and input
- **GLFW**: Alternative windowing library
- **None**: Console-only build without windowing support

### Build Types
- **Debug** (default): Development build with debug symbols
- **Release**: Optimized production build

## Quick Build

### Linux/macOS
```bash
git clone <repository-url>
cd new_renderer

# Quick build with defaults (SDL, Debug)
./build.sh

# Run the application
cd build/src && ./path_tracer_renderer
```

### Windows
```cmd
git clone <repository-url>
cd new_renderer

# Quick build with defaults
build.bat

# Run the application
cd build\src\Debug && path_tracer_renderer.exe
```

## Advanced Build Options

### Linux/macOS Build Script Options
```bash
./build.sh [options]

Options:
  -c, --clean          Clean build directory before building
  -r, --release        Build in Release mode (default: Debug)
  -j, --jobs N         Number of parallel jobs for building
  --use-glfw           Use GLFW instead of SDL for windowing
  --no-windowing       Build without windowing support (console only)
  -h, --help           Show help message

Examples:
./build.sh --clean --release --jobs 8
./build.sh --use-glfw --release
./build.sh --no-windowing
```

### Windows Build Script Options
```cmd
build.bat [options]

Options:
  -c, --clean          Clean build directory before building
  -r, --release        Build in Release mode (default: Debug)
  -j, --jobs N         Number of parallel jobs for building
  --use-glfw           Use GLFW instead of SDL for windowing
  -h, --help           Show help message

Examples:
build.bat --clean --release --jobs 4
build.bat --use-glfw --release
```

## Manual CMake Build

If you prefer to use CMake directly:

### Basic Build
```bash
mkdir build
cd build
cmake ..
cmake --build . --config Debug
```

### With Options
```bash
mkdir build
cd build

# Release build with SDL
cmake -DCMAKE_BUILD_TYPE=Release -DUSE_SDL=ON -DUSE_GLFW=OFF ..

# Debug build with GLFW
cmake -DCMAKE_BUILD_TYPE=Debug -DUSE_SDL=OFF -DUSE_GLFW=ON ..

# Console-only build
cmake -DCMAKE_BUILD_TYPE=Release -DUSE_SDL=OFF -DUSE_GLFW=OFF ..

cmake --build . --config Release -j 4
```

## Cross-Platform Build Matrix

| Platform | Compiler | SDL Support | GLFW Support | Console Only |
|----------|----------|-------------|--------------|--------------|
| Linux    | GCC/Clang| ✅          | ✅           | ✅           |
| macOS    | Clang    | ✅          | ✅           | ✅           |
| Windows  | MSVC     | ✅          | ✅           | ✅           |

## Troubleshooting

### Automatic Dependency Download Issues

**Problem: Dependencies fail to download**
```
CMake Error: Failed to configure dependencies: SDL2::SDL2, SDL2::SDL2main
```

**Solutions:**
1. **Check internet connection** - Initial download requires internet access
2. **Clear build cache** - `rm -rf build/` and try again
3. **Verify CMake version** - Ensure CMake 3.16 or newer is installed
4. **Check compiler** - Ensure C++17 compatible compiler is available
5. **Corporate firewalls** - May block Git HTTPS clones, try using system libraries

**Problem: Build fails during dependency compilation**
This may happen with newer system libraries conflicting with downloaded versions.

**Solutions:**
1. **Use system libraries** - Install system SDL2/GLFW as fallback
   ```bash
   # Linux
   sudo apt install libsdl2-dev libglfw3-dev  # Ubuntu/Debian
   sudo dnf install SDL2-devel glfw-devel     # Fedora/RHEL
   
   # macOS
   brew install sdl2 glfw
   ```
2. **Clean build** - `rm -rf build/` and reconfigure
3. **Try different windowing library** - Use `-DUSE_GLFW=ON -DUSE_SDL=OFF`

### Legacy Manual Installation (Not Recommended)

**Only needed if automatic dependency management fails:**

**SDL2 Manual Installation:**
- Linux: `sudo apt install libsdl2-dev`
- macOS: `brew install sdl2`  
- Windows: Download from https://www.libsdl.org/download-2.0.php

**GLFW Manual Installation:**
- Linux: `sudo apt install libglfw3-dev`
- macOS: `brew install glfw`
- Windows: Download from https://www.glfw.org/download.html

### CMake Version Too Old
Update CMake to version 3.16 or newer:
```bash
# Linux - install from official repository or snap
sudo snap install cmake --classic

# macOS
brew upgrade cmake

# Windows - download from https://cmake.org/download/
```

### Compiler Errors
Ensure you have a C++17 compatible compiler:
- **GCC**: 7.0 or newer
- **Clang**: 5.0 or newer  
- **MSVC**: Visual Studio 2017 or newer

### Build Warnings
The project currently produces some harmless warnings about unused parameters in stub implementations. These will be resolved as the implementation progresses.

## Output Locations

### Linux/macOS
- **Executable**: `build/src/path_tracer_renderer`
- **Build files**: `build/`

### Windows
- **Debug executable**: `build/src/Debug/path_tracer_renderer.exe`
- **Release executable**: `build/src/Release/path_tracer_renderer.exe`
- **Build files**: `build/`

## Next Steps

After a successful build:
1. Run the executable to verify it works
2. Explore the source code in the `src/` directory
3. Review the architecture documentation in `docs/architecture/`
4. Check out the project requirements in `docs/prd/`

For development, use Debug builds. For distribution, use Release builds with appropriate windowing support.