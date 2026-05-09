#pragma once

#include "InputAction.h"
#include "ActionContext.h"
#include "ComponentSystem/ComponentSystem.h"

#include <memory>
#include <vector>
#include <string>
#include <unordered_map>

namespace Input {

class MouseManager;
class GamepadManager;

/** @brief Manages a stack of ActionContexts and dispatches ActionEvents to subscribers.
 *
 *  ### Typical usage
 *  @code
 *  // 1. Build contexts
 *  auto gameplay = std::make_shared<ActionContext>("Gameplay");
 *  gameplay->AddBinding(InputBinding::GamepadButton_("Jump",   GamepadButton::A))
 *           .AddBinding(InputBinding::GamepadHeld   ("Crouch", GamepadButton::B, 0.4f))
 *           .AddBinding(InputBinding::MouseDoubleTap("Dodge",  MouseButton::Left, 0.25f));
 *
 *  // 2. Wire up the manager (as a Component in a ComponentSystem)
 *  auto& mgr = system.AddComponent<InputActionManager>();
 *  mgr.AttachMouseManager(&mouseManager);
 *  mgr.AttachGamepadManager(&gamepadManager);
 *
 *  // 3. Subscribe
 *  auto handle = mgr.Subscribe("Jump", [](const ActionEvent& e) {
 *      if (e.phase == ActionPhase::Started) player.Jump();
 *  });
 *
 *  // 4. Activate a context
 *  mgr.PushContext(gameplay);          // blocking layer
 *  mgr.PushContext(uiContext, true);   // pass-through: also evaluates gameplay below
 *  @endcode
 *
 *  ### Stack semantics
 *  The stack is evaluated top-to-bottom each frame.  An entry with
 *  `fallthrough = false` (the default) blocks further evaluation once at least
 *  one of its bindings produces an event.  An entry with `fallthrough = true`
 *  always evaluates the layer beneath it as well.
 *
 *  ### Trigger state machines
 *  | Trigger   | Started         | Ongoing             | Completed                | Canceled                      |
 *  |-----------|-----------------|---------------------|--------------------------|-------------------------------|
 *  | Pressed   | justActivated   | —                   | —                        | —                             |
 *  | Released  | —               | —                   | justDeactivated          | —                             |
 *  | Held      | justActivated   | each frame (timer)  | holdDuration reached     | released before holdDuration  |
 *  | DoubleTap | —               | —                   | 2nd tap within tapWindow | —                             |
 *  | Combo     | mods held on justActivated | —         | —                        | —                             |
 */
class InputActionManager : public ComponentSystem::Component
{
public:
    REFL_DECLARE_OBJECT(InputActionManager, ComponentSystem::Component);

private:
    // ?? Attached devices ??????????????????????????????????????????????????????

    MouseManager*   m_mouseManager   = nullptr;
    GamepadManager* m_gamepadManager = nullptr;

    // ?? Context stack ?????????????????????????????????????????????????????????

    struct StackEntry
    {
        std::shared_ptr<ActionContext> context;
        /** If true, evaluation continues to the layer below regardless of whether
         *  this layer produced any events. */
        bool fallthrough = false;
    };
    std::vector<StackEntry> m_stack; ///< back = top of stack

    // ?? Listeners ?????????????????????????????????????????????????????????????

    struct ListenerEntry
    {
        uint32_t       id;
        ActionCallback callback;
    };

    /** Per-action listeners, keyed by action name. */
    std::unordered_map<std::string, std::vector<ListenerEntry>> m_listeners;
    /** Wildcard listeners that receive every ActionEvent. */
    std::vector<ListenerEntry> m_globalListeners;
    uint32_t m_nextListenerId = 1;

    // ?? Per-binding runtime state ?????????????????????????????????????????????

    /** Mutable state tracked for each binding across frames. */
    struct BindingState
    {
        // Held trigger
        float holdTimer  = 0.f;
        bool  holdFired  = false; ///< true once Completed has been dispatched
        bool  holdActive = false; ///< true while the source is being held

        // DoubleTap trigger
        double lastTapTime = -1000.0;
        int    tapCount    = 0;

        // Axis edge detection (source was active last frame)
        bool wasActive = false;
    };

    /** Key: "<contextName>/<bindingIndex>" */
    std::unordered_map<std::string, BindingState> m_bindingStates;

    double m_totalTime = 0.0;

    // ?? Helpers ???????????????????????????????????????????????????????????????

    std::string  MakeKey(const ActionContext& ctx, size_t bindingIndex) const;

    void         ProcessStack(double deltaTime);
    bool         ProcessContext(const StackEntry& entry, double deltaTime);
    bool         EvaluateBinding(const InputBinding& binding, BindingState& state,
                                 double deltaTime, ActionEvent& outEvent);

    /** Returns true if the source is currently active (pressed / above threshold). */
    bool         IsSourceActive(const InputBinding& binding)         const;

    /** Returns true on the first frame the source becomes active.
     *  For button sources this delegates to WasButtonJustPressed.
     *  For axis sources it compares against BindingState::wasActive. */
    bool         WasSourceJustActivated(const InputBinding& binding,
                                        const BindingState& state)   const;

    /** Returns true on the first frame the source becomes inactive. */
    bool         WasSourceJustDeactivated(const InputBinding& binding,
                                          const BindingState& state) const;

    bool         IsModifierActive(const InputBinding::Modifier& mod,
                                  int primaryGamepadId)              const;

    ActionValue  SampleSourceValue(const InputBinding& binding)      const;

    void         DispatchEvent(const ActionEvent& event);

protected:
    bool OnInitialize()             override;
    void OnUpdate(double deltaTime) override;
    void OnShutdown()               override;

public:
    explicit InputActionManager();
    ~InputActionManager() override;

    // ?? Device attachment ?????????????????????????????????????????????????????

    /** Attach a MouseManager whose state will be sampled each frame. */
    void AttachMouseManager(MouseManager* mouse);

    /** Attach a GamepadManager whose state will be sampled each frame. */
    void AttachGamepadManager(GamepadManager* gamepad);

    void DetachMouseManager();
    void DetachGamepadManager();

    MouseManager*   GetMouseManager()   const { return m_mouseManager; }
    GamepadManager* GetGamepadManager() const { return m_gamepadManager; }

    // ?? Context stack ?????????????????????????????????????????????????????????

    /** Push a context onto the top of the stack.
     *  @param context    The context to push.
     *  @param fallthrough  If true, the layer below is also evaluated even when
     *                      this layer produces events (pass-through / additive). */
    void PushContext(std::shared_ptr<ActionContext> context, bool fallthrough = false);

    /** Pop the top context from the stack. No-op if the stack is empty. */
    void PopContext();

    /** Remove all contexts from the stack. */
    void ClearContextStack();

    std::shared_ptr<ActionContext> GetTopContext()          const;
    size_t                         GetContextStackSize()    const { return m_stack.size(); }
    bool                           IsContextStackEmpty()    const { return m_stack.empty(); }

    // ?? Listener subscription ?????????????????????????????????????????????????

    /** Subscribe to events for a specific action name.
     *  @returns A handle that can be passed to Unsubscribe. */
    ActionListenerHandle Subscribe(const std::string& actionName, ActionCallback callback);

    /** Subscribe to all action events (wildcard).
     *  @returns A handle that can be passed to Unsubscribe. */
    ActionListenerHandle SubscribeAll(ActionCallback callback);

    /** Remove the listener identified by `handle`. */
    void Unsubscribe(const ActionListenerHandle& handle);

    /** Remove all listeners for a specific action name. */
    void UnsubscribeAll(const std::string& actionName);
};

} // namespace Input
