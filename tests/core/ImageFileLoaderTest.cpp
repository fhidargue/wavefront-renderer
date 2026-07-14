#include <gtest/gtest.h>
#include <core/ImageFileLoader.h>

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
} // namespace

TEST(ImageFileLoaderTest, LoadingNonexistentFileReturnsFalse)
{
    LoadedImageRGB image;

    EXPECT_FALSE(loadImageRGB("/no/path/texture.png", image, "texture"));
}

TEST(ImageFileLoaderTest, LoadingValidFloatFileReturnsTrue)
{
    string path = writeSolidColorFloatImage("loaderFloatTest.exr", 8, 4, 0.5f, 0.5f, 0.5f);

    LoadedImageRGB image;

    EXPECT_TRUE(loadImageRGB(path, image, "texture"));
}

TEST(ImageFileLoaderTest, ReadsCorrectWidthAndHeight)
{
    string path = writeSolidColorFloatImage("loaderDimensionsTest.exr", 16, 9, 0.1f, 0.2f, 0.3f);

    LoadedImageRGB image;
    ASSERT_TRUE(loadImageRGB(path, image, "texture"));

    EXPECT_EQ(image.width, 16);
    EXPECT_EQ(image.height, 9);
}

TEST(ImageFileLoaderTest, PixelBufferSizeIsWidthTimesHeightTimesThree)
{
    string path = writeSolidColorFloatImage("loaderBufferSizeTest.exr", 8, 4, 0.1f, 0.2f, 0.3f);

    LoadedImageRGB image;
    ASSERT_TRUE(loadImageRGB(path, image, "texture"));

    EXPECT_EQ(image.pixels.size(), static_cast<size_t>(8 * 4 * 3));
}

TEST(ImageFileLoaderTest, ReadsAuthoredFloatPixelValues)
{
    string path = writeSolidColorFloatImage("loaderFloatValuesTest.exr", 4, 4, 0.2f, 0.4f, 0.8f);

    LoadedImageRGB image;
    ASSERT_TRUE(loadImageRGB(path, image, "texture"));

    EXPECT_NEAR(image.pixels[0], 0.2f, 0.01f);
    EXPECT_NEAR(image.pixels[1], 0.4f, 0.01f);
    EXPECT_NEAR(image.pixels[2], 0.8f, 0.01f);
}

TEST(ImageFileLoaderTest, FloatSourceIsNotFlaggedEightBit)
{
    string path = writeSolidColorFloatImage("loaderFloatFlagTest.exr", 4, 4, 0.5f, 0.5f, 0.5f);

    LoadedImageRGB image;
    ASSERT_TRUE(loadImageRGB(path, image, "texture"));

    EXPECT_FALSE(image.isEightBitSource);
}

TEST(ImageFileLoaderTest, EightBitSourceIsFlaggedEightBit)
{
    string path = writeSolidColorEightBitImage("loaderEightBitFlagTest.png", 4, 4, 128, 128, 128);

    LoadedImageRGB image;
    ASSERT_TRUE(loadImageRGB(path, image, "texture"));

    EXPECT_TRUE(image.isEightBitSource);
}

TEST(ImageFileLoaderTest, ReadsApproximateEightBitPixelValues)
{
    string path = writeSolidColorEightBitImage("loaderEightBitValuesTest.png", 4, 4, 191, 191, 191);

    LoadedImageRGB image;
    ASSERT_TRUE(loadImageRGB(path, image, "texture"));

    EXPECT_NEAR(image.pixels[0], 191.0f / 255.0f, 0.01f);
}