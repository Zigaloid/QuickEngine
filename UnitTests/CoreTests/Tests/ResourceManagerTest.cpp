#include <gtest/gtest.h>
#include <thread>
#include <chrono>
#include <memory>
#include <string>
#include <vector>
#include <unordered_map>
#include <atomic>

#include "ResourceManager/ResourceManager.h"
#include "FileSystem/FileSystemManager.h"
#include "FileSystem/FileSystemInterface.h"

using namespace ResourceSystem;
using namespace FileSystem;
using namespace testing;

// Mock FileSystem for testing
class MockFileSystem : public IFileSystem {
private:
	std::unordered_map<std::string, std::vector<uint8_t>> files_;
	std::unordered_map<std::string, std::string> textFiles_;
	bool shouldFailReads_ = false;

public:
	// Control mock behavior
	void SetShouldFailReads(bool shouldFail) { shouldFailReads_ = shouldFail; }

	void AddMockFile(const std::string& path, const std::vector<uint8_t>& data) {
		files_[path] = data;
	}

	void AddMockTextFile(const std::string& path, const std::string& content) {
		textFiles_[path] = content;
		std::vector<uint8_t> data(content.begin(), content.end());
		files_[path] = data;
	}

	// IFileSystem interface implementation (minimal for testing)
	Result<std::unique_ptr<IFile>> OpenFile(const std::string& path, FileMode mode) override {
		return Result<std::unique_ptr<IFile>>("Not implemented in mock");
	}

	Result<std::unique_ptr<IFile>> CreateFile(const std::string& path) override {
		return Result<std::unique_ptr<IFile>>("Not implemented in mock");
	}

	Result<void> DeleteFile(const std::string& path) override {
		return Result<void>("Not implemented in mock");
	}

	Result<void> CopyFile(const std::string& sourcePath, const std::string& destinationPath, bool overwrite = false) override {
		return Result<void>("Not implemented in mock");
	}

	Result<void> MoveFile(const std::string& sourcePath, const std::string& destinationPath) override {
		return Result<void>("Not implemented in mock");
	}

	Result<std::unique_ptr<IDirectory>> OpenDirectory(const std::string& path) override {
		return Result<std::unique_ptr<IDirectory>>("Not implemented in mock");
	}

	Result<void> CreateDirectory(const std::string& path) override {
		return Result<void>("Not implemented in mock");
	}

	Result<void> DeleteDirectory(const std::string& path, bool recursive = false) override {
		return Result<void>("Not implemented in mock");
	}

	Result<void> CopyDirectory(const std::string& sourcePath, const std::string& destinationPath, bool recursive = true) override {
		return Result<void>("Not implemented in mock");
	}

	Result<void> MoveDirectory(const std::string& sourcePath, const std::string& destinationPath) override {
		return Result<void>("Not implemented in mock");
	}

	bool Exists(const std::string& path) override {
		return files_.find(path) != files_.end();
	}

	bool IsFile(const std::string& path) override {
		return Exists(path);
	}

	bool IsDirectory(const std::string& path) override {
		return false;
	}

	Result<FileInfo> GetInfo(const std::string& path) override {
		return Result<FileInfo>("Not implemented in mock");
	}

	std::string GetFileName(const std::string& path) override {
		auto pos = path.find_last_of("/\\");
		return pos != std::string::npos ? path.substr(pos + 1) : path;
	}

	std::string GetFileNameWithoutExtension(const std::string& path) override {
		std::string filename = GetFileName(path);
		auto pos = filename.find_last_of('.');
		return pos != std::string::npos ? filename.substr(0, pos) : filename;
	}

	std::string GetExtension(const std::string& path) override {
		auto pos = path.find_last_of('.');
		return pos != std::string::npos ? path.substr(pos) : "";
	}

	std::string GetDirectoryName(const std::string& path) override {
		auto pos = path.find_last_of("/\\");
		return pos != std::string::npos ? path.substr(0, pos) : "";
	}

	std::string GetFullPath(const std::string& path) override {
		return path;
	}

	std::string CombinePath(const std::string& path1, const std::string& path2) override {
		return path1 + "/" + path2;
	}

