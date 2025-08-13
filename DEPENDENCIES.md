# Dependency Version Requirements

This document lists the exact versions of external dependencies used in this project to ensure consistent builds across all platforms.

## External Dependencies

### SDL2
- **Version**: 2.28.5
- **Repository**: https://github.com/libsdl-org/SDL.git
- **Tag**: release-2.28.5
- **Build Type**: Static library
- **Purpose**: Windowing and input handling
- **C++ Standard**: Compatible with C++17
- **Configuration**: Managed via `SDL2_VERSION` variable in `cmake/dependencies.cmake`

### GLFW
- **Version**: 3.3.9
- **Repository**: https://github.com/glfw/glfw.git
- **Tag**: 3.3.9
- **Build Type**: Static library
- **Purpose**: Alternative windowing and input handling
- **C++ Standard**: Compatible with C++17
- **Configuration**: Managed via `GLFW_VERSION` variable in `cmake/dependencies.cmake`

## Dependency Resolution Strategy

1. **System Check**: First attempt to find system-installed versions
2. **Automatic Download**: If not found or incomplete, automatically download and build from source
3. **Version Locking**: Specific versions are pinned to ensure reproducible builds
4. **Cross-Platform**: Works consistently across Linux, Windows, and macOS

## Build Requirements

- CMake 3.16 or higher
- C++17 compatible compiler
- Internet connection for initial dependency download
- Git for repository cloning

## Troubleshooting

If dependency resolution fails:

1. Check internet connectivity for initial download
2. Ensure CMake version meets requirements
3. Verify compiler supports C++17
4. Check build logs for specific error messages
5. Try cleaning build directory and reconfiguring

## Offline Builds

After the first successful build, dependencies are cached in the build directory. Subsequent builds can work offline as long as the build cache is preserved.