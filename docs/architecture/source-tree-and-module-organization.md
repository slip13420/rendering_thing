# Source Tree and Module Organization

## Project Structure (Current - Pre-GPU)

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

## Key Modules and Their GPU Integration Requirements

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
