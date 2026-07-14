#include <gtest/gtest.h>
#include <shading/Texture.h>

#include <cmath>

#include <OpenImageIO/imageio.h>

using std::string;

namespace
{
string writeSolidColorFloatImage(const string& filename, int width, int height, float r, float g,
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

string writeSolidColorEightBitImage(const string& filename, int width, int height, unsigned char r,
                                    unsigned char g, unsigned char b)
{
    string path = "/tmp/" + filename;

    auto out = OIIO::ImageOutput::create(path);
    OIIO::ImageSpec spec(width, height, 3, OIIO::TypeDesc::UINT8);
    out->open(path, spec);

    std::vector<unsigned char> pixels(static_cast<size_t>(width) * height * 3);
    for (size_t i = 0; i < pixels.size(); i += 3)
    {
        pixels[i + 0] = r;
        pixels[i + 1] = g;
        pixels[i + 2] = b;
    }

    out->write_image(OIIO::TypeDesc::UINT8, pixels.data());
    out->close();

    return path;
}

float referenceSrgbToLinear(float srgb)
{
    if (srgb <= 0.04045f)
        return srgb / 12.92f;

    return std::pow((srgb + 0.055f) / 1.055f, 2.4f);
}
} // namespace

TEST(TextureTest, LoadingNonexistentFileReturnsFalse)
{
    Texture texture;

    EXPECT_FALSE(texture.load("/no/path/texture.png"));
}

TEST(TextureTest, LoadingValidEightBitFileReturnsTrue)
{
    string path = writeSolidColorEightBitImage("textureLoadTest.png", 8, 4, 128, 128, 128);

    Texture texture;

    EXPECT_TRUE(texture.load(path));
}

TEST(TextureTest, ReadsCorrectWidthAndHeight)
{
    string path = writeSolidColorEightBitImage("textureDimensionsTest.png", 16, 9, 128, 128, 128);

    Texture texture;
    ASSERT_TRUE(texture.load(path));

    EXPECT_EQ(texture.width, 16);
    EXPECT_EQ(texture.height, 9);
}

TEST(TextureTest, StoresFilePathAsName)
{
    string path = writeSolidColorEightBitImage("textureNameTest.png", 4, 4, 128, 128, 128);

    Texture texture;
    ASSERT_TRUE(texture.load(path));

    EXPECT_EQ(texture.name, path);
}

TEST(TextureTest, EightBitSourceIsDecodedFromSRGBToLinear)
{
    unsigned char eightBitValue = 191;
    float srgbNormalized = eightBitValue / 255.0f;
    float expectedLinear = referenceSrgbToLinear(srgbNormalized);

    string path = writeSolidColorEightBitImage("textureSrgbDecodeTest.png", 4, 4, eightBitValue,
                                               eightBitValue, eightBitValue);

    Texture texture;
    ASSERT_TRUE(texture.load(path));

    Color sampled = texture.sample(0.5f, 0.5f);

    EXPECT_NEAR(sampled.x, expectedLinear, 0.01f);
    EXPECT_NEAR(sampled.y, expectedLinear, 0.01f);
    EXPECT_NEAR(sampled.z, expectedLinear, 0.01f);
}

TEST(TextureTest, FloatSourceIsNotSRGBDecoded)
{
    string path = writeSolidColorFloatImage("textureFloatNoDecodeTest.exr", 4, 4, 0.5f, 0.5f, 0.5f);

    Texture texture;
    ASSERT_TRUE(texture.load(path));

    Color sampled = texture.sample(0.5f, 0.5f);

    EXPECT_NEAR(sampled.x, 0.5f, 0.01f);
}

TEST(TextureTest, BlackAndWhiteSurviveSRGBDecodeUnchanged)
{
    string blackPath = writeSolidColorEightBitImage("textureBlackTest.png", 4, 4, 0, 0, 0);
    string whitePath = writeSolidColorEightBitImage("textureWhiteTest.png", 4, 4, 255, 255, 255);

    Texture black;
    Texture white;
    ASSERT_TRUE(black.load(blackPath));
    ASSERT_TRUE(white.load(whitePath));

    EXPECT_NEAR(black.sample(0.5f, 0.5f).x, 0.0f, 0.01f);
    EXPECT_NEAR(white.sample(0.5f, 0.5f).x, 1.0f, 0.01f);
}