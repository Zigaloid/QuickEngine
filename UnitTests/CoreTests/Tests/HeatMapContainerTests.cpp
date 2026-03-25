#include <gtest/gtest.h>
#include <string>
#include <vector>
#include <optional>

#include "Analysis/HeatMapContainer.h"

TEST(HeatMapContainerTests, ConstructsGridWithExpectedDimensions)
{
    // width=5, height=3, cell=1 -> cols=5, rows=3
    HeatMapContainer hm(5.0f, 3.0f, 1.0f, 10.0f, 20.0f);

    // Inside corners
    EXPECT_NE(hm.TryGetCell(0, 0), nullptr);
    EXPECT_NE(hm.TryGetCell(4, 2), nullptr);

    // Outside (just beyond)
    EXPECT_EQ(hm.TryGetCell(5, 0), nullptr);
    EXPECT_EQ(hm.TryGetCell(0, 3), nullptr);
    EXPECT_EQ(hm.TryGetCell(4, 3), nullptr);
    EXPECT_EQ(hm.TryGetCell(5, 2), nullptr);
}

TEST(HeatMapContainerTests, IsInsideHonorsOriginAndExclusiveFarEdge)
{
    HeatMapContainer hm(2.0f, 1.0f, 0.5f, 10.0f, 20.0f);

    EXPECT_TRUE(hm.IsInside(10.0f, 20.0f));          // exactly at origin
    EXPECT_TRUE(hm.IsInside(11.999f, 20.999f));      // just within far edge
    EXPECT_FALSE(hm.IsInside(12.0f, 21.0f));         // exactly on far edge -> outside
    EXPECT_FALSE(hm.IsInside(9.999f, 20.0f));        // before origin x
    EXPECT_FALSE(hm.IsInside(10.0f, 19.999f));       // before origin y
}

TEST(HeatMapContainerTests, GetCellIndicesForPointMapsCorrectlyAndRejectsOutside)
{
    HeatMapContainer hm(4.0f, 4.0f, 1.0f, 0.0f, 0.0f);

    std::size_t c = 999, r = 999;
    ASSERT_TRUE(hm.GetCellIndicesForPoint(0.0f, 0.0f, c, r));
    EXPECT_EQ(c, 0u);
    EXPECT_EQ(r, 0u);

    ASSERT_TRUE(hm.GetCellIndicesForPoint(0.99f, 0.99f, c, r));
    EXPECT_EQ(c, 0u);
    EXPECT_EQ(r, 0u);

    ASSERT_TRUE(hm.GetCellIndicesForPoint(1.01f, 0.99f, c, r));
    EXPECT_EQ(c, 1u);
    EXPECT_EQ(r, 0u);

    ASSERT_TRUE(hm.GetCellIndicesForPoint(3.999f, 3.999f, c, r));
    EXPECT_EQ(c, 3u);
    EXPECT_EQ(r, 3u);

    EXPECT_FALSE(hm.GetCellIndicesForPoint(4.0f, 1.0f, c, r)); // outside on far edge
}

TEST(HeatMapContainerTests, AddValueAndGetAverageAtWorksPerSeriesAndCell)
{
    HeatMapContainer hm(2.0f, 2.0f, 1.0f, 0.0f, 0.0f);

    // Add multiple samples for series "A" in one cell
    EXPECT_TRUE(hm.AddValue(0.2f, 0.4f, "A", 1.0));
    EXPECT_TRUE(hm.AddValue(0.2f, 0.4f, "A", 3.0));

    // Different series in same cell
    EXPECT_TRUE(hm.AddValue(0.2f, 0.4f, "B", 10.0));

    // Same series "A" but different cell
    EXPECT_TRUE(hm.AddValue(1.2f, 1.4f, "A", 100.0));

    auto avgA_cell0 = hm.GetAverageAt(0.2f, 0.4f, "A");
    ASSERT_TRUE(avgA_cell0.has_value());
    EXPECT_DOUBLE_EQ(avgA_cell0.value(), 2.0);

    auto avgB_cell0 = hm.GetAverageAt(0.2f, 0.4f, "B");
    ASSERT_TRUE(avgB_cell0.has_value());
    EXPECT_DOUBLE_EQ(avgB_cell0.value(), 10.0);

    auto avgA_cell11 = hm.GetAverageAt(1.2f, 1.4f, "A");
    ASSERT_TRUE(avgA_cell11.has_value());
    EXPECT_DOUBLE_EQ(avgA_cell11.value(), 100.0);

    // Missing series
    auto missing = hm.GetAverageAt(0.2f, 0.4f, "C");
    EXPECT_FALSE(missing.has_value());
}

