#include <gtest/gtest.h>
#include <scheduling/ShadingQueue.h>

static Ray makeRay()
{
    return Ray(Point3(0.0f, 0.0f, 0.0f), Vec3(0.0f, 0.0f, -1.0f));
}

static RayQueue makeSingleRayQueue(int pixelIndex)
{
    RayQueue queue;
    queue.addPrimary(makeRay(), pixelIndex);

    return queue;
}

static HitRecord makeHitRecord(int materialID, int textureID = 0)
{
    HitRecord record;
    record.materialID = materialID;
    record.textureID = textureID;
    record.distance = 1.0f;
    record.frontFace = true;

    return record;
}

TEST(ShadingQueueTest, StartsEmpty)
{
    ShadingQueue queue(SchedulingPolicy::None);

    EXPECT_EQ(queue.size(), 0);
}

TEST(ShadingQueueTest, AddIncreasesSize)
{
    ShadingQueue queue(SchedulingPolicy::None);
    auto queue0 = makeSingleRayQueue(0);
    auto queue1 = makeSingleRayQueue(1);
    queue.add(queue0, 0, makeHitRecord(0));
    queue.add(queue1, 0, makeHitRecord(1));

    EXPECT_EQ(queue.size(), 2);
}

TEST(ShadingQueueTest, ClearResetsSize)
{
    ShadingQueue queue(SchedulingPolicy::None);
    auto queue0 = makeSingleRayQueue(0);
    queue.add(queue0, 0, makeHitRecord(0));
    queue.clear();

    EXPECT_EQ(queue.size(), 0);
}

TEST(ShadingQueueTest, NonePolicyPreservesArrivalOrder)
{
    ShadingQueue queue(SchedulingPolicy::None);
    auto queue0 = makeSingleRayQueue(0);
    auto queue1 = makeSingleRayQueue(1);
    auto queue2 = makeSingleRayQueue(2);
    queue.add(queue0, 0, makeHitRecord(2));
    queue.add(queue1, 0, makeHitRecord(0));
    queue.add(queue2, 0, makeHitRecord(1));
    queue.schedule();

    EXPECT_EQ(queue.getHitRecord(0).materialID, 2);
    EXPECT_EQ(queue.getHitRecord(1).materialID, 0);
    EXPECT_EQ(queue.getHitRecord(2).materialID, 1);
}

TEST(ShadingQueueTest, MaterialAwareSortsByMaterialIDAscending)
{
    ShadingQueue queue(SchedulingPolicy::MaterialAware);
    auto queue0 = makeSingleRayQueue(0);
    auto queue1 = makeSingleRayQueue(1);
    auto queue2 = makeSingleRayQueue(2);
    queue.add(queue0, 0, makeHitRecord(2));
    queue.add(queue1, 0, makeHitRecord(0));
    queue.add(queue2, 0, makeHitRecord(1));
    queue.schedule();

    EXPECT_EQ(queue.getHitRecord(0).materialID, 0);
    EXPECT_EQ(queue.getHitRecord(1).materialID, 1);
    EXPECT_EQ(queue.getHitRecord(2).materialID, 2);
}

TEST(ShadingQueueTest, MaterialAwareGroupsSameMaterialTogether)
{
    ShadingQueue queue(SchedulingPolicy::MaterialAware);

    for (int materialID : {1, 0, 1, 0, 1})
    {
        auto queue0 = makeSingleRayQueue(materialID);
        queue.add(queue0, 0, makeHitRecord(materialID));
    }
    queue.schedule();

    EXPECT_EQ(queue.getHitRecord(0).materialID, 0);
    EXPECT_EQ(queue.getHitRecord(1).materialID, 0);
    EXPECT_EQ(queue.getHitRecord(2).materialID, 1);
    EXPECT_EQ(queue.getHitRecord(3).materialID, 1);
    EXPECT_EQ(queue.getHitRecord(4).materialID, 1);
}

TEST(ShadingQueueTest, TextureAwareSortsByMaterialThenTexture)
{
    ShadingQueue queue(SchedulingPolicy::TextureAware);
    auto queue0 = makeSingleRayQueue(0);
    auto queue1 = makeSingleRayQueue(1);
    auto queue2 = makeSingleRayQueue(2);
    auto queue3 = makeSingleRayQueue(3);
    queue.add(queue0, 0, makeHitRecord(1, 2));
    queue.add(queue1, 0, makeHitRecord(0, 1));
    queue.add(queue2, 0, makeHitRecord(1, 0));
    queue.add(queue3, 0, makeHitRecord(0, 0));
    queue.schedule();

    EXPECT_EQ(queue.getHitRecord(0).materialID, 0);
    EXPECT_EQ(queue.getHitRecord(0).textureID, 0);
    EXPECT_EQ(queue.getHitRecord(1).materialID, 0);
    EXPECT_EQ(queue.getHitRecord(1).textureID, 1);
    EXPECT_EQ(queue.getHitRecord(2).materialID, 1);
    EXPECT_EQ(queue.getHitRecord(2).textureID, 0);
    EXPECT_EQ(queue.getHitRecord(3).materialID, 1);
    EXPECT_EQ(queue.getHitRecord(3).textureID, 2);
}

TEST(ShadingQueueTest, SingleEntryScheduleIsStable)
{
    ShadingQueue queue(SchedulingPolicy::MaterialAware);
    auto queue0 = makeSingleRayQueue(0);
    queue.add(queue0, 0, makeHitRecord(5));
    queue.schedule();

    EXPECT_EQ(queue.size(), 1);
    EXPECT_EQ(queue.getHitRecord(0).materialID, 5);
}

