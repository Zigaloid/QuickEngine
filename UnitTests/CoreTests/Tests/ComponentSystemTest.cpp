#include <gtest/gtest.h>
#include "CoreSystem/CoreSystem.h"
#include "ComponentSystem/ComponentSystem.h"
#include <memory>
#include <vector>
#include <string>

using namespace ComponentSystem;

// Test component classes for testing
class TestComponent : public Component {
private:
    int testValue_;
    bool onInitializeCalled_;
    bool onUpdateCalled_;
    bool onShutdownCalled_;

public:
    TestComponent(int value = 0)
        : testValue_(value), onInitializeCalled_(false), onUpdateCalled_(false), onShutdownCalled_(false) {}

    int GetTestValue() const { return testValue_; }
    void SetTestValue(int value) { testValue_ = value; }

    bool WasInitializeCalled() const { return onInitializeCalled_; }
    bool WasUpdateCalled() const { return onUpdateCalled_; }
    bool WasShutdownCalled() const { return onShutdownCalled_; }

    void ResetFlags() {
        onInitializeCalled_ = false;
        onUpdateCalled_ = false;
        onShutdownCalled_ = false;
    }

protected:
    bool OnInitialize() override {
        onInitializeCalled_ = true;
        return true;
    }
    void OnUpdate(double) override {
        onUpdateCalled_ = true;
    }
    void OnShutdown() override {
        onShutdownCalled_ = true;
    }
};

class FailingTestComponent : public Component {
protected:
    bool OnInitialize() override { return false; }
};

class AnotherTestComponent : public Component {
private:
    std::string name_;
public:
    AnotherTestComponent(const std::string& name = "default") : name_(name) {}
    const std::string& GetName() const { return name_; }
    void SetName(const std::string& name) { name_ = name; } // Added SetName method
};

// Component Tests
class ComponentTest : public ::testing::Test {
protected:
    void SetUp() override {
        Core::CoreSystem::Initialize(); // Ensure CoreSystem is initialized
        manager = std::make_unique<ComponentManager>();
        component = manager->CreateComponent<TestComponent>();
        component->SetTestValue(42);
    }
    void TearDown() override {
        component = nullptr;
        manager.reset();
        Core::CoreSystem::Shutdown(); // Clean up CoreSystem after test
    }
    std::unique_ptr<ComponentManager> manager;
    TestComponent* component;
};

TEST_F(ComponentTest, Construction) {
    EXPECT_FALSE(component->IsInitialized());
    EXPECT_TRUE(component->IsActive());
    EXPECT_EQ(component->GetParent(), nullptr);
    EXPECT_TRUE(component->GetChildren().empty());
    EXPECT_GT(component->GetId(), 0);
}

TEST_F(ComponentTest, Initialization) {
    EXPECT_FALSE(component->WasInitializeCalled());
    EXPECT_TRUE(component->Initialize());
    EXPECT_TRUE(component->IsInitialized());
    EXPECT_TRUE(component->WasInitializeCalled());
    component->SetTestValue(0);
    EXPECT_TRUE(component->Initialize());
}

TEST_F(ComponentTest, InitializationFailure) {
    FailingTestComponent* failingComponent = manager->CreateComponent<FailingTestComponent>();
    EXPECT_FALSE(failingComponent->Initialize());
    EXPECT_FALSE(failingComponent->IsInitialized());
}

TEST_F(ComponentTest, Update) {
    EXPECT_FALSE(component->WasUpdateCalled());
    component->Update(0.016);
    EXPECT_FALSE(component->WasUpdateCalled());
    component->Initialize();
    component->Update(0.016);
    EXPECT_TRUE(component->WasUpdateCalled());
    TestComponent* resetComponent = manager->CreateComponent<TestComponent>();
    resetComponent->SetActive(false);
    resetComponent->Initialize();
    resetComponent->Update(0.016);
    EXPECT_FALSE(resetComponent->WasUpdateCalled());
}

TEST_F(ComponentTest, Shutdown) {
    component->Initialize();
    EXPECT_FALSE(component->WasShutdownCalled());
    component->Shutdown();
    EXPECT_TRUE(component->WasShutdownCalled());
    EXPECT_FALSE(component->IsInitialized());
    component->ResetFlags();
    component->Shutdown();
    EXPECT_FALSE(component->WasShutdownCalled());
}

TEST_F(ComponentTest, ActiveState) {
    EXPECT_TRUE(component->IsActive());
    component->SetActive(false);
    EXPECT_FALSE(component->IsActive());
    component->SetActive(true);
    EXPECT_TRUE(component->IsActive());
}

