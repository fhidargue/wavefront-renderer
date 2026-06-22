#include <gtest/gtest.h>
#include <core/Ray.h>

TEST(RayTest, StoresOriginAndDirection) {
    Ray ray(Point3(0.0f, 0.0f, 0.0f), Vec3(0.0f, 0.0f, -1.0f));

    EXPECT_EQ(ray.origin.x, 0.0f);
    EXPECT_EQ(ray.origin.y, 0.0f);
    EXPECT_EQ(ray.origin.z, 0.0f);
    EXPECT_EQ(ray.direction.z, -1.0f);
}

TEST(RayTest, AtReturnsCorrectPointAlongRay) {
    Ray ray(Point3(1.0f, 0.0f, 0.0f), Vec3(1.0f, 0.0f, 0.0f));
    Point3 result = ray.at(2.0f);

    EXPECT_EQ(result.x, 3.0f);
    EXPECT_EQ(result.y, 0.0f);
    EXPECT_EQ(result.z, 0.0f);
}

TEST(RayTest, AtWithDistanceZeroReturnsOrigin) {
    Ray ray(Point3(5.0f, 3.0f, 1.0f), Vec3(0.0f, 1.0f, 0.0f));
    Point3 result = ray.at(0.0f);

    EXPECT_EQ(result.x, 5.0f);
    EXPECT_EQ(result.y, 3.0f);
    EXPECT_EQ(result.z, 1.0f);
}