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

	// Base Component class
	
	class Component : public CReflectedBase {
	public:
		REFL_DECLARE_OBJECT(ComponentSystem::Component, CReflectedBase);
	private:
		static ComponentId m_nextId;
		ComponentId m_id;
		bool m_initialized;
		bool m_active;
		Component* m_parent;
		std::vector<Component*> m_children;		
	protected:
		// Called during initialization phase
		virtual bool OnInitialize() { return true; }

		// Called during update phase
		virtual void OnUpdate(double deltaTime) {}

		// Called during shutdown phase
		virtual void OnShutdown() {}

		// Called when component is activated
		virtual void OnActivate() {}

		// Called when component is deactivated
		virtual void OnDeactivate() {}

	public:
		Component()
			: m_id(m_nextId++), m_initialized(false), m_active(true), m_parent(nullptr) {
		}

		virtual ~Component() = default;

		// Non-copyable but movable
		Component(const Component&) = delete;
		Component& operator=(const Component&) = delete;
		Component(Component&&) = default;
		Component& operator=(Component&&) = default;

		// Component lifecycle
		bool ReInitialize()
		{
			Shutdown();
			return Initialize();
		}
		bool Initialize() {
			if (m_initialized) return true;

			if (OnInitialize()) {
				m_initialized = true;

				// Initialize all children
				for (auto& child : m_children) {
					if (!child->Initialize()) {
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

		void AddChild(Component* child) {
			if (child) {
				child->m_parent = this;
				if (m_initialized) {
					child->Initialize();
				}
				m_children.push_back(child);
			}
		}


		// New templated CreateChild method
		template<typename T>
		T* CreateChild() {
			static_assert(std::is_base_of_v<Component, T>, "T must derive from Component");
			auto* manager = Core::CoreSystem::GetComponentManager();
			assert(manager && "ComponentManager must be initialized");

            T* childPtr = manager->template CreateComponent<T>();
			if (childPtr) {
				childPtr->m_parent = this;
				m_children.push_back(childPtr);
				if (m_initialized) {
					childPtr->Initialize();
				}
			}
			return childPtr;
		}			
		// Also add an overload that takes a Component pointer for convenience:
		void RemoveChild(Component* child);
		// Getters
		ComponentId GetId() const { return m_id; }
		bool IsInitialized() const { return m_initialized; }
		bool IsActive() const { return m_active; }
		Component* GetParent() const { return m_parent; }
        const std::vector<Component*>& GetChildren() const { return m_children; }
		// Enhanced activation/deactivation functionality
		void SetActive(bool active) { 
			if (m_active != active) {
				m_active = active;
				
				if (active) {
					OnActivate();
				} else {
					OnDeactivate();
				}
				
				// Propagate activation state to children (optional behavior)
				// You can comment this out if you want independent child activation
				for (auto& child : m_children) {
					child->SetActive(active);
				}
			}
		}

		// Check if component is active in hierarchy (considers parent activation)
		bool IsActiveInHierarchy() const {
			if (!m_active) return false;
			if (m_parent) return m_parent->IsActiveInHierarchy();
			return true;
		}

		// Activate/Deactivate convenience methods
		void Activate() { SetActive(true); }
		void Deactivate() { SetActive(false); }

		// Toggle activation state
		void ToggleActive() { SetActive(!m_active); }

		// Find child components by type
		template<typename T>
		T* FindChild() const {
			static_assert(std::is_base_of_v<Component, T>, "T must derive from Component");
			for (const auto& child : m_children) {
				if (auto* typedChild = dynamic_cast<T*>(child)) {
					return typedChild;
				}
			}
			return nullptr;
		}

		template<typename T>
		std::vector<T*> FindChildren() const {
			static_assert(std::is_base_of_v<Component, T>, "T must derive from Component");
			std::vector<T*> result;
			for (const auto& child : m_children) {
				if (auto* typedChild = dynamic_cast<T*>(child)) {
					result.push_back(typedChild);
				}
			}
			return result;
		}

		// Find active child components by type
		template<typename T>
		T* FindActiveChild() const {
			static_assert(std::is_base_of_v<Component, T>, "T must derive from Component");
			for (const auto& child : m_children) {
				if (child->IsActive()) {
					if (auto* typedChild = dynamic_cast<T*>(child)) {
						return typedChild;
					}
				}
			}
			return nullptr;
		}

		template<typename T>
		std::vector<T*> FindActiveChildren() const {
			static_assert(std::is_base_of_v<Component, T>, "T must derive from Component");
			std::vector<T*> result;
			for (const auto& child : m_children) {
				if (child->IsActive()) {
					if (auto* typedChild = dynamic_cast<T*>(child)) {
						result.push_back(typedChild);
					}
				}
			}
			return result;
		}
	};

	// Component factory interface
	class ComponentFactory {
	public:
		virtual ~ComponentFactory() = default;
		virtual std::unique_ptr<Component> Create() = 0;
		virtual std::type_index GetComponentType() const = 0;
	};

	// Templated component factory
	template<typename T>
	class TypedComponentFactory : public ComponentFactory {
		static_assert(std::is_base_of_v<Component, T>, "T must derive from Component");

	public:
		std::unique_ptr<Component> Create() override {
			return std::make_unique<T>();
		}

		std::type_index GetComponentType() const override {
			return std::type_index(typeid(T));
		}
	};

	// Memory pool for components of the same type
	class ComponentPool {
	private:
		std::vector<std::unique_ptr<Component>> m_pool;
		std::vector<Component*> m_active;
		std::vector<Component*> m_inactive;
		std::unique_ptr<ComponentFactory> m_factory;
		size_t m_maxSize;

	public:
		ComponentPool(std::unique_ptr<ComponentFactory> factory, size_t initialSize = 10, size_t maxSize = 1000)
			: m_factory(std::move(factory)), m_maxSize(maxSize) {
			m_pool.reserve(initialSize);
			m_active.reserve(initialSize);
			m_inactive.reserve(initialSize);

			// Pre-allocate initial components
			for (size_t i = 0; i < initialSize; ++i) {
				auto component = m_factory->Create();
				m_inactive.push_back(component.get());
				m_pool.push_back(std::move(component));
			}
		}

		~ComponentPool() = default;

		// Get a component from the pool
		Component* Acquire() {
			Component* component = nullptr;

			if (!m_inactive.empty()) {
				// Reuse existing component
				component = m_inactive.back();
				m_inactive.pop_back();
			}
			else if (m_pool.size() < m_maxSize) {
				// Create new component
				auto newComponent = m_factory->Create();
				component = newComponent.get();
				m_pool.push_back(std::move(newComponent));
			}

			if (component) {
				m_active.push_back(component);
				component->SetActive(true);
			}

			return component;
		}

		// Return a component to the pool
		void Release(Component* component) {
			if (!component) return;

			// Find and remove from active list
			auto it = std::find(m_active.begin(), m_active.end(), component);
			if (it != m_active.end()) {
				m_active.erase(it);
				component->Shutdown();
				component->SetActive(false);				
				m_inactive.push_back(component);
			}
		}

		// Update all active components of this type
		void UpdateAll(double deltaTime) {
			for (Component* component : m_active) {
				if (component && component->IsActive()) {
					component->Update(deltaTime);
				}
			}
		}

		// Update only components that are active in hierarchy
		void UpdateAllInHierarchy(double deltaTime) {
			for (Component* component : m_active) {
				if (component && component->IsActiveInHierarchy()) {
					component->Update(deltaTime);
				}
			}
		}

		// Initialize all active components of this type
		void InitializeAll() {
			for (Component* component : m_active) {
				if (component && !component->IsInitialized()) {
					component->Initialize();
				}
			}
		}

		// Shutdown all active components of this type
		void ShutdownAll() {
			for (Component* component : m_active) {
				if (component) {
					component->Shutdown();
				}
			}

			// FIXED: Move active components to inactive instead of adding all pool components
			for (Component* component : m_active) {
				m_inactive.push_back(component);
			}
			m_active.clear();
		}

		// Get count of components that are active in hierarchy
		size_t GetActiveInHierarchyCount() const {
			size_t count = 0;
			for (Component* component : m_active) {
				if (component && component->IsActiveInHierarchy()) {
					count++;
				}
			}
			return count;
		}

		// Getters
		size_t GetActiveCount() const { return m_active.size(); }
		size_t GetInactiveCount() const { return m_inactive.size(); }
		size_t GetTotalCount() const { return m_pool.size(); }
		std::type_index GetComponentType() const { return m_factory->GetComponentType(); }
		const std::vector<Component*>& GetActiveComponents() const { return m_active; }
	};

	// Component Manager - manages all component pools and provides factory interface
	class ComponentManager {
	private:
		std::unordered_map<std::type_index, std::unique_ptr<ComponentPool>> m_pools;
		std::unordered_map<std::string, std::type_index> m_nameToTypeIndex; // NEW
		std::unordered_map<std::string, std::function<Component*()>> m_stringFactory; // NEW
		bool m_initialized;

	public:
		ComponentManager() : m_initialized(false) {}

		~ComponentManager() {
			Shutdown();
		}

		// Register a component type with the manager
		template<typename T>
		void RegisterComponentType(size_t initialPoolSize = 10, size_t maxPoolSize = 1000) {
			static_assert(std::is_base_of_v<Component, T>, "T must derive from Component");

			std::type_index typeIndex(typeid(T));
			if (m_pools.find(typeIndex) == m_pools.end()) {
				auto factory = std::make_unique<TypedComponentFactory<T>>();
				auto pool = std::make_unique<ComponentPool>(std::move(factory), initialPoolSize, maxPoolSize);
				m_pools[typeIndex] = std::move(pool);
			}
		}

		// Register a component type with a string name
		template<typename T>
		void RegisterComponentType(const std::string& className, size_t initialPoolSize = 10, size_t maxPoolSize = 1000) {
			static_assert(std::is_base_of_v<Component, T>, "T must derive from Component");

			std::type_index typeIndex(typeid(T));
			if (m_pools.find(typeIndex) == m_pools.end()) {
				auto factory = std::make_unique<TypedComponentFactory<T>>();
				auto pool = std::make_unique<ComponentPool>(std::move(factory), initialPoolSize, maxPoolSize);
				m_pools[typeIndex] = std::move(pool);
				m_nameToTypeIndex.emplace(className, typeIndex);
				m_stringFactory[className] = []() { return new T(); };
			}
		}

		// Create a component of the specified type
		template<typename T>
		T* CreateComponent() {
			static_assert(std::is_base_of_v<Component, T>, "T must derive from Component");

			std::type_index typeIndex(typeid(T));
			auto it = m_pools.find(typeIndex);

			if (it == m_pools.end()) {
				// Auto-register the type if not found
				RegisterComponentType<T>();
				it = m_pools.find(typeIndex);
			}

			if (it != m_pools.end()) {
				Component* component = it->second->Acquire();
				return static_cast<T*>(component);
			}

			return nullptr;
		}

		// Create a component by string name
		Component* CreateComponentByName(const std::string& className) {
			auto it = m_stringFactory.find(className);
			if (it != m_stringFactory.end()) {
				// Use pool if available
				auto typeIt = m_nameToTypeIndex.find(className);
				if (typeIt != m_nameToTypeIndex.end()) {
					auto poolIt = m_pools.find(typeIt->second);
					if (poolIt != m_pools.end()) {
						return poolIt->second->Acquire();
					}
				}
				// Fallback: create raw if not pooled
				return it->second();
			}
			return nullptr;
		}

		// Release a component back to its pool
		void ReleaseComponent(Component* component) {
			if (!component) return;

			// Find the appropriate pool for this component type
			for (auto& [typeIndex, pool] : m_pools) {
				if (typeIndex == std::type_index(typeid(*component))) {
					pool->Release(component);
					return;
				}
			}
		}

		// Update all components by type (pools)
		void UpdateAllComponents(double deltaTime) {
			for (auto& [typeIndex, pool] : m_pools) {
				pool->UpdateAll(deltaTime);
			}
		}

		// Update all components considering hierarchy activation
		void UpdateAllComponentsInHierarchy(double deltaTime) {
			for (auto& [typeIndex, pool] : m_pools) {
				pool->UpdateAllInHierarchy(deltaTime);
			}
		}

		// Update specific component type
		template<typename T>
		void UpdateComponentsOfType(double deltaTime) {
			static_assert(std::is_base_of_v<Component, T>, "T must derive from Component");

			std::type_index typeIndex(typeid(T));
			auto it = m_pools.find(typeIndex);
			if (it != m_pools.end()) {
				it->second->UpdateAll(deltaTime);
			}
		}

		// Initialize the component manager
		bool Initialize() {
			//if (m_initialized) return true;

			for (auto& [typeIndex, pool] : m_pools) {
				pool->InitializeAll();
			}

			m_initialized = true;
			return true;
		}

		// Shutdown the component manager
		void Shutdown() {
			if (!m_initialized) return;

			for (auto& [typeIndex, pool] : m_pools) {
				pool->ShutdownAll();
			}

			m_pools.clear();
			m_initialized = false;
		}

		// Get pool statistics
		template<typename T>
		size_t GetActiveComponentCount() const {
			static_assert(std::is_base_of_v<Component, T>, "T must derive from Component");

			std::type_index typeIndex(typeid(T));
			auto it = m_pools.find(typeIndex);
			return it != m_pools.end() ? it->second->GetActiveCount() : 0;
		}

		template<typename T>
		size_t GetActiveInHierarchyComponentCount() const {
			static_assert(std::is_base_of_v<Component, T>, "T must derive from Component");

			std::type_index typeIndex(typeid(T));
			auto it = m_pools.find(typeIndex);
			return it != m_pools.end() ? it->second->GetActiveInHierarchyCount() : 0;
		}

		template<typename T>
		size_t GetTotalComponentCount() const {
			static_assert(std::is_base_of_v<Component, T>, "T must derive from Component");

			std::type_index typeIndex(typeid(T));
			auto it = m_pools.find(typeIndex);
			return it != m_pools.end() ? it->second->GetTotalCount() : 0;
		}

		// Get all active components of a specific type
		template<typename T>
		std::vector<T*> GetComponentsOfType() const {
			static_assert(std::is_base_of_v<Component, T>, "T must derive from Component");

			std::vector<T*> result;
			std::type_index typeIndex(typeid(T));
			auto it = m_pools.find(typeIndex);

			if (it != m_pools.end()) {
				const auto& activeComponents = it->second->GetActiveComponents();
				result.reserve(activeComponents.size());

				for (Component* component : activeComponents) {
					if (T* typedComponent = static_cast<T*>(component)) {
						result.push_back(typedComponent);
					}
				}
			}

			return result;
		}

		// Get only components that are active in hierarchy
		template<typename T>
		std::vector<T*> GetActiveComponentsOfType() const {
			static_assert(std::is_base_of_v<Component, T>, "T must derive from Component");

			std::vector<T*> result;
			std::type_index typeIndex(typeid(T));
			auto it = m_pools.find(typeIndex);

			if (it != m_pools.end()) {
				const auto& activeComponents = it->second->GetActiveComponents();
				
				for (Component* component : activeComponents) {
					if (component && component->IsActiveInHierarchy()) {
						if (T* typedComponent = static_cast<T*>(component)) {
							result.push_back(typedComponent);
						}
					}
				}
			}

			return result;
		}

		// Check if manager is initialized
		bool IsInitialized() const { return m_initialized; }

		// Get total number of registered component types
		size_t GetRegisteredTypeCount() const { return m_pools.size(); }
	};

	// Static component ID counter initialization
	inline ComponentId Component::m_nextId = 1;

} // namespace ComponentSystem