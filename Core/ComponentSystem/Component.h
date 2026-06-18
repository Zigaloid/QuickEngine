#pragma once
#include "Reflection/Reflection.h"
#include <vector>
#include <memory>
#include <cassert>
#include <type_traits>

// Forward declarations
namespace Core { class CoreSystem; }
namespace ComponentSystem { class ComponentManager; }

namespace ComponentSystem {

    // Unique component ID type
    using ComponentId = size_t;

    // ?? Component ?????????????????????????????????????????????????????????????????

    /** @brief Base class for all engine components; participates in the ECS lifecycle. */
    class Component : public CReflectedBase, public std::enable_shared_from_this<Component> {
    public:
        REFL_DECLARE_OBJECT(Component, CReflectedBase);
    private:
        static ComponentId m_nextId;
        ComponentId m_id;
        bool m_initialized = false;
        bool m_active = true;
        std::weak_ptr<Component> m_parent;
        std::vector<std::shared_ptr<Component>> m_children;
    protected:
        /** @brief Called once when the component is first initialized. */
        virtual bool OnInitialize() { return true; }
        /** @param deltaTime Elapsed seconds since the last frame. */
        virtual void OnUpdate(double deltaTime) {}
        /** @brief Called when the component is being destroyed. */
        virtual void OnShutdown() {}
        /** @brief Called when the component transitions to active. */
        virtual void OnActivate() {}
        /** @brief Called when the component transitions to inactive. */
        virtual void OnDeactivate() {}

    public:
        Component()
            : m_id(m_nextId++)
        {
        }

        virtual ~Component() = default;

        // Non-copyable but movable
        Component(const Component&) = delete;
        Component& operator=(const Component&) = delete;
        Component(Component&&) = default;
        Component& operator=(Component&&) = default;

        // ?? Lifecycle ?????????????????????????????????????????????????????????????

        /** @brief Shuts down and re-initializes this component. */
        bool ReInitialize()
        {
            Shutdown();
            return Initialize();
        }

        /** @brief Initializes this component and all children. */
        bool Initialize()
        {
            if (m_initialized) return true;

            if (OnInitialize())
            {
                m_initialized = true;

                for (auto& child : m_children)
                {
                    if (!child->Initialize())
                    {
                        m_initialized = false;
                        return false;
                    }
                }
                return true;
            }
            return false;
        }

        void Update(double deltaTime);
        void Shutdown();

        // ?? Hierarchy ?????????????????????????????????????????????????????????????

        /** @param child Component to attach as a child of this one (raw pointer overload). */
        void AddChild(Component* child);

        /** @param child Shared-pointer overload — preferred when the caller already holds a shared_ptr. */
        void AddChild(std::shared_ptr<Component> child);

        /** @brief Creates a child component of type T via the ComponentManager.
         *  @param Returns a pointer to the new child, or nullptr on failure. */
        template<typename T>
        T* CreateChild()
        {
            static_assert(std::is_base_of_v<Component, T>, "T must derive from Component");
            auto* manager = Core::CoreSystem::GetComponentManager();
            assert(manager && "ComponentManager must be initialized");

            if (auto* scheduler = Core::CoreSystem::GetJobSystemScheduler())
                scheduler->template RegisterComponentType<T>();

            T* childPtr = manager->template CreateComponent<T>();
            if (childPtr)
            {
                childPtr->m_parent = weak_from_this();
                if (auto sp = manager->GetSharedPtr(childPtr))
                    m_children.push_back(std::move(sp));
                else
                    m_children.push_back(std::shared_ptr<Component>(childPtr));
                if (m_initialized)
                    childPtr->Initialize();
            }
            return childPtr;
        }

        /** @param child The child component to detach and shut down. */
        void RemoveChild(Component* child);

        // ?? Activation ????????????????????????????????????????????????????????????

        /** @param active New active state; propagated to all children. */
        void SetActive(bool active)
        {
            if (m_active != active)
            {
                m_active = active;

                if (active)
                {
                    OnActivate();
                }
                else
                {
                    OnDeactivate();
                }

                for (auto& child : m_children)
                {
                    child->SetActive(active);
                }
            }
        }

        /** @brief Returns true if this component and all ancestors are active. */
        bool IsActiveInHierarchy() const
        {
            if (!m_active) return false;
            if (auto p = m_parent.lock()) return p->IsActiveInHierarchy();
            return true;
        }

        void Activate()     { SetActive(true); }
        void Deactivate()   { SetActive(false); }
        void ToggleActive() { SetActive(!m_active); }

        // ?? Getters ???????????????????????????????????????????????????????????????

        ComponentId GetId()    const { return m_id; }
        bool IsInitialized()   const { return m_initialized; }
        bool IsActive()        const { return m_active; }
        Component* GetParent() const { return m_parent.lock().get(); }
        const std::vector<std::shared_ptr<Component>>& GetChildren() const { return m_children; }

        // ?? Sibling search ????????????????????????????????????????????????????????????

