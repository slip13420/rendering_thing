# New Renderer Brownfield Architecture Document

## Introduction

This document captures the CURRENT STATE of the New Renderer path tracing project, including the existing CPU-based implementation and architectural requirements for GPU acceleration integration. It serves as a reference for AI agents implementing Epic 2.5 (GPU Acceleration) before Epic 3 (Scene & Primitive Management).

### Document Scope

**Focused on areas relevant to: GPU Acceleration Integration (Epic 2.5)**
- Current CPU-based rendering pipeline analysis
- GPU acceleration integration points
- Performance optimization requirements
- Architectural enhancement for Epic 3 preparation

### Change Log

| Date       | Version | Description                          | Author  |
| ---------- | ------- | ------------------------------------ | ------- |
| 2025-08-15 | 1.0     | Initial brownfield analysis for GPU  | Winston |

## Quick Reference - Key Files and Entry Points

### Critical Files for Understanding the System

- **Main Entry**: `src/main/main.cpp` - Application initialization and main loop
- **Configuration**: `CMakeLists.txt`, `cmake/dependencies.cmake` - Build system and dependency management
- **Core Business Logic**: `src/core/scene_manager.{cpp,h}`, `src/core/primitives.{cpp,h}`
- **Rendering Pipeline**: `src/render/path_tracer.{cpp,h}`, `src/render/render_engine.{cpp,h}`
- **Performance Critical**: `src/render/path_tracer.cpp` - Ray intersection calculations (CPU bottleneck)

### GPU Acceleration Impact Areas

**High Priority for GPU Integration**:
- `src/render/path_tracer.{cpp,h}` - Primary compute workload
- `src/render/render_engine.{cpp,h}` - Coordination between CPU/GPU
- `src/core/scene_manager.{cpp,h}` - Scene data GPU memory management
- `cmake/dependencies.cmake` - GPU library integration

## High Level Architecture

### Technical Summary

**Current State**: CPU-based path tracing with progressive rendering
**Target State**: Hybrid CPU/GPU with GPU-accelerated compute kernels

### Actual Tech Stack (from CMakeLists.txt and dependencies.cmake)

| Category          | Current Technology | Version     | GPU Enhancement Needed               |
| ----------------- | ------------------ | ----------- | ------------------------------------ |
| Runtime           | C++17              | GCC/Clang   | GPU compute library integration      |
| Build System      | CMake              | 3.16+       | GPU library detection and linking    |
| Windowing         | SDL2/GLFW          | 2.28.5/3.9  | GPU context creation                 |
| Graphics          | Software Rendering | CPU-only    | **ADD: OpenGL/CUDA/Vulkan compute** |
| Memory Management | Standard C++       | STL         | **ADD: GPU memory management**       |
| Threading         | std::thread        | C++17       | GPU-CPU synchronization             |

### Repository Structure Reality Check

- **Type**: Monorepo with clear separation of concerns
- **Package Manager**: CMake FetchContent for dependencies
- **Notable**: Clean architecture with established component boundaries (good for GPU integration)

## Source Tree and Module Organization

### Project Structure (Current - Pre-GPU)

```text
new_renderer/
├── src/
│   ├── core/                # Scene and primitive management
│   │   ├── scene_manager.*  # Central scene coordination (GPU memory management needed)
│   │   ├── primitives.*     # Primitive objects (GPU data structures needed)
│   │   └── camera.*         # Camera system (minimal GPU impact)
│   ├── render/              # Rendering pipeline (MAJOR GPU INTEGRATION)
│   │   ├── path_tracer.*    # CPU ray tracing (CONVERT TO GPU COMPUTE)
│   │   ├── render_engine.*  # Render coordination (GPU-CPU orchestration)
│   │   └── image_output.*   # Final image processing (GPU texture output)
│   ├── ui/                  # User interface (minimal GPU impact)
│   │   ├── ui_manager.*     # UI coordination
│   │   └── ui_input.*       # Input handling
│   └── main/                # Application entry point
├── include/                 # Public headers (GPU interface additions needed)
├── tests/                   # Unit tests (GPU compute tests needed)
├── cmake/                   # Build system (GPU library integration)
└── docs/                    # Documentation (architecture updates)
```

