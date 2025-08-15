# High Level Architecture

## Technical Summary

**Current State**: CPU-based path tracing with progressive rendering
**Target State**: Hybrid CPU/GPU with GPU-accelerated compute kernels

## Actual Tech Stack (from CMakeLists.txt and dependencies.cmake)

| Category          | Current Technology | Version     | GPU Enhancement Needed               |
| ----------------- | ------------------ | ----------- | ------------------------------------ |
| Runtime           | C++17              | GCC/Clang   | GPU compute library integration      |
| Build System      | CMake              | 3.16+       | GPU library detection and linking    |
| Windowing         | SDL2/GLFW          | 2.28.5/3.9  | GPU context creation                 |
| Graphics          | Software Rendering | CPU-only    | **ADD: OpenGL/CUDA/Vulkan compute** |
| Memory Management | Standard C++       | STL         | **ADD: GPU memory management**       |
| Threading         | std::thread        | C++17       | GPU-CPU synchronization             |

## Repository Structure Reality Check

- **Type**: Monorepo with clear separation of concerns
- **Package Manager**: CMake FetchContent for dependencies
- **Notable**: Clean architecture with established component boundaries (good for GPU integration)
