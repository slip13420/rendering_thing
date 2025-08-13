
### 3. User Stories

User stories are short, simple descriptions of a feature told from the perspective of the user.

**Epic: Project Setup and Scaffolding**
* **As a new contributor**, I want a **cross-platform build system**, so that I can easily compile the project on my operating system.
* **As a developer**, I want to be able to **get all external dependencies automatically**, so that I can set up the development environment quickly.
* **As a user**, I want the project to have a **clear, documented file structure**, so that I can easily navigate the codebase.

**Epic: Core Rendering Engine**
* **As a user**, I want the application to render a 3D scene using a **path tracing algorithm**, so that I can see realistic lighting and shadows.
* **As a user**, I want to be able to **control the camera position**, so that I can view the scene from different perspectives.
* **As a user**, I want to be able to **start and stop the rendering process**, so that I have control over when the final image is generated.
* **As a user**, I want to see the image **progressively render** with a low-sample preview that refines over time, so that I can get real-time feedback on the rendering process.
* **As a user**, I want to **save the final rendered image to a file**, so that I can export my work.

**Epic: Scene & Primitive Management**
* **As a user**, I want to **add and remove primitives** (torus, sphere, pyramid, and cube) from the scene, so that I can build a custom scene.
* **As a user**, I want the primitives in the scene to have a **continuous spinning animation**, so that I can see the effects of lighting and reflections in motion.

**Epic: User Interface**
* **As a user**, I want to see a **self-rendered UI overlay**, so that I can interact with the application.
* **As a user**, I want to see **checkboxes for each primitive**, so that I can easily toggle their visibility in the scene.
* **As a user**, I want to see an **input field for the camera position**, so that I can precisely control my viewpoint.
* **As a user**, I want to see a **progress bar**, so that I can track the status of the rendering process.
