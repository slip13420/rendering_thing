# Data Models and APIs

## Current Data Models

**Key Data Structures** (from src/core/common.h and headers):

- **Vector3**: Basic 3D vector for positions, directions, colors
- **Ray**: Ray structure with origin and direction for path tracing
- **Camera**: Camera positioning and projection parameters
- **Primitive Classes**: Object-oriented design with virtual intersection methods

**Critical for GPU**: Current object-oriented design needs flattening for GPU efficiency

## API Specifications

**Current Rendering API** (from src/render/path_tracer.h):
- `trace()` - Main ray tracing entry point
- `trace_progressive()` - Progressive rendering with callbacks
- `ray_color()` - Recursive ray intersection and color calculation

**GPU Integration Points**:
- GPU compute dispatch interfaces needed
- GPU memory management APIs required
- CPU-GPU synchronization primitives