	Result<std::string> GetCurrentDirectory() override {
		return Result<std::string>("Not implemented in mock");
	}

	Result<void> SetCurrentDirectory(const std::string& path) override {
		return Result<void>("Not implemented in mock");
	}

	Result<std::string> GetTempDirectory() override {
		return Result<std::string>("Not implemented in mock");
	}

	Result<std::string> GetUserDirectory() override {
		return Result<std::string>("Not implemented in mock");
	}

	Result<std::string> GetApplicationDirectory() override {
		return Result<std::string>("Not implemented in mock");
	}

	Result<std::vector<uint8_t>> ReadAllBytes(const std::string& path) override {
		if (shouldFailReads_) {
			return Result<std::vector<uint8_t>>("Mock read failure");
		}

		auto it = files_.find(path);
		if (it != files_.end()) {
			return Result<std::vector<uint8_t>>(std::vector<uint8_t>(it->second));
		}
		return Result<std::vector<uint8_t>>("File not found: " + path);
	}

	Result<std::string> ReadAllText(const std::string& path) override {
		if (shouldFailReads_) {
			return Result<std::string>("Mock read failure");
		}

		auto it = textFiles_.find(path);
		if (it != textFiles_.end()) {
			return Result<std::string>(it->second);
		}
		return Result<std::string>("File not found: " + path);
	}

	Result<void> WriteAllBytes(const std::string& path, const std::vector<uint8_t>& data) override {
		return Result<void>("Not implemented in mock");
	}

	Result<void> WriteAllText(const std::string& path, const std::string& text) override {
		return Result<void>("Not implemented in mock");
	}
};

// Test-specific resource classes
class TestResource : public Resource {
public:
	TestResource(const std::string& path) : Resource(path) {}

	bool Initialize() override {
		initializeCallCount_++;
		return Resource::Initialize();
	}

	void Finalize() override {
		finalizeCallCount_++;
		Resource::Finalize();
	}

	static int GetInitializeCallCount() { return initializeCallCount_; }
	static int GetFinalizeCallCount() { return finalizeCallCount_; }
	static void ResetCounts() { initializeCallCount_ = 0; finalizeCallCount_ = 0; }

private:
	static int initializeCallCount_;
	static int finalizeCallCount_;
};

int TestResource::initializeCallCount_ = 0;
int TestResource::finalizeCallCount_ = 0;

class SlowLoadingResource : public Resource {
public:
	SlowLoadingResource(const std::string& path) : Resource(path) {}

	bool Update(FileSystemManager& fileSystem) override {
		updateCallCount_++;
		if (updateCallCount_ < requiredUpdateCalls_) {
			return false; // Not ready yet
		}
		return Resource::Update(fileSystem);
	}

	static void SetRequiredUpdateCalls(int calls) { requiredUpdateCalls_ = calls; }
	static int GetUpdateCallCount() { return updateCallCount_; }
	static void ResetUpdateCount() { updateCallCount_ = 0; }

private:
	static int requiredUpdateCalls_;
	static int updateCallCount_;
};

int SlowLoadingResource::requiredUpdateCalls_ = 3;
int SlowLoadingResource::updateCallCount_ = 0;

// Main test fixture
class ResourceManagerTest : public ::testing::Test {
protected:
	void SetUp() override {
		// Create file system manager first
		fileSystemManager_ = std::make_unique<FileSystemManager>();

		// Create mock file system and add files before adding to manager
		auto mockFS = std::make_unique<MockFileSystem>();
		mockFS->AddMockTextFile("test.txt", "Test file content");
		mockFS->AddMockTextFile("config.json", "{\"test\": true}");
		mockFS->AddMockFile("image.png", { 0x89, 0x50, 0x4E, 0x47, 0x0D, 0x0A });

		// Store raw pointer for later use if needed
		mockFS_ = mockFS.get();

		// Add mock to file system manager
		fileSystemManager_->AddFileSystem(std::move(mockFS));

		// Create resource manager
		resourceManager_ = std::make_unique<ResourceManager>(fileSystemManager_.get());

		// Reset static counters
		TestResource::ResetCounts();
		SlowLoadingResource::ResetUpdateCount();
	}