TEST(ShadingQueueTest, AlreadySortedInputIsStable)
{
    ShadingQueue queue(SchedulingPolicy::MaterialAware);

    for (int materialID : {0, 1, 2})
    {
        auto queue0 = makeSingleRayQueue(materialID);
        queue.add(queue0, 0, makeHitRecord(materialID));
    }
    queue.schedule();

    EXPECT_EQ(queue.getHitRecord(0).materialID, 0);
    EXPECT_EQ(queue.getHitRecord(1).materialID, 1);
    EXPECT_EQ(queue.getHitRecord(2).materialID, 2);
}

TEST(ShadingQueueTest, PixelIndexPreservedThroughSort)
{
    ShadingQueue queue(SchedulingPolicy::MaterialAware);
    auto queue0 = makeSingleRayQueue(42);
    auto queue1 = makeSingleRayQueue(99);
    queue.add(queue0, 0, makeHitRecord(1));
    queue.add(queue1, 0, makeHitRecord(0));
    queue.schedule();

    // After sort the materialID 0 first (px 99), then materialID 1 (px 42)
    EXPECT_EQ(queue.getPixelIndex(0), 99);
    EXPECT_EQ(queue.getPixelIndex(1), 42);
}

TEST(ShadingQueueTest, MaterializeReordersDataToMatchSortedIndices)
{
    ShadingQueue queue(SchedulingPolicy::MaterialAware);

    RayQueue dummyInput;
    dummyInput.addPrimary(Ray(Point3(0, 0, 0), Vec3(0, 0, -1)), 0);
    dummyInput.addPrimary(Ray(Point3(0, 0, 0), Vec3(0, 0, -1)), 1);
    dummyInput.addPrimary(Ray(Point3(0, 0, 0), Vec3(0, 0, -1)), 2);

    // Add rays with materialIDs in a deliberately unsorted order: 2, 0, 1
    HitRecord recordA;
    recordA.materialID = 2;
    recordA.point = Point3(1.0f, 0.0f, 0.0f);
    recordA.normal = Vec3(0.0f, 1.0f, 0.0f);

    HitRecord recordB;
    recordB.materialID = 0;
    recordB.point = Point3(2.0f, 0.0f, 0.0f);
    recordB.normal = Vec3(0.0f, 1.0f, 0.0f);

    HitRecord recordC;
    recordC.materialID = 1;
    recordC.point = Point3(3.0f, 0.0f, 0.0f);
    recordC.normal = Vec3(0.0f, 1.0f, 0.0f);

    queue.add(dummyInput, 0, recordA);
    queue.add(dummyInput, 1, recordB);
    queue.add(dummyInput, 2, recordC);

    queue.schedule();
    queue.materialize();

    EXPECT_EQ(queue.materialIDs[0], 0);
    EXPECT_EQ(queue.materialIDs[1], 1);
    EXPECT_EQ(queue.materialIDs[2], 2);
}

TEST(ShadingQueueTest, MaterializeKeepsHitPointsConsistentWithMaterialIDs)
{
    ShadingQueue queue(SchedulingPolicy::MaterialAware);

    RayQueue dummyInput;
    dummyInput.addPrimary(Ray(Point3(0, 0, 0), Vec3(0, 0, -1)), 0);
    dummyInput.addPrimary(Ray(Point3(0, 0, 0), Vec3(0, 0, -1)), 1);

    HitRecord recordA;
    recordA.materialID = 5;
    recordA.point = Point3(99.0f, 0.0f, 0.0f);
    recordA.normal = Vec3(0.0f, 1.0f, 0.0f);

    HitRecord recordB;
    recordB.materialID = 1;
    recordB.point = Point3(1.0f, 0.0f, 0.0f);
    recordB.normal = Vec3(0.0f, 1.0f, 0.0f);

    queue.add(dummyInput, 0, recordA);
    queue.add(dummyInput, 1, recordB);

    queue.schedule();
    queue.materialize();

    // Position 0 should now hold materialID 1
    EXPECT_EQ(queue.materialIDs[0], 1);
    EXPECT_NEAR(queue.hitPointsX[0], 1.0f, 0.0001f);
}

TEST(ShadingQueueTest, MaterializeResetsSortedIndicesToIdentity)
{
    ShadingQueue queue(SchedulingPolicy::MaterialAware);

    RayQueue dummyInput;
    dummyInput.addPrimary(Ray(Point3(0, 0, 0), Vec3(0, 0, -1)), 0);
    dummyInput.addPrimary(Ray(Point3(0, 0, 0), Vec3(0, 0, -1)), 1);

    HitRecord recordA;
    recordA.materialID = 3;
    recordA.point = Point3(0.0f, 0.0f, 0.0f);
    recordA.normal = Vec3(0.0f, 1.0f, 0.0f);

    HitRecord recordB;
    recordB.materialID = 1;
    recordB.point = Point3(0.0f, 0.0f, 0.0f);
    recordB.normal = Vec3(0.0f, 1.0f, 0.0f);

    queue.add(dummyInput, 0, recordA);
    queue.add(dummyInput, 1, recordB);

    queue.schedule();
    queue.materialize();

    EXPECT_EQ(queue.sortedIndices[0], 0);
    EXPECT_EQ(queue.sortedIndices[1], 1);
}