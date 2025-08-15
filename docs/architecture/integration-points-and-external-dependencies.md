# Integration Points and External Dependencies

## Current External Dependencies

| Service | Purpose         | Integration Type | Key Files                      |
| ------- | --------------- | ---------------- | ------------------------------ |
| SDL2    | Windowing/Input | Static Library   | `cmake/dependencies.cmake`     |
| GLFW    | Windowing Alt   | Static Library   | `cmake/dependencies.cmake`     |

## GPU Integration Architecture Requirements

### GPU Compute Integration Points

| Component        | Current CPU Implementation                | Required GPU Enhancement                    |
| ---------------- | ----------------------------------------- | ------------------------------------------- |
| **Path Tracer**  | `PathTracer::ray_color()` recursive CPU  | GPU compute shader with thread groups      |
| **Scene Data**   | `std::vector<Primitive>` CPU memory      | GPU buffer objects with efficient transfer |
| **Random Gen**   | `std::mt19937` per-thread                 | GPU-parallel RNG (cuRAND or compute)       |
| **Image Buffer** | `std::vector<Color>` CPU array           | GPU texture/buffer with CPU readback       |

### Proposed GPU Architecture Components

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

### External Dependencies for GPU Acceleration

| GPU API    | Integration Complexity | Performance | Cross-Platform | Recommendation        |
| ---------- | ---------------------- | ----------- | -------------- | --------------------- |
| OpenGL CS  | Medium                 | Good        | Excellent      | **RECOMMENDED START** |
| CUDA       | Medium                 | Excellent   | NVIDIA only    | High-end option       |
| Vulkan     | High                   | Excellent   | Good           | Future enhancement    |
| OpenCL     | Medium                 | Good        | Good           | Alternative option    |