	void TearDown() override {
		if (resourceManager_) {
			resourceManager_->Stop();
		}
		resourceManager_.reset();
		fileSystemManager_.reset();
		mockFS_ = nullptr; // Don't delete, it was moved to fileSystemManager
	}

	// Helper to wait for resource loading with timeout
	bool WaitForResourceLoaded(const std::string& path, std::chrono::milliseconds timeout = std::chrono::milliseconds(1000)) {
		auto startTime = std::chrono::steady_clock::now();
		while (!resourceManager_->IsResourceLoaded(path)) {
			resourceManager_->UpdateFinalization();
			std::this_thread::sleep_for(std::chrono::milliseconds(10));

			if (std::chrono::steady_clock::now() - startTime > timeout) {
				return false;
			}
		}
		return true;
	}

	bool WaitForResourceFinalized(const std::string& path, std::chrono::milliseconds timeout = std::chrono::milliseconds(1000)) {
		auto startTime = std::chrono::steady_clock::now();
		while (!resourceManager_->IsResourceFinalized(path)) {
			resourceManager_->UpdateFinalization();
			std::this_thread::sleep_for(std::chrono::milliseconds(10));

			if (std::chrono::steady_clock::now() - startTime > timeout) {
				return false;
			}
		}
		return true;
	}

	MockFileSystem* mockFS_; // Raw pointer, ownership transferred to fileSystemManager_
	std::unique_ptr<FileSystemManager> fileSystemManager_;
	std::unique_ptr<ResourceManager> resourceManager_;
};

// ============================================================================
// Basic ResourceManager Tests
// ============================================================================

TEST_F(ResourceManagerTest, ConstructorAndBasicProperties) {
	EXPECT_FALSE(resourceManager_->IsRunning());
	EXPECT_EQ(resourceManager_->GetLoadedResourceCount(), 0);
	EXPECT_EQ(resourceManager_->GetPendingLoadCount(), 0);
	EXPECT_EQ(resourceManager_->GetPendingFinalizationCount(), 0);
}

TEST_F(ResourceManagerTest, StartAndStop) {
	// Test starting
	EXPECT_TRUE(resourceManager_->Start());
	EXPECT_TRUE(resourceManager_->IsRunning());

	// Test starting when already running
	EXPECT_FALSE(resourceManager_->Start());

	// Test stopping
	resourceManager_->Stop();
	EXPECT_FALSE(resourceManager_->IsRunning());

	// Test stopping when already stopped (should not crash)
	resourceManager_->Stop();
	EXPECT_FALSE(resourceManager_->IsRunning());
}

TEST_F(ResourceManagerTest, ConstructorWithNullFileSystem) {
	ResourceManager invalidManager(nullptr);
	EXPECT_FALSE(invalidManager.Start());
	EXPECT_FALSE(invalidManager.IsRunning());
}

// ============================================================================
// Resource Request and Loading Tests
// ============================================================================

TEST_F(ResourceManagerTest, RequestBasicResource) {
	EXPECT_TRUE(resourceManager_->Start());

	// Request a resource
	auto resource = resourceManager_->RequestResource<TestResource>("test.txt");
	ASSERT_NE(resource, nullptr);
	EXPECT_EQ(resource->GetPath(), "test.txt");
	EXPECT_FALSE(resource->IsLoaded());
	EXPECT_FALSE(resource->IsFinalized());

	// Resource should be added to loaded resources count
	EXPECT_EQ(resourceManager_->GetLoadedResourceCount(), 1);
	EXPECT_GT(resourceManager_->GetPendingLoadCount(), 0);
}

TEST_F(ResourceManagerTest, RequestSameResourceTwice) {
	EXPECT_TRUE(resourceManager_->Start());

	auto resource1 = resourceManager_->RequestResource<TestResource>("test.txt");
	auto resource2 = resourceManager_->RequestResource<TestResource>("test.txt");

	// Should return the same instance
	EXPECT_EQ(resource1.get(), resource2.get());
	EXPECT_EQ(resourceManager_->GetLoadedResourceCount(), 1);
}

