#include "HeatMapContainer.h"
#include <cmath>
#include <fstream>
#include <limits>

namespace {
    constexpr std::uint32_t kMagic = 0x50414D48u; // "HMAP" little-endian
    constexpr std::uint32_t kVersion = 1;

    template <typename T>
    bool write_raw(std::ofstream& os, const T& v) {
        os.write(reinterpret_cast<const char*>(&v), sizeof(T));
        return os.good();
    }
    template <typename T>
    bool read_raw(std::ifstream& is, T& v) {
        is.read(reinterpret_cast<char*>(&v), sizeof(T));
        return is.good();
    }
    bool write_string(std::ofstream& os, const std::string& s) {
        const std::uint32_t len = static_cast<std::uint32_t>(s.size());
        if (!write_raw(os, len)) return false;
        if (len == 0) return true;
        os.write(s.data(), len);
        return os.good();
    }
    bool read_string(std::ifstream& is, std::string& s) {
        std::uint32_t len = 0;
        if (!read_raw(is, len)) return false;
        s.resize(len);
        if (len == 0) return true;
        is.read(s.data(), len);
        return is.good();
    }
}

HeatMapContainer::HeatMapContainer(float widthMeters,
                                   float heightMeters,
                                   float cellSizeMeters,
                                   float originX,
                                   float originY)
    : m_originX(originX)
    , m_originY(originY)
    , m_widthMeters(std::max(0.0f, widthMeters))
    , m_heightMeters(std::max(0.0f, heightMeters))
    , m_cellSizeMeters(std::max(0.0001f, cellSizeMeters)) // avoid divide-by-zero
{
    const double colsF = std::ceil(static_cast<double>(m_widthMeters) / static_cast<double>(m_cellSizeMeters));
    const double rowsF = std::ceil(static_cast<double>(m_heightMeters) / static_cast<double>(m_cellSizeMeters));
    m_cols = static_cast<std::size_t>(colsF > 0.0 ? colsF : 0.0);
    m_rows = static_cast<std::size_t>(rowsF > 0.0 ? rowsF : 0.0);

    m_cells.resize(m_cols * m_rows);
}

bool HeatMapContainer::IsInside(float xMeters, float yMeters) const noexcept
{
    const float relX = xMeters - m_originX;
    const float relY = yMeters - m_originY;
    return (relX >= 0.0f && relY >= 0.0f && relX < m_widthMeters && relY < m_heightMeters);
}

bool HeatMapContainer::GetCellIndicesForPoint(float xMeters, float yMeters, std::size_t& outCol, std::size_t& outRow) const noexcept
{
    if (!IsInside(xMeters, yMeters) || m_cols == 0 || m_rows == 0) {
        return false;
    }
    const float relX = xMeters - m_originX;
    const float relY = yMeters - m_originY;

    std::size_t col = static_cast<std::size_t>(std::floor(relX / m_cellSizeMeters));
    std::size_t row = static_cast<std::size_t>(std::floor(relY / m_cellSizeMeters));

    // Clamp edge case where x or y is exactly on the far edge
    if (col >= m_cols) col = m_cols - 1;
    if (row >= m_rows) row = m_rows - 1;

    outCol = col;
    outRow = row;
    return true;
}

bool HeatMapContainer::AddValue(float xMeters, float yMeters, const std::string& name, double value) noexcept
{
    std::size_t col = 0, row = 0;
    if (!GetCellIndicesForPoint(xMeters, yMeters, col, row)) {
        return false;
    }
    return AddValueByIndex(col, row, name, value);
}

bool HeatMapContainer::AddValueByIndex(std::size_t col, std::size_t row, const std::string& name, double value) noexcept
{
    if (!InBounds(col, row)) {
        return false;
    }
    auto& cell = m_cells[FlatIndex(col, row)];
    auto& series = cell.seriesByName[name]; // creates if missing
    series.Add(value);
    return true;
}

std::optional<double> HeatMapContainer::GetAverageAt(float xMeters, float yMeters, const std::string& name) const noexcept
{
    std::size_t col = 0, row = 0;
    if (!GetCellIndicesForPoint(xMeters, yMeters, col, row)) {
        return std::nullopt;
    }
    return GetAverageAtIndex(col, row, name);
}

