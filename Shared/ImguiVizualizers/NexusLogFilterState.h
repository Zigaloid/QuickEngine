#pragma once

#include "Reflection/Reflection.h"
#include <unordered_map>
#include <string>

/**
 * @brief Reflected data object for persisting NexusLogVisualizer filter/collection state.
 *
 * Uses the core Reflection System to serialize the four string-bool maps
 * (app/pipe visibility and app/pipe collection) to JSON.
 */
class NexusLogFilterState : public CReflectedBase
{
public:
    REFL_DECLARE_OBJECT(NexusLogFilterState, CReflectedBase)

    std::unordered_map<std::string, bool> m_appVisibility;
    std::unordered_map<std::string, bool> m_pipeVisibility;
    std::unordered_map<std::string, bool> m_appCollectionEnabled;
    std::unordered_map<std::string, bool> m_pipeCollectionEnabled;
};