---
name: conventions
description: This skill should be used when the user asks to "validate conventions", "check conventions", "fix conventions", "apply conventions", or requests a conventions review of C++ files in this project. Enforces QuickScope C++ coding standards derived from ImGui3DViewVisualizer as the canonical reference.
---

# QuickScope C++ Coding Conventions

Apply these conventions when writing, reviewing, or fixing C++ files in `Shared/`.

## Naming

| Element | Convention | Example |
|---|---|---|
| Classes | PascalCase | `ImGui3DViewVisualizer`, `BgfxRenderPrimitives` |
| Member variables | `m_` + camelCase | `m_name`, `m_showGrid`, `m_meshComp` |
| Methods | PascalCase | `Initialize()`, `GetName()`, `SetShowGrid()` |
| Parameters | camelCase | `meshPath`, `deltaTime`, `viewId` |
| Local variables | camelCase | `contentSize`, `remaining`, `viewMtx` |
| Namespaces | PascalCase | `ImGuiVisualizers`, `Core` |
| Type aliases (`using`) | PascalCase | `RenderCallback` |

## Header (.h) File Structure

```
#pragma once

// local/project headers (quotes, no angle brackets)
// external/system headers (angle brackets)

// forward declarations

namespace Foo {

/**
 * @brief Class description.
 *
 * Additional details.
 */
class MyClass : public IBase 
{
public:
    /**
     * @brief Type alias doc.
     */
    using MyCallback = std::function<void(...)>;

    /**
     * @param name  Description.
     */
    explicit MyClass(const char* name = "Default");

    ~MyClass() override = default;

    // ── IBase interface ─────────────────────────────────────────────────

    void        Initialize() override;
    const char* GetName() const override { return m_name.c_str(); }

    // ── Public API ──────────────────────────────────────────────────────

    void SetFoo(bool v) { m_foo = v; }

private:
    // Logical group label
    std::string m_name;

    // Another group
    bool m_foo = false;
    int* m_ptr = nullptr;
};

} // namespace Foo
```

## Source (.cpp) File Structure

```
#include "MyClass.h"

// Project headers
#include "OtherThing.h"

// Standard headers
#include <algorithm>

namespace Foo {

// ── Construction ────────────────────────────────────────────────────────

MyClass::MyClass(const char* name)
    : m_name(name ? name : "Default")
{
}

// ── IBase lifecycle ──────────────────────────────────────────────────────

void MyClass::Initialize()
{
    // ...
}

} // namespace Foo
```

## Brace Style (Allman)

Opening brace on its own line for function bodies, `if`/`else`/`for`/`while`:

```cpp
void MyClass::Foo()
{
    if (condition)
    {
        DoThing();
    }
    else
    {
        DoOther();
    }
}
```

**Exception — trivial single-line guards** may stay inline:
```cpp
if (size.x < 1.0f) size.x = 1.0f;
```

**Exception — short inline getters/setters in the header**:
```cpp
const char* GetName() const override { return m_name.c_str(); }
void SetFoo(bool v) { m_foo = v; }
```

## Section Banners

Use `──` box-drawing characters in both `.h` and `.cpp`:

```cpp
// ── Section Name ────────────────────────────────────────────────────────
```

Typical sections: `Construction`, `IXxx interface` / `IXxx lifecycle`, `Public API`, `Render`, `Mouse input`, etc.

## Member Initialization

Initialise POD members and pointers in-class:

```cpp
bool     m_showGrid = true;
float    m_value    = 0.0f;
MyClass* m_ptr      = nullptr;
ImVec2   m_size     = { 0.0f, 0.0f };
```

Use a constructor initializer list for members set from parameters:

```cpp
MyClass::MyClass(const char* name, const char* shortcut)
    : m_name(name ? name : "Default")
    , m_shortcut(shortcut ? shortcut : "")
{
}
```

## Documentation Comments

- **Class**: `/** @brief ... */` block directly above the class declaration.
- **Public methods / type aliases**: `/** @brief ... @param name Description. */` blocks.
- **Private member groups**: short `//` label (e.g., `// Mouse drag tracking`).
- **Inline WHY comments** in `.cpp`: explain non-obvious intent only — never restate what the code does.
- No multi-line comment blocks on private methods.

## Keywords & Type Safety

- `nullptr` (never `NULL` or `0` for pointers)
- `static_cast<T>()` (never C-style casts)
- `std::move()` for transferring ownership
- `explicit` on single-argument constructors
- `override` on every overridden virtual method
- `= default` for defaulted special members
- `const` on all methods that don't modify state
- `std::string_view` for non-owning string inspection

## Guard Clauses / Early Return

Check preconditions at the top and return early to avoid deep nesting:

```cpp
void MyClass::DoWork()
{
    if (!m_ready)
        return;

    auto mgr = CoreSystem::GetManager();
    if (!mgr)
        return;

    mgr->DoStuff();
}
```

## Validation Checklist

When validating a file, check each item and fix any violations:

- [ ] All member variables use `m_` prefix + camelCase
- [ ] All methods are PascalCase
- [ ] Classes/namespaces are PascalCase
- [ ] Parameters and locals are camelCase
- [ ] Brace style is Allman (except allowed inline exceptions)
- [ ] Section banners use `──` box-drawing characters
- [ ] POD/pointer members have in-class initialization
- [ ] `nullptr` used (not `NULL`/`0`)
- [ ] `static_cast` used (not C-style casts)
- [ ] `override` on all virtual overrides
- [ ] `explicit` on single-arg constructors
- [ ] `const` correctness on methods
- [ ] Header includes: project headers first (quotes), then system (angle brackets)
- [ ] `.cpp` includes own header first, then project headers, then system headers
- [ ] Namespace closing brace annotated: `} // namespace Foo`
- [ ] Doc comments on public class members in header; WHY-only inline comments in `.cpp`

## How to Apply This Skill

1. Read each target file.
2. Work through the checklist above.
3. Fix all violations in-place — preserve all existing logic and behavior.
4. Report a brief summary of what was changed per file.
