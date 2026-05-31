#pragma once
#include "Component.h"
#include <memory>
#include <typeindex>
#include <type_traits>

namespace ComponentSystem {

    // ?? ComponentFactory ??????????????????????????????????????????????????????????

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

} // namespace ComponentSystem
