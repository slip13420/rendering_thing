# Naming Conventions

## File Naming

### Source Files (.cpp)
- Use lowercase with underscores for separation
- Examples: `scene_manager.cpp`, `ui_input.cpp`, `path_tracer.cpp`

### Header Files (.h)
- Match corresponding source file names
- Examples: `scene_manager.h`, `ui_input.h`, `path_tracer.h`
- Common headers can be descriptive: `common.h`

### Directory Names
- Use lowercase, descriptive names
- Separate words with underscores if needed
- Examples: `ui/`, `core/`, `render/`, `main/`

## Code Naming

### Classes
- Use PascalCase (CapitalizedWords)
- Examples: `SceneManager`, `UIManager`, `RenderEngine`, `PathTracer`

### Functions/Methods
- Use camelCase (lowerCamelCase)
- Examples: `renderFrame()`, `processInput()`, `traceRay()`

### Variables
- Use camelCase for local variables
- Use snake_case for member variables with trailing underscore
- Examples: `rayCount`, `maxDepth`, `scene_data_`

### Constants
- Use UPPER_CASE with underscores
- Examples: `MAX_RAY_DEPTH`, `DEFAULT_SAMPLES`, `PI_VALUE`

## File Organization Rules

### Header File Placement
- Public headers go in `include/` directory structure
- Private headers stay with source files in `src/` directories
- Mirror the `src/` directory structure in `include/`

### Source File Placement
- Implementation files go in corresponding `src/` subdirectories
- Main entry point in `src/main/main.cpp`
- Group related functionality in same directory

### Test File Placement
- Test files go in corresponding `tests/` subdirectories
- Name test files with `_test` suffix: `scene_manager_test.cpp`
- Mirror the `src/` directory structure in `tests/`

## Directory Structure Rules

### Component Organization
- `src/core/` - Core logic components (Scene Manager, Primitives)
- `src/ui/` - User interface components (UI Manager, Input handling)
- `src/render/` - Rendering components (Render Engine, Path Tracer, Image Output)
- `src/main/` - Application entry point

### Support Directories  
- `include/` - Public header files mirroring src structure
- `tests/` - Unit tests mirroring src structure
- `assets/` - Resources, shaders, sample scenes
- `docs/` - Documentation files
- `cmake/` - CMake modules and configuration files
- `build/` - Build output (generated, not committed)

## Examples

### New Component Creation
When adding a new component called "LightManager":

1. Source: `src/render/light_manager.cpp`
2. Header: `src/render/light_manager.h` (private) or `include/render/light_manager.h` (public)
3. Test: `tests/render/light_manager_test.cpp`
4. Class name: `LightManager`
5. Main method: `calculateLighting()`

### New Utility Creation
When adding a utility called "MathUtils":

1. Source: `src/core/math_utils.cpp`
2. Header: `include/core/math_utils.h`
3. Test: `tests/core/math_utils_test.cpp`
4. Namespace: `math` (lowercase)
5. Functions: `dot()`, `cross()`, `normalize()`