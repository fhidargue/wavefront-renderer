#include <gtest/gtest.h>
#include <scheduling/RayQueue.h>

static Ray makeRay()
{
    return Ray(Point3(0.0f, 0.0f, 0.0f), Vec3(0.0f, 0.0f, -1.0f));
}

TEST(RayQueueTest, StartsEmpty)
{
    RayQueue queue;

    EXPECT_TRUE(queue.empty());
    EXPECT_EQ(queue.size(), 0);
}

TEST(RayQueueTest, AddIncreasesSize)
{
    RayQueue queue;

    queue.add(RayState(makeRay(), 0));

    EXPECT_EQ(queue.size(), 1);
    EXPECT_FALSE(queue.empty());
}

TEST(RayQueueTest, ClearResetsToEmpty)
{
    RayQueue queue;

    queue.add(RayState(makeRay(), 0));
    queue.add(RayState(makeRay(), 1));
    queue.clear();

    EXPECT_TRUE(queue.empty());
    EXPECT_EQ(queue.size(), 0);
}

TEST(RayStateTest, ThroughputInitialisedToWhite)
{
    RayState state(makeRay(), 0);

    EXPECT_EQ(state.throughtput.x, 1.0f);
    EXPECT_EQ(state.throughtput.y, 1.0f);
    EXPECT_EQ(state.throughtput.z, 1.0f);
}

TEST(RayStateTest, AccumLightInitialisedToBlack)
{
    RayState state(makeRay(), 0);

    EXPECT_EQ(state.accumLight.x, 0.0f);
    EXPECT_EQ(state.accumLight.y, 0.0f);
    EXPECT_EQ(state.accumLight.z, 0.0f);
}

TEST(RayStateTest, PixelIndexStoredCorrectly)
{
    int pixelIndex = 137;
    RayState state(makeRay(), pixelIndex);

    EXPECT_EQ(state.rayPixelIndex, pixelIndex);
}

TEST(RayStateTest, StartsAtDepthZeroAndAlive)
{
    RayState state(makeRay(), 0);

    EXPECT_EQ(state.rayDepth, 0);
    EXPECT_TRUE(state.isRayAlive);
}