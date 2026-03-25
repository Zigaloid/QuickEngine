#pragma once

#include <vector>
#include <string>
#include <algorithm>
#include <functional>
#include <chrono>
#include <memory>
#include <sstream>
#include <iomanip>
#include <cmath>
#include <limits>
#include <tuple>

/**
 * @brief A heuristic class that tracks metrics into definable buckets
 * 
 * This class allows you to define buckets with ranges and track how many values
 * fall into each bucket. It's useful for creating histograms, performance analysis,
 * and general statistical analysis of data streams.
 */
class MetricBucketHeuristic
{
public:
    /**
     * @brief Defines a bucket with a range and label
     */
    struct Bucket
    {
        float minValue;     // Inclusive minimum value
        float maxValue;     // Exclusive maximum value (except for the last bucket)
        std::string label;  // Human-readable label for the bucket
        size_t count;       // Number of values that have fallen into this bucket
        float sum;          // Sum of all values in this bucket (for average calculation)
        
        Bucket(float min, float max, const std::string& lbl = "")
            : minValue(min), maxValue(max), label(lbl), count(0), sum(0.0f) {}
        
        bool Contains(float value, bool isLastBucket = false) const
        {
            if (isLastBucket)
                return value >= minValue && value <= maxValue; // Inclusive on both ends for last bucket
            else
                return value >= minValue && value < maxValue;  // Exclusive on max
        }
        
        float GetAverage() const
        {
            return count > 0 ? sum / static_cast<float>(count) : 0.0f;
        }
        
        float GetPercentage(size_t totalSamples) const
        {
            return totalSamples > 0 ? (static_cast<float>(count) / static_cast<float>(totalSamples)) * 100.0f : 0.0f;
        }
    };

    /**
     * @brief Statistics for the entire metric collection
     */
    struct Statistics
    {
        size_t totalSamples = 0;
        float minValue = 0.0f;
        float maxValue = 0.0f;
        float average = 0.0f;
        float sum = 0.0f;
        std::chrono::steady_clock::time_point firstSampleTime;
        std::chrono::steady_clock::time_point lastSampleTime;
        
        double GetSamplingDurationSeconds() const
        {
            if (totalSamples < 2) return 0.0;
            auto duration = lastSampleTime - firstSampleTime;
            return std::chrono::duration<double>(duration).count();
        }
        
        double GetSamplesPerSecond() const
        {
            double duration = GetSamplingDurationSeconds();
            return duration > 0.0 ? static_cast<double>(totalSamples) / duration : 0.0;
        }
    };

protected:
    std::vector<Bucket> m_Buckets;
    Statistics m_Stats;
    size_t m_MaxSamples;
    bool m_AutoUpdateStats;

public:
    /**
     * @brief Constructor
     * @param maxSamples Maximum number of samples to track (0 = unlimited)
     * @param autoUpdateStats Whether to automatically update statistics on each sample
     */
    explicit MetricBucketHeuristic(size_t maxSamples = 0, bool autoUpdateStats = true)
        : m_MaxSamples(maxSamples), m_AutoUpdateStats(autoUpdateStats)
    {
        m_Stats.firstSampleTime = std::chrono::steady_clock::now();
    }

    virtual ~MetricBucketHeuristic() = default;

    /**
     * @brief Define buckets using ranges
     * @param ranges Vector of {min, max, label} for each bucket
     */
    void DefineBuckets(const std::vector<std::tuple<float, float, std::string>>& ranges)
    {
        m_Buckets.clear();
        for (const auto& [min, max, label] : ranges)
        {
            m_Buckets.emplace_back(min, max, label);
        }
        Reset();
    }

