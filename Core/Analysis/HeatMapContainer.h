#pragma once
#include <cstdint>
#include <string>
#include <vector>
#include <unordered_map>
#include <array>
#include <optional>

/** @brief Grid-based spatial container for accumulating named metric series per cell. */
class HeatMapContainer {
public:
    static constexpr std::size_t MaxHistory = 1024;

    HeatMapContainer(float widthMeters,
                     float heightMeters,
                     float cellSizeMeters,
                     float originX = 0.0f,
                     float originY = 0.0f);

    // Geometry
    float GetWidthMeters() const noexcept { return m_widthMeters; }
    float GetHeightMeters() const noexcept { return m_heightMeters; }
    float GetCellSizeMeters() const noexcept { return m_cellSizeMeters; }
    float GetOriginX() const noexcept { return m_originX; }
    float GetOriginY() const noexcept { return m_originY; }
    std::size_t GetColumns() const noexcept { return m_cols; }
    std::size_t GetRows() const noexcept { return m_rows; }

    bool IsInside(float xMeters, float yMeters) const noexcept;
    bool GetCellIndicesForPoint(float xMeters, float yMeters, std::size_t& outCol, std::size_t& outRow) const noexcept;

    // Data operations
    bool AddValue(float xMeters, float yMeters, const std::string& name, double value) noexcept;
    bool AddValueByIndex(std::size_t col, std::size_t row, const std::string& name, double value) noexcept;
    std::optional<double> GetAverageAt(float xMeters, float yMeters, const std::string& name) const noexcept;
    std::optional<double> GetAverageAtIndex(std::size_t col, std::size_t row, const std::string& name) const noexcept;

    bool GetLastValuesAt(float xMeters, float yMeters, const std::string& name,
                         std::vector<double>& outValues, std::size_t maxCount = MaxHistory) const;
    bool GetLastValuesAtIndex(std::size_t col, std::size_t row, const std::string& name,
                              std::vector<double>& outValues, std::size_t maxCount = MaxHistory) const;

    // Maintenance
    void Clear();
    bool ClearCell(std::size_t col, std::size_t row);
    bool ClearSeries(std::size_t col, std::size_t row, const std::string& name);

    struct DataSeries {
        std::array<double, MaxHistory> history{};
        std::size_t writeIndex = 0;   // next write position
        std::size_t historySize = 0;  // number of valid entries in history (<= MaxHistory)
        std::uint64_t sampleCount = 0; // total samples received
        double sum = 0.0;             // accumulated sum for average

        void Add(double v) noexcept;
        double Average() const noexcept { return sampleCount ? (sum / static_cast<double>(sampleCount)) : 0.0; }
        void CopyLast(std::vector<double>& out, std::size_t maxCount) const;
    };

    struct Cell {
        std::unordered_map<std::string, DataSeries> seriesByName;
        void Clear() { seriesByName.clear(); }
    };

    std::size_t FlatIndex(std::size_t col, std::size_t row) const noexcept { return (row * m_cols) + col; }
    bool InBounds(std::size_t col, std::size_t row) const noexcept { return col < m_cols && row < m_rows; }

    const Cell* TryGetCell(std::size_t col, std::size_t row) const noexcept;
    Cell* TryGetCell(std::size_t col, std::size_t row) noexcept;

    // Binary serialization API
    bool Save(const std::string& filepath) const;
    bool Load(const std::string& filepath);

private:
    float m_originX = 0.0f;
    float m_originY = 0.0f;
    float m_widthMeters = 0.0f;
    float m_heightMeters = 0.0f;
    float m_cellSizeMeters = 1.0f;
    std::size_t m_cols = 0;
    std::size_t m_rows = 0;
    std::vector<Cell> m_cells;
};