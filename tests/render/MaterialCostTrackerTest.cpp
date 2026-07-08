#include <gtest/gtest.h>
#include <render/MaterialCostTracker.h>

TEST(MaterialCostTrackerTest, RelativeCostIsNeutralWithNoSamples)
{
    MaterialCostTracker tracker(3);

    EXPECT_NEAR(tracker.relativeCost(0), 1.0, 0.0001);
}

TEST(MaterialCostTrackerTest, RelativeCostIsNeutralBelowSampleThreshold)
{
    MaterialCostTracker tracker(2);

    for (int i = 0; i < 10; ++i)
        tracker.record(0, 100.0);

    // Fewer than the threshold should stay neutral
    EXPECT_NEAR(tracker.relativeCost(0), 1.0, 0.0001);
}

TEST(MaterialCostTrackerTest, ExpensiveMaterialHasRelativeCostAboveOne)
{
    MaterialCostTracker tracker(2);

    for (int i = 0; i < 60; ++i)
    {
        tracker.record(0, 10.0); // cheap material
        tracker.record(1, 30.0); // expensive material
    }

    EXPECT_GT(tracker.relativeCost(1), 1.0);
    EXPECT_LT(tracker.relativeCost(0), 1.0);
}

TEST(MaterialCostTrackerTest, EqualCostMaterialsHaveRelativeCostNearOne)
{
    MaterialCostTracker tracker(2);

    for (int i = 0; i < 60; ++i)
    {
        tracker.record(0, 15.0);
        tracker.record(1, 15.0);
    }

    EXPECT_NEAR(tracker.relativeCost(0), 1.0, 0.05);
    EXPECT_NEAR(tracker.relativeCost(1), 1.0, 0.05);
}