TEST_F(ResourceManagerTest, RequestMultipleResources) {
	EXPECT_TRUE(resourceManager_->Start());

	auto resource1 = resourceManager_->RequestResource<TestResource>("test.txt");
	auto resource2 = resourceManager_->RequestResource<TestResource>("config.json");
	auto resource3 = resourceManager_->RequestResource<TestResource>("image.png");

	EXPECT_NE(resource1.get(), resource2.get());
	EXPECT_NE(resource2.get(), resource3.get());
	EXPECT_EQ(resourceManager_->GetLoadedResourceCount(), 3);
}

TEST_F(ResourceManagerTest, ResourceLoadingLifecycle) {
	EXPECT_TRUE(resourceManager_->Start());

	auto resource = resourceManager_->RequestResource<TestResource>("test.txt");

	// Wait for resource to load
	ASSERT_TRUE(WaitForResourceLoaded("test.txt"));

	// Check that resource is loaded
	EXPECT_TRUE(resource->IsLoaded());
	EXPECT_FALSE(resource->IsFinalized());
	EXPECT_GT(TestResource::GetInitializeCallCount(), 0);

	// Wait for finalization
	ASSERT_TRUE(WaitForResourceFinalized("test.txt"));

	// Check that resource is finalized
	EXPECT_TRUE(resource->IsFinalized());
	EXPECT_GT(TestResource::GetFinalizeCallCount(), 0);

	// Verify data was loaded correctly
	const auto& data = resource->GetData();
	std::string content(data.begin(), data.end());
	EXPECT_EQ(content, "Test file content");
}

TEST_F(ResourceManagerTest, GetResourceBeforeLoaded) {
	EXPECT_TRUE(resourceManager_->Start());

	resourceManager_->RequestResource<TestResource>("test.txt");

	// Should return nullptr when not yet loaded
	auto resource = resourceManager_->GetResource<TestResource>("test.txt");
	EXPECT_EQ(resource, nullptr);

	// Wait for loading
	ASSERT_TRUE(WaitForResourceLoaded("test.txt"));

	// Should return valid resource after loading
	resource = resourceManager_->GetResource<TestResource>("test.txt");
	EXPECT_NE(resource, nullptr);
	EXPECT_TRUE(resource->IsLoaded());
}

TEST_F(ResourceManagerTest, GetNonExistentResource) {
	EXPECT_TRUE(resourceManager_->Start());

	auto resource = resourceManager_->GetResource<TestResource>("nonexistent.txt");
	EXPECT_EQ(resource, nullptr);
}

// ============================================================================
// Resource State Query Tests
// ============================================================================

TEST_F(ResourceManagerTest, IsResourceLoadedQueries) {
	EXPECT_TRUE(resourceManager_->Start());

	// Non-existent resource
	EXPECT_FALSE(resourceManager_->IsResourceLoaded("nonexistent.txt"));

	// Requested but not loaded
	resourceManager_->RequestResource<TestResource>("test.txt");
	EXPECT_FALSE(resourceManager_->IsResourceLoaded("test.txt"));

	// After loading
	WaitForResourceLoaded("test.txt");
	EXPECT_TRUE(resourceManager_->IsResourceLoaded("test.txt"));
}

TEST_F(ResourceManagerTest, IsResourceFinalizedQueries) {
	EXPECT_TRUE(resourceManager_->Start());

	// Non-existent resource
	EXPECT_FALSE(resourceManager_->IsResourceFinalized("nonexistent.txt"));

	// Requested but not finalized
	resourceManager_->RequestResource<TestResource>("test.txt");
	EXPECT_FALSE(resourceManager_->IsResourceFinalized("test.txt"));

	// After loading but before finalization
	WaitForResourceLoaded("test.txt");
	EXPECT_FALSE(resourceManager_->IsResourceFinalized("test.txt"));

	// After finalization
	WaitForResourceFinalized("test.txt");
	EXPECT_TRUE(resourceManager_->IsResourceFinalized("test.txt"));
}

// ============================================================================
// Statistics and Management Tests
// ============================================================================