TEST(HeatMapContainerTests, AddValueByIndexValidatesBounds)
{
    HeatMapContainer hm(2.0f, 2.0f, 1.0f, 0.0f, 0.0f); // 2x2 grid: indices [0..1]

    EXPECT_TRUE(hm.AddValueByIndex(1, 1, "X", 5.0));
    EXPECT_FALSE(hm.AddValueByIndex(2, 1, "X", 5.0)); // col out of bounds
    EXPECT_FALSE(hm.AddValueByIndex(1, 2, "X", 5.0)); // row out of bounds

    auto avg = hm.GetAverageAtIndex(1, 1, "X");
    ASSERT_TRUE(avg.has_value());
    EXPECT_DOUBLE_EQ(avg.value(), 5.0);
}

TEST(HeatMapContainerTests, GetLastValuesAtReturnsOldestToNewestAndRespectsMaxCount)
{
    HeatMapContainer hm(2.0f, 2.0f, 1.0f, 0.0f, 0.0f);

    for (int i = 1; i <= 5; ++i) {
        ASSERT_TRUE(hm.AddValue(0.1f, 0.1f, "S", static_cast<double>(i)));
    }

    std::vector<double> out;
    ASSERT_TRUE(hm.GetLastValuesAt(0.1f, 0.1f, "S", out, 3));
    ASSERT_EQ(out.size(), 3u);
    EXPECT_DOUBLE_EQ(out[0], 3.0);
    EXPECT_DOUBLE_EQ(out[1], 4.0);
    EXPECT_DOUBLE_EQ(out[2], 5.0);

    // Request more than available -> returns all in oldest->newest
    ASSERT_TRUE(hm.GetLastValuesAt(0.1f, 0.1f, "S", out, 100));
    ASSERT_EQ(out.size(), 5u);
    EXPECT_DOUBLE_EQ(out[0], 1.0);
    EXPECT_DOUBLE_EQ(out[1], 2.0);
    EXPECT_DOUBLE_EQ(out[2], 3.0);
    EXPECT_DOUBLE_EQ(out[3], 4.0);
    EXPECT_DOUBLE_EQ(out[4], 5.0);

    // Missing series -> false
    EXPECT_FALSE(hm.GetLastValuesAt(0.1f, 0.1f, "Missing", out, 3));
}

TEST(HeatMapContainerTests, ClearSeriesCellAndAllWork)
{
    HeatMapContainer hm(3.0f, 3.0f, 1.0f, 0.0f, 0.0f);

    // Populate cell (1,1) with two series
    EXPECT_TRUE(hm.AddValueByIndex(1, 1, "A", 1.0));
    EXPECT_TRUE(hm.AddValueByIndex(1, 1, "A", 3.0));
    EXPECT_TRUE(hm.AddValueByIndex(1, 1, "B", 10.0));

    // Clear only series A
    EXPECT_TRUE(hm.ClearSeries(1, 1, "A"));
    auto a = hm.GetAverageAtIndex(1, 1, "A");
    EXPECT_FALSE(a.has_value()); // removed
    auto b = hm.GetAverageAtIndex(1, 1, "B");
    ASSERT_TRUE(b.has_value());  // still present
    EXPECT_DOUBLE_EQ(b.value(), 10.0);

    // Clear entire cell
    EXPECT_TRUE(hm.ClearCell(1, 1));
    EXPECT_FALSE(hm.GetAverageAtIndex(1, 1, "B").has_value());

    // Add elsewhere, then Clear() everything
    EXPECT_TRUE(hm.AddValueByIndex(0, 0, "Z", 42.0));
    EXPECT_TRUE(hm.AddValueByIndex(2, 2, "Z", 24.0));
    hm.Clear();
    EXPECT_FALSE(hm.GetAverageAtIndex(0, 0, "Z").has_value());
    EXPECT_FALSE(hm.GetAverageAtIndex(2, 2, "Z").has_value());
}

TEST(HeatMapContainerTests, DegenerateZeroOrNegativeSizesProduceNoCells)
{
    // Negative width clamps to 0; height=0 -> no cells
    HeatMapContainer hm(-1.0f, 0.0f, 1.0f, 0.0f, 0.0f);

    // No valid cells
    EXPECT_EQ(hm.TryGetCell(0, 0), nullptr);

    // IsInside should be false everywhere due to zero dimensions
    EXPECT_FALSE(hm.IsInside(0.0f, 0.0f));

    // Coordinate-based operations fail
    std::vector<double> out;
    EXPECT_FALSE(hm.AddValue(0.1f, 0.1f, "X", 1.0));
    EXPECT_FALSE(hm.GetLastValuesAt(0.1f, 0.1f, "X", out, 1));
    EXPECT_FALSE(hm.GetAverageAt(0.1f, 0.1f, "X").has_value());
}