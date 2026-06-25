#pragma once

#include <vector>
#include <string>
#include <math/Vec3.h>

// Stores the rendered image as linear floating point pixels.
// Handled by OpenColorIO.
struct Image
{
    int width;
    int height;
    std::vector<Color> pixels;

    Image(int width, int height)
        : width(width), height(height), pixels(width * height, Color(0.0f, 0.0f, 0.0f))
    {
    }

    int pixelIndex(int x, int y) const
    {
        return y * width + x;
    }

    void write(const std::string& filePath, bool applyColorTransform = true) const;
};