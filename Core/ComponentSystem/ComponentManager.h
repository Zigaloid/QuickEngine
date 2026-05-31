#pragma once
#include "ComponentPool.h"
#include <unordered_map>
#include <string>
#include <functional>
#include <optional>
#include <typeindex>
#include <type_traits>

namespace ComponentSystem {

    // ?? ComponentManager ??????????????????????????????????????????????????????????

    /** @brief Owns all ComponentPool instances and provides typed create/release and iteration. */
    class ComponentManager {
    private:
        std::unordered_map<std::type_index, std::unique_ptr<ComponentPool>> m_pools;
        std::unordered_map<std::string, std::type_index> m_nameToTypeIndex;
        std::unordered_map<std::string, std::function<Component*()>> m_stringFactory;
        bool m_initialized = false;

    public:
        ComponentManager() = default;

        ~ComponentManager()
        {
            Shutdown();
        }

        /** @brief Registers a component type backed by a pool.
         *  @param initialPoolSize Number of instances to pre-allocate.
         *  @param maxPoolSize     Maximum pool capacity. */
        template<typename T>
        void RegisterComponentType(size_t initialPoolSize = 10, size_t maxPoolSize = 1000)
        {
            static_assert(std::is_base_of_v<Component, T>, "T must derive from Component");

            std::type_index typeIndex(typeid(T));
            if (m_pools.find(typeIndex) == m_pools.end())
            {
                auto factory = std::make_unique<TypedComponentFactory<T>>();
                auto pool = std::make_unique<ComponentPool>(std::move(factory), initialPoolSize, maxPoolSize);
                m_pools[typeIndex] = std::move(pool);
                // Auto-register the reflection class name so AddDependencyByName can resolve it
                m_nameToTypeIndex.emplace(T::ClassName(), typeIndex);
            }
        }

        /** @brief Registers a component type with both a pool and a string name lookup.
         *  @param className       String key for CreateComponentByName lookups. */
        template<typename T>
        void RegisterComponentType(const std::string& className, size_t initialPoolSize = 10, size_t maxPoolSize = 1000)
        {
            static_assert(std::is_base_of_v<Component, T>, "T must derive from Component");

            std::type_index typeIndex(typeid(T));
            if (m_pools.find(typeIndex) == m_pools.end())
            {
                auto factory = std::make_unique<TypedComponentFactory<T>>();
                auto pool = std::make_unique<ComponentPool>(std::move(factory), initialPoolSize, maxPoolSize);
                m_pools[typeIndex] = std::move(pool);
                m_nameToTypeIndex.emplace(className, typeIndex);
                m_stringFactory[className] = []() { return new T(); };
            }
        }

        /** @brief Acquires or auto-creates an instance of type T from its pool. */
        template<typename T>
        T* CreateComponent()
        {
            static_assert(std::is_base_of_v<Component, T>, "T must derive from Component");

            std::type_index typeIndex(typeid(T));
            auto it = m_pools.find(typeIndex);

            if (it == m_pools.end())
            {
                RegisterComponentType<T>();
                it = m_pools.find(typeIndex);
            }

            if (it != m_pools.end())
            {
                Component* component = it->second->Acquire();
                return static_cast<T*>(component);
            }

            return nullptr;
        }

        /** @brief Acquires a pool-backed shared_ptr<T>. The pool retains co-ownership so
         *  the component is not destroyed until both the pool releases it and all
         *  external shared_ptr copies are gone. */
        template<typename T>
        std::shared_ptr<T> CreateComponentShared()
        {
            static_assert(std::is_base_of_v<Component, T>, "T must derive from Component");
            T* raw = CreateComponent<T>();
            if (!raw) return nullptr;
            if (auto sp = GetSharedPtr(raw))
                return std::static_pointer_cast<T>(sp);
            return std::shared_ptr<T>(raw);
        }

        /** @brief String-name overload of CreateComponentShared. */
        std::shared_ptr<Component> CreateComponentSharedByName(const std::string& className)
        {
            Component* raw = CreateComponentByName(className);
            if (!raw) return nullptr;
            if (auto sp = GetSharedPtr(raw))
                return sp;
            return std::shared_ptr<Component>(raw);
        }

        /** @param className String name used during RegisterComponentType.
         *  @param Returns a pooled component, or nullptr if the name is unknown. */
        Component* CreateComponentByName(const std::string& className)
        {
            auto it = m_stringFactory.find(className);
            if (it != m_stringFactory.end())
            {
                auto typeIt = m_nameToTypeIndex.find(className);
                if (typeIt != m_nameToTypeIndex.end())
                {
                    auto poolIt = m_pools.find(typeIt->second);
                    if (poolIt != m_pools.end())
                    {
                        return poolIt->second->Acquire();
                    }
                }
                return it->second();
            }
            return nullptr;
        }

