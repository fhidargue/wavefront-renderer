#include <gtest/gtest.h>
#include <core/EnvironmentMap.h>

#include <OpenImageIO/imageio.h>

using std::string;

static string writeSolidColorImage(const string& filename, int width, int height, float r, float g,
                                   float b)
{
    string path = "/tmp/" + filename;

    auto out = OIIO::ImageOutput::create(path);
    OIIO::ImageSpec spec(width, height, 3, OIIO::TypeDesc::FLOAT);
    out->open(path, spec);

    std::vector<float> pixels(static_cast<size_t>(width) * height * 3);
    for (size_t i = 0; i < pixels.size(); i += 3)
    {
        pixels[i + 0] = r;
        pixels[i + 1] = g;
        pixels[i + 2] = b;
    }

    out->write_image(OIIO::TypeDesc::FLOAT, pixels.data());
    out->close();

    return path;
}

TEST(EnvironmentMapTest, LoadingNonexistentFileReturnsFalse)
{
    EnvironmentMap env;

    EXPECT_FALSE(env.load("/no/path/env.exr"));
    EXPECT_FALSE(env.isLoaded());
}

TEST(EnvironmentMapTest, SampleOnUnloadedMapReturnsBlack)
{
    EnvironmentMap env;

    Color result = env.sample(Vec3(0.0f, 1.0f, 0.0f));

    EXPECT_FLOAT_EQ(result.x, 0.0f);
    EXPECT_FLOAT_EQ(result.y, 0.0f);
    EXPECT_FLOAT_EQ(result.z, 0.0f);
}

TEST(EnvironmentMapTest, LoadingValidFileReturnsTrue)
{
    string path = writeSolidColorImage("envSolidTest.exr", 8, 4, 0.5f, 0.5f, 0.5f);

    EnvironmentMap env;

    EXPECT_TRUE(env.load(path));
    EXPECT_TRUE(env.isLoaded());
}

TEST(EnvironmentMapTest, SampleReturnsAuthoredColorForSolidImage)
{
    string path = writeSolidColorImage("envColorTest.exr", 8, 4, 0.2f, 0.4f, 0.8f);

    EnvironmentMap env;
    ASSERT_TRUE(env.load(path));

    // A solid color environment should return the same color no matter direction
    Color up = env.sample(Vec3(0.0f, 1.0f, 0.0f));
    Color side = env.sample(Vec3(1.0f, 0.0f, 0.0f));
    Color down = env.sample(Vec3(0.0f, -1.0f, 0.0f));

    EXPECT_NEAR(up.x, 0.2f, 0.01f);
    EXPECT_NEAR(up.y, 0.4f, 0.01f);
    EXPECT_NEAR(up.z, 0.8f, 0.01f);

    EXPECT_NEAR(side.x, 0.2f, 0.01f);
    EXPECT_NEAR(down.x, 0.2f, 0.01f);
}

TEST(EnvironmentMapTest, IntensityScaleMultipliesSampledColor)
{
    string path = writeSolidColorImage("envIntensityTest.exr", 8, 4, 1.0f, 1.0f, 1.0f);

    EnvironmentMap env;
    ASSERT_TRUE(env.load(path));
    env.intensityScale = 2.0f;

    Color result = env.sample(Vec3(0.0f, 1.0f, 0.0f));

    EXPECT_NEAR(result.x, 2.0f, 0.01f);
}

TEST(EnvironmentMapTest, TintMultipliesSampledColor)
{
    string path = writeSolidColorImage("envTintTest.exr", 8, 4, 1.0f, 1.0f, 1.0f);

    EnvironmentMap env;
    ASSERT_TRUE(env.load(path));
    env.tint = Color(1.0f, 0.0f, 0.0f);

    Color result = env.sample(Vec3(0.0f, 1.0f, 0.0f));

    EXPECT_NEAR(result.x, 1.0f, 0.01f);
    EXPECT_NEAR(result.y, 0.0f, 0.01f);
    EXPECT_NEAR(result.z, 0.0f, 0.01f);
}

TEST(EnvironmentMapTest, DirectionIsNormalizedBeforeSampling)
{
    string path = writeSolidColorImage("envNormalizeTest.exr", 8, 4, 0.3f, 0.3f, 0.3f);

    EnvironmentMap env;
    ASSERT_TRUE(env.load(path));

    // A non unit length direction should still sample correctly
    Color result = env.sample(Vec3(0.0f, 5.0f, 0.0f));

    EXPECT_NEAR(result.x, 0.3f, 0.01f);
}