#pragma once

#include "Reflection/ReflectionBase.h"
#include <string>
#include <vector>

/**
 * @brief Defines a single dependency relationship between two components.
 * 
 * A component with className 'dependent' must wait for all components with
 * className 'dependsOn' to complete their updates before proceeding.
 * 
 * Supports reflection-based serialization for loading from JSON/config files.
 * 
 * Usage:
 * @code
 *   ComponentDependencyDefinition dep;
 *   dep.dependent = "CThirdPersonInputComponent";
 *   dep.dependsOn = "CPhysicsBodyComponent";
 *   
 *   // Or load from JSON
 *   ComponentDependencyDefinitionList depList;
 *   depList.Read("dependencies.json");
 *   
 *   // Apply to scheduler
 *   for (const auto& dep : depList.dependencies) {
 *       scheduler->AddDependencyByName(dep.dependent, dep.dependsOn);
 *   }
 * @endcode
 */
class ComponentDependencyDefinition : public CReflectedBase
{
public:
    REFL_DECLARE_OBJECT(ComponentDependencyDefinition, CReflectedBase);

    /** @brief The component type that has a dependency (the "dependent"). */
    std::string m_dependent;

    /** @brief The component type that must complete before the dependent (the "dependency"). */
    std::string m_dependsOn;

    ComponentDependencyDefinition(){}

    /**
     * @brief Constructs a dependency definition with the given class names.
     * @param dependentClassName The component that has the dependency.
     * @param dependencyClassName The component it depends on.
     */
    ComponentDependencyDefinition(const std::string& dependentClassName, 
                                  const std::string& dependencyClassName)
        : m_dependent(dependentClassName), m_dependsOn(dependencyClassName)
    {
    }

    virtual ~ComponentDependencyDefinition() = default;
};

/**
 * @brief Container for a list of component dependency definitions.
 * 
 * Supports reflection-based serialization for loading/saving dependency
 * configurations from/to JSON files.
 * 
 * Usage:
 * @code
 *   // Load from JSON file
 *   ComponentDependencyDefinitionList depList;
 *   depList.Read("dependencies.json");
 *   
 *   // Apply all dependencies to the scheduler
 *   for (const auto& dep : depList.dependencies) {
 *       if (!scheduler->AddDependencyByName(dep.dependent, dep.dependsOn)) {
 *           LOG_WARNING("Failed to add dependency: %s -> %s",
 *                      dep.dependent.c_str(), dep.dependsOn.c_str());
 *       }
 *   }
 * @endcode
 */
class ComponentDependencyDefinitionList : public CReflectedBase
{
public:
    REFL_DECLARE_OBJECT(ComponentDependencyDefinitionList, CReflectedBase);

    /** @brief Vector of component dependency definitions. */
    std::vector<ComponentDependencyDefinition *> m_dependencies;

    ComponentDependencyDefinitionList() = default;
    virtual ~ComponentDependencyDefinitionList()
    {
        for (auto dep : m_dependencies) {
            delete dep;
        }
        m_dependencies.clear();
    }

    /**
     * @brief Returns the number of dependencies in the list.
     */
    size_t GetDependencyCount() const
    {
        return m_dependencies.size();
    }

    /**
     * @brief Adds a dependency to the list.
     * @param dependent The component that has the dependency.
     * @param dependsOn The component it depends on.
     */
    void AddDependency(const std::string& dependent, const std::string& dependsOn)
    {
        m_dependencies.emplace_back(new ComponentDependencyDefinition(dependent, dependsOn));
    }

    /**
     * @brief Clears all dependencies from the list.
     */
    void Clear()
    {
        for (auto dep : m_dependencies) {
            delete dep;
        }
        m_dependencies.clear();
    }

    /**
     * @brief Gets the dependency at the specified index.
     * @return Const reference to the dependency, or throws if index is out of range.
     */
    const ComponentDependencyDefinition& GetDependency(size_t index) const
    {
        return *m_dependencies.at(index);
    }

    /**
     * @brief Gets the dependency at the specified index.
     * @return Reference to the dependency, or throws if index is out of range.
     */
    ComponentDependencyDefinition& GetDependency(size_t index)
    {
        return *m_dependencies.at(index);
    }
};
