# Project Structure

This document describes the organization and purpose of directories and files in the New Renderer project.

## Directory Overview

```
new_renderer/
├── src/                    # Source code (implementation files)
├── include/                # Public header files
├── tests/                  # Unit tests and test files
├── assets/                 # Resources and assets
├── docs/                   # Documentation
├── cmake/                  # CMake modules and build configuration
├── build/                  # Build output (generated)
├── BUILD.md                # Build instructions
├── DEPENDENCIES.md         # Dependency documentation
├── NAMING_CONVENTIONS.md   # Code and file naming standards
└── PROJECT_STRUCTURE.md    # This file
```

## Source Code Organization (`src/`)

The source code is organized by architectural components following the system's separation of concerns:

### `src/core/`
**Purpose**: Core logic components that handle fundamental application functionality.

**Contents**:
- `common.h` - Shared data structures, constants, and utility definitions
- `scene_manager.{cpp,h}` - Scene management and object coordination
- `primitives.{cpp,h}` - Geometric primitive objects (Sphere, Cube, etc.)

**Responsibility**: Manages the core application logic, scene data, and fundamental geometric objects.

### `src/ui/`
**Purpose**: User interface layer components for windowing and input handling.

**Contents**:
- `ui_manager.{cpp,h}` - Main UI system coordination and window management
- `ui_input.{cpp,h}` - User input processing and event handling

**Responsibility**: Handles all user interface interactions, window management, and input processing.

### `src/render/`
**Purpose**: Rendering pipeline components for image generation and path tracing.

**Contents**:
- `render_engine.{cpp,h}` - Main rendering system coordination
- `path_tracer.{cpp,h}` - Path tracing algorithm implementation
- `image_output.{cpp,h}` - Image file output and format handling

**Responsibility**: Implements the rendering pipeline, path tracing algorithms, and image output functionality.

### `src/main/`
**Purpose**: Application entry point and initialization.

**Contents**:
- `main.cpp` - Application entry point with initialization and main loop

**Responsibility**: Application startup, component initialization, and main execution loop.

## Header Files (`include/`)

Public header files are organized to mirror the source structure, providing clean interfaces for component interaction.

### `include/core/`
Public interfaces for core functionality components.

### `include/ui/`  
Public interfaces for user interface components.

### `include/render/`
Public interfaces for rendering components.

**Organization Principle**: Headers in `include/` are public interfaces, while headers co-located with source files in `src/` are private implementation details.

## Tests (`tests/`)

Unit tests organized by component, mirroring the source structure for easy navigation.

### `tests/core/`
Tests for core logic components.

### `tests/ui/`
Tests for user interface components.

### `tests/render/`
Tests for rendering components.

**Naming Convention**: Test files use `*_test.cpp` suffix (e.g., `scene_manager_test.cpp`).

## Assets (`assets/`)

Resource files used by the application during runtime.

**Intended Contents**:
- Scene definition files
- Shader files (if needed)
- Sample input data
- Reference images for testing
- Configuration files

## Documentation (`docs/`)

Project documentation including architecture, requirements, and development guides.

**Key Files**:
- `architecture.md` - System architecture overview
- `prd.md` - Product Requirements Document
- `stories/` - Development user stories
- Architecture breakdown in `architecture/` subdirectory

## Build System (`cmake/`)

CMake modules and build configuration files.

**Contents**:
- `FindSDL2.cmake` - Custom SDL2 detection module
- `dependencies.cmake` - Automatic dependency management configuration

## Generated Files (`build/`)

Build output directory (auto-generated, not committed to version control).

**Contents**:
- Compiled object files
- Executable binaries
- CMake cache and temporary files

## Architectural Alignment

The directory structure directly maps to the system architecture:

### UI Layer
- **Components**: UI Manager, User Input
- **Location**: `src/ui/` and `include/ui/`
- **Purpose**: Handle user interface and input processing

### Core Logic Layer  
- **Components**: Scene Manager, Primitives & Animations
- **Location**: `src/core/` and `include/core/`
- **Purpose**: Manage application logic and scene data

### Rendering Layer
- **Components**: Render Engine, Path Tracer Module, Image Output
- **Location**: `src/render/` and `include/render/`
- **Purpose**: Handle rendering pipeline and image generation

## Navigation Guide

### Finding Existing Code
- **UI functionality**: Look in `src/ui/` and `include/ui/`
- **Core logic**: Look in `src/core/` and `include/core/`  
- **Rendering code**: Look in `src/render/` and `include/render/`
- **Application startup**: Check `src/main/main.cpp`
- **Tests**: Mirror the source structure in `tests/`

### Adding New Code
- **New UI component**: Add to `src/ui/` with header in `include/ui/`
- **New core component**: Add to `src/core/` with header in `include/core/`
- **New rendering feature**: Add to `src/render/` with header in `include/render/`
- **Tests for new code**: Add to corresponding `tests/` subdirectory
- **Resources**: Place in `assets/` directory

### Build Integration
- **Source files**: Automatically discovered by CMake in `src/` subdirectories
- **Include paths**: `include/` directory is automatically added to include paths
- **Dependencies**: Managed automatically via `cmake/dependencies.cmake`
- **Build output**: Generated in `build/` directory

## File Organization Principles

1. **Separation of Concerns**: Each directory has a single, clear responsibility
2. **Mirrored Structure**: `include/` and `tests/` mirror the `src/` organization  
3. **Architectural Alignment**: Directory structure matches system architecture
4. **Discoverability**: Related files are grouped together logically
5. **Build System Integration**: Structure supports CMake's file discovery patterns

## Maintenance Guidelines

- Keep the structure aligned with the architecture as components are added
- Update this documentation when new directories are created
- Ensure new files follow the established naming conventions
- Mirror any new `src/` subdirectories in `include/` and `tests/`
- Place resources in appropriate `assets/` subdirectories