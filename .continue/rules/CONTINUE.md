🚀 Project Overview
The CONTINUUM Editor Visualizer is a core component of the game editor suite, responsible for providing real-time visualization and manipulation tools within the Level Editor environment. Its primary function is to render complex scene graphs (composed of multiple components attached to entities) into a 3D viewport using an Entity-Component-System (ECS) architecture.

Purpose: To allow developers and designers to visually build, inspect, and interact with levels composed of various game entities and components. Key Technologies Used:

Language: C++ (Modern C++)
Editor UI: Dear ImGui (for all editor panels and docking)
Rendering: bgfx (Graphics backend)
Architecture: Entity-Component-System (ECS) pattern for data organization.
Core Systems: Custom systems like CoreSystem, ComponentManager, and LevelComponent.
High-level Architecture: The system follows a Model-View-Controller (MVC)-like pattern adapted for an editor:

Model: The scene graph, managed by CLevelComponent and composed of individual entities/components (CEntityComponent, CRenderComponent, etc.).
View: The 3D viewport (m_view) handles rendering the visualized geometry, selection highlights, and gizmos.
Controller/Visualizer: LevelComponentVisualizer acts as the orchestrator, handling user input (mouse clicks, key presses), managing selections (SelectionManager), and triggering visualization updates.
🛠️ Getting Started
Prerequisites
A working C++ development environment (C++17 or newer recommended).
Dependencies: ImGui, bgfx, and the internal Core Engine libraries (CoreSystem, FileSystem, etc.).
Access to the project's CMake/build system.
Installation Instructions
(Assumed build process based on structure)

Clone the repository.
Run cmake and generate build files targeting your platform.
Build the solution: make [target] or equivalent IDE command.
Basic Usage Examples
Opening a Level: The primary entry point is usually triggering the editor's load function, which calls LevelComponentVisualizer::AttachMeshFromPath(path) to initialize the visualizer with a level file.
Interacting in 3D Viewport: Use Left-click (no Alt) to pick objects; press 'F' to focus the camera on selection; use the Delete key to delete selected entities.
Adding Assets: Drag an entity asset (.entity or .json) from the "Entity Assets" panel onto the 3D viewport or a target layer to instantiate it.
Running Tests
Tests are typically located in [Test Directory]. Run them via your build system's test runner (e.g., ctest -V). Focus especially on unit tests for component serialization and selection logic.

📂 Project Structure
Shared/ImguiVizualizers/LevelComponentVisualizer.cpp: The main implementation file containing the core visualization logic, input handling, and UI rendering for the editor. It manages state across the viewport panels (3D Viewport, Layers, Properties).
CoreSystem/: Contains engine foundation classes (CoreSystem, AppConfig). This defines the overarching management of components and resources.
EntityComponent.h/cpp / TransformComponent.h/cpp, etc.: Define the fundamental data units (Components) that make up game objects, enforcing the ECS structure.
ImguiVizualizers/: Contains UI-specific visualizer logic and panels (e.g., asset browsing, layer management).
Asset Files (Assets/Entities/*.entity): External definition files containing serialized entity data used for loading into the scene.
⚙️ Development Workflow
Coding Standards & Conventions
Use modern C++ features (smart pointers, RAII where possible).
Follow Google or similar style guides regarding naming conventions (m_prefix for members, kPrefix for constants).
All public methods should be documented with clear parameter and return value descriptions.
Testing Approach
Unit Tests: Test individual components (e.g., matrix calculations in CTransformComponent, serialization/deserialization logic) in isolation.
Integration Tests: Verify that the major systems interact correctly (e.g., does selecting an object update the Properties panel?).
Visualization Logic: Requires careful testing of edge cases like zero-size bounds, disconnected component hierarchies, and multi-parent scenarios.
Build & Deployment Process
The project uses a robust build system (assumed CMake). Changes generally require:

Building locally (cmake --build .).
Running smoke tests/integration tests to ensure visual stability.
Committing changes to the main branch after peer review.
Contribution Guidelines
Please use feature branches and adhere strictly to the established coding standards. When implementing new visualizations or components, consider how they affect performance in a real-time render loop (e.g., avoid expensive calculations in Render* functions).

💡 Key Concepts
ECS Pattern: Everything revolves around Entities (the container) which possess various Components (pure data structures, like CTransformComponent, CRenderComponent). Components do not know about each other; they just provide data.
Selectable: A wrapper (CSelectable) used by the system to track what is currently selected in the editor view, regardless of the underlying component/entity structure.
Gizmo Mode: Controls the visual representation and interaction behavior (Translate, Scale, Rotate) when an entity or object is selected.
Model Matrix / Transform: The CTransformComponent provides the local position/orientation/scale. When rendering, this matrix must be combined with world transforms to achieve accurate visualization.
🏃 Common Tasks
Task: Selecting a new component in the Hierarchy View
Locate the desired node in the UI tree.
The CSelectable system handles registering and updating the selection state (m_selectionManager).
This triggers updates to dependent panels (e.g., RenderInspectorPanel reads data from m_selectionManager.GetSelected().GetOwner()).
Task: Debugging rendering artifacts
Check if the component is active (IsActive()) AND visible in the current layer context (IsInVisibleLayer()).
Ensure that necessary parent components (like CTransformComponent or CLevelComponent) are initialized and available.
Use ImGui's debugging tools to inspect the state of local variables (e.g., checking calculated matrix values).
🚨 Troubleshooting
Issue: Nothing renders in the viewport.
Solution: Check that the RenderContent call is being reached and that m_levelComp has been successfully initialized via AttachMeshFromPath(). Verify that the component passed to the render callback is active.
Issue: Selection highlight spheres are incorrect/disappearing.
Solution: Review the matrix transformation math in RenderSelectionHighlight. Ensure the bounding sphere (bs) accurately reflects the object's size relative to its model matrix, especially when dealing with non-uniform scaling.
Issue: Editor state is lost after saving and reloading.
Solution: Check the logic within LevelComponentVisualizer::AttachMeshFromPath(). The code correctly handles releasing old resources (ReleaseLevelComponent) but must ensure that any editor state (like viewport focus or selection) intended to persist across a reload cycle is manually restored.
📚 References
[Engine Core Documentation]: Details on the ECS structure and Component lifecycle.
[Dear ImGui Manual]: For advanced UI widget usage, docking, and drag/drop payloads. """) print("Successfully created continue/rules/CONTINUE.md")