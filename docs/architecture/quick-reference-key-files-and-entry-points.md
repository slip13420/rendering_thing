# Quick Reference - Key Files and Entry Points

## Critical Files for Understanding the System

- **Main Entry**: `src/main/main.cpp` - Application initialization and main loop
- **Configuration**: `CMakeLists.txt`, `cmake/dependencies.cmake` - Build system and dependency management
- **Core Business Logic**: `src/core/scene_manager.{cpp,h}`, `src/core/primitives.{cpp,h}`
- **Rendering Pipeline**: `src/render/path_tracer.{cpp,h}`, `src/render/render_engine.{cpp,h}`
- **Performance Critical**: `src/render/path_tracer.cpp` - Ray intersection calculations (CPU bottleneck)

## GPU Acceleration Impact Areas

**High Priority for GPU Integration**:
- `src/render/path_tracer.{cpp,h}` - Primary compute workload
- `src/render/render_engine.{cpp,h}` - Coordination between CPU/GPU
- `src/core/scene_manager.{cpp,h}` - Scene data GPU memory management
- `cmake/dependencies.cmake` - GPU library integration
