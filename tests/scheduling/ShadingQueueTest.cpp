#include <gtest/gtest.h>
#include <scheduling/ShadingQueue.h>

static RayState makeRayState(int pixelIndex)
{
    Ray ray(Point3(0.0f, 0.0f, 0.0f), Vec3(0.0f, 0.0f, -1.0f));

    return RayState(ray, pixelIndex);
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

    queue.add(makeRayState(0), makeHitRecord(0));
    queue.add(makeRayState(1), makeHitRecord(1));

    EXPECT_EQ(queue.size(), 2);
}

TEST(ShadingQueueTest, ClearResetsSize)
{
    ShadingQueue queue(SchedulingPolicy::None);

    queue.add(makeRayState(0), makeHitRecord(0));
    queue.clear();

    EXPECT_EQ(queue.size(), 0);
}

TEST(ShadingQueueTest, NonePolicyPreservesArrivalOrder)
{
    ShadingQueue queue(SchedulingPolicy::None);

    queue.add(makeRayState(0), makeHitRecord(2));
    queue.add(makeRayState(1), makeHitRecord(0));
    queue.add(makeRayState(2), makeHitRecord(1));
    queue.schedule();

    EXPECT_EQ(queue.getSorted(0).hitRecord.materialID, 2);
    EXPECT_EQ(queue.getSorted(1).hitRecord.materialID, 0);
    EXPECT_EQ(queue.getSorted(2).hitRecord.materialID, 1);
}

TEST(ShadingQueueTest, MaterialAwareSortsByMaterialIDascending)
{
    ShadingQueue queue(SchedulingPolicy::MaterialAware);

    queue.add(makeRayState(0), makeHitRecord(2));
    queue.add(makeRayState(1), makeHitRecord(0));
    queue.add(makeRayState(2), makeHitRecord(1));
    queue.schedule();

    EXPECT_EQ(queue.getSorted(0).hitRecord.materialID, 0);
    EXPECT_EQ(queue.getSorted(1).hitRecord.materialID, 1);
    EXPECT_EQ(queue.getSorted(2).hitRecord.materialID, 2);
}

TEST(ShadingQueueTest, MaterialAwareGroupsSameMaterialTogether)
{
    ShadingQueue queue(SchedulingPolicy::MaterialAware);

    queue.add(makeRayState(0), makeHitRecord(1));
    queue.add(makeRayState(1), makeHitRecord(0));
    queue.add(makeRayState(2), makeHitRecord(1));
    queue.add(makeRayState(3), makeHitRecord(0));
    queue.add(makeRayState(4), makeHitRecord(1));
    queue.schedule();

    EXPECT_EQ(queue.getSorted(0).hitRecord.materialID, 0);
    EXPECT_EQ(queue.getSorted(1).hitRecord.materialID, 0);
    EXPECT_EQ(queue.getSorted(2).hitRecord.materialID, 1);
    EXPECT_EQ(queue.getSorted(3).hitRecord.materialID, 1);
    EXPECT_EQ(queue.getSorted(4).hitRecord.materialID, 1);
}

TEST(ShadingQueueTest, TextureAwareSortsByMaterialThenTexture)
{
    ShadingQueue queue(SchedulingPolicy::TextureAware);

    queue.add(makeRayState(0), makeHitRecord(1, 2));
    queue.add(makeRayState(1), makeHitRecord(0, 1));
    queue.add(makeRayState(2), makeHitRecord(1, 0));
    queue.add(makeRayState(3), makeHitRecord(0, 0));
    queue.schedule();

    EXPECT_EQ(queue.getSorted(0).hitRecord.materialID, 0);
    EXPECT_EQ(queue.getSorted(0).hitRecord.textureID, 0);
    EXPECT_EQ(queue.getSorted(1).hitRecord.materialID, 0);
    EXPECT_EQ(queue.getSorted(1).hitRecord.textureID, 1);
    EXPECT_EQ(queue.getSorted(2).hitRecord.materialID, 1);
    EXPECT_EQ(queue.getSorted(2).hitRecord.textureID, 0);
    EXPECT_EQ(queue.getSorted(3).hitRecord.materialID, 1);
    EXPECT_EQ(queue.getSorted(3).hitRecord.textureID, 2);
}

TEST(ShadingQueueTest, SingleEntryScheduleIsStable)
{
    ShadingQueue queue(SchedulingPolicy::MaterialAware);

    queue.add(makeRayState(0), makeHitRecord(5));
    queue.schedule();

    EXPECT_EQ(queue.size(), 1);
    EXPECT_EQ(queue.getSorted(0).hitRecord.materialID, 5);
}

TEST(ShadingQueueTest, AlreadySortedInputIsStable)
{
    ShadingQueue queue(SchedulingPolicy::MaterialAware);

    queue.add(makeRayState(0), makeHitRecord(0));
    queue.add(makeRayState(1), makeHitRecord(1));
    queue.add(makeRayState(2), makeHitRecord(2));
    queue.schedule();

    EXPECT_EQ(queue.getSorted(0).hitRecord.materialID, 0);
    EXPECT_EQ(queue.getSorted(1).hitRecord.materialID, 1);
    EXPECT_EQ(queue.getSorted(2).hitRecord.materialID, 2);
}