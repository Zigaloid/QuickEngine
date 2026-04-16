#pragma once

#include <cstdint>
#include <mutex>
#include <vector>

#include <bgfx/bgfx.h>

namespace ImGuiVisualizers {

/**
 * @brief Thread-safe allocator for BGFX view IDs.
 *
 * BGFX supports up to 256 views (0-255). View 0 is reserved for the main
 * backbuffer pass, and view 255 is typically used by the ImGui overlay.
 * This allocator hands out IDs from a configurable range for offscreen
 * framebuffer passes (e.g. 3D viewport panels).
 */
class BgfxViewIdAllocator
{
public:
    static BgfxViewIdAllocator& Get()
    {
        static BgfxViewIdAllocator instance;
        return instance;
    }

    /**
     * @brief Allocate a free view ID.
     * @return A valid bgfx::ViewId, or kInvalidViewId on failure.
     */
    bgfx::ViewId Allocate()
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        for (uint16_t id = m_rangeBegin; id < m_rangeEnd; ++id) {
            if (!m_used[id]) {
                m_used[id] = true;
                return static_cast<bgfx::ViewId>(id);
            }
        }
        return kInvalidViewId;
    }

    /**
     * @brief Return a previously allocated view ID.
     */
    void Free(bgfx::ViewId id)
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (id >= m_rangeBegin && id < m_rangeEnd) {
            m_used[id] = false;
        }
    }

    static constexpr bgfx::ViewId kInvalidViewId = 0xFFFF;

private:
    BgfxViewIdAllocator()
    {
        m_used.resize(256, false);
        // Reserve view 0 (main) and view 255 (imgui overlay)
        m_used[0]   = true;
        m_used[255] = true;
    }

    ~BgfxViewIdAllocator() = default;
    BgfxViewIdAllocator(const BgfxViewIdAllocator&) = delete;
    BgfxViewIdAllocator& operator=(const BgfxViewIdAllocator&) = delete;

    std::mutex          m_mutex;
    std::vector<bool>   m_used;
    uint16_t            m_rangeBegin = 1;
    uint16_t            m_rangeEnd   = 200;   // leave headroom for other systems
};

} // namespace ImGuiVisualizers
