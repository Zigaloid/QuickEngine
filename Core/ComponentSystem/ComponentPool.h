#pragma once
#include "ComponentFactory.h"
#include <vector>
#include <memory>
#include <algorithm>
#include <typeindex>

namespace ComponentSystem {

    // ?? ComponentPool ?????????????????????????????????????????????????????????????

    /** @brief Object pool for components of a single type, supporting acquire/release recycling. */
    class ComponentPool {
    private:
        std::vector<std::shared_ptr<Component>> m_pool;   // shared ownership so shared_from_this works
        std::vector<Component*> m_active;
        std::vector<Component*> m_inactive;
        std::unique_ptr<ComponentFactory> m_factory;
        size_t m_maxSize;

    public:
        ComponentPool(std::unique_ptr<ComponentFactory> factory, size_t initialSize = 10, size_t maxSize = 1000)
            : m_factory(std::move(factory)), m_maxSize(maxSize)
        {
            m_pool.reserve(initialSize);
            m_active.reserve(initialSize);
            m_inactive.reserve(initialSize);

            for (size_t i = 0; i < initialSize; ++i)
            {
                auto component = std::shared_ptr<Component>(m_factory->Create().release());
                m_inactive.push_back(component.get());
                m_pool.push_back(std::move(component));
            }
        }

        ~ComponentPool() = default;

        /** @brief Acquires an available component from the pool, creating one if needed. */
        Component* Acquire()
        {
            Component* component = nullptr;

            if (!m_inactive.empty())
            {
                component = m_inactive.back();
                m_inactive.pop_back();
            }
            else if (m_pool.size() < m_maxSize)
            {
                auto newComponent = std::shared_ptr<Component>(m_factory->Create().release());
                component = newComponent.get();
                m_pool.push_back(std::move(newComponent));
            }

            if (component)
            {
                m_active.push_back(component);
                component->SetActive(true);
            }

            return component;
        }

        /** @brief Acquires a component and returns a shared_ptr backed by the pool's own ownership. */
        std::shared_ptr<Component> AcquireShared()
        {
            Component* raw = Acquire();
            if (!raw) return nullptr;
            return GetSharedPtr(raw);
        }

        /** @param component The component to return to the inactive pool. */
        void Release(Component* component)
        {
            if (!component) return;

            auto it = std::find(m_active.begin(), m_active.end(), component);
            if (it != m_active.end())
            {
                m_active.erase(it);
                component->Shutdown();
                component->SetActive(false);
                m_inactive.push_back(component);
            }
        }

        void UpdateAll(double deltaTime)
        {
            for (Component* component : m_active)
            {
                if (component && component->IsActive())
                {
                    component->Update(deltaTime);
                }
            }
        }

        void UpdateAllInHierarchy(double deltaTime)
        {
            for (Component* component : m_active)
            {
                if (component && component->IsActiveInHierarchy())
                {
                    component->Update(deltaTime);
                }
            }
        }

        void InitializeAll()
        {
            for (Component* component : m_active)
            {
                if (component && !component->IsInitialized())
                {
                    component->Initialize();
                }
            }
        }

        void ShutdownAll()
        {
            for (Component* component : m_active)
            {
                if (component)
                {
                    component->Shutdown();
                }
            }

            for (Component* component : m_active)
            {
                m_inactive.push_back(component);
            }
            m_active.clear();
        }

        /** @brief Returns the number of active components that are active in hierarchy. */
        size_t GetActiveInHierarchyCount() const
        {
            size_t count = 0;
            for (Component* component : m_active)
            {
                if (component && component->IsActiveInHierarchy())
                {
                    count++;
                }
            }
            return count;
        }

        size_t GetActiveCount()   const { return m_active.size(); }
        size_t GetInactiveCount() const { return m_inactive.size(); }
        size_t GetTotalCount()    const { return m_pool.size(); }
        std::type_index GetComponentType() const { return m_factory->GetComponentType(); }
        const std::vector<Component*>& GetActiveComponents() const { return m_active; }

        /** @brief Returns the pool-owned shared_ptr for a raw pointer, or nullptr if not found. */
        std::shared_ptr<Component> GetSharedPtr(Component* raw) const
        {
            for (const auto& sp : m_pool)
            {
                if (sp.get() == raw)
                    return sp;
            }
            return nullptr;
        }
    };

} // namespace ComponentSystem
