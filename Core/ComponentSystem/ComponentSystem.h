#pragma once

#include "Reflection/Reflection.h"
#include "ComponentRegistry.h"
#include <vector>
#include <memory>
#include <string>
#include <unordered_map>
#include <typeindex>
#include <type_traits>
#include <cassert>
#include <algorithm>
#include <functional>


namespace ComponentSystem {

	// Forward declarations
	class ComponentPool;
	class ComponentManager;

	// Unique component ID type
	using ComponentId = size_t;

	// ── Component ─────────────────────────────────────────────────────────────────

	/** @brief Base class for all engine components; participates in the ECS lifecycle. */
	class Component : public CReflectedBase {
	public:
		REFL_DECLARE_OBJECT(Component, CReflectedBase);
	private:
		static ComponentId m_nextId;
		ComponentId m_id;
		bool m_initialized = false;
		bool m_active = true;
		Component* m_parent = nullptr;
		std::vector<Component*> m_children;
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

		// ── Lifecycle ─────────────────────────────────────────────────────────────

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

		// ── Hierarchy ─────────────────────────────────────────────────────────────

		/** @param child Component to attach as a child of this one. */
		void AddChild(Component* child)
		{
			if (child)
			{
				child->m_parent = this;
				if (m_initialized)
				{
					child->Initialize();
				}
				m_children.push_back(child);
			}
		}

		/** @brief Creates a child component of type T via the ComponentManager.
		 *  @param Returns a pointer to the new child, or nullptr on failure. */
		template<typename T>
		T* CreateChild()
		{
			static_assert(std::is_base_of_v<Component, T>, "T must derive from Component");
			auto* manager = Core::CoreSystem::GetComponentManager();
			assert(manager && "ComponentManager must be initialized");

			T* childPtr = manager->template CreateComponent<T>();
			if (childPtr)
			{
				childPtr->m_parent = this;
				m_children.push_back(childPtr);
				if (m_initialized)
				{
					childPtr->Initialize();
				}
			}
			return childPtr;
		}

		/** @param child The child component to detach and shut down. */
		void RemoveChild(Component* child);

		// ── Activation ────────────────────────────────────────────────────────────

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
			if (m_parent) return m_parent->IsActiveInHierarchy();
			return true;
		}

		void Activate()     { SetActive(true); }
		void Deactivate()   { SetActive(false); }
		void ToggleActive() { SetActive(!m_active); }

		// ── Getters ───────────────────────────────────────────────────────────────

		ComponentId GetId()    const { return m_id; }
		bool IsInitialized()   const { return m_initialized; }
		bool IsActive()        const { return m_active; }
		Component* GetParent() const { return m_parent; }
		const std::vector<Component*>& GetChildren() const { return m_children; }

		// ── Child search ──────────────────────────────────────────────────────────

		/** @brief Finds the first direct child component of type T. */
		template<typename T>
		T* FindChild() const
		{
			static_assert(std::is_base_of_v<Component, T>, "T must derive from Component");
			for (const auto& child : m_children)
			{
				if (auto* typedChild = dynamic_cast<T*>(child))
				{
					return typedChild;
				}
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
				if (auto* typedChild = dynamic_cast<T*>(child))
				{
					result.push_back(typedChild);
				}
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
					if (auto* typedChild = dynamic_cast<T*>(child))
					{
						return typedChild;
					}
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
					if (auto* typedChild = dynamic_cast<T*>(child))
					{
						result.push_back(typedChild);
					}
				}
			}
			return result;
		}
	};

	// ── ComponentFactory ──────────────────────────────────────────────────────────

	/** @brief Abstract factory interface for creating Component instances. */
	class ComponentFactory {
	public:
		virtual ~ComponentFactory() = default;
		virtual std::unique_ptr<Component> Create() = 0;
		virtual std::type_index GetComponentType() const = 0;
	};

	/** @brief Concrete factory that creates instances of a specific Component subtype. */
	template<typename T>
	class TypedComponentFactory : public ComponentFactory {
		static_assert(std::is_base_of_v<Component, T>, "T must derive from Component");

	public:
		std::unique_ptr<Component> Create() override
		{
			return std::make_unique<T>();
		}

		std::type_index GetComponentType() const override
		{
			return std::type_index(typeid(T));
		}
	};

	// ── ComponentPool ─────────────────────────────────────────────────────────────

	/** @brief Object pool for components of a single type, supporting acquire/release recycling. */
	class ComponentPool {
	private:
		std::vector<std::unique_ptr<Component>> m_pool;
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
				auto component = m_factory->Create();
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
				auto newComponent = m_factory->Create();
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
	};

	// ── ComponentManager ──────────────────────────────────────────────────────────

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

		bool IsInitialized()         const { return m_initialized; }
		size_t GetRegisteredTypeCount() const { return m_pools.size(); }
	};

	// ── Static member initialization ──────────────────────────────────────────────

	inline ComponentId Component::m_nextId = 1;

} // namespace ComponentSystem