    /**
     * @brief Define buckets using uniform distribution
     * @param minValue Minimum value for the range
     * @param maxValue Maximum value for the range
     * @param bucketCount Number of buckets to create
     * @param labelPrefix Prefix for bucket labels
     */
    void DefineUniformBuckets(float minValue, float maxValue, size_t bucketCount, const std::string& labelPrefix = "Bucket")
    {
        m_Buckets.clear();
        
        if (bucketCount == 0) return;
        
        float range = maxValue - minValue;
        float bucketSize = range / static_cast<float>(bucketCount);
        
        for (size_t i = 0; i < bucketCount; ++i)
        {
            float bucketMin = minValue + (static_cast<float>(i) * bucketSize);
            float bucketMax = (i == bucketCount - 1) ? maxValue : (minValue + (static_cast<float>(i + 1) * bucketSize));
            std::string label = labelPrefix + " " + std::to_string(i + 1);
            
            m_Buckets.emplace_back(bucketMin, bucketMax, label);
        }
        Reset();
    }

    /**
     * @brief Define buckets using step-based ranges with value labels
     * @param minValue Minimum value for the range
     * @param maxValue Maximum value for the range  
     * @param stepValue Step size between bucket boundaries
     */
    void DefineStepBuckets(float minValue, float maxValue, float stepValue)
    {
        m_Buckets.clear();

        if (stepValue <= 0.0f || maxValue <= minValue) return;

        float currentMin = minValue;
        
        while (currentMin < maxValue)
        {
            float currentMax = currentMin + stepValue;
            bool isLastBucket = (currentMax >= maxValue);
            
            if (isLastBucket)
            {
                currentMax = maxValue;
            }
            
            // Create label showing the range
            std::string label =  FormatBucketValue(currentMin);

            m_Buckets.emplace_back(currentMin, currentMax, label);
            
            if (isLastBucket) break;
            currentMin = currentMax;
        }
        
        Reset();
    }

    /**
     * @brief Define performance buckets (useful for frame time analysis)
     * @param targetFrameTime Target frame time in milliseconds (e.g., 16.67 for 60 FPS)
     */
    void DefinePerformanceBuckets(float targetFrameTime = 16.67f)
    {
        std::vector<std::tuple<float, float, std::string>> ranges = {
            {0.0f, targetFrameTime * 0.5f, "Excellent"},
            {targetFrameTime * 0.5f, targetFrameTime, "Good"},
            {targetFrameTime, targetFrameTime * 1.5f, "Acceptable"},
            {targetFrameTime * 1.5f, targetFrameTime * 2.0f, "Poor"},
            {targetFrameTime * 2.0f, targetFrameTime * 4.0f, "Bad"},
            {targetFrameTime * 4.0f, std::numeric_limits<float>::max(), "Terrible"}
        };
        DefineBuckets(ranges);
    }

    /**
     * @brief Add a sample to the appropriate bucket
     * @param value The value to categorize
     * @return Index of the bucket the value was added to, or -1 if no bucket matched
     */
    virtual int AddSample(float value)
    {
        // Update statistics
        if (m_Stats.totalSamples == 0)
        {
            m_Stats.minValue = m_Stats.maxValue = value;
            m_Stats.sum = value;
            m_Stats.firstSampleTime = std::chrono::steady_clock::now();
        }
        else
        {
            m_Stats.minValue = std::min(m_Stats.minValue, value);
            m_Stats.maxValue = std::max(m_Stats.maxValue, value);
            m_Stats.sum += value;
        }
        
        m_Stats.totalSamples++;
        m_Stats.lastSampleTime = std::chrono::steady_clock::now();
        
        if (m_AutoUpdateStats)
        {
            m_Stats.average = m_Stats.sum / static_cast<float>(m_Stats.totalSamples);
        }

        // Check if we need to limit samples
        if (m_MaxSamples > 0 && m_Stats.totalSamples > m_MaxSamples)
        {
            // Simple approach: reset when limit is reached
            // Override this method for more sophisticated strategies
            OnMaxSamplesReached();
        }

        // Find appropriate bucket
        for (size_t i = 0; i < m_Buckets.size(); ++i)
        {
            bool isLastBucket = (i == m_Buckets.size() - 1);
            if (m_Buckets[i].Contains(value, isLastBucket))
            {
                m_Buckets[i].count++;
                m_Buckets[i].sum += value;
                return static_cast<int>(i);
            }
        }
        
        return -1; // No bucket matched
    }

    /**
     * @brief Add multiple samples at once
     */
    void AddSamples(const std::vector<float>& values)
    {
        for (float value : values)
        {
            AddSample(value);
        }
    }

