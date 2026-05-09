#include "ActionContext.h"
#include <algorithm>

namespace Input {

ActionContext::ActionContext(std::string name)
    : m_name(std::move(name))
{}

ActionContext& ActionContext::AddBinding(InputBinding binding)
{
    m_bindings.push_back(std::move(binding));
    return *this;
}

ActionContext& ActionContext::RemoveBindingsForAction(const std::string& actionName)
{
    std::erase_if(m_bindings, [&](const InputBinding& b) { return b.actionName == actionName; });
    return *this;
}

ActionContext& ActionContext::ClearBindings()
{
    m_bindings.clear();
    return *this;
}

} // namespace Input
