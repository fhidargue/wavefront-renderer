#include <shading/Texture.h>
#include <core/ImageFileLoader.h>

#include <cmath>
#include <iostream>

using std::cout;
using std::endl;
using std::string;

namespace
{
// Physically Based Rendering - Gamma Correction and Image Formats
constexpr float SRGB_LINEAR_SEGMENT_THRESHOLD = 0.04045f;
constexpr float SRGB_LINEAR_SEGMENT_SLOPE = 12.92f;
constexpr float SRGB_GAMMA_SEGMENT_OFFSET = 0.055f;
constexpr float SRGB_GAMMA_SEGMENT_SCALE = 1.055f;
constexpr float SRGB_GAMMA_SEGMENT_EXPONENT = 2.4f;

float srgbToLinear(float srgb)
{
    if (srgb <= SRGB_LINEAR_SEGMENT_THRESHOLD)
        return srgb / SRGB_LINEAR_SEGMENT_SLOPE;

    return std::pow((srgb + SRGB_GAMMA_SEGMENT_OFFSET) / SRGB_GAMMA_SEGMENT_SCALE,
                    SRGB_GAMMA_SEGMENT_EXPONENT);
}
} // namespace

bool Texture::load(const string& filePath)
{
    LoadedImageRGB image;

    if (!loadImageRGB(filePath, image, "texture"))
        return false;

    width = image.width;
    height = image.height;
    pixels.resize(static_cast<size_t>(width) * height);

    for (size_t i = 0; i < pixels.size(); ++i)
    {
        float r = image.pixels[i * 3 + 0];
        float g = image.pixels[i * 3 + 1];
        float b = image.pixels[i * 3 + 2];

        if (image.isEightBitSource)
        {
            r = srgbToLinear(r);
            g = srgbToLinear(g);
            b = srgbToLinear(b);
        }

        pixels[i] = Color(r, g, b);
    }

    name = filePath;
    cout << "Texture loaded: " << filePath << " (" << width << "x" << height << ")" << endl;

    return true;
}