        /** @brief Finds the first sibling component of type T (excluding self).
         *  Searches among the parent's children for a component matching type T. */
        template<typename T>
        T* FindSibling() const
        {
            static_assert(std::is_base_of_v<Component, T>, "T must derive from Component");
            auto p = m_parent.lock();
            if (!p) return nullptr;
            for (const auto& child : p->m_children)
            {
                if (child.get() != this)
                {
                    if (auto* typedChild = dynamic_cast<T*>(child.get()))
                    {
                        return typedChild;
                    }
                }
            }
            return nullptr;
        }

        // ?? Child search ??????????????????????????????????????????????????????????

        /** @brief Finds the first direct child component of type T. */
        template<typename T>
        T* FindChild() const
        {
            static_assert(std::is_base_of_v<Component, T>, "T must derive from Component");
            for (const auto& child : m_children)
            {
                if (auto* typedChild = dynamic_cast<T*>(child.get()))
                    return typedChild;
            }
            return nullptr;
        }

        /** @brief Returns all direct child components of type T. */
        template<typename T>
        std::vector<T*> FindChildren() const
        {
            static_assert(std::is_base_of_v<Component, T>, "T must derive from Component");
            std::vector<T*> result;
            for (const auto& child : m_children)
            {
                if (auto* typedChild = dynamic_cast<T*>(child.get()))
                    result.push_back(typedChild);
            }
            return result;
        }

        /** @brief Finds the first active direct child component of type T. */
        template<typename T>
        T* FindActiveChild() const
        {
            static_assert(std::is_base_of_v<Component, T>, "T must derive from Component");
            for (const auto& child : m_children)
            {
                if (child->IsActive())
                {
                    if (auto* typedChild = dynamic_cast<T*>(child.get()))
                        return typedChild;
                }
            }
            return nullptr;
        }

        /** @brief Returns all active direct child components of type T. */
        template<typename T>
        std::vector<T*> FindActiveChildren() const
        {
            static_assert(std::is_base_of_v<Component, T>, "T must derive from Component");
            std::vector<T*> result;
            for (const auto& child : m_children)
            {
                if (child->IsActive())
                {
                    if (auto* typedChild = dynamic_cast<T*>(child.get()))
                        result.push_back(typedChild);
                }
            }
            return result;
        }

        /** @brief Recursively finds the first descendant component of type T using depth-first search.
         *  Searches the entire hierarchy tree, not just direct children. */
        template<typename T>
        T* FindDescendant() const
        {
            static_assert(std::is_base_of_v<Component, T>, "T must derive from Component");
            for (const auto& child : m_children)
            {
                if (auto* typedChild = dynamic_cast<T*>(child.get()))
                    return typedChild;
            }
            for (const auto& child : m_children)
            {
                if (auto* found = child->FindDescendant<T>())
                    return found;
            }
            return nullptr;
        }

        /** @brief Recursively finds all descendant components of type T using depth-first search.
         *  Searches the entire hierarchy tree, not just direct children. */
        template<typename T>
        std::vector<T*> FindDescendants() const
        {
            static_assert(std::is_base_of_v<Component, T>, "T must derive from Component");
            std::vector<T*> result;
            for (const auto& child : m_children)
            {
                if (auto* typedChild = dynamic_cast<T*>(child.get()))
                    result.push_back(typedChild);
            }
            for (const auto& child : m_children)
            {
                auto childResults = child->FindDescendants<T>();
                result.insert(result.end(), childResults.begin(), childResults.end());
            }
            return result;
        }

        /** @brief Recursively finds the first active descendant component of type T using depth-first search.
         *  Searches the entire hierarchy tree respecting active state. */
        template<typename T>
        T* FindActiveDescendant() const
        {
            static_assert(std::is_base_of_v<Component, T>, "T must derive from Component");
            for (const auto& child : m_children)
            {
                if (child->IsActive())
                {
                    if (auto* typedChild = dynamic_cast<T*>(child.get()))
                        return typedChild;
                }
            }
            for (const auto& child : m_children)
            {
                if (child->IsActive())
                {
                    if (auto* found = child->FindActiveDescendant<T>())
                        return found;
                }
            }
            return nullptr;
        }

        /** @brief Recursively finds all active descendant components of type T using depth-first search.
         *  Searches the entire hierarchy tree respecting active state. */
        template<typename T>
        std::vector<T*> FindActiveDescendants() const
        {
            static_assert(std::is_base_of_v<Component, T>, "T must derive from Component");
            std::vector<T*> result;
            for (const auto& child : m_children)
            {
                if (child->IsActive())
                {
                    if (auto* typedChild = dynamic_cast<T*>(child.get()))
                        result.push_back(typedChild);
                }
            }
            for (const auto& child : m_children)
            {
                if (child->IsActive())
                {
                    auto childResults = child->FindActiveDescendants<T>();
                    result.insert(result.end(), childResults.begin(), childResults.end());
                }
            }
            return result;
        }
    };

    // ?? Static member initialization ??????????????????????????????????????????????

    inline ComponentId Component::m_nextId = 1;

} // namespace ComponentSystem
