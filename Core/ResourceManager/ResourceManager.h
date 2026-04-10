#pragma once

#include "Filesystem/FileSystemManager.h"
#include "Profiler/Profiler.h"

#include <string>
#include <vector>
#include <memory>
#include <unordered_map>
#include <queue>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <atomic>

namespace ResourceSystem {

    // Forward declarations
    class ResourceManager;

    // Base Resource Class
    class Resource {
    protected:
        std::string path_;
        std::vector<uint8_t> data_;
        std::atomic<bool> isInitialized_{ false };
        std::atomic<bool> isLoaded_{ false };
        std::atomic<bool> isFinalized_{ false };

    public:
        Resource(const std::string& path) : path_(path) {}
        virtual ~Resource() = default;

        // Virtual lifecycle methods
        virtual bool Initialize() {
            isInitialized_ = true;
            return true;
        }

        virtual bool Update(FileSystem::FileSystemManager& fileSystem) {
            DECLARE_FUNC_LOW();
            if (isLoaded_) {
                return true;
            }

            auto result = fileSystem.ReadAllBytes(path_);
            if (result.IsSuccess()) {
                data_ = result.GetValue();
                isLoaded_ = true;
                return true;
            }

            return false;
        }

        virtual void Finalize() {
            isFinalized_ = true;
        }

        // Getters
        const std::string& GetPath() const { return path_; }
        const std::vector<uint8_t>& GetData() const { return data_; }
        // Returns the size (in bytes) of the loaded data block.
        size_t GetLoadedSize() const { return data_.size(); }
        bool IsInitialized() const { return isInitialized_; }
        bool IsLoaded() const { return isLoaded_; }
        bool IsFinalized() const { return isFinalized_; }
    };

    // Resource Manager
    class ResourceManager {
    private:
        // Thread management
        std::unique_ptr<std::thread> workerThread_;
        std::atomic<bool> isRunning_{ false };
        std::atomic<bool> shouldStop_{ false };

        // File system reference
        FileSystem::FileSystemManager* fileSystemManager_;

        // Resource storage
        std::unordered_map<std::string, std::shared_ptr<Resource>> loadedResources_;
        mutable std::mutex loadedResourcesMutex_;

        // Processing queues
        std::queue<std::shared_ptr<Resource>> loadingQueue_;
        mutable std::mutex loadingQueueMutex_;
        std::condition_variable loadingQueueCondition_;

        std::queue<std::shared_ptr<Resource>> finalizationQueue_;
        mutable std::mutex finalizationQueueMutex_;

        // Worker thread function
        void WorkerThreadFunction() {
            while (!shouldStop_) {
                DECLARE_FUNC_MEDIUM();
                std::shared_ptr<Resource> resource = nullptr;

                // Wait for work or stop signal
                {
                    std::unique_lock<std::mutex> lock(loadingQueueMutex_);
                    loadingQueueCondition_.wait(lock, [this] {
                        return !loadingQueue_.empty() || shouldStop_;
                        });

                    if (shouldStop_) {
                        break;
                    }

                    if (!loadingQueue_.empty()) {
                        resource = loadingQueue_.front();
                        loadingQueue_.pop();
                    }
                }

                // Process the resource if we have one
                if (resource) {
                    // Initialize if not already done
                    if (!resource->IsInitialized()) {
                        resource->Initialize();
                    }

                    // Update until complete
                    bool updateComplete = false;
                    while (!updateComplete && !shouldStop_) {
                        updateComplete = resource->Update(*fileSystemManager_);

                        if (!updateComplete) {
                            // Small delay to prevent busy waiting
                            std::this_thread::sleep_for(std::chrono::milliseconds(1));
                        }
                    }

                    // Move to finalization queue if update completed
                    if (updateComplete) {
                        std::lock_guard<std::mutex> lock(finalizationQueueMutex_);
                        finalizationQueue_.push(resource);
                    }
                }
            }
        }

    public:
        ResourceManager(FileSystem::FileSystemManager* fileSystemManager)
            : fileSystemManager_(fileSystemManager) {
        }

        ~ResourceManager() {
            Stop();
        }

        // Start the resource manager
        bool Start() {
            if (isRunning_) {
                return false; // Already running
            }

            if (!fileSystemManager_) {
                return false; // No file system manager
            }

            shouldStop_ = false;
            workerThread_ = std::make_unique<std::thread>(&ResourceManager::WorkerThreadFunction, this);
            isRunning_ = true;
            return true;
        }