### Key Modules and Their GPU Integration Requirements

- **Path Tracer** (`src/render/path_tracer.*`): **CRITICAL GPU TARGET**
  - Current: CPU-based ray-sphere intersections, color calculations
  - GPU Need: Convert to compute shaders/CUDA kernels
  - Performance Impact: 5-10x expected speedup
  
- **Scene Manager** (`src/core/scene_manager.*`): **GPU MEMORY COORDINATION** 
  - Current: CPU memory management for primitives
  - GPU Need: GPU buffer management, CPU-GPU data synchronization
  
- **Render Engine** (`src/render/render_engine.*`): **GPU-CPU ORCHESTRATION**
  - Current: CPU threading and progressive rendering
  - GPU Need: GPU compute dispatch, result collection
  
- **Primitives** (`src/core/primitives.*`): **GPU DATA STRUCTURES**
  - Current: C++ classes with virtual methods
  - GPU Need: GPU-friendly data layouts, structure-of-arrays

## Data Models and APIs

### Current Data Models

**Key Data Structures** (from src/core/common.h and headers):

- **Vector3**: Basic 3D vector for positions, directions, colors
- **Ray**: Ray structure with origin and direction for path tracing
- **Camera**: Camera positioning and projection parameters
- **Primitive Classes**: Object-oriented design with virtual intersection methods

**Critical for GPU**: Current object-oriented design needs flattening for GPU efficiency

### API Specifications

**Current Rendering API** (from src/render/path_tracer.h):
- `trace()` - Main ray tracing entry point
- `trace_progressive()` - Progressive rendering with callbacks
- `ray_color()` - Recursive ray intersection and color calculation

**GPU Integration Points**:
- GPU compute dispatch interfaces needed
- GPU memory management APIs required
- CPU-GPU synchronization primitives

## Technical Debt and Known Issues

### Critical Technical Debt for GPU Integration

1. **Path Tracer Algorithm**: Currently uses object-oriented ray intersection
   - **Impact**: Virtual function calls not GPU-friendly
   - **Solution**: Flatten to GPU-compatible data structures

2. **Memory Layout**: Current primitive storage uses inheritance
   - **Impact**: Poor GPU memory access patterns
   - **Solution**: Structure-of-arrays for GPU efficiency

3. **Threading Model**: CPU-only std::thread coordination
   - **Impact**: No GPU-CPU work distribution
   - **Solution**: GPU compute queue integration

### Performance Bottlenecks (Current CPU Implementation)

- **Ray-Sphere Intersection**: ~70% of compute time in `ray_color()` function
- **Random Number Generation**: Serial RNG limits parallel scaling
- **Memory Access**: Object-oriented design causes cache misses
- **Progressive Rendering**: CPU thread coordination overhead

### Workarounds and Gotchas for GPU Development

- **Cross-Platform Compatibility**: Must support Windows/Linux/macOS GPU drivers
- **Memory Synchronization**: GPU-CPU data transfer overhead critical
- **Precision Differences**: GPU float precision vs CPU double precision
- **Debug Complexity**: GPU compute shader debugging more difficult than CPU

## Integration Points and External Dependencies

### Current External Dependencies

| Service | Purpose         | Integration Type | Key Files                      |
| ------- | --------------- | ---------------- | ------------------------------ |
| SDL2    | Windowing/Input | Static Library   | `cmake/dependencies.cmake`     |
| GLFW    | Windowing Alt   | Static Library   | `cmake/dependencies.cmake`     |

### GPU Integration Architecture Requirements

#### GPU Compute Integration Points

