#include <core/EnvironmentMap.h>
#include <core/ImageFileLoader.h>

#include <cmath>
#include <iostream>

#include <OpenImageIO/imageio.h>

using std::cerr;
using std::cout;
using std::endl;
using std::string;

bool EnvironmentMap::load(const string& filePath)
{
    LoadedImageRGB image;

    if (!loadImageRGB(filePath, image, "environment map"))
        return false;

    width = image.width;
    height = image.height;
    pixels = std::move(image.pixels);

    loaded = true;
    cout << "Environment map loaded: " << filePath << " (" << width << "x" << height << ")" << endl;

    return true;
}

Color EnvironmentMap::sample(const Vec3& direction) const
{
    if (!loaded)
        return Color(0.0f, 0.0f, 0.0f);

    Vec3 d = direction.normalized();

    // UV mapping
    float u = 0.5f + std::atan2(d.z, d.x) / (2.0f * static_cast<float>(M_PI));
    float v = 0.5f - std::asin(std::max(-1.0f, std::min(1.0f, d.y))) / static_cast<float>(M_PI);

    int x = std::min(width - 1, std::max(0, static_cast<int>(u * width)));
    int y = std::min(height - 1, std::max(0, static_cast<int>(v * height)));

    size_t index = (static_cast<size_t>(y) * width + x) * 3;

    Color raw(pixels[index + 0], pixels[index + 1], pixels[index + 2]);

    return raw * intensityScale * tint;
};