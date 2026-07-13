#include <gtest/gtest.h>
#include <scheduling/RayQueue.h>
#include <algorithm>

static Ray makeRay()
{
    return Ray(Point3(0.0f, 0.0f, 0.0f), Vec3(0.0f, 0.0f, -1.0f));
}

static Ray makeRay(const Point3& origin, const Vec3& direction)
{
    return Ray(origin, direction);
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
    queue.add(makeRay(), Color(0.5f, 0.3f, 0.1f), Color(0.0f, 0.0f, 0.0f), 0, 1, true, 1.0f);
    Color throughput = queue.getThroughput(0);

    EXPECT_FLOAT_EQ(throughput.x, 0.5f);
    EXPECT_FLOAT_EQ(throughput.y, 0.3f);
    EXPECT_FLOAT_EQ(throughput.z, 0.1f);
}

// A 20-unit cube centred used as the scene bounds for every schedule call
static const Point3 testSceneMin(-10.0f, -10.0f, -10.0f);
static const Point3 testSceneMax(10.0f, 10.0f, 10.0f);

TEST(RayQueueTest, ScheduleOnEmptyQueueDoesNotCrash)
{
    RayQueue queue;
    queue.schedule(testSceneMin, testSceneMax);

    EXPECT_EQ(queue.size(), 0);
    EXPECT_EQ(queue.sortedIndices.size(), 0u);
}

TEST(RayQueueTest, SingleRayScheduleAndMaterializeIsStable)
{
    RayQueue queue;
    queue.addPrimary(makeRay(Point3(3.0f, 4.0f, 5.0f), Vec3(0.0f, 0.0f, -1.0f)), 7);

    queue.schedule(testSceneMin, testSceneMax);
    queue.materialize();

    EXPECT_EQ(queue.size(), 1);
    EXPECT_EQ(queue.pixelIndices[0], 7);
    EXPECT_FLOAT_EQ(queue.originsX[0], 3.0f);
}

TEST(RayQueueTest, ScheduleGroupsRaysBySameDirectionOctantTogether)
{
    RayQueue queue;

    queue.addPrimary(makeRay(Point3(0.0f, 0.0f, 0.0f), Vec3(1.0f, 1.0f, 1.0f)), 10);
    queue.addPrimary(makeRay(Point3(0.0f, 0.0f, 0.0f), Vec3(-1.0f, -1.0f, -1.0f)), 20);
    queue.addPrimary(makeRay(Point3(0.0f, 0.0f, 0.0f), Vec3(1.0f, 1.0f, 1.0f)), 11);
    queue.addPrimary(makeRay(Point3(0.0f, 0.0f, 0.0f), Vec3(-1.0f, -1.0f, -1.0f)), 21);

    queue.schedule(testSceneMin, testSceneMax);
    queue.materialize();

    std::vector<int> firstHalf = {queue.pixelIndices[0], queue.pixelIndices[1]};
    std::vector<int> secondHalf = {queue.pixelIndices[2], queue.pixelIndices[3]};
    std::sort(firstHalf.begin(), firstHalf.end());
    std::sort(secondHalf.begin(), secondHalf.end());

    bool groupedAsExpected =
        (firstHalf == std::vector<int>{10, 11} && secondHalf == std::vector<int>{20, 21}) ||
        (firstHalf == std::vector<int>{20, 21} && secondHalf == std::vector<int>{10, 11});

    EXPECT_TRUE(groupedAsExpected);
}

TEST(RayQueueTest, ScheduleOrdersRaysBySpatialProximityWithinSameOctant)
{
    RayQueue queue;

    Vec3 sharedDirection(0.0f, 0.0f, 1.0f);
    queue.addPrimary(makeRay(Point3(5.0f, 0.0f, 0.0f), sharedDirection), 100);
    queue.addPrimary(makeRay(Point3(-5.0f, 0.0f, 0.0f), sharedDirection), 200);
    queue.addPrimary(makeRay(Point3(0.0f, 0.0f, 0.0f), sharedDirection), 300);

    queue.schedule(testSceneMin, testSceneMax);
    queue.materialize();

    EXPECT_EQ(queue.pixelIndices[0], 200); // Quantizes lowest
    EXPECT_EQ(queue.pixelIndices[1], 300);
    EXPECT_EQ(queue.pixelIndices[2], 100); // Quantizes highest
}

TEST(RayQueueTest, MaterializeKeepsOriginDirectionAndPixelIndexPaired)
{
    RayQueue queue;

    queue.addPrimary(makeRay(Point3(9.0f, 0.0f, 0.0f), Vec3(1.0f, 0.0f, 0.0f)), 1);
    queue.addPrimary(makeRay(Point3(-9.0f, 0.0f, 0.0f), Vec3(1.0f, 0.0f, 0.0f)), 2);

    queue.schedule(testSceneMin, testSceneMax);
    queue.materialize();

    for (int i = 0; i < queue.size(); ++i)
    {
        if (queue.pixelIndices[i] == 1)
            EXPECT_FLOAT_EQ(queue.originsX[i], 9.0f);
        else
            EXPECT_FLOAT_EQ(queue.originsX[i], -9.0f);
    }
}

TEST(RayQueueTest, MaterializeResetsSortedIndicesToIdentity)
{
    RayQueue queue;
    queue.addPrimary(makeRay(Point3(3.0f, 0.0f, 0.0f), Vec3(1.0f, 0.0f, 0.0f)), 0);
    queue.addPrimary(makeRay(Point3(-3.0f, 0.0f, 0.0f), Vec3(1.0f, 0.0f, 0.0f)), 1);

    queue.schedule(testSceneMin, testSceneMax);
    queue.materialize();

    EXPECT_EQ(queue.sortedIndices[0], 0);
    EXPECT_EQ(queue.sortedIndices[1], 1);
}

TEST(RayQueueTest, ScheduleClampsOriginsOutsideSceneBoundsWithoutCrashing)
{
    RayQueue queue;

    queue.addPrimary(makeRay(Point3(9999.0f, 9999.0f, 9999.0f), Vec3(0.0f, 0.0f, 1.0f)), 0);
    queue.addPrimary(makeRay(Point3(0.0f, 0.0f, 0.0f), Vec3(0.0f, 0.0f, 1.0f)), 1);

    queue.schedule(testSceneMin, testSceneMax);
    queue.materialize();

    EXPECT_EQ(queue.size(), 2);
}