        // Stop the resource manager
        void Stop() {
            if (!isRunning_) {
                return;
            }

            shouldStop_ = true;
            loadingQueueCondition_.notify_all();

            if (workerThread_ && workerThread_->joinable()) {
                workerThread_->join();
            }

            workerThread_.reset();
            isRunning_ = false;
        }

        // Request a resource (returns existing one if already requested)
        template<typename T = Resource>
        std::shared_ptr<T> RequestResource(const std::string& path) {
            static_assert(std::is_base_of_v<Resource, T>, "T must derive from Resource");

            std::lock_guard<std::mutex> lock(loadedResourcesMutex_);

            // Check if resource already exists
            auto it = loadedResources_.find(path);
            if (it != loadedResources_.end()) {
                return std::static_pointer_cast<T>(it->second);
            }

            // Create new resource
            auto resource = std::make_shared<T>(path);
            loadedResources_[path] = resource;

            // Add to loading queue
            {
                std::lock_guard<std::mutex> queueLock(loadingQueueMutex_);
                loadingQueue_.push(resource);
            }
            loadingQueueCondition_.notify_one();

            return resource;
        }

        // Update function to finalize resources on main thread
        void UpdateFinalization() {
            DECLARE_FUNC_LOW();
            std::queue<std::shared_ptr<Resource>> resourcesToFinalize;

            // Move all pending resources to local queue
            {
                std::lock_guard<std::mutex> lock(finalizationQueueMutex_);
                resourcesToFinalize = std::move(finalizationQueue_);
                finalizationQueue_ = std::queue<std::shared_ptr<Resource>>(); // Reset queue
            }

            // Finalize resources on main thread
            while (!resourcesToFinalize.empty()) {
                auto resource = resourcesToFinalize.front();
                resourcesToFinalize.pop();

                if (!resource->IsFinalized()) {
                    resource->Finalize();
                }
            }
        }

        // Get resource if it exists and is loaded
        template<typename T = Resource>
        std::shared_ptr<T> GetResource(const std::string& path) {
            static_assert(std::is_base_of_v<Resource, T>, "T must derive from Resource");

            std::lock_guard<std::mutex> lock(loadedResourcesMutex_);

            auto it = loadedResources_.find(path);
            if (it != loadedResources_.end() && it->second->IsLoaded()) {
                return std::static_pointer_cast<T>(it->second);
            }

            return nullptr;
        }

        // Check if resource exists and is loaded
        bool IsResourceLoaded(const std::string& path) const {
            std::lock_guard<std::mutex> lock(loadedResourcesMutex_);

            auto it = loadedResources_.find(path);
            return it != loadedResources_.end() && it->second->IsLoaded();
        }

        // Check if resource exists and is finalized
        bool IsResourceFinalized(const std::string& path) const {
            std::lock_guard<std::mutex> lock(loadedResourcesMutex_);

            auto it = loadedResources_.find(path);
            return it != loadedResources_.end() && it->second->IsFinalized();
        }

        // Get statistics
        size_t GetLoadedResourceCount() const {
            std::lock_guard<std::mutex> lock(loadedResourcesMutex_);
            return loadedResources_.size();
        }

        size_t GetPendingLoadCount() const {
            std::lock_guard<std::mutex> lock(loadingQueueMutex_);
            return loadingQueue_.size();
        }

        size_t GetPendingFinalizationCount() const {
            std::lock_guard<std::mutex> lock(finalizationQueueMutex_);
            return finalizationQueue_.size();
        }

        // Remove a resource (useful for cleanup)
        bool RemoveResource(const std::string& path) {
            std::lock_guard<std::mutex> lock(loadedResourcesMutex_);

            auto it = loadedResources_.find(path);
            if (it != loadedResources_.end()) {
                loadedResources_.erase(it);
                return true;
            }
            return false;
        }

        // Clear all resources
        void ClearResources() {
            std::lock_guard<std::mutex> lock(loadedResourcesMutex_);
            loadedResources_.clear();
        }

        // Check if manager is running
        bool IsRunning() const {
            return isRunning_;
        }
    };

} // namespace ResourceSystem


class CResourceReference : public CReflectedBase
{
public:
    REFL_DECLARE_OBJECT(CResourceReference, CReflectedBase);
	const std::string GetResourceFileName() const { return m_resourceFileName; }
private:
    std::string m_resourceFileName = "undifined";
};
