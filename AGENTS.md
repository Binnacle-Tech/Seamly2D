# AI Developer Rules & Guidelines: Development-3D Branch

## 1. Core Architectural Principle: Adjunct & Non-Invasive
* **Adjacent Code Only:** All 3D features, tools, and UI extensions must live in separate, modular directories, classes, or namespaces (e.g., Seamly3D, separate plugin wrappers, or dedicated modules). 
* **Minimize Upstream Conflicts:** Avoid modifying existing source files from the main develop branch wherever possible. This ensures we can cleanly pull downstream updates and bug fixes from the main Seamly2D repository without merge conflicts.
* **Non-Destructive Integration:** If an integration point into the core 2D application is absolutely required, use clean observer patterns, signals/slots, or lightweight wrappers rather than rewriting core classes.

---

## 2. Scope of 3D Development
This branch covers the full spectrum of 3D features for Seamly2D, including:
* **Visualization:** Rendering 2D patterns and meshes in 3D space (using Qt3D, OpenGL, or lightweight native renderers).
* **Avatars & Measurements:** Importing, managing, and mapping 3D body models (e.g., OBJ/FBX handling) and linking them to Seamly2D measurement files (.vit).
* **Geometry & Simulation:** 3D-to-2D/2D-to-3D math transformations, UV mapping, mesh flattening, and cloth simulation logic.

---

## 3. Tech Stack & Compatibility
* **Language & Standard:** C++ matching the project's target C++ standard (C++17/C++20 as defined by CMake).
* **Framework:** Qt (Qt5/Qt6) for UI components and 3D graphics rendering.
* **Build System:** CMake. Any new files or modules must be cleanly integrated via modular CMakeLists.txt additions without disrupting the main project structure.
* **Dependencies:** Prefer native Qt libraries or header-only math/geometry libraries. Do not introduce heavy third-party dependencies unless explicitly requested.
* **Compatibility:** Never modify existing .val (pattern) or .vit (measurement) file XML schemas in a way that breaks backward compatibility with standard Seamly2D files.

---

## 4. AI Code Generation Guidelines
* **Code Compatibility:** Match the existing codebase's coding conventions (Qt-style naming, smart pointers where applicable, clean memory management).
* **Self-Contained Output:** When writing code for new features, deliver complete, modular components that plug into the architecture without requiring edits to core legacy files.