TEST_F(ComponentTest, ChildManagement) {
    TestComponent* child1 = manager->CreateComponent<TestComponent>();
    child1->SetTestValue(1);
    AnotherTestComponent* child2 = manager->CreateComponent<AnotherTestComponent>();
    child2->SetName("child2");
    component->AddChild(child1);
    component->AddChild(child2);
    EXPECT_EQ(component->GetChildren().size(), 2);
    EXPECT_EQ(component->GetChildren()[0]->GetParent(), component);
    EXPECT_EQ(component->GetChildren()[1]->GetParent(), component);
    component->RemoveChild(child1);
    EXPECT_EQ(component->GetChildren().size(), 1);
    EXPECT_EQ(component->GetChildren()[0]->GetId(), child2->GetId());
}

TEST_F(ComponentTest, CreateChild) {
    TestComponent* child = component->CreateChild<TestComponent>();
    child->SetTestValue(123);
    EXPECT_NE(child, nullptr);
    EXPECT_EQ(child->GetTestValue(), 123);
    EXPECT_EQ(child->GetParent(), component);
    EXPECT_EQ(component->GetChildren().size(), 1);
}

TEST_F(ComponentTest, FindChild) {
    auto child1 = component->CreateChild<TestComponent>();
    child1->SetTestValue(1);
    auto child2 = component->CreateChild<AnotherTestComponent>();
    child2->SetName("test");
    auto child3 = component->CreateChild<TestComponent>();
    child3->SetTestValue(2);
    TestComponent* testChild = component->FindChild<TestComponent>();
    EXPECT_NE(testChild, nullptr);
    EXPECT_EQ(testChild->GetTestValue(), 1);
    AnotherTestComponent* anotherChild = component->FindChild<AnotherTestComponent>();
    EXPECT_NE(anotherChild, nullptr);
    EXPECT_EQ(anotherChild->GetName(), "test");
    auto testChildren = component->FindChildren<TestComponent>();
    EXPECT_EQ(testChildren.size(), 2);
	EXPECT_EQ(testChildren[0]->GetTestValue(), 1);
	EXPECT_EQ(testChildren[1]->GetTestValue(), 2);
}

TEST_F(ComponentTest, ChildInitializationOnAdd) {
    component->Initialize();
    TestComponent* child = manager->CreateComponent<TestComponent>();
    EXPECT_FALSE(child->IsInitialized());
    component->AddChild(child);
    EXPECT_TRUE(component->GetChildren()[0]->IsInitialized());
}

TEST_F(ComponentTest, ChildInitializationHierarchy) {
    TestComponent* child = manager->CreateComponent<TestComponent>();
    TestComponent* grandchild = manager->CreateComponent<TestComponent>();
    child->AddChild(grandchild);
    component->AddChild(child);
    EXPECT_TRUE(component->Initialize());
    EXPECT_TRUE(component->GetChildren()[0]->IsInitialized());
    EXPECT_TRUE(component->GetChildren()[0]->GetChildren()[0]->IsInitialized());
}

TEST_F(ComponentTest, ChildInitializationFailure) {
    FailingTestComponent* failingChild = manager->CreateComponent<FailingTestComponent>();
    component->AddChild(failingChild);
    EXPECT_FALSE(component->Initialize());
    EXPECT_FALSE(component->IsInitialized());
}

TEST_F(ComponentTest, ChildShutdownHierarchy) {
    TestComponent* child = manager->CreateComponent<TestComponent>();
    TestComponent* grandchild = manager->CreateComponent<TestComponent>();
    child->AddChild(grandchild);
    component->AddChild(child);
    component->Initialize();
    component->Shutdown();
    EXPECT_TRUE(child->WasShutdownCalled());
    EXPECT_TRUE(grandchild->WasShutdownCalled());
}

TEST_F(ComponentTest, ShutdownTwice) {
    component->Initialize();
    EXPECT_TRUE(component->IsInitialized());
    EXPECT_FALSE(component->WasShutdownCalled());
    component->Shutdown();
    EXPECT_FALSE(component->IsInitialized());
    EXPECT_TRUE(component->WasShutdownCalled());
    component->ResetFlags();
    component->Shutdown();
    EXPECT_FALSE(component->WasShutdownCalled());
}

// Component Manager Tests
class ComponentManagerTest : public ::testing::Test {
protected:
    void SetUp() override {
        Core::CoreSystem::Initialize(); // Ensure CoreSystem is initialized
        manager = std::make_unique<ComponentManager>();
    }
    void TearDown() override {
        manager.reset();
        Core::CoreSystem::Shutdown(); // Clean up CoreSystem after test
    }
    std::unique_ptr<ComponentManager> manager;
};

