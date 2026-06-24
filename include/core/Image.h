#pragma once

#include <vector>
#include <string>
#include <fstream>
#include <algorithm>
#include <math/Vec3.h>

using std::clamp;
using std::cout;
using std::endl;
using std::ofstream;
using std::string;
using std::vector;

// Stores the rendered image as a flat array of colors

struct Image
{
    int width;
    int height;
    vector<Color> pixels;

    Image(int width, int height)
        : width(width), height(height), pixels(width * height, Color(0, 0, 0))
    {
    }

    void setPixel(int x, int y, const Color& color)
    {
        pixels[y * width + x] = color;
    }

    void writePPMFile(const string& filename) const
    {
        ofstream file(filename);

        // PPM header
        file << "P3\n";
        file << width << " " << height << "\n";
        file << "255\n";

        // Write all pixels
        for (const Color& c : pixels)
        {
            int r = static_cast<int>(255 * clamp(c.x, 0.0f, 1.0f));
            int g = static_cast<int>(255 * clamp(c.y, 0.0f, 1.0f));
            int b = static_cast<int>(255 * clamp(c.z, 0.0f, 1.0f));

            file << r << " " << g << " " << b << "\n";
        }

        cout << "Image written to: " << filename << endl;
    }
};