        /** @param component Component to return to its pool. */
        void ReleaseComponent(Component* component)
        {
            if (!component) return;

            for (auto& [typeIndex, pool] : m_pools)
            {
                if (typeIndex == std::type_index(typeid(*component)))
                {
                    pool->Release(component);
                    return;
                }
            }
        }

        void UpdateAllComponents(double deltaTime)
        {
            for (auto& [typeIndex, pool] : m_pools)
            {
                pool->UpdateAll(deltaTime);
            }
        }

        void UpdateAllComponentsInHierarchy(double deltaTime)
        {
            for (auto& [typeIndex, pool] : m_pools)
            {
                pool->UpdateAllInHierarchy(deltaTime);
            }
        }

        template<typename T>
        void UpdateComponentsOfType(double deltaTime)
        {
            static_assert(std::is_base_of_v<Component, T>, "T must derive from Component");

            std::type_index typeIndex(typeid(T));
            auto it = m_pools.find(typeIndex);
            if (it != m_pools.end())
            {
                it->second->UpdateAll(deltaTime);
            }
        }

        bool Initialize()
        {
            for (auto& [typeIndex, pool] : m_pools)
            {
                pool->InitializeAll();
            }

            m_initialized = true;
            return true;
        }

        void Shutdown()
        {
            if (!m_initialized) return;

            for (auto& [typeIndex, pool] : m_pools)
            {
                pool->ShutdownAll();
            }

            m_pools.clear();
            m_initialized = false;
        }

        template<typename T>
        size_t GetActiveComponentCount() const
        {
            static_assert(std::is_base_of_v<Component, T>, "T must derive from Component");

            std::type_index typeIndex(typeid(T));
            auto it = m_pools.find(typeIndex);
            return it != m_pools.end() ? it->second->GetActiveCount() : 0;
        }

        template<typename T>
        size_t GetActiveInHierarchyComponentCount() const
        {
            static_assert(std::is_base_of_v<Component, T>, "T must derive from Component");

            std::type_index typeIndex(typeid(T));
            auto it = m_pools.find(typeIndex);
            return it != m_pools.end() ? it->second->GetActiveInHierarchyCount() : 0;
        }

        template<typename T>
        size_t GetTotalComponentCount() const
        {
            static_assert(std::is_base_of_v<Component, T>, "T must derive from Component");

            std::type_index typeIndex(typeid(T));
            auto it = m_pools.find(typeIndex);
            return it != m_pools.end() ? it->second->GetTotalCount() : 0;
        }

        /** @brief Returns all active components of type T (pool-level active, not hierarchy). */
        template<typename T>
        std::vector<T*> GetComponentsOfType() const
        {
            static_assert(std::is_base_of_v<Component, T>, "T must derive from Component");

            std::vector<T*> result;
            std::type_index typeIndex(typeid(T));
            auto it = m_pools.find(typeIndex);

            if (it != m_pools.end())
            {
                const auto& activeComponents = it->second->GetActiveComponents();
                result.reserve(activeComponents.size());

                for (Component* component : activeComponents)
                {
                    if (T* typedComponent = static_cast<T*>(component))
                    {
                        result.push_back(typedComponent);
                    }
                }
            }

            return result;
        }

        /** @brief Returns only components that are active in the full hierarchy. */
        template<typename T>
        std::vector<T*> GetActiveComponentsOfType() const
        {
            static_assert(std::is_base_of_v<Component, T>, "T must derive from Component");

            std::vector<T*> result;
            std::type_index typeIndex(typeid(T));
            auto it = m_pools.find(typeIndex);

            if (it != m_pools.end())
            {
                const auto& activeComponents = it->second->GetActiveComponents();

                for (Component* component : activeComponents)
                {
                    if (component && component->IsActiveInHierarchy())
                    {
                        if (T* typedComponent = static_cast<T*>(component))
                        {
                            result.push_back(typedComponent);
                        }
                    }
                }
            }

            return result;
        }

        bool IsInitialized()            const { return m_initialized; }
        size_t GetRegisteredTypeCount() const { return m_pools.size(); }

        /** @brief Returns the pool-owned shared_ptr for a raw pointer, searching all pools.
         *  Returns nullptr if the component is not pool-managed (e.g. created with raw new). */
        std::shared_ptr<Component> GetSharedPtr(Component* raw) const
        {
            if (!raw) return nullptr;
            for (const auto& [typeIndex, pool] : m_pools)
            {
                if (auto sp = pool->GetSharedPtr(raw))
                    return sp;
            }
            return nullptr;
        }

        /** @brief Retrieves the type index for a registered component by its reflection class name.
         *  @param className The reflection class name (e.g. from GetRflClassName()).
         *  @return Optional type_index if found; empty optional otherwise. */
        std::optional<std::type_index> GetTypeIndexByClassName(const std::string& className) const
        {
            auto it = m_nameToTypeIndex.find(className);
            if (it != m_nameToTypeIndex.end())
            {
                return it->second;
            }
            return std::nullopt;
        }
    };

} // namespace ComponentSystem
