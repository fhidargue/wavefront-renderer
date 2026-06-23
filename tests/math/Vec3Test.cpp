#include <gtest/gtest.h>
#include <math/Vec3.h>

TEST(Vec3Test, DefaultConstructorInitialisesToZero)
{
    Vec3 vector;

    EXPECT_EQ(vector.x, 0.0f);
    EXPECT_EQ(vector.y, 0.0f);
    EXPECT_EQ(vector.z, 0.0f);
}

TEST(Vec3Test, ParameterisedConstructorStoresValues)
{
    Vec3 vector(1.0f, 2.0f, 3.0f);

    EXPECT_EQ(vector.x, 1.0f);
    EXPECT_EQ(vector.y, 2.0f);
    EXPECT_EQ(vector.z, 3.0f);
}

TEST(Vec3Test, Addition)
{
    Vec3 result = Vec3(1.0f, 2.0f, 3.0f) + Vec3(4.0f, 5.0f, 6.0f);

    EXPECT_EQ(result.x, 5.0f);
    EXPECT_EQ(result.y, 7.0f);
    EXPECT_EQ(result.z, 9.0f);
}

TEST(Vec3Test, Subtraction)
{
    Vec3 result = Vec3(5.0f, 7.0f, 9.0f) - Vec3(1.0f, 2.0f, 3.0f);

    EXPECT_EQ(result.x, 4.0f);
    EXPECT_EQ(result.y, 5.0f);
    EXPECT_EQ(result.z, 6.0f);
}

TEST(Vec3Test, ScalarMultiplicationRight)
{
    Vec3 result = Vec3(1.0f, 2.0f, 3.0f) * 2.0f;

    EXPECT_EQ(result.x, 2.0f);
    EXPECT_EQ(result.y, 4.0f);
    EXPECT_EQ(result.z, 6.0f);
}

TEST(Vec3Test, ScalarMultiplicationLeft)
{
    Vec3 result = 2.0f * Vec3(1.0f, 2.0f, 3.0f);

    EXPECT_EQ(result.x, 2.0f);
    EXPECT_EQ(result.y, 4.0f);
    EXPECT_EQ(result.z, 6.0f);
}

TEST(Vec3Test, ComponentWiseMultiplication)
{
    Vec3 result = Vec3(2.0f, 3.0f, 4.0f) * Vec3(5.0f, 6.0f, 7.0f);

    EXPECT_EQ(result.x, 10.0f);
    EXPECT_EQ(result.y, 18.0f);
    EXPECT_EQ(result.z, 28.0f);
}

TEST(Vec3Test, DivisionByScalar)
{
    Vec3 result = Vec3(4.0f, 6.0f, 8.0f) / 2.0f;

    EXPECT_EQ(result.x, 2.0f);
    EXPECT_EQ(result.y, 3.0f);
    EXPECT_EQ(result.z, 4.0f);
}

TEST(Vec3Test, LengthOfUnitVector)
{
    EXPECT_NEAR(Vec3(1.0f, 0.0f, 0.0f).length(), 1.0f, 0.0001f);
}

TEST(Vec3Test, LengthOfKnownVector)
{
    EXPECT_NEAR(Vec3(3.0f, 4.0f, 0.0f).length(), 5.0f, 0.0001f);
}

TEST(Vec3Test, DotProduct)
{
    EXPECT_NEAR(Vec3(1.0f, 2.0f, 3.0f).dot(Vec3(4.0f, 5.0f, 6.0f)), 32.0f, 0.0001f);
}

TEST(Vec3Test, DotProductOfPerpendicularVectorsIsZero)
{
    EXPECT_NEAR(Vec3(0.0f, 1.0f, 0.0f).dot(Vec3(1.0f, 0.0f, 0.0f)), 0.0f, 0.0001f);
}

TEST(Vec3Test, CrossProductOfStandardBasisVectors)
{
    Vec3 result = Vec3(1.0f, 0.0f, 0.0f).cross(Vec3(0.0f, 1.0f, 0.0f));

    EXPECT_NEAR(result.x, 0.0f, 0.0001f);
    EXPECT_NEAR(result.y, 0.0f, 0.0001f);
    EXPECT_NEAR(result.z, 1.0f, 0.0001f);
}

TEST(Vec3Test, NormalisedVectorHasLengthOne)
{
    EXPECT_NEAR(Vec3(3.0f, 4.0f, 0.0f).normalized().length(), 1.0f, 0.0001f);
}

TEST(Vec3Test, NormalisedPreservesDirection)
{
    Vec3 result = Vec3(0.0f, 5.0f, 0.0f).normalized();

    EXPECT_NEAR(result.x, 0.0f, 0.0001f);
    EXPECT_NEAR(result.y, 1.0f, 0.0001f);
    EXPECT_NEAR(result.z, 0.0f, 0.0001f);
}