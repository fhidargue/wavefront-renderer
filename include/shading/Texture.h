#pragma once

#include <vector>
#include <algorithm>
#include <cmath>
#include <math/Vec3.h>
#include <math/Random.h>

using std::floor;
using std::min;
using std::vector;

struct Texture
{
    int width;
    int height;
    vector<Color> pixels;

    Texture(int width, int height)
        : width(width), height(height), pixels(width * height, Color(1.0f, 1.0f, 1.0f))
    {
    }

    void setTexel(int x, int y, const Color& color)
    {
        pixels[y * width + x] = color;
    }

    Color sample(float u, float v) const
    {
        u = u - floor(u);
        v = v - floor(v);

        int x = static_cast<int>(u * width);
        int y = static_cast<int>(v * height);

        x = std::max(0, min(x, width - 1));
        y = std::max(0, min(y, height - 1));

        return pixels[y * width + x];
    }
};

// Generates random noise textures for benchmarking
struct TextureGenerator
{
    static Texture generateNoise(int size)
    {
        Texture texture(size, size);

        for (int y = 0; y < size; ++y)
        {
            for (int x = 0; x < size; ++x)
            {
                Color randomColor(randomFloat(), randomFloat(), randomFloat());
                texture.setTexel(x, y, randomColor);
            }
        }

        return texture;
    }
};