| Component        | Current CPU Implementation                | Required GPU Enhancement                    |
| ---------------- | ----------------------------------------- | ------------------------------------------- |
| **Path Tracer**  | `PathTracer::ray_color()` recursive CPU  | GPU compute shader with thread groups      |
| **Scene Data**   | `std::vector<Primitive>` CPU memory      | GPU buffer objects with efficient transfer |
| **Random Gen**   | `std::mt19937` per-thread                 | GPU-parallel RNG (cuRAND or compute)       |
| **Image Buffer** | `std::vector<Color>` CPU array           | GPU texture/buffer with CPU readback       |

#### Proposed GPU Architecture Components

**New Components Needed**:

1. **GPU Memory Manager** (`src/render/gpu_memory.*`)
   - GPU buffer allocation and management
   - CPU-GPU data synchronization
   - Memory pool optimization

2. **GPU Compute Pipeline** (`src/render/gpu_compute.*`)
   - Compute shader/kernel management
   - GPU dispatch coordination
   - Performance profiling integration

3. **GPU-CPU Hybrid Path Tracer** (enhance existing `path_tracer.*`)
   - GPU compute kernel for ray tracing
   - CPU fallback for compatibility
   - Performance comparison and switching

#### External Dependencies for GPU Acceleration

| GPU API    | Integration Complexity | Performance | Cross-Platform | Recommendation        |
| ---------- | ---------------------- | ----------- | -------------- | --------------------- |
| OpenGL CS  | Medium                 | Good        | Excellent      | **RECOMMENDED START** |
| CUDA       | Medium                 | Excellent   | NVIDIA only    | High-end option       |
| Vulkan     | High                   | Excellent   | Good           | Future enhancement    |
| OpenCL     | Medium                 | Good        | Good           | Alternative option    |

## Development and Deployment

### Current Build System Analysis

**CMake Configuration** (`CMakeLists.txt`):
- C++17 standard with cross-platform support
- FetchContent for automatic dependency resolution
- Support for both SDL2 and GLFW windowing

**GPU Integration Requirements**:
```cmake
# Required additions to CMakeLists.txt
find_package(OpenGL REQUIRED)  # For OpenGL compute shaders
# OR
find_package(CUDA REQUIRED)    # For CUDA acceleration
# OR  
find_package(Vulkan REQUIRED)  # For Vulkan compute
```

### Local Development Setup

**Current Setup** (from BUILD.md and CMakeLists.txt):
1. CMake 3.16+ required
2. C++17 compatible compiler
3. Automatic dependency download via FetchContent
4. Cross-platform build scripts (build.sh, build.bat)

**GPU Enhancement Setup**:
1. GPU drivers installed and updated
2. GPU development libraries (vendor-specific)
3. GPU debugging tools (optional but recommended)

### Dependency Management Enhancement

**Current Dependencies** (`cmake/dependencies.cmake`):
- SDL2 2.28.5 (with audio disabled for minimal build)
- GLFW 3.9 (alternative windowing)
- FetchContent-based automatic download

**GPU Dependencies to Add**:
- GPU compute library (OpenGL/CUDA/Vulkan)
- GPU math libraries (GLM for OpenGL, Thrust for CUDA)
- GPU profiling tools (optional but recommended)

## Testing Reality for GPU Integration

### Current Test Coverage

- **Unit Tests**: Core functionality in `tests/core/` (scene management, cameras)
- **Integration Tests**: Rendering pipeline coordination in `tests/render/`
- **Render Tests**: Image output validation and progressive rendering
- **Performance Tests**: Progressive rendering timing

### GPU Testing Requirements

**New Test Categories Needed**:

1. **GPU Compute Tests** (`tests/render/gpu_compute_test.cpp`)
   - GPU kernel correctness validation
   - CPU-GPU result comparison
   - Memory transfer performance

2. **Performance Benchmarks** (`tests/performance/`)
   - CPU vs GPU rendering time comparison
   - Scaling tests with scene complexity
   - Memory usage analysis

3. **Cross-Platform GPU Tests**
   - GPU driver compatibility validation
   - Fallback to CPU testing
   - Platform-specific GPU features

### Running Tests

