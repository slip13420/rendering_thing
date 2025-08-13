# System Architecture: Component Breakdown

## UI Layer
This is the user-facing part of the application. It's a self-rendered, 2D overlay on top of the 3D scene.

- UI Manager: Translates raw mouse and keyboard input into UI-specific actions. It handles events like button clicks and checkbox toggles.
- UI Input: The windowing library (e.g., SDL or GLFW) provides raw input events.
- Image Output: The final rendered pixels are sent to an internal screen buffer for display.

## Core Logic
The "back-end" of the application, which contains the rendering and scene management services.

- Scene Manager: Maintains the state of the 3D world. Stores all primitives, their properties, and light sources. Provides an API for the UI to add, remove, and modify objects.
- Primitives & Animations: Contains the data structures for each primitive (torus, sphere, pyramid, cube) and the logic for their spinning animations. Managed by the Scene Manager.
- Render Engine: Orchestrates the rendering process. Initiates the rendering thread, manages the Path Tracer's execution for each pixel, and sends the final image data to the Image Output module.
- Path Tracer Module: Implements the path tracing algorithm, including ray generation, intersection tests, and light simulation. Queries the Scene Manager for scene information.
- Image Output: Handles the final pixel data. Writes the image to the screen buffer for display and can also save the image to a file.
