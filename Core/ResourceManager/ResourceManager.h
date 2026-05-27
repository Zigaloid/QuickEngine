#pragma once

#include "Filesystem/FileSystemManager.h"
#include "Profiler/Profiler.h"
#include "Reflection/ReflectionBase.h"
#include "CoreSystem/CoreDebugChannels.h"
#include "CoreSystem/AppConfig.h"
#include "CoreSystem/CoreSystem.h"

#include <string>
#include <vector>
#include <memory>
#include <unordered_map>
#include <queue>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <atomic>

#define RESOURCE_MANAGER_DEBUG 1

namespace ResourceSystem {

	// Forward declarations
	class ResourceManager;

	// ── Helpers ───────────────────────────────────────────────────────────────────

	/** @brief Extracts the compound extension from a file path.
	 *  e.g. "assets/mesh.staticmesh.json" -> ".staticmesh.json" */
	inline std::string ExtractCompoundExtension(const std::string& path)
	{
		const size_t sepPos  = path.find_last_of("/\\");
		const size_t startPos = (sepPos == std::string::npos) ? 0 : sepPos + 1;
		const size_t dotPos  = path.find('.', startPos);
		if (dotPos == std::string::npos)
			return {};
		return path.substr(dotPos);
	}

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

		/** @brief Returns the file extensions this resource type accepts.
		 *  Derived types should shadow this with their own static implementation.
		 *  An empty list means no extension restriction is enforced.
		 *  Extensions must include all parts, e.g. ".staticmesh.json". */
		static std::vector<std::string_view> GetSupportedExtensions() { return {}; }

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