std::optional<double> HeatMapContainer::GetAverageAtIndex(std::size_t col, std::size_t row, const std::string& name) const noexcept
{
    if (!InBounds(col, row)) {
        return std::nullopt;
    }
    const auto& cell = m_cells[FlatIndex(col, row)];
    const auto it = cell.seriesByName.find(name);
    if (it == cell.seriesByName.end() || it->second.sampleCount == 0) {
        return std::nullopt;
    }
    return it->second.Average();
}

bool HeatMapContainer::GetLastValuesAt(float xMeters, float yMeters, const std::string& name,
                                       std::vector<double>& outValues, std::size_t maxCount) const
{
    std::size_t col = 0, row = 0;
    if (!GetCellIndicesForPoint(xMeters, yMeters, col, row)) {
        return false;
    }
    return GetLastValuesAtIndex(col, row, name, outValues, maxCount);
}

bool HeatMapContainer::GetLastValuesAtIndex(std::size_t col, std::size_t row, const std::string& name,
                                            std::vector<double>& outValues, std::size_t maxCount) const
{
    if (!InBounds(col, row)) {
        return false;
    }
    const auto& cell = m_cells[FlatIndex(col, row)];
    const auto it = cell.seriesByName.find(name);
    if (it == cell.seriesByName.end() || it->second.historySize == 0) {
        return false;
    }
    it->second.CopyLast(outValues, maxCount);
    return true;
}

void HeatMapContainer::Clear()
{
    for (auto& c : m_cells) {
        c.Clear();
    }
}

bool HeatMapContainer::ClearCell(std::size_t col, std::size_t row)
{
    if (!InBounds(col, row)) {
        return false;
    }
    m_cells[FlatIndex(col, row)].Clear();
    return true;
}

bool HeatMapContainer::ClearSeries(std::size_t col, std::size_t row, const std::string& name)
{
    if (!InBounds(col, row)) {
        return false;
    }
    auto& cell = m_cells[FlatIndex(col, row)];
    const auto it = cell.seriesByName.find(name);
    if (it == cell.seriesByName.end()) {
        return false;
    }
    cell.seriesByName.erase(it);
    return true;
}

const HeatMapContainer::Cell* HeatMapContainer::TryGetCell(std::size_t col, std::size_t row) const noexcept
{
    if (!InBounds(col, row)) {
        return nullptr;
    }
    return &m_cells[FlatIndex(col, row)];
}

HeatMapContainer::Cell* HeatMapContainer::TryGetCell(std::size_t col, std::size_t row) noexcept
{
    if (!InBounds(col, row)) {
        return nullptr;
    }
    return &m_cells[FlatIndex(col, row)];
}

// ---------------- DataSeries ----------------

void HeatMapContainer::DataSeries::Add(double v) noexcept
{
    history[writeIndex] = v;
    writeIndex = (writeIndex + 1) % MaxHistory;
    if (historySize < MaxHistory) {
        ++historySize;
    }
    sum += v;
    ++sampleCount;
}

void HeatMapContainer::DataSeries::CopyLast(std::vector<double>& out, std::size_t maxCount) const
{
    const std::size_t n = std::min(historySize, std::min(maxCount, MaxHistory));
    out.resize(n);
    // Oldest -> newest order
    const std::size_t start = (historySize == MaxHistory) ? writeIndex : 0;
    for (std::size_t i = 0; i < n; ++i) {
        const std::size_t idx = (start + (historySize - n) + i) % MaxHistory;
        out[i] = history[idx];
    }
}

// ---------------- Serialization ----------------

