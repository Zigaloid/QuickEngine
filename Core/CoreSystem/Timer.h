#pragma once

#include <chrono>
#include <functional>

namespace Core {

    /**
     * @brief A simple timer utility for tracking elapsed time and timeouts.
     *
     * Supports pause/unpause, reset, expiration checking, and optional
     * auto-restart (looping) behavior. Uses std::chrono::steady_clock
     * for monotonic, high-resolution timing.
     *
     * Usage:
     *   Core::Timer timer(2.0);          // 2-second timeout
     *   if (timer.HasExpired()) { ... }   // Check if time is up
     *   timer.Reset();                    // Restart the timer
     *   timer.Pause(); / timer.Unpause(); // Pause/resume
     */
    class Timer {
    public:
        using Clock = std::chrono::steady_clock;
        using Duration = std::chrono::duration<double>;
        using TimePoint = Clock::time_point;

        /// @brief Construct a timer with a timeout specified in seconds.
        /// @param timeoutSeconds The duration in seconds before the timer expires. 
        ///        A value <= 0 means the timer is immediately expired.
        explicit Timer(double timeoutSeconds)
            : m_timeout(timeoutSeconds)
            , m_startTime(Clock::now())
            , m_pausedDuration(Duration::zero())
            , m_paused(false)
            , m_looping(false)
            , m_pauseStartTime{}
        {
        }

        /// @brief Construct a timer with a std::chrono duration.
        /// @param timeout The duration before the timer expires.
        template<typename Rep, typename Period>
        explicit Timer(std::chrono::duration<Rep, Period> timeout)
            : Timer(std::chrono::duration_cast<Duration>(timeout).count())
        {
        }

        /// @brief Default constructor - creates an expired timer with zero timeout.
        Timer()
            : Timer(0.0)
        {
        }

        ~Timer() = default;

        // Copy/move semantics
        Timer(const Timer&) = default;
        Timer& operator=(const Timer&) = default;
        Timer(Timer&&) noexcept = default;
        Timer& operator=(Timer&&) noexcept = default;

        // ---- Core API ----

        /// @brief Check whether the timer has expired (elapsed time >= timeout).
        /// @return true if the timer has expired, false otherwise.
        bool HasExpired() const
        {
            return GetElapsed() >= m_timeout.count();
        }

        /// @brief Reset the timer, restarting from now with the same timeout.
        ///        Also unpauses if paused.
        void Reset()
        {
            m_startTime = Clock::now();
            m_pausedDuration = Duration::zero();
            m_paused = false;
        }

        /// @brief Reset the timer with a new timeout value in seconds.
        /// @param timeoutSeconds The new timeout duration in seconds.
        void Reset(double timeoutSeconds)
        {
            m_timeout = Duration(timeoutSeconds);
            Reset();
        }

        /// @brief Pause the timer. Elapsed time stops accumulating.
        ///        Has no effect if already paused.
        void Pause()
        {
            if (!m_paused) {
                m_paused = true;
                m_pauseStartTime = Clock::now();
            }
        }

        /// @brief Unpause (resume) the timer. Elapsed time resumes accumulating.
        ///        Has no effect if not paused.
        void Unpause()
        {
            if (m_paused) {
                m_pausedDuration += Clock::now() - m_pauseStartTime;
                m_paused = false;
            }
        }

        // ---- Query Methods ----

        /// @brief Get the elapsed time in seconds since the timer was started/reset,
        ///        excluding any time spent paused.
        /// @return Elapsed time in seconds.
        double GetElapsed() const
        {
            Duration elapsed;
            if (m_paused) {
                // While paused, only count time up to when we paused
                elapsed = (m_pauseStartTime - m_startTime) - m_pausedDuration;
            }
            else {
                elapsed = (Clock::now() - m_startTime) - m_pausedDuration;
            }
            return elapsed.count();
        }

        /// @brief Get the remaining time in seconds before the timer expires.
        /// @return Remaining time in seconds. Returns 0 if already expired.
        double GetRemaining() const
        {
            double remaining = m_timeout.count() - GetElapsed();
            return remaining > 0.0 ? remaining : 0.0;
        }

        /// @brief Get the timeout duration in seconds.
        /// @return The timeout value in seconds.
        double GetTimeout() const
        {
            return m_timeout.count();
        }

        /// @brief Set a new timeout duration without resetting the timer.
        /// @param timeoutSeconds The new timeout duration in seconds.
        void SetTimeout(double timeoutSeconds)
        {
            m_timeout = Duration(timeoutSeconds);
        }

        /// @brief Get the progress as a normalized value [0.0, 1.0].
        ///        0.0 = just started, 1.0 = expired (or beyond).
        /// @return Normalized progress value, clamped to [0.0, 1.0].
        double GetProgress() const
        {
            if (m_timeout.count() <= 0.0)
                return 1.0;
            double progress = GetElapsed() / m_timeout.count();
            return progress < 0.0 ? 0.0 : (progress > 1.0 ? 1.0 : progress);
        }

        /// @brief Check if the timer is currently paused.
        /// @return true if paused, false otherwise.
        bool IsPaused() const
        {
            return m_paused;
        }

        /// @brief Check if the timer is currently running (not paused and not expired).
        /// @return true if the timer is actively running.
        bool IsRunning() const
        {
            return !m_paused && !HasExpired();
        }

        // ---- Looping Support ----

        /// @brief Enable or disable looping. When looping is enabled, 
        ///        CheckAndReset() will auto-reset the timer on expiration.
        /// @param looping true to enable looping, false to disable.
        void SetLooping(bool looping)
        {
            m_looping = looping;
        }

        /// @brief Check if looping is enabled.
        /// @return true if looping is enabled.
        bool IsLooping() const
        {
            return m_looping;
        }

        /// @brief Check if the timer has expired and automatically reset it if so.
        ///        Useful for repeating timers in game loops.
        /// @return true if the timer had expired (and was reset), false otherwise.
        bool CheckAndReset()
        {
            if (HasExpired()) {
                Reset();
                return true;
            }
            return false;
        }

        // ---- Convenience ----

        /// @brief Implicit conversion to bool - returns true if the timer has NOT expired.
        ///        Allows usage like: if (timer) { /* still running */ }
        explicit operator bool() const
        {
            return !HasExpired();
        }

    private:
        Duration    m_timeout;          ///< The timeout duration
        TimePoint   m_startTime;        ///< When the timer was started/reset
        Duration    m_pausedDuration;   ///< Total accumulated paused time
        TimePoint   m_pauseStartTime;   ///< When the current pause began
        bool        m_paused;           ///< Whether the timer is currently paused
        bool        m_looping;          ///< Whether the timer auto-resets on expiration
    };

} // namespace Core