		/** @brief Releases the resource's current state and re-initializes it,
		 *  allowing the async pipeline to reload it via Update() and Finalize().
		 *  Derived classes may override to release GPU/API handles before reloading.
		 *  @return true if re-initialization succeeded. */
		virtual bool Reload()
		{
			m_isFinalized   = false;
			m_isLoaded      = false;
			m_isInitialized = false;
			m_data.clear();
			m_data.shrink_to_fit();

			return Initialize();
		}
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
						ResourceManagerDebug.printf("Enqueueing resource for finalization: %s\n", resource->GetPath().c_str());
						std::lock_guard<std::mutex> lock(m_finalizationQueueMutex);
						m_finalizationQueue.push(resource);
					}
				}
			}
		}

		/** @brief Checks that @p path has an extension accepted by T.
		 *  Logs an error and returns false if there is a mismatch. */
		template<typename T>
		static bool ValidateExtension(const std::string& path)
		{
			const auto extensions = T::GetSupportedExtensions();
			if (extensions.empty())
				return true; // no restriction declared

			const std::string ext = ExtractCompoundExtension(path);
			for (const auto& supported : extensions)
			{
				if (ext == supported)
					return true;
			}

			// Build a comma-separated list for the error message.
			std::string list;
			for (size_t i = 0; i < extensions.size(); ++i)
			{
				if (i > 0) list += ", ";
				list += extensions[i];
			}

			ResourceManagerDebug.printf(
				"ERROR: Resource type mismatch - '%s' has extension '%s' but expected one of: [%s]\n",
				path.c_str(), ext.c_str(), list.c_str());

			return false;
		}

	public:
		explicit ResourceManager(FileSystem::FileSystemManager* fileSystemManager)
			: m_fileSystemManager(fileSystemManager)
		{
			ResourceManagerDebug.SetEnabled(RESOURCE_MANAGER_DEBUG);
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

			const std::string resolvedPath = Core::AppConfig::Instance().ResolvePath(path);
			ResourceManagerDebug.printf("Requesting resource: %s\n", resolvedPath.c_str());

			if (!ValidateExtension<T>(resolvedPath))
			{
				// Do not proceed with loading if extension doesn't match the requested resource type.
				return nullptr;
			}

			std::lock_guard<std::mutex> lock(m_loadedResourcesMutex);

			auto it = m_loadedResources.find(resolvedPath);
			if (it != m_loadedResources.end())
			{
				ResourceManagerDebug.printf("Return already requested: %s\n", resolvedPath.c_str());
				return std::static_pointer_cast<T>(it->second);
			}

			auto resource = std::make_shared<T>(resolvedPath);
			m_loadedResources[resolvedPath] = resource;
			{
				ResourceManagerDebug.printf("Enqueueing resource for loading: %s\n", resolvedPath.c_str());
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
					ResourceManagerDebug.printf("Finalizing resource: %s\n", resource->GetPath().c_str());
					resource->Finalize();

					if (!resource->IsFinalized())
					{
						ResourceManagerDebug.printf("Deferring finalization (will retry later): %s\n", resource->GetPath().c_str());
						std::lock_guard<std::mutex> lock(m_finalizationQueueMutex);
						m_finalizationQueue.push(resource);
					}
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

		/** @brief Finds a resource by path, calls Reload() on it, and re-enqueues it
		 *  through the async loading pipeline (Update -> Finalize).
		 *  @param path File path of the resource to reload.
		 *  @return true if the resource was found and successfully re-initialized. */
		bool ReloadResource(const std::string& path)
		{
			const std::string resolvedPath = Core::AppConfig::Instance().ResolvePath(path);
			ResourceManagerDebug.printf("Reloading resource: %s\n", resolvedPath.c_str());

			std::shared_ptr<Resource> resource;
			{
				std::lock_guard<std::mutex> lock(m_loadedResourcesMutex);
				auto it = m_loadedResources.find(resolvedPath);
				if (it == m_loadedResources.end())
				{
					ResourceManagerDebug.printf("ReloadResource failed - resource not found: %s\n", resolvedPath.c_str());
					return false;
				}
				resource = it->second;
			}

			if (!resource->Reload())
			{
				ResourceManagerDebug.printf("ReloadResource failed - Reload() returned false: %s\n", resolvedPath.c_str());
				return false;
			}

			{
				ResourceManagerDebug.printf("Re-enqueueing resource for loading: %s\n", resolvedPath.c_str());
				std::lock_guard<std::mutex> lock(m_loadingQueueMutex);
				m_loadingQueue.push(resource);
			}
			m_loadingQueueCondition.notify_one();

			return true;
		}

		bool RemoveResource(const std::string& path)
		{
			ResourceManagerDebug.printf("Removing resource: %s\n", path.c_str());
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


/** @brief When true, GetResource/GetResourceAs will warn if the loaded resource path
 *  no longer matches the reference's file name. Can be toggled at runtime. */
inline bool CResourceReference_MonitorPathChange = true;

class CResourceReference : public CReflectedBase
{
public:
	REFL_DECLARE_OBJECT(CResourceReference, CReflectedBase);

	const std::string GetResourceFileName() const { return m_resourceFileName; }

	std::shared_ptr<ResourceSystem::Resource> GetResource() const
	{
		if (CResourceReference_MonitorPathChange && m_resource)
		{
			const std::string resolvedPath = Core::AppConfig::Instance().ResolvePath(m_resourceFileName);
			if (m_resource->GetPath() != resolvedPath)
			{
				ResourceManagerDebug.warning("CResourceReference path mismatch: loaded '%s', current file name resolves to '%s'. Replacing resource.\n",
					m_resource->GetPath().c_str(), resolvedPath.c_str());
				ReloadResource();
			}
		}
		return m_resource;
	}

	template<typename T>
	std::shared_ptr<T> GetResourceAs() const
	{
		static_assert(std::is_base_of_v<ResourceSystem::Resource, T>, "T must derive from Resource");
		if (CResourceReference_MonitorPathChange && m_resource)
		{
			const std::string resolvedPath = Core::AppConfig::Instance().ResolvePath(m_resourceFileName);
			if (m_resource->GetPath() != resolvedPath)
			{
				ResourceManagerDebug.warning("CResourceReference path mismatch: loaded '%s', current file name resolves to '%s'. Replacing resource.\n",
					m_resource->GetPath().c_str(), resolvedPath.c_str());
				ReloadResource();
			}
		}
		return std::dynamic_pointer_cast<T>(m_resource);
	}

	void SetResourceFileName(const std::string& fileName)
	{
		m_resourceFileName = fileName;
	}
	
	virtual std::string GetReourceTypeName() const
	{
		return "Resource";
	}

	void OnLoaded() override
	{
	}

	/** @brief Called when a path mismatch is detected; derived types should replace m_resource with the newly requested resource. */
	virtual void ReloadResource() const {}

protected:
	mutable std::shared_ptr<ResourceSystem::Resource> m_resource;
private:
	std::string m_resourceFileName = "undifined";

};

template<typename TResource>
class CTypedResourceReference : public CResourceReference
{
public:
	void OnLoaded() override
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

	void ReloadResource() const override
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

	std::string GetReourceTypeName() const
	{
		return typeid(TResource).name();
	}
};