TEST_F(ResourceManagerTest, StatisticsTracking) {
	EXPECT_TRUE(resourceManager_->Start());

	EXPECT_EQ(resourceManager_->GetLoadedResourceCount(), 0);
	EXPECT_EQ(resourceManager_->GetPendingLoadCount(), 0);
	EXPECT_EQ(resourceManager_->GetPendingFinalizationCount(), 0);

	// Request multiple resources
	resourceManager_->RequestResource<TestResource>("test.txt");
	resourceManager_->RequestResource<TestResource>("config.json");

	EXPECT_EQ(resourceManager_->GetLoadedResourceCount(), 2);
	EXPECT_GT(resourceManager_->GetPendingLoadCount(), 0);

	// Wait for loading to complete
	WaitForResourceLoaded("test.txt");
	WaitForResourceLoaded("config.json");

	// Pending load count should decrease, finalization count should increase
	EXPECT_GE(resourceManager_->GetPendingFinalizationCount(), 0);

	// Process finalization
	resourceManager_->UpdateFinalization();

	// All should be processed
	EXPECT_EQ(resourceManager_->GetPendingLoadCount(), 0);
	EXPECT_EQ(resourceManager_->GetPendingFinalizationCount(), 0);
}

TEST_F(ResourceManagerTest, RemoveResource) {
	EXPECT_TRUE(resourceManager_->Start());

	resourceManager_->RequestResource<TestResource>("test.txt");
	EXPECT_EQ(resourceManager_->GetLoadedResourceCount(), 1);

	// Remove existing resource
	EXPECT_TRUE(resourceManager_->RemoveResource("test.txt"));
	EXPECT_EQ(resourceManager_->GetLoadedResourceCount(), 0);

	// Remove non-existent resource
	EXPECT_FALSE(resourceManager_->RemoveResource("nonexistent.txt"));
}

TEST_F(ResourceManagerTest, ClearResources) {
	EXPECT_TRUE(resourceManager_->Start());

	resourceManager_->RequestResource<TestResource>("test.txt");
	resourceManager_->RequestResource<TestResource>("config.json");
	resourceManager_->RequestResource<TestResource>("image.png");

	EXPECT_EQ(resourceManager_->GetLoadedResourceCount(), 3);

	resourceManager_->ClearResources();
	EXPECT_EQ(resourceManager_->GetLoadedResourceCount(), 0);
}

// ============================================================================
// Threading and Concurrency Tests
// ============================================================================

TEST_F(ResourceManagerTest, ConcurrentResourceRequests) {
	EXPECT_TRUE(resourceManager_->Start());

	const int numThreads = 4;
	const int resourcesPerThread = 10;
	std::vector<std::thread> threads;
	std::vector<std::vector<std::shared_ptr<TestResource>>> resources(numThreads);

	// Launch multiple threads requesting resources
	for (int t = 0; t < numThreads; ++t) {
		threads.emplace_back([this, t, resourcesPerThread, &resources]() {
			for (int i = 0; i < resourcesPerThread; ++i) {
				std::string path = "thread_" + std::to_string(t) + "_resource_" + std::to_string(i) + ".txt";

				auto resource = resourceManager_->RequestResource<TestResource>(path);
				resources[t].push_back(resource);
			}
			});
	}

	// Wait for all threads to complete
	for (auto& thread : threads) {
		thread.join();
	}

	// Verify all resources were created
	EXPECT_EQ(resourceManager_->GetLoadedResourceCount(), numThreads * resourcesPerThread);

	// Verify each resource is valid
	for (int t = 0; t < numThreads; ++t) {
		for (int i = 0; i < resourcesPerThread; ++i) {
			EXPECT_NE(resources[t][i], nullptr);
		}
	}
}

