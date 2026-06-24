#include <gtest/gtest.h>
#include <geometry/Sphere.h>

TEST(SphereTest, RayHitsSphereHeadOn)
{
    Sphere sphere(Point3(0.0f, 0.0f, -1.0f), 0.5f, 0);
    Ray ray(Point3(0.0f, 0.0f, 0.0f), Vec3(0.0f, 0.0f, -1.0f));
    HitRecord record;

    EXPECT_TRUE(sphere.hit(ray, 0.001f, 1e9f, record));
    EXPECT_NEAR(record.distance, 0.5f, 0.0001f);
}

TEST(SphereTest, RayMissesSphere)
{
    Sphere sphere(Point3(0.0f, 0.0f, -1.0f), 0.5f, 0);
    Ray ray(Point3(5.0f, 0.0f, 0.0f), Vec3(0.0f, 0.0f, -1.0f));
    HitRecord record;

    EXPECT_FALSE(sphere.hit(ray, 0.001f, 1e9f, record));
}

TEST(SphereTest, HitRecordStoresMaterialID)
{
    Sphere sphere(Point3(0.0f, 0.0f, -1.0f), 0.5f, 3);
    Ray ray(Point3(0.0f, 0.0f, 0.0f), Vec3(0.0f, 0.0f, -1.0f));
    HitRecord record;

    sphere.hit(ray, 0.001f, 1e9f, record);

    EXPECT_EQ(record.materialID, 3);
}

TEST(SphereTest, FrontFaceNormalPointsTowardRay)
{
    Sphere sphere(Point3(0.0f, 0.0f, -1.0f), 0.5f, 0);
    Ray ray(Point3(0.0f, 0.0f, 0.0f), Vec3(0.0f, 0.0f, -1.0f));
    HitRecord record;

    sphere.hit(ray, 0.001f, 1e9f, record);

    EXPECT_TRUE(record.frontFace);
    EXPECT_GT(record.normal.z, 0.0f);
}

TEST(SphereTest, RayInsideSphereHitsBackFace)
{
    Sphere sphere(Point3(0.0f, 0.0f, 0.0f), 1.0f, 0);
    Ray ray(Point3(0.0f, 0.0f, 0.0f), Vec3(0.0f, 0.0f, -1.0f));
    HitRecord record;

    EXPECT_TRUE(sphere.hit(ray, 0.001f, 1e9f, record));
    EXPECT_FALSE(record.frontFace);
}