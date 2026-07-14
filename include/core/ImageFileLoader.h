#pragma once

#include <string>
#include <vector>

struct LoadedImageRGB
{
    std::vector<float> pixels;
    int width = 0;
    int height = 0;
    bool isEightBitSource = false;
};

bool loadImageRGB(const std::string& filePath, LoadedImageRGB& outImage,
                  const std::string& loggingLabel);