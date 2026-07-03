#pragma once
#include <cstdint>
#include <atomic>

namespace CauseAndEventSystem
{

struct CauseGUID
{
    uint64_t m_low = 0;
    uint64_t m_high = 0;

    bool operator==(const CauseGUID& other) const
    {
        return m_low == other.m_low && m_high == other.m_high;
    }

    bool operator!=(const CauseGUID& other) const { return !(*this == other); }

    bool IsValid() const { return m_low != 0 || m_high != 0; }

    static CauseGUID Generate()
    {
        static std::atomic<uint64_t> s_counter{1};
        static const uint64_t s_high = 0xCA11;
        return { s_counter++, s_high };
    }

    static CauseGUID Invalid() { return { 0, 0 }; }
};

}