```bash
# Current test commands
cd build && ctest                    # Run all unit tests
./path_tracer_test                   # Manual test execution

# Future GPU test commands
ctest -L gpu                         # Run GPU-specific tests
./path_tracer_test --gpu-benchmark   # GPU performance tests
./path_tracer_test --gpu-validate    # CPU-GPU result comparison
```

## GPU Enhancement Impact Analysis

### Files That Will Need Major Modification

**High Impact Changes**:
- `src/render/path_tracer.{cpp,h}` - GPU compute kernel implementation
- `src/render/render_engine.{cpp,h}` - GPU-CPU coordination
- `src/core/scene_manager.{cpp,h}` - GPU memory management
- `cmake/dependencies.cmake` - GPU library integration
- `CMakeLists.txt` - GPU build configuration

### New Files/Modules Needed

**New GPU Components**:
- `src/render/gpu_memory.{cpp,h}` - GPU memory management
- `src/render/gpu_compute.{cpp,h}` - GPU compute dispatch
- `src/render/shaders/` - GPU shader/kernel source files
- `include/render/gpu_*.h` - GPU public interfaces
- `tests/render/gpu_*_test.cpp` - GPU-specific testing

### Integration Considerations

**Architecture Compatibility**:
- Existing component interfaces should remain stable
- GPU acceleration should be optional (CPU fallback required)
- Progressive rendering integration with GPU timing
- Memory management must handle GPU-CPU transfers efficiently

**Build System Integration**:
- CMake GPU library detection and configuration
- Cross-platform GPU build support
- Optional GPU features with graceful degradation

### Performance Expectations

**Target Performance Improvements**:
- **Ray Tracing**: 5-10x speedup for complex scenes
- **Animation Compute**: 20-50x speedup for many primitives (Epic 3)
- **Memory Bandwidth**: Efficient GPU-CPU transfers (<5% overhead)
- **Scalability**: Linear performance scaling with GPU cores

**Performance Validation**:
- Benchmark suite comparing CPU vs GPU performance
- Memory usage profiling for GPU buffers
- Cross-platform performance consistency testing

## Appendix - GPU Development Commands and Scripts

### Frequently Used Commands (Post-GPU Integration)

```bash
# Build with GPU support
cmake -DUSE_GPU=ON -DGPU_API=OpenGL -B build
cmake --build build

# Alternative GPU APIs
cmake -DUSE_GPU=ON -DGPU_API=CUDA -B build     # NVIDIA CUDA
cmake -DUSE_GPU=ON -DGPU_API=Vulkan -B build   # Vulkan compute

# Run with GPU acceleration
./build/path_tracer --gpu-accelerated --gpu-debug

# Performance comparison
./build/path_tracer --benchmark --compare-cpu-gpu

# GPU memory profiling
./build/path_tracer --gpu-profile --scene-complexity=high
```

### GPU Debugging and Troubleshooting

- **GPU Logs**: Check GPU driver logs and compute shader compilation errors
- **Performance Profiling**: Use GPU vendor tools (NVIDIA Nsight, AMD RGP, Intel VTune)
- **Memory Analysis**: Monitor GPU memory usage and transfer patterns
- **Fallback Testing**: Ensure CPU path still works when GPU unavailable
- **Cross-Platform Issues**: Test GPU acceleration on different platforms and drivers

### Common GPU Development Issues

1. **GPU Memory Exhaustion**: Monitor GPU memory usage, implement memory pools
2. **Compute Shader Compilation**: Debug shader syntax and compatibility issues
3. **CPU-GPU Synchronization**: Profile transfer overhead and optimize data flow
4. **Driver Compatibility**: Test across different GPU vendors and driver versions
5. **Performance Regression**: Validate that GPU acceleration actually improves performance

---

This comprehensive brownfield architecture document provides the foundation for implementing GPU acceleration while maintaining the existing clean architecture. The focus on real-world constraints and specific file impacts should enable effective AI agent implementation of Epic 2.5 (GPU Acceleration Integration).