TEST_F(ResourceManagerTest, ThreadSafetyOfStateQueries) {
	EXPECT_TRUE(resourceManager_->Start());

	resourceManager_->RequestResource<TestResource>("test.txt");

	std::atomic<bool> stopFlag{ false };
	std::atomic<int> errorCount{ 0 };

	// Launch multiple threads querying resource state
	std::vector<std::thread> threads;
	for (int i = 0; i < 4; ++i) {
		threads.emplace_back([this, &stopFlag, &errorCount]() {
			while (!stopFlag) {
				try {
					resourceManager_->IsResourceLoaded("test.txt");
					resourceManager_->IsResourceFinalized("test.txt");
					resourceManager_->GetLoadedResourceCount();
					resourceManager_->GetPendingLoadCount();
					resourceManager_->GetPendingFinalizationCount();
				}
				catch (...) {
					errorCount++;
				}
				std::this_thread::sleep_for(std::chrono::milliseconds(1));
			}
			});
	}

	// Let threads run for a short time
	std::this_thread::sleep_for(std::chrono::milliseconds(100));
	stopFlag = true;

	// Wait for threads to complete
	for (auto& thread : threads) {
		thread.join();
	}

	// Should not have any errors from concurrent access
	EXPECT_EQ(errorCount.load(), 0);
}

// ============================================================================
// Error Handling Tests
// ============================================================================

TEST_F(ResourceManagerTest, NonExistentFileHandling) {
	EXPECT_TRUE(resourceManager_->Start());

	auto resource = resourceManager_->RequestResource<TestResource>("nonexistent.txt");
	ASSERT_NE(resource, nullptr);

	// Wait a reasonable time - resource should not load
	std::this_thread::sleep_for(std::chrono::milliseconds(100));

	EXPECT_FALSE(resource->IsLoaded());
	EXPECT_FALSE(resourceManager_->IsResourceLoaded("nonexistent.txt"));
}

TEST_F(ResourceManagerTest, FileSystemFailureHandling) {
	// Create a new mock that will fail reads
	auto failingFS = std::make_unique<MockFileSystem>();
	failingFS->SetShouldFailReads(true);
	failingFS->AddMockTextFile("test.txt", "Content"); // Add file but it will fail to read

	auto failingFSManager = std::make_unique<FileSystemManager>();
	failingFSManager->AddFileSystem(std::move(failingFS));

	ResourceManager failingResourceManager(failingFSManager.get());
	EXPECT_TRUE(failingResourceManager.Start());

	auto resource = failingResourceManager.RequestResource<TestResource>("test.txt");
	ASSERT_NE(resource, nullptr);

	// Wait a reasonable time - resource should not load due to file system failure
	std::this_thread::sleep_for(std::chrono::milliseconds(100));

	EXPECT_FALSE(resource->IsLoaded());
	EXPECT_FALSE(failingResourceManager.IsResourceLoaded("test.txt"));

	failingResourceManager.Stop();
}

// ============================================================================
// Specialized Resource Tests
// ============================================================================

TEST_F(ResourceManagerTest, SlowLoadingResource) {
	EXPECT_TRUE(resourceManager_->Start());

	SlowLoadingResource::SetRequiredUpdateCalls(5);
	SlowLoadingResource::ResetUpdateCount();

	auto resource = resourceManager_->RequestResource<SlowLoadingResource>("test.txt");

	// Should take multiple update cycles to load
	std::this_thread::sleep_for(std::chrono::milliseconds(50));
	EXPECT_FALSE(resource->IsLoaded());
	EXPECT_GT(SlowLoadingResource::GetUpdateCallCount(), 1);

	// Eventually should load
	ASSERT_TRUE(WaitForResourceLoaded("test.txt", std::chrono::milliseconds(500)));
	EXPECT_TRUE(resource->IsLoaded());
	EXPECT_GE(SlowLoadingResource::GetUpdateCallCount(), 5);
}

TEST_F(ResourceManagerTest, TemplatedResourceTypes) {
	EXPECT_TRUE(resourceManager_->Start());

	// Request resources with different template types
	auto baseResource = resourceManager_->RequestResource("test.txt");
	auto testResource = resourceManager_->RequestResource<TestResource>("config.json");
	auto slowResource = resourceManager_->RequestResource<SlowLoadingResource>("image.png");

	EXPECT_NE(baseResource, nullptr);
	EXPECT_NE(testResource, nullptr);
	EXPECT_NE(slowResource, nullptr);

	// All should be different instances - cast to void* to compare addresses
	EXPECT_NE(static_cast<void*>(baseResource.get()), static_cast<void*>(testResource.get()));
	EXPECT_NE(static_cast<void*>(testResource.get()), static_cast<void*>(slowResource.get()));

	EXPECT_EQ(resourceManager_->GetLoadedResourceCount(), 3);
}