    /**
     * @brief Reset all bucket counts and statistics
     */
    virtual void Reset()
    {
        for (auto& bucket : m_Buckets)
        {
            bucket.count = 0;
            bucket.sum = 0.0f;
        }
        
        m_Stats = Statistics();
        m_Stats.firstSampleTime = std::chrono::steady_clock::now();
    }

    /**
     * @brief Get all buckets
     */
    const std::vector<Bucket>& GetBuckets() const { return m_Buckets; }

    /**
     * @brief Get a specific bucket by index
     */
    const Bucket& GetBucket(size_t index) const 
    { 
        static Bucket emptyBucket(0, 0, "Invalid");
        return index < m_Buckets.size() ? m_Buckets[index] : emptyBucket;
    }

    /**
     * @brief Get overall statistics
     */
    const Statistics& GetStatistics() const { return m_Stats; }

    /**
     * @brief Update statistics manually (if auto-update is disabled)
     */
    void UpdateStatistics()
    {
        if (m_Stats.totalSamples > 0)
        {
            m_Stats.average = m_Stats.sum / static_cast<float>(m_Stats.totalSamples);
        }
    }

    /**
     * @brief Get the bucket with the highest count
     */
    const Bucket* GetMostFrequentBucket() const
    {
        if (m_Buckets.empty()) return nullptr;
        
        auto maxIt = std::max_element(m_Buckets.begin(), m_Buckets.end(),
            [](const Bucket& a, const Bucket& b) { return a.count < b.count; });
        
        return &(*maxIt);
    }

    /**
     * @brief Get the bucket with the lowest count (excluding empty buckets)
     */
    const Bucket* GetLeastFrequentBucket() const
    {
        if (m_Buckets.empty()) return nullptr;
        
        auto minIt = std::min_element(m_Buckets.begin(), m_Buckets.end(),
            [](const Bucket& a, const Bucket& b) { 
                if (a.count == 0) return false;
                if (b.count == 0) return true;
                return a.count < b.count; 
            });
        
        return minIt != m_Buckets.end() && minIt->count > 0 ? &(*minIt) : nullptr;
    }

    /**
     * @brief Get bucket count
     */
    size_t GetBucketCount() const { return m_Buckets.size(); }

    /**
     * @brief Check if any samples have been added
     */
    bool HasSamples() const { return m_Stats.totalSamples > 0; }

    /**
     * @brief Set maximum sample limit
     */
    void SetMaxSamples(size_t maxSamples) { m_MaxSamples = maxSamples; }

    /**
     * @brief Get maximum sample limit
     */
    size_t GetMaxSamples() const { return m_MaxSamples; }

private:
    /**
     * @brief Format a bucket value for display in labels
     * @param value The value to format
     * @return Formatted string representation of the value
     */
    std::string FormatBucketValue(float value) const
    {
        // Check if the value is effectively an integer
        if (std::abs(value - std::round(value)) < 1e-6f)
        {
            return std::to_string(static_cast<int>(std::round(value)));
        }
        else
        {
            // Format with appropriate precision based on magnitude
            std::ostringstream oss;
            if (std::abs(value) >= 1000.0f)
            {
                oss << std::fixed << std::setprecision(0) << value;
            }
            else if (std::abs(value) >= 100.0f)
            {
                oss << std::fixed << std::setprecision(1) << value;
            }
            else if (std::abs(value) >= 10.0f)
            {
                oss << std::fixed << std::setprecision(2) << value;
            }
            else
            {
                oss << std::fixed << std::setprecision(3) << value;
            }
            
            std::string result = oss.str();
            
            // Remove trailing zeros after decimal point
            if (result.find('.') != std::string::npos)
            {
                result = result.substr(0, result.find_last_not_of('0') + 1);
                if (result.back() == '.')
                {
                    result.pop_back();
                }
            }
            
            return result;
        }
    }

protected:
    /**
     * @brief Called when max samples limit is reached
     * Override this to implement custom behavior (e.g., sliding window)
     */
    virtual void OnMaxSamplesReached()
    {
        // Default behavior: reset everything
        Reset();
    }
};