bool HeatMapContainer::Save(const std::string& filepath) const
{
    std::ofstream os(filepath, std::ios::binary | std::ios::out | std::ios::trunc);
    if (!os.is_open()) return false;

    // Header
    if (!write_raw(os, kMagic)) return false;
    if (!write_raw(os, kVersion)) return false;

    // Geometry
    if (!write_raw(os, m_originX)) return false;
    if (!write_raw(os, m_originY)) return false;
    if (!write_raw(os, m_widthMeters)) return false;
    if (!write_raw(os, m_heightMeters)) return false;
    if (!write_raw(os, m_cellSizeMeters)) return false;

    // Grid dims
    const std::uint64_t cols64 = static_cast<std::uint64_t>(m_cols);
    const std::uint64_t rows64 = static_cast<std::uint64_t>(m_rows);
    if (!write_raw(os, cols64)) return false;
    if (!write_raw(os, rows64)) return false;

    // Cells and series
    for (std::size_t r = 0; r < m_rows; ++r) {
        for (std::size_t c = 0; c < m_cols; ++c) {
            const auto& cell = m_cells[FlatIndex(c, r)];
            const std::uint32_t seriesCount = static_cast<std::uint32_t>(cell.seriesByName.size());
            if (!write_raw(os, seriesCount)) return false;

            for (const auto& kv : cell.seriesByName) {
                const std::string& name = kv.first;
                const DataSeries& s = kv.second;

                // Series name
                if (!write_string(os, name)) return false;

                // Series meta
                const std::uint32_t historySize32 = static_cast<std::uint32_t>(s.historySize);
                const std::uint32_t writeIndex32 = static_cast<std::uint32_t>(s.writeIndex);
                if (!write_raw(os, historySize32)) return false;
                if (!write_raw(os, writeIndex32)) return false;
                if (!write_raw(os, s.sampleCount)) return false;
                if (!write_raw(os, s.sum)) return false;

                // Chronological history values (oldest -> newest)
                const std::size_t n = s.historySize;
                const std::size_t start = (s.historySize == MaxHistory) ? s.writeIndex : 0;
                for (std::size_t i = 0; i < n; ++i) {
                    const std::size_t idx = (start + i) % MaxHistory;
                    const double v = s.history[idx];
                    if (!write_raw(os, v)) return false;
                }
            }
        }
    }

    return os.good();
}

bool HeatMapContainer::Load(const std::string& filepath)
{
    std::ifstream is(filepath, std::ios::binary | std::ios::in);
    if (!is.is_open()) return false;

    // Header
    std::uint32_t magic = 0, version = 0;
    if (!read_raw(is, magic)) return false;
    if (magic != kMagic) return false;

    if (!read_raw(is, version)) return false;
    if (version != kVersion) return false;

    // Geometry
    float originX = 0, originY = 0, widthMeters = 0, heightMeters = 0, cellSizeMeters = 0;
    if (!read_raw(is, originX)) return false;
    if (!read_raw(is, originY)) return false;
    if (!read_raw(is, widthMeters)) return false;
    if (!read_raw(is, heightMeters)) return false;
    if (!read_raw(is, cellSizeMeters)) return false;

    // Grid dims
    std::uint64_t cols64 = 0, rows64 = 0;
    if (!read_raw(is, cols64)) return false;
    if (!read_raw(is, rows64)) return false;

    // Rebuild container
    m_originX = originX;
    m_originY = originY;
    m_widthMeters = std::max(0.0f, widthMeters);
    m_heightMeters = std::max(0.0f, heightMeters);
    m_cellSizeMeters = std::max(0.0001f, cellSizeMeters);

    m_cols = static_cast<std::size_t>(cols64);
    m_rows = static_cast<std::size_t>(rows64);
    if (m_cols == 0 || m_rows == 0) {
        m_cells.clear();
    } else {
        m_cells.assign(m_cols * m_rows, Cell{});
    }

    // Cells and series
    for (std::size_t r = 0; r < m_rows; ++r) {
        for (std::size_t c = 0; c < m_cols; ++c) {
            auto& cell = m_cells[FlatIndex(c, r)];
            std::uint32_t seriesCount = 0;
            if (!read_raw(is, seriesCount)) return false;

            for (std::uint32_t sIdx = 0; sIdx < seriesCount; ++sIdx) {
                std::string name;
                if (!read_string(is, name)) return false;

                std::uint32_t historySize32 = 0;
                std::uint32_t writeIndex32 = 0;
                std::uint64_t sampleCount = 0;
                double sum = 0.0;

                if (!read_raw(is, historySize32)) return false;
                if (!read_raw(is, writeIndex32)) return false;
                if (!read_raw(is, sampleCount)) return false;
                if (!read_raw(is, sum)) return false;

                HeatMapContainer::DataSeries series{};
                series.historySize = std::min<std::size_t>(historySize32, MaxHistory);
                series.writeIndex = std::min<std::size_t>(writeIndex32, MaxHistory);
                series.sampleCount = sampleCount;
                series.sum = sum;

                // Read chronological values and place in ring buffer slots 0..n-1
                for (std::size_t i = 0; i < series.historySize; ++i) {
                    double v = 0.0;
                    if (!read_raw(is, v)) return false;
                    series.history[i] = v;
                }

                // Normalize writeIndex to be the "next" position
                series.writeIndex = series.historySize % MaxHistory;

                cell.seriesByName.emplace(std::move(name), std::move(series));
            }
        }
    }

    return is.good();
}