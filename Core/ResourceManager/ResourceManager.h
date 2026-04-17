#pragma once

#include "Filesystem/FileSystemManager.h"
#include "Profiler/Profiler.h"
#include "Reflection/ReflectionBase.h"

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

	// ── Resource ──────────────────────────────────────────────────────────────────
	class Resource : public CReflectedBase
	{
	public:
		REFL_DECLARE_OBJECT(Resource, CReflectedBase);
		Resource() = default;
	protected:
		std::string             m_path;
		std::vector<uint8_t>    m_data;
		std::atomic<bool>       m_isInitialized{ false };
		std::atomic<bool>       m_isLoaded{ false };
		std::atomic<bool>       m_isFinalized{ false };

	public:
		explicit Resource(const std::string& path) : m_path(path) {}
		virtual ~Resource() = default;

		/** @return true on success. */
		virtual bool Initialize()
		{
			m_isInitialized = true;
			return true;
		}

		/** @param fileSystem File system to read from.
		 *  @return true once the data has been fully loaded. */
		virtual bool Update(FileSystem::FileSystemManager& fileSystem)
		{
			DECLARE_FUNC_LOW();
			if (m_isLoaded)
			{
				return true;
			}

			auto result = fileSystem.ReadAllBytes(m_path);
			if (result.IsSuccess())
			{
				m_data = result.GetValue();
				m_isLoaded = true;
				return true;
			}

			return false;
		}

		/** @brief Called on the main thread after Update() completes. */
		virtual void Finalize()
		{
			m_isFinalized = true;
		}

		const std::string& GetPath()       const { return m_path; }
		const std::vector<uint8_t>& GetData()       const { return m_data; }
		size_t                      GetLoadedSize() const { return m_data.size(); }
		bool                        IsInitialized() const { return m_isInitialized; }
		bool                        IsLoaded()      const { return m_isLoaded; }
		bool                        IsFinalized()   const { return m_isFinalized; }
	};

	// ── ResourceManager ───────────────────────────────────────────────────────────

	/** @brief Asynchronous resource manager with a worker-thread loading pipeline. */
	class ResourceManager
	{
	private:
		// ── Thread Management ─────────────────────────────────────────────────────
		std::unique_ptr<std::thread>    m_workerThread;
		std::atomic<bool>               m_isRunning{ false };
		std::atomic<bool>               m_shouldStop{ false };

		// ── File System Reference ─────────────────────────────────────────────────
		FileSystem::FileSystemManager* m_fileSystemManager;

		// ── Resource Storage ──────────────────────────────────────────────────────
		std::unordered_map<std::string, std::shared_ptr<Resource>> m_loadedResources;
		mutable std::mutex              m_loadedResourcesMutex;

		// ── Processing Queues ─────────────────────────────────────────────────────
		std::queue<std::shared_ptr<Resource>> m_loadingQueue;
		mutable std::mutex              m_loadingQueueMutex;
		std::condition_variable         m_loadingQueueCondition;

		std::queue<std::shared_ptr<Resource>> m_finalizationQueue;
		mutable std::mutex              m_finalizationQueueMutex;

		void WorkerThreadFunction()
		{
			while (!m_shouldStop)
			{
				DECLARE_FUNC_MEDIUM();
				std::shared_ptr<Resource> resource = nullptr;

				{
					std::unique_lock<std::mutex> lock(m_loadingQueueMutex);
					m_loadingQueueCondition.wait(lock, [this]
						{
							return !m_loadingQueue.empty() || m_shouldStop.load();
						});

					if (m_shouldStop)
					{
						break;
					}

					if (!m_loadingQueue.empty())
					{
						resource = m_loadingQueue.front();
						m_loadingQueue.pop();
					}
				}

				if (resource)
				{
					if (!resource->IsInitialized())
					{
						resource->Initialize();
					}

					bool updateComplete = false;
					while (!updateComplete && !m_shouldStop)
					{
						updateComplete = resource->Update(*m_fileSystemManager);

						if (!updateComplete)
						{
							std::this_thread::sleep_for(std::chrono::milliseconds(1));
						}
					}

					if (updateComplete)
					{
						std::lock_guard<std::mutex> lock(m_finalizationQueueMutex);
						m_finalizationQueue.push(resource);
					}
				}
			}
		}

	public:
		explicit ResourceManager(FileSystem::FileSystemManager* fileSystemManager)
			: m_fileSystemManager(fileSystemManager)
		{
		}

		~ResourceManager()
		{
			Stop();
		}

		/** @return false if already running or no file system manager is set. */
		bool Start()
		{
			if (m_isRunning)
			{
				return false;
			}

			if (!m_fileSystemManager)
			{
				return false;
			}

			m_shouldStop = false;
			m_workerThread = std::make_unique<std::thread>(&ResourceManager::WorkerThreadFunction, this);
			m_isRunning = true;
			return true;
		}

		void Stop()
		{
			if (!m_isRunning)
			{
				return;
			}

			m_shouldStop = true;
			m_loadingQueueCondition.notify_all();

			if (m_workerThread && m_workerThread->joinable())
			{
				m_workerThread->join();
			}

			m_workerThread.reset();
			m_isRunning = false;
		}

		/** @brief Request a resource by path. Returns an existing instance if already requested.
		 *  @tparam T   Resource type (must derive from Resource).
		 *  @param path File path.
		 *  @return Shared pointer to the resource. */
		template<typename T = Resource>
		std::shared_ptr<T> RequestResource(const std::string& path)
		{
			static_assert(std::is_base_of_v<Resource, T>, "T must derive from Resource");

			std::lock_guard<std::mutex> lock(m_loadedResourcesMutex);

			auto it = m_loadedResources.find(path);
			if (it != m_loadedResources.end())
			{
				return std::static_pointer_cast<T>(it->second);
			}

			auto resource = std::make_shared<T>(path);
			m_loadedResources[path] = resource;

			{
				std::lock_guard<std::mutex> queueLock(m_loadingQueueMutex);
				m_loadingQueue.push(resource);
			}
			m_loadingQueueCondition.notify_one();

			return resource;
		}

		/** @brief Finalize all resources that have completed loading. Must be called on the main thread. */
		void UpdateFinalization()
		{
			DECLARE_FUNC_LOW();
			std::queue<std::shared_ptr<Resource>> resourcesToFinalize;

			{
				std::lock_guard<std::mutex> lock(m_finalizationQueueMutex);
				resourcesToFinalize = std::move(m_finalizationQueue);
				m_finalizationQueue = std::queue<std::shared_ptr<Resource>>();
			}

			while (!resourcesToFinalize.empty())
			{
				auto resource = resourcesToFinalize.front();
				resourcesToFinalize.pop();

				if (!resource->IsFinalized())
				{
					resource->Finalize();
				}
			}
		}

		/** @brief Returns an already-loaded resource, or nullptr.
		 *  @tparam T Resource type.
		 *  @param path File path. */
		template<typename T = Resource>
		std::shared_ptr<T> GetResource(const std::string& path)
		{
			static_assert(std::is_base_of_v<Resource, T>, "T must derive from Resource");

			std::lock_guard<std::mutex> lock(m_loadedResourcesMutex);

			auto it = m_loadedResources.find(path);
			if (it != m_loadedResources.end() && it->second->IsLoaded())
			{
				return std::static_pointer_cast<T>(it->second);
			}

			return nullptr;
		}

		bool IsResourceLoaded(const std::string& path) const
		{
			std::lock_guard<std::mutex> lock(m_loadedResourcesMutex);
			auto it = m_loadedResources.find(path);
			return it != m_loadedResources.end() && it->second->IsLoaded();
		}

		bool IsResourceFinalized(const std::string& path) const
		{
			std::lock_guard<std::mutex> lock(m_loadedResourcesMutex);
			auto it = m_loadedResources.find(path);
			return it != m_loadedResources.end() && it->second->IsFinalized();
		}

		size_t GetLoadedResourceCount() const
		{
			std::lock_guard<std::mutex> lock(m_loadedResourcesMutex);
			return m_loadedResources.size();
		}

		size_t GetPendingLoadCount() const
		{
			std::lock_guard<std::mutex> lock(m_loadingQueueMutex);
			return m_loadingQueue.size();
		}

		size_t GetPendingFinalizationCount() const
		{
			std::lock_guard<std::mutex> lock(m_finalizationQueueMutex);
			return m_finalizationQueue.size();
		}

		bool RemoveResource(const std::string& path)
		{
			std::lock_guard<std::mutex> lock(m_loadedResourcesMutex);
			auto it = m_loadedResources.find(path);
			if (it != m_loadedResources.end())
			{
				m_loadedResources.erase(it);
				return true;
			}
			return false;
		}

		void ClearResources()
		{
			std::lock_guard<std::mutex> lock(m_loadedResourcesMutex);
			m_loadedResources.clear();
		}

		bool IsRunning() const { return m_isRunning; }
	};

} // namespace ResourceSystem


class CResourceReference : public CReflectedBase
{
public:
	REFL_DECLARE_OBJECT(CResourceReference, CReflectedBase);

	const std::string GetResourceFileName() const { return m_resourceFileName; }
	std::shared_ptr<ResourceSystem::Resource> GetResource() const { return m_resource; }

	// Templated helper: returns the stored resource cast to the requested derived type.
	template<typename T>
	std::shared_ptr<T> GetResourceAs() const
	{
		static_assert(std::is_base_of_v<ResourceSystem::Resource, T>, "T must derive from Resource");
		return std::dynamic_pointer_cast<T>(m_resource);
	}

	void OnLoaded()
	{
	}

protected:
	std::shared_ptr<ResourceSystem::Resource> m_resource;
private:
	std::string m_resourceFileName = "undifined";
	
};

template<typename TResource>
class CTypedResourceReference : public CResourceReference
{
public:
	void OnLoaded()
	{
		const std::string fileName = GetResourceFileName();
		if (!fileName.empty() && fileName != "undifined")
		{
			ResourceSystem::ResourceManager* resourceManager = Core::CoreSystem::GetResourceManager();
			if (resourceManager)
			{
				m_resource = resourceManager->RequestResource<TResource>(fileName);
			}
		}
	}
};