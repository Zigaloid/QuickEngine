#pragma once
#include "Component.h"
#include <memory>
#include <type_traits>

namespace ComponentSystem {

    // ?? CComponentReference ???????????????????????????????????????????????????????

    /** @brief Defines the search/resolution modes for CComponentReference. */
    enum EComponentResolutionMode {
        FIRST_SIBLING,
        FIRST_DECENDENT,
        FIRST_IN_HIERARCHY,
        FIRST_ROOT_SIBLING
    };

    /** @brief Finds and retains a weak reference to a component in the component hierarchy.
     *  An instance of this class can be added to any component and it will resolve
     *  the component's pointer when Get() is called on it. Once resolved, it will
     *  retain a weak_ptr and return a raw pointer on subsequent calls. If the target
     *  component is destroyed the reference automatically re-resolves on the next Get.
     */
    template<typename T>
    class CComponentReference {
        static_assert(std::is_base_of_v<Component, T>, "T must derive from Component");

    private:
        EComponentResolutionMode m_mode;
        mutable std::weak_ptr<T> m_resolvedPtr;
        mutable bool m_resolved = false;

    public:
        explicit CComponentReference(EComponentResolutionMode mode)
            : m_mode(mode)
            , m_resolved(false)
        {
        }

        ~CComponentReference() = default;

        // Support standard copy and move operations
        CComponentReference(const CComponentReference&) = default;
        CComponentReference& operator=(const CComponentReference&) = default;
        CComponentReference(CComponentReference&&) noexcept = default;
        CComponentReference& operator=(CComponentReference&&) noexcept = default;

        /** @brief Resolves (if not already resolved) and returns the component pointer.
         *  If the previously resolved component has been destroyed, re-resolves automatically.
         *  @param owner The component calling this get, used as the starting search context.
         *  @return Raw pointer valid for the current scope, or nullptr if unresolvable. */
        T* Get(const Component* owner) const
        {
            if (m_resolved)
            {
                if (auto sp = m_resolvedPtr.lock())
                    return sp.get();

                // Target was destroyed — clear and re-resolve
                m_resolvedPtr.reset();
                m_resolved = false;
            }

            if (!owner)
                return nullptr;

            T* raw = Resolve(owner);
            if (raw)
            {
                if (auto sp = std::dynamic_pointer_cast<T>(raw->shared_from_this()))
                    m_resolvedPtr = sp;
            }
            m_resolved = true;
            return raw;
        }

        /** @brief Returns the cached resolved pointer, or nullptr if not yet resolved or destroyed. */
        T* Get() const
        {
            return m_resolvedPtr.lock().get();
        }

        /** @brief Clears the cached weak_ptr, forcing re-resolution on next Get. */
        void Reset() const
        {
            m_resolvedPtr.reset();
            m_resolved = false;
        }

        /** @brief Manually sets/overrides the resolved component via shared_ptr. */
        void Set(std::shared_ptr<T> ptr)
        {
            m_resolvedPtr = ptr;
            m_resolved = true;
        }

        /** @brief Checks whether the reference has been resolved and the target is still alive. */
        bool IsResolved() const
        {
            return m_resolved && !m_resolvedPtr.expired();
        }

        /** @brief Returns preferred resolution mode. */
        EComponentResolutionMode GetResolutionMode() const
        {
            return m_mode;
        }

    private:
        T* Resolve(const Component* owner) const
        {
            switch (m_mode)
            {
            case FIRST_SIBLING:
            {
                return owner->FindSibling<T>();
            }
            case FIRST_DECENDENT:
            {
                return owner->FindDescendant<T>();
            }
            case FIRST_IN_HIERARCHY:
            {
                // Walk up to find the root of the hierarchy
                const Component* root = owner;
                while (root->GetParent() != nullptr)
                {
                    root = root->GetParent();
                }

                // Check root itself
                if (auto* typedRoot = dynamic_cast<const T*>(root))
                {
                    return const_cast<T*>(typedRoot);
                }

                // Search descendants of the root
                return root->FindDescendant<T>();
            }
            case FIRST_ROOT_SIBLING:
            {
                const Component* root = owner;
                while (root->GetParent() != nullptr)
                    root = root->GetParent();

                if (auto* typedRoot = dynamic_cast<const T*>(root))
                    return const_cast<T*>(typedRoot);

                for (const auto& child : root->GetChildren())
                {
                    if (auto* typedChild = dynamic_cast<T*>(child.get()))
                        return typedChild;
                }
                return nullptr;
            }
            default:
                return nullptr;
            }
        }
    };

} // namespace ComponentSystem