TEST_F(ComponentManagerTest, Construction) {
    EXPECT_FALSE(manager->IsInitialized());
    EXPECT_EQ(manager->GetRegisteredTypeCount(), 0);
}

TEST_F(ComponentManagerTest, RegisterComponentType) {
    manager->RegisterComponentType<TestComponent>(5, 20);
    EXPECT_EQ(manager->GetRegisteredTypeCount(), 1);
    EXPECT_EQ(manager->GetTotalComponentCount<TestComponent>(), 5);
    EXPECT_EQ(manager->GetActiveComponentCount<TestComponent>(), 0);
}

TEST_F(ComponentManagerTest, CreateComponent) {
    TestComponent* comp = manager->CreateComponent<TestComponent>();
    EXPECT_NE(comp, nullptr);
    EXPECT_TRUE(comp->IsActive());
    EXPECT_EQ(manager->GetActiveComponentCount<TestComponent>(), 1);
}

TEST_F(ComponentManagerTest, AutoRegistration) {
    AnotherTestComponent* comp = manager->CreateComponent<AnotherTestComponent>();
    EXPECT_NE(comp, nullptr);
    EXPECT_EQ(manager->GetRegisteredTypeCount(), 1);
    EXPECT_EQ(manager->GetActiveComponentCount<AnotherTestComponent>(), 1);
}

TEST_F(ComponentManagerTest, ReleaseComponent) {
    TestComponent* comp = manager->CreateComponent<TestComponent>();
    EXPECT_EQ(manager->GetActiveComponentCount<TestComponent>(), 1);
    manager->ReleaseComponent(comp);
    EXPECT_EQ(manager->GetActiveComponentCount<TestComponent>(), 0);
}

TEST_F(ComponentManagerTest, UpdateAllComponents) {
    TestComponent* comp1 = manager->CreateComponent<TestComponent>();
    AnotherTestComponent* comp2 = manager->CreateComponent<AnotherTestComponent>();
    comp1->Initialize();
    comp2->Initialize();
    EXPECT_FALSE(comp1->WasUpdateCalled());
    manager->UpdateAllComponents(0.016);
    EXPECT_TRUE(comp1->WasUpdateCalled());
}

TEST_F(ComponentManagerTest, UpdateComponentsOfType) {
    TestComponent* comp1 = manager->CreateComponent<TestComponent>();
    AnotherTestComponent* comp2 = manager->CreateComponent<AnotherTestComponent>();
    comp1->Initialize();
    comp2->Initialize();
    EXPECT_FALSE(comp1->WasUpdateCalled());
    manager->UpdateComponentsOfType<TestComponent>(0.016);
    EXPECT_TRUE(comp1->WasUpdateCalled());
}

TEST_F(ComponentManagerTest, Initialize) {
    TestComponent* comp = manager->CreateComponent<TestComponent>();
    EXPECT_FALSE(comp->IsInitialized());
    manager->Initialize();
    EXPECT_TRUE(manager->IsInitialized());
    EXPECT_TRUE(comp->IsInitialized());
}

TEST_F(ComponentManagerTest, Shutdown) {
    TestComponent* comp = manager->CreateComponent<TestComponent>();
    comp->Initialize();
    manager->Initialize();
    manager->Shutdown();
    EXPECT_FALSE(manager->IsInitialized());
    EXPECT_EQ(manager->GetRegisteredTypeCount(), 0);
    EXPECT_TRUE(comp->WasShutdownCalled());
}

TEST_F(ComponentManagerTest, GetComponentsOfType) {
    TestComponent* comp1 = manager->CreateComponent<TestComponent>();
    TestComponent* comp2 = manager->CreateComponent<TestComponent>();
    AnotherTestComponent* comp3 = manager->CreateComponent<AnotherTestComponent>();
    auto testComponents = manager->GetComponentsOfType<TestComponent>();
    auto anotherComponents = manager->GetComponentsOfType<AnotherTestComponent>();
    EXPECT_EQ(testComponents.size(), 2);
    EXPECT_EQ(anotherComponents.size(), 1);
    EXPECT_TRUE(std::find(testComponents.begin(), testComponents.end(), comp1) != testComponents.end());
    EXPECT_TRUE(std::find(testComponents.begin(), testComponents.end(), comp2) != testComponents.end());
    EXPECT_EQ(anotherComponents[0], comp3);
}

