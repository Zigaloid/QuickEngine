#include "InputActionManager.h"
#include "InputActionManager.h"
#include "MouseManager.h"
#include "GamepadManager.h"

#include <algorithm>
#include <cmath>
#include <format>

using namespace Input;
REFL_DEFINE_OBJECT(InputActionManager)
REFL_DEFINE_END

namespace Input {

// ?? Lifecycle ?????????????????????????????????????????????????????????????????

InputActionManager::InputActionManager() = default;

InputActionManager::~InputActionManager()
{
    OnShutdown();
}

bool InputActionManager::OnInitialize()
{
    return true;
}

void InputActionManager::OnShutdown()
{
    DetachMouseManager();
    DetachGamepadManager();
    m_stack.clear();
    m_listeners.clear();
    m_globalListeners.clear();
    m_bindingStates.clear();
}

void InputActionManager::OnUpdate(double deltaTime)
{
    m_totalTime += deltaTime;
    ProcessStack(deltaTime);
}

// ?? Device attachment ?????????????????????????????????????????????????????????

void InputActionManager::AttachMouseManager(MouseManager* mouse)
{
    m_mouseManager = mouse;
}

void InputActionManager::DetachMouseManager()
{
    m_mouseManager = nullptr;
}

void InputActionManager::AttachGamepadManager(GamepadManager* gamepad)
{
    m_gamepadManager = gamepad;
}

void InputActionManager::DetachGamepadManager()
{
    m_gamepadManager = nullptr;
}

// ?? Context stack ?????????????????????????????????????????????????????????????

void InputActionManager::PushContext(std::shared_ptr<ActionContext> context, bool fallthrough)
{
    if (!context)
        return;

    m_stack.push_back({ std::move(context), fallthrough });
}

void InputActionManager::PopContext()
{
    if (!m_stack.empty())
        m_stack.pop_back();
}

void InputActionManager::ClearContextStack()
{
    m_stack.clear();
}

std::shared_ptr<ActionContext> InputActionManager::GetTopContext() const
{
    return m_stack.empty() ? nullptr : m_stack.back().context;
}

// ?? Listener subscription ?????????????????????????????????????????????????????

ActionListenerHandle InputActionManager::Subscribe(const std::string& actionName, ActionCallback callback)
{
    const uint32_t id = m_nextListenerId++;
    m_listeners[actionName].push_back({ id, std::move(callback) });
    return ActionListenerHandle{ id, actionName };
}

ActionListenerHandle InputActionManager::SubscribeAll(ActionCallback callback)
{
    const uint32_t id = m_nextListenerId++;
    m_globalListeners.push_back({ id, std::move(callback) });
    return ActionListenerHandle{ id, {} };
}

void InputActionManager::Unsubscribe(const ActionListenerHandle& handle)
{
    if (!handle.IsValid())
        return;

    if (handle.actionName.empty())
    {
        std::erase_if(m_globalListeners, [&](const ListenerEntry& e) { return e.id == handle.id; });
    }
    else
    {
        auto it = m_listeners.find(handle.actionName);
        if (it != m_listeners.end())
        {
            std::erase_if(it->second, [&](const ListenerEntry& e) { return e.id == handle.id; });
            if (it->second.empty())
                m_listeners.erase(it);
        }
    }
}

void InputActionManager::UnsubscribeAll(const std::string& actionName)
{
    m_listeners.erase(actionName);
}

// ?? Stack processing ??????????????????????????????????????????????????????????

void InputActionManager::ProcessStack(double deltaTime)
{
    // Evaluate from top of stack downward.
    // Stop descending when a blocking layer (fallthrough=false) produces events.
    for (int i = static_cast<int>(m_stack.size()) - 1; i >= 0; --i)
    {
        const bool anyFired = ProcessContext(m_stack[i], deltaTime);
        if (anyFired && !m_stack[i].fallthrough)
            break;
    }
}

bool InputActionManager::ProcessContext(const StackEntry& entry, double deltaTime)
{
    const ActionContext& ctx      = *entry.context;
    const auto&          bindings = ctx.GetBindings();

    bool anyFired = false;

    for (size_t idx = 0; idx < bindings.size(); ++idx)
    {
        const InputBinding& binding = bindings[idx];
        const std::string   key     = MakeKey(ctx, idx);

        BindingState& state = m_bindingStates[key];

        ActionEvent event;
        event.actionName = binding.actionName;
        event.deltaTime  = deltaTime;

        if (EvaluateBinding(binding, state, deltaTime, event))
        {
            DispatchEvent(event);
            anyFired = true;
        }

        // Update edge-detection state for next frame.
        state.wasActive = IsSourceActive(binding);
    }

    return anyFired;
}

// ?? Binding evaluation ????????????????????????????????????????????????????????

bool InputActionManager::EvaluateBinding(const InputBinding& binding, BindingState& state,
                                         double deltaTime, ActionEvent& outEvent)
{
    const bool active         = IsSourceActive(binding);
    const bool justActivated  = WasSourceJustActivated(binding, state);
    const bool justDeactivated = WasSourceJustDeactivated(binding, state);

    outEvent.value = SampleSourceValue(binding);

    switch (binding.triggerType)
    {
    // ?? Pressed ???????????????????????????????????????????????????????????????
    case TriggerType::Pressed:
    {
        if (justActivated)
        {
            outEvent.phase           = ActionPhase::Started;
            outEvent.value.triggered = true;
            return true;
        }
        return false;
    }

    // ?? Released ??????????????????????????????????????????????????????????????
    case TriggerType::Released:
    {
        if (justDeactivated)
        {
            outEvent.phase           = ActionPhase::Completed;
            outEvent.value.triggered = true;
            return true;
        }
        return false;
    }

    // ?? Continuous ????????????????????????????????????????????????????????????
    case TriggerType::Continuous:
    {
        if (justActivated)
        {
            outEvent.phase           = ActionPhase::Started;
            outEvent.value.triggered = true;
            return true;
        }
        if (active)
        {
            outEvent.phase           = ActionPhase::Ongoing;
            outEvent.value.triggered = true;
            return true;
        }
        if (justDeactivated)
        {
            outEvent.phase           = ActionPhase::Completed;
            outEvent.value.triggered = false;
            return true;
        }
        return false;
    }

    // ?? Held ??????????????????????????????????????????????????????????????????
    case TriggerType::Held:
    {
        if (justActivated)
        {
            state.holdActive = true;
            state.holdTimer  = 0.f;
            state.holdFired  = false;

            outEvent.phase           = ActionPhase::Started;
            outEvent.value.triggered = true;
            outEvent.value.progress  = 0.f;
            return true;
        }

        if (state.holdActive)
        {
            if (active)
            {
                state.holdTimer += static_cast<float>(deltaTime);
                const float progress = std::min(state.holdTimer / binding.holdDuration, 1.f);

                if (!state.holdFired && state.holdTimer >= binding.holdDuration)
                {
                    state.holdFired          = true;
                    outEvent.phase           = ActionPhase::Completed;
                    outEvent.value.triggered = true;
                    outEvent.value.progress  = 1.f;
                    return true;
                }

                if (!state.holdFired)
                {
                    outEvent.phase           = ActionPhase::Ongoing;
                    outEvent.value.triggered = false;
                    outEvent.value.progress  = progress;
                    return true;
                }
            }
            else
            {
                state.holdActive = false;

                if (!state.holdFired)
                {
                    state.holdFired          = true; // prevent double cancel
                    outEvent.phase           = ActionPhase::Canceled;
                    outEvent.value.triggered = false;
                    outEvent.value.progress  = state.holdTimer / binding.holdDuration;
                    return true;
                }
            }
        }
        return false;
    }

    // ?? DoubleTap ?????????????????????????????????????????????????????????????
    case TriggerType::DoubleTap:
    {
        if (justActivated)
        {
            const double timeSinceLast = m_totalTime - state.lastTapTime;

            if (timeSinceLast <= static_cast<double>(binding.tapWindow))
            {
                ++state.tapCount;
            }
            else
            {
                state.tapCount = 1;
            }

            state.lastTapTime = m_totalTime;

            if (state.tapCount >= 2)
            {
                state.tapCount           = 0;
                outEvent.phase           = ActionPhase::Completed;
                outEvent.value.triggered = true;
                return true;
            }
        }
        return false;
    }

    // ?? Combo ?????????????????????????????????????????????????????????????????
    case TriggerType::Combo:
    {
        if (!justActivated)
            return false;

        const int resolvedGamepadId = binding.gamepadId; // -1 = any

        for (const auto& mod : binding.comboModifiers)
        {
            if (!IsModifierActive(mod, resolvedGamepadId))
                return false;
        }

        outEvent.phase           = ActionPhase::Started;
        outEvent.value.triggered = true;
        return true;
    }
    }

    return false;
}

// ?? Source state queries ??????????????????????????????????????????????????????

bool InputActionManager::IsSourceActive(const InputBinding& binding) const
{
    switch (binding.sourceType)
    {
    case InputSourceType::MouseButton:
    {
        if (!m_mouseManager || !m_mouseManager->HasMouse())
            return false;
        return m_mouseManager->IsButtonPressed(binding.mouseButton);
    }

    case InputSourceType::MouseAxis:
    {
        if (!m_mouseManager || !m_mouseManager->HasMouse())
            return false;
        IMouse* mouse = m_mouseManager->GetMouse();
        const float value = mouse->GetState().GetAxis(binding.mouseAxis);
        return std::abs(value) >= binding.axisThreshold;
    }

    case InputSourceType::GamepadButton:
    {
        if (!m_gamepadManager)
            return false;

        if (binding.gamepadId == -1)
            return m_gamepadManager->IsAnyButtonPressed(binding.gamepadButton);

        const IGamepad* pad = m_gamepadManager->GetGamepad(binding.gamepadId);
        return pad && pad->IsButtonPressed(binding.gamepadButton);
    }

    case InputSourceType::GamepadAxis:
    {
        if (!m_gamepadManager)
            return false;

        const IGamepad* pad = (binding.gamepadId == -1)
            ? m_gamepadManager->GetFirstConnectedGamepad()
            : m_gamepadManager->GetGamepad(binding.gamepadId);

        if (!pad)
            return false;

        const float value = pad->GetState().GetAxis(binding.gamepadAxis);
        return std::abs(value) >= binding.axisThreshold;
    }
    }
    return false;
}

bool InputActionManager::WasSourceJustActivated(const InputBinding& binding,
                                                 const BindingState& state) const
{
    switch (binding.sourceType)
    {
    case InputSourceType::MouseButton:
    {
        if (!m_mouseManager || !m_mouseManager->HasMouse())
            return false;
        return m_mouseManager->WasButtonJustPressed(binding.mouseButton);
    }

    case InputSourceType::GamepadButton:
    {
        if (!m_gamepadManager)
            return false;

        if (binding.gamepadId == -1)
            return m_gamepadManager->IsAnyButtonJustPressed(binding.gamepadButton);

        const IGamepad* pad = m_gamepadManager->GetGamepad(binding.gamepadId);
        return pad && pad->WasButtonJustPressed(binding.gamepadButton);
    }

    // For axis sources, edge-detect via previous frame's wasActive flag.
    case InputSourceType::MouseAxis:
    case InputSourceType::GamepadAxis:
        return !state.wasActive && IsSourceActive(binding);
    }
    return false;
}

bool InputActionManager::WasSourceJustDeactivated(const InputBinding& binding,
                                                   const BindingState& state) const
{
    switch (binding.sourceType)
    {
    case InputSourceType::MouseButton:
    {
        if (!m_mouseManager || !m_mouseManager->HasMouse())
            return false;
        return m_mouseManager->WasButtonJustReleased(binding.mouseButton);
    }

    case InputSourceType::GamepadButton:
    {
        if (!m_gamepadManager)
            return false;

        if (binding.gamepadId == -1)
        {
            // Fired on any gamepad releasing this button
            for (int id : m_gamepadManager->GetConnectedGamepadIds())
            {
                const IGamepad* pad = m_gamepadManager->GetGamepad(id);
                if (pad && pad->WasButtonJustReleased(binding.gamepadButton))
                    return true;
            }
            return false;
        }

        const IGamepad* pad = m_gamepadManager->GetGamepad(binding.gamepadId);
        return pad && pad->WasButtonJustReleased(binding.gamepadButton);
    }

    // For axis sources, edge-detect via previous frame's wasActive flag.
    case InputSourceType::MouseAxis:
    case InputSourceType::GamepadAxis:
        return state.wasActive && !IsSourceActive(binding);
    }
    return false;
}

bool InputActionManager::IsModifierActive(const InputBinding::Modifier& mod,
                                           int primaryGamepadId) const
{
    switch (mod.sourceType)
    {
    case InputSourceType::MouseButton:
    {
        if (!m_mouseManager || !m_mouseManager->HasMouse())
            return false;
        return m_mouseManager->IsButtonPressed(mod.mouseButton);
    }

    case InputSourceType::GamepadButton:
    {
        if (!m_gamepadManager)
            return false;

        const int resolvedId = (mod.gamepadId == -1) ? primaryGamepadId : mod.gamepadId;

        if (resolvedId == -1)
            return m_gamepadManager->IsAnyButtonPressed(mod.gamepadButton);

        const IGamepad* pad = m_gamepadManager->GetGamepad(resolvedId);
        return pad && pad->IsButtonPressed(mod.gamepadButton);
    }

    default:
        return false;
    }
}

ActionValue InputActionManager::SampleSourceValue(const InputBinding& binding) const
{
    ActionValue val;

    switch (binding.sourceType)
    {
    case InputSourceType::MouseButton:
        val.triggered = m_mouseManager && m_mouseManager->IsButtonPressed(binding.mouseButton);
        val.x         = val.triggered ? 1.f : 0.f;
        break;

    case InputSourceType::MouseAxis:
    {
        if (!m_mouseManager || !m_mouseManager->HasMouse())
            break;
        IMouse* mouse = m_mouseManager->GetMouse();
        const float raw = mouse->GetState().GetAxis(binding.mouseAxis);
        val.x           = std::abs(raw) >= binding.axisDeadzone ? raw : 0.f;
        val.triggered   = std::abs(val.x) >= binding.axisThreshold;
        break;
    }

    case InputSourceType::GamepadButton:
    {
        if (!m_gamepadManager)
            break;
        const IGamepad* pad = (binding.gamepadId == -1)
            ? m_gamepadManager->GetGamepadWithButtonPressed(binding.gamepadButton)
            : m_gamepadManager->GetGamepad(binding.gamepadId);
        if (pad)
        {
            val.triggered = pad->IsButtonPressed(binding.gamepadButton);
            val.x         = val.triggered ? 1.f : 0.f;
        }
        break;
    }

    case InputSourceType::GamepadAxis:
    {
        if (!m_gamepadManager)
            break;
        const IGamepad* pad = (binding.gamepadId == -1)
            ? m_gamepadManager->GetFirstConnectedGamepad()
            : m_gamepadManager->GetGamepad(binding.gamepadId);
        if (pad)
        {
            const float raw = pad->GetState().GetAxis(binding.gamepadAxis);
            val.x           = std::abs(raw) >= binding.axisDeadzone ? raw : 0.f;
            val.triggered   = std::abs(val.x) >= binding.axisThreshold;
        }
        break;
    }
    }

    return val;
}

// ?? Dispatch ??????????????????????????????????????????????????????????????????

void InputActionManager::DispatchEvent(const ActionEvent& event)
{
    // Per-action listeners
    const auto it = m_listeners.find(std::string(event.actionName));
    if (it != m_listeners.end())
        for (const auto& entry : it->second)
            entry.callback(event);

    // Wildcard listeners
    for (const auto& entry : m_globalListeners)
        entry.callback(event);
}

// ?? Helpers ???????????????????????????????????????????????????????????????????

std::string InputActionManager::MakeKey(const ActionContext& ctx, size_t bindingIndex) const
{
    return std::format("{}/{}", ctx.GetName(), bindingIndex);
}

} // namespace Input
