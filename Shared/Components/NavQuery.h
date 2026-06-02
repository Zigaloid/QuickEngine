#pragma once

#include "Math/Vector3f.h"

#include <vector>
#include <atomic>
#include <mutex>

/**
 * @brief Entity-owned navigation query handle.
 *
 * An entity creates and holds a shared_ptr<CNavQuery>, sets its start and
 * destination, then registers it with a CNavigationManagerComponent.  The
 * manager services the query asynchronously via the JobSystem and writes the
 * resulting path back into this object.
 *
 * ### Typical usage
 * @code
 *  // On entity init:
 *  m_navQuery = std::make_shared<CNavQuery>();
 *  navManager->RegisterQuery(m_navQuery);
 *
 *  // Each frame:
 *  m_navQuery->SetStart(GetPosition());
 *  if (destinationChanged)
 *      m_navQuery->SetDestination(newDest);   // automatically re-queues
 *
 *  if (m_navQuery->GetState() == CNavQuery::State::Ready)
 *  {
 *      auto path = m_navQuery->GetPath();     // snapshot under lock
 *      // follow path...
 *  }
 * @endcode
 */
class CNavQuery
{
public:
    enum class State
    {
        Pending,     ///< Waiting to be picked up by the manager.
        InProgress,  ///< Submitted to the JobSystem; result not yet available.
        Ready,       ///< Path is valid and can be read via GetPath().
        Failed       ///< Query completed but no path could be found.
    };

    CNavQuery()  = default;
    ~CNavQuery() = default;

    // Non-copyable, movable.
    CNavQuery(const CNavQuery&)            = delete;
    CNavQuery& operator=(const CNavQuery&) = delete;
    CNavQuery(CNavQuery&&)                 = default;
    CNavQuery& operator=(CNavQuery&&)      = default;

    // ?? Entity-side setters ?????????????????????????????????????????????

    /**
     * @brief Update the entity's current world-space position.
     *  Does not invalidate the query; only affects the next submission.
     */
    void SetStart(const Vector3f& start);

    /**
     * @brief Change the desired destination.
     *  Marks the query as dirty so the manager re-runs it.
     */
    void SetDestination(const Vector3f& destination);

    // ?? Manager-side interface (called from CNavigationManagerComponent) ?

    /** @brief Called by the manager before submitting the job. */
    void MarkInProgress();

    /** @brief Called by the worker job on success. */
    void SetResult(std::vector<Vector3f> path);

    /** @brief Called by the worker job on failure. */
    void SetFailed();

    // ?? Entity-side readers ?????????????????????????????????????????????

    /** @brief Snapshot of the current state. Safe to call from any thread. */
    State GetState() const { return m_state.load(std::memory_order_acquire); }

    Vector3f GetStart()       const { return m_start; }
    Vector3f GetDestination() const { return m_destination; }

    /**
     * @brief Returns a copy of the resolved path.
     *  Thread-safe — locks internally so the entity always sees a consistent
     *  snapshot even if the worker is writing a new result concurrently.
     */
    std::vector<Vector3f> GetPath() const;

    /**
     * @brief Returns true if the destination has changed since the last job
     *  was submitted.  Read by the manager during OnUpdate.
     */
    bool IsDirty() const { return m_dirty.load(std::memory_order_acquire); }

private:
    Vector3f m_start{};
    Vector3f m_destination{};

    mutable std::mutex    m_pathMutex;
    std::vector<Vector3f> m_path;

    std::atomic<State> m_state{ State::Pending };
    std::atomic<bool>  m_dirty{ true };   ///< true on construction so first tick submits.
};