TEST_F(ComponentManagerTest, ReleaseNullComponent) {
    manager->ReleaseComponent(nullptr);
    EXPECT_EQ(manager->GetActiveComponentCount<TestComponent>(), 0);
}

TEST_F(ComponentManagerTest, ComponentCounts) {
    manager->RegisterComponentType<TestComponent>(3, 10);
    EXPECT_EQ(manager->GetActiveComponentCount<TestComponent>(), 0);
    EXPECT_EQ(manager->GetTotalComponentCount<TestComponent>(), 3);
    TestComponent* comp1 = manager->CreateComponent<TestComponent>();
    TestComponent* comp2 = manager->CreateComponent<TestComponent>();
    EXPECT_EQ(manager->GetActiveComponentCount<TestComponent>(), 2);
    EXPECT_EQ(manager->GetTotalComponentCount<TestComponent>(), 3);
    manager->ReleaseComponent(comp1);
    EXPECT_EQ(manager->GetActiveComponentCount<TestComponent>(), 1);
    EXPECT_EQ(manager->GetTotalComponentCount<TestComponent>(), 3);
}

// Component Factory Tests
TEST(ComponentFactoryTest, TypedComponentFactory) {
    TypedComponentFactory<TestComponent> factory;

    EXPECT_EQ(factory.GetComponentType(), std::type_index(typeid(TestComponent)));

    auto component = factory.Create();
    EXPECT_NE(component, nullptr);

    TestComponent* testComp = dynamic_cast<TestComponent*>(component.get());
    EXPECT_NE(testComp, nullptr);
}

// Integration Tests
TEST(ComponentSystemIntegrationTest, CompleteWorkflow) {
    Core::CoreSystem::Initialize();
    ComponentManager& manager = *Core::CoreSystem::GetComponentManager();

    manager.RegisterComponentType<TestComponent>(2, 5);
    manager.RegisterComponentType<AnotherTestComponent>(2, 5);

    TestComponent* comp1 = manager.CreateComponent<TestComponent>();
    TestComponent* comp2 = manager.CreateComponent<TestComponent>();
    AnotherTestComponent* comp3 = manager.CreateComponent<AnotherTestComponent>();

    comp1->CreateChild<TestComponent>();
    comp1->CreateChild<AnotherTestComponent>();

    manager.Initialize();

    EXPECT_TRUE(manager.IsInitialized());
    EXPECT_TRUE(comp1->IsInitialized());
    EXPECT_TRUE(comp2->IsInitialized());
    EXPECT_TRUE(comp3->IsInitialized());
    EXPECT_TRUE(comp1->GetChildren()[0]->IsInitialized());
    EXPECT_TRUE(comp1->GetChildren()[1]->IsInitialized());

    manager.UpdateAllComponents(0.016);

    EXPECT_TRUE(static_cast<TestComponent*>(comp1)->WasUpdateCalled());
    EXPECT_TRUE(static_cast<TestComponent*>(comp2)->WasUpdateCalled());

    EXPECT_EQ(manager.GetActiveComponentCount<TestComponent>(), 3);
    EXPECT_EQ(manager.GetActiveComponentCount<AnotherTestComponent>(), 2);

    manager.Shutdown();

    EXPECT_FALSE(manager.IsInitialized());
    EXPECT_EQ(manager.GetRegisteredTypeCount(), 0);

    Core::CoreSystem::Shutdown();
}

TEST(ComponentSystemIntegrationTest, ChildHierarchyManagement) {
    Core::CoreSystem::Initialize();
    ComponentManager& manager = *Core::CoreSystem::GetComponentManager();

    TestComponent* parent = manager.CreateComponent<TestComponent>();
    

    TestComponent* child1 = parent->CreateChild<TestComponent>();
    AnotherTestComponent* child2 = parent->CreateChild<AnotherTestComponent>();
    TestComponent* grandchild = child1->CreateChild<TestComponent>();

    manager.Initialize();

    EXPECT_TRUE(child1->IsInitialized());
    EXPECT_TRUE(child2->IsInitialized());
    EXPECT_TRUE(grandchild->IsInitialized());

    auto testChildren = parent->FindChildren<TestComponent>();
    EXPECT_EQ(testChildren.size(), 1);
    EXPECT_EQ(testChildren[0], child1);

    auto anotherChild = parent->FindChild<AnotherTestComponent>();
    EXPECT_EQ(anotherChild, child2);
  
    parent->RemoveChild(child1);

    EXPECT_EQ(parent->GetChildren().size(), 1);
    EXPECT_EQ(parent->GetChildren()[0], child2);

    Core::CoreSystem::Shutdown();
}