#pragma once

#include "InputBinding.h"

#include <string>
#include <vector>

namespace Input {

/** @brief A named, reusable collection of InputBindings.
 *
 *  ActionContexts are pushed onto the InputActionManager stack to activate a set
 *  of game input mappings (e.g. "Gameplay", "UI", "Cinematic").
 *
 *  @code
 *  auto gameplay = std::make_shared<ActionContext>("Gameplay");
 *  gameplay->AddBinding(InputBinding::GamepadButton_("Jump",   GamepadButton::A))
 *           .AddBinding(InputBinding::GamepadHeld   ("Crouch", GamepadButton::B, 0.4f))
 *           .AddBinding(InputBinding::GamepadAxis_  ("MoveX",  GamepadAxis::LeftX));
 *
 *  manager.PushContext(gameplay);
 *  @endcode
 */
class ActionContext
{
    std::string               m_name;
    std::vector<InputBinding> m_bindings;

public:
    explicit ActionContext(std::string name);

    // ?? Accessors ?????????????????????????????????????????????????????????????

    const std::string&              GetName()     const { return m_name; }
    const std::vector<InputBinding>& GetBindings() const { return m_bindings; }

    // ?? Binding management ????????????????????????????????????????????????????

    /** Add a binding; returns *this for chaining. */
    ActionContext& AddBinding(InputBinding binding);

    /** Remove all bindings whose actionName matches. Returns *this for chaining. */
    ActionContext& RemoveBindingsForAction(const std::string& actionName);

    /** Remove all bindings. Returns *this for chaining. */
    ActionContext& ClearBindings();
};

} // namespace Input
