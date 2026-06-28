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

TEST(RayQueueTest, AddPrimaryIncreasesSize)
{
    RayQueue queue;
    queue.addPrimary(makeRay(), 0);

    EXPECT_EQ(queue.size(), 1);
    EXPECT_FALSE(queue.empty());
}

TEST(RayQueueTest, ClearResetsToEmpty)
{
    RayQueue queue;
    queue.addPrimary(makeRay(), 0);
    queue.addPrimary(makeRay(), 1);
    queue.clear();

    EXPECT_TRUE(queue.empty());
    EXPECT_EQ(queue.size(), 0);
}

TEST(RayStateTest, ThroughputInitialisedToWhite)
{
    RayQueue queue;
    queue.addPrimary(makeRay(), 0);

    EXPECT_EQ(queue.throughputsR[0], 1.0f);
    EXPECT_EQ(queue.throughputsG[0], 1.0f);
    EXPECT_EQ(queue.throughputsB[0], 1.0f);
}

TEST(RayQueueTest, AccumLightInitialisedToBlack)
{
    RayQueue queue;
    queue.addPrimary(makeRay(), 0);

    EXPECT_EQ(queue.accumLightR[0], 0.0f);
    EXPECT_EQ(queue.accumLightG[0], 0.0f);
    EXPECT_EQ(queue.accumLightB[0], 0.0f);
}

TEST(RayQueueTest, PixelIndexStoredCorrectly)
{
    RayQueue queue;
    queue.addPrimary(makeRay(), 137);

    EXPECT_EQ(queue.pixelIndices[0], 137);
}

TEST(RayQueueTest, StartsAtDepthZeroAndAlive)
{
    RayQueue queue;
    queue.addPrimary(makeRay(), 0);

    EXPECT_EQ(queue.depths[0], 0);
    EXPECT_TRUE(queue.alive[0]);
}

TEST(RayQueueTest, GetRayReturnsCorrectOriginAndDirection)
{
    RayQueue queue;
    Ray ray(Point3(1.0f, 2.0f, 3.0f), Vec3(0.0f, 1.0f, 0.0f));
    queue.addPrimary(ray, 0);
    Ray retrieved = queue.getRay(0);

    EXPECT_FLOAT_EQ(retrieved.origin.x, 1.0f);
    EXPECT_FLOAT_EQ(retrieved.origin.y, 2.0f);
    EXPECT_FLOAT_EQ(retrieved.origin.z, 3.0f);
    EXPECT_FLOAT_EQ(retrieved.direction.x, 0.0f);
    EXPECT_FLOAT_EQ(retrieved.direction.y, 1.0f);
    EXPECT_FLOAT_EQ(retrieved.direction.z, 0.0f);
}

TEST(RayQueueTest, GetThroughputReturnsCorrectValues)
{
    RayQueue queue;
    queue.add(makeRay(), Color(0.5f, 0.3f, 0.1f), Color(0.0f, 0.0f, 0.0f), 0, 1, true);
    Color throughput = queue.getThroughput(0);

    EXPECT_FLOAT_EQ(throughput.x, 0.5f);
    EXPECT_FLOAT_EQ(throughput.y, 0.3f);
    EXPECT_FLOAT_EQ(throughput.z, 0.1f);
}