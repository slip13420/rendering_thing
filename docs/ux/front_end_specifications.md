# UX: Updated Front-End Specifications

The UI is rendered using the application's own graphics pipeline, not a separate technology like HTML/CSS.

- UI Components: All UI elements (buttons, checkboxes, text) will be represented as textured geometric primitives within the scene.
- Input Handling: The application will handle raw mouse and keyboard input. Mouse clicks will be translated into ray intersection tests against the UI geometry to determine which element was "clicked."
- Interaction Logic: User actions on the UI will directly trigger functions within the renderer's internal API (e.g., a click on "Render Scene" calls the RenderEngine's StartRender function).

This UX document provides a clear blueprint for the user-facing part of the Basic Render Pipeline, ensuring that the project's design remains simple, consistent, and aligned with its educational purpose.
