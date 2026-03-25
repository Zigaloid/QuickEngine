#pragma once

#include "CoreSystem/CoreSystem.h"
#include "ComponentSystem/ComponentSystem.h"
#include "RenderComponent.h"
#include <vector>
#include <string>

namespace Rendering {

    /**
     * Simple LevelComponent that manages RenderComponents as child components.
     * No rendering logic - just basic add/remove functionality.
     */
    class LevelComponent : public ComponentSystem::Component {
    public:
        REFL_DECLARE_OBJECT(LevelComponent, ComponentSystem::Component);

    private:
        std::string m_levelName;

    protected:
        bool OnInitialize() override;
        void OnUpdate(double deltaTime) override;
        void OnShutdown() override;

    public:
        LevelComponent();
        virtual ~LevelComponent() = default;
      
        // Basic level properties
        void SetLevelName(const std::string& name) { m_levelName = name; }
        const std::string& GetLevelName() const { return m_levelName; }

        // RenderComponent management helpers
        
        /**
         * Add a RenderComponent as a child component
         * @return Pointer to the created RenderComponent, nullptr if failed
         */
        RenderComponent* AddRenderComponent();
        
        /**
         * Add a specific type of RenderComponent as a child component
         * @return Pointer to the created RenderComponent, nullptr if failed
         */
        template<typename T>
        T* AddRenderComponent() {
            static_assert(std::is_base_of_v<RenderComponent, T>, "T must derive from RenderComponent");
            return CreateChild<T>();
        }
                
        /**
         * Get all RenderComponent children
         * @return Vector of RenderComponent pointers
         */
        std::vector<RenderComponent*> GetRenderComponents() const;
        
        /**
         * Get count of RenderComponent children
         * @return Number of RenderComponent children
         */
        size_t GetRenderComponentCount() const;
        
        /**
         * Remove all RenderComponent children
         */
        void RemoveAllRenderComponents();

        // Serialization support
        
        /**
         * Save the level to a file
         * @param fileName The file path to save to (supports .json and .bin extensions)
         * @return True if successful, false otherwise
         */
        bool SaveLevel(const std::string& fileName);
        
        /**
         * Load the level from a file
         * @param fileName The file path to load from
         * @return True if successful, false otherwise
         */
        bool LoadLevel(const std::string& fileName);
    };

} // namespace Rendering