// ============================================================================
// Integration Tests
// ============================================================================

TEST_F(ResourceManagerTest, FullWorkflowIntegration) {
	EXPECT_TRUE(resourceManager_->Start());

	// Request multiple resources
	auto resource1 = resourceManager_->RequestResource<TestResource>("test.txt");
	auto resource2 = resourceManager_->RequestResource<TestResource>("config.json");

	// Initially not loaded
	EXPECT_FALSE(resource1->IsLoaded());
	EXPECT_FALSE(resource2->IsLoaded());

	// Wait for loading
	ASSERT_TRUE(WaitForResourceLoaded("test.txt"));
	ASSERT_TRUE(WaitForResourceLoaded("config.json"));

	// Verify loaded state
	EXPECT_TRUE(resource1->IsLoaded());
	EXPECT_TRUE(resource2->IsLoaded());

	// Verify content
	std::string content1(resource1->GetData().begin(), resource1->GetData().end());
	std::string content2(resource2->GetData().begin(), resource2->GetData().end());

	EXPECT_EQ(content1, "Test file content");
	EXPECT_EQ(content2, "{\"test\": true}");

	// Wait for finalization
	ASSERT_TRUE(WaitForResourceFinalized("test.txt"));
	ASSERT_TRUE(WaitForResourceFinalized("config.json"));

	// Verify finalized state
	EXPECT_TRUE(resource1->IsFinalized());
	EXPECT_TRUE(resource2->IsFinalized());

	// Verify resources can be retrieved
	auto retrieved1 = resourceManager_->GetResource<TestResource>("test.txt");
	auto retrieved2 = resourceManager_->GetResource<TestResource>("config.json");

	EXPECT_EQ(retrieved1.get(), resource1.get());
	EXPECT_EQ(retrieved2.get(), resource2.get());

	// Cleanup
	resourceManager_->Stop();
	EXPECT_FALSE(resourceManager_->IsRunning());
}

TEST_F(ResourceManagerTest, ResourceManagerDestruction) {
	{
		ResourceManager localManager(fileSystemManager_.get());
		EXPECT_TRUE(localManager.Start());

		auto resource = localManager.RequestResource<TestResource>("test.txt");
		EXPECT_NE(resource, nullptr);

		// Manager destructor should handle cleanup properly
	} // localManager goes out of scope here

	// Test should complete without hanging or crashing
	SUCCEED();
}

// ============================================================================
// Edge Cases and Stress Tests
// ============================================================================

TEST_F(ResourceManagerTest, EmptyPathHandling) {
	EXPECT_TRUE(resourceManager_->Start());

	auto resource = resourceManager_->RequestResource<TestResource>("");
	EXPECT_NE(resource, nullptr);
	EXPECT_EQ(resource->GetPath(), "");
}

TEST_F(ResourceManagerTest, VeryLongPathHandling) {
	EXPECT_TRUE(resourceManager_->Start());

	std::string longPath(1000, 'x');
	longPath += ".txt";

	auto resource = resourceManager_->RequestResource<TestResource>(longPath);
	EXPECT_NE(resource, nullptr);
	EXPECT_EQ(resource->GetPath(), longPath);
}

TEST_F(ResourceManagerTest, ManyResourcesStressTest) {
	EXPECT_TRUE(resourceManager_->Start());

	const int numResources = 100;
	std::vector<std::shared_ptr<TestResource>> resources;

	// Request many resources
	for (int i = 0; i < numResources; ++i) {
		std::string path = "stress_test_" + std::to_string(i) + ".txt";
		auto resource = resourceManager_->RequestResource<TestResource>(path);
		resources.push_back(resource);
	}

	EXPECT_EQ(resourceManager_->GetLoadedResourceCount(), numResources);

	// All resources should be valid and have unique paths
	for (int i = 0; i < numResources; ++i) {
		EXPECT_NE(resources[i], nullptr);
		std::string expectedPath = "stress_test_" + std::to_string(i) + ".txt";
		EXPECT_EQ(resources[i]->GetPath(), expectedPath);
	}
}