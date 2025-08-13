# System Architecture: High-Level Architecture

The Basic Render Pipeline is a self-contained desktop application with a clear separation of concerns. The architecture follows a classic rendering pipeline model, where data flows from a scene description through a series of modules to produce a final image. The user interface is an integral part of this pipeline, rendered directly by the application itself.

```mermaid
graph TD
    subgraph UI Layer
        UI_Input[User Input] --> UI_Manager[UI Manager];
        UI_Manager --> Scene_Manager[Scene Manager];
    end

    subgraph Core Logic
        Scene_Manager --> Render_Engine[Render Engine];
        Scene_Manager --> Primitives[Primitives & Animations];
        Render_Engine --> Path_Tracer[Path Tracer Module];
        Path_Tracer --> Scene_Manager;
        Render_Engine --> Image_Output[Image Output];
    end
    
    UI_Manager --> Render_Engine;
    Image_Output --> Screen[Screen Buffer];
    Image_Output --> File[File Output];
```
