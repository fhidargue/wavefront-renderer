#include <gtest/gtest.h>
#include <geometry/Triangle.h>

TEST(TriangleTest, RayHitsTriangleDeadCentre)
{
    Point3 v0(-1.0f, 0.0f, 0.0f);
    Point3 v1(1.0f, 0.0f, 0.0f);
    Point3 v2(0.0f, 1.0f, 0.0f);
    Ray ray(Point3(0.0f, 0.3f, 1.0f), Vec3(0.0f, 0.0f, -1.0f));

    float outDistance, outU, outV;
    bool hit = intersectTriangle(ray, v0, v1, v2, 0.001f, 1e9f, outDistance, outU, outV);

    EXPECT_TRUE(hit);
    EXPECT_NEAR(outDistance, 1.0f, 0.0001f);
}

TEST(TriangleTest, RayMissesTriangle)
{
    Point3 v0(-1.0f, 0.0f, 0.0f);
    Point3 v1(1.0f, 0.0f, 0.0f);
    Point3 v2(0.0f, 1.0f, 0.0f);
    Ray ray(Point3(5.0f, 5.0f, 1.0f), Vec3(0.0f, 0.0f, -1.0f));

    float outDistance, outU, outV;
    bool hit = intersectTriangle(ray, v0, v1, v2, 0.001f, 1e9f, outDistance, outU, outV);

    EXPECT_FALSE(hit);
}

TEST(TriangleTest, RayParallelToTriangleMisses)
{
    Point3 v0(-1.0f, 0.0f, 0.0f);
    Point3 v1(1.0f, 0.0f, 0.0f);
    Point3 v2(0.0f, 1.0f, 0.0f);
    Ray ray(Point3(0.0f, 0.3f, 1.0f), Vec3(1.0f, 0.0f, 0.0f));

    float outDistance, outU, outV;
    bool hit = intersectTriangle(ray, v0, v1, v2, 0.001f, 1e9f, outDistance, outU, outV);

    EXPECT_FALSE(hit);
}

TEST(TriangleTest, HitBeyondMaxDistanceReturnsFalse)
{
    Point3 v0(-1.0f, 0.0f, 0.0f);
    Point3 v1(1.0f, 0.0f, 0.0f);
    Point3 v2(0.0f, 1.0f, 0.0f);
    Ray ray(Point3(0.0f, 0.3f, 5.0f), Vec3(0.0f, 0.0f, -1.0f));

    float outDistance, outU, outV;
    bool hit = intersectTriangle(ray, v0, v1, v2, 0.001f, 2.0f, outDistance, outU, outV);

    EXPECT_FALSE(hit);
}