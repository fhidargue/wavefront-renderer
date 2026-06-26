#include <core/Image.h>

#include <iostream>
#include <vector>

#include <OpenImageIO/imageio.h>

using std::cerr;
using std::cout;
using std::endl;
using std::string;
using std::vector;

void Image::write(const std::string& filePath, bool applyColorTransform) const
{
    vector<float> buffer(width * height * 3);

    for (int i = 0; i < width * height; ++i)
    {
        buffer[i * 3 + 0] = pixels[i].x;
        buffer[i * 3 + 1] = pixels[i].y;
        buffer[i * 3 + 2] = pixels[i].z;
    }

    auto out = OIIO::ImageOutput::create(filePath);

    if (!out)
    {
        cerr << "ERROR: Could not create image output for: " << filePath << endl;
        return;
    }

    OIIO::ImageSpec spec(width, height, 3, OIIO::TypeDesc::FLOAT);
    spec.attribute("compression", "zip");
    spec.attribute("Software", "Wavefront Renderer - MSc Thesis");

    out->open(filePath, spec);
    out->write_image(OIIO::TypeDesc::FLOAT, buffer.data());
    out->close();

    cout << "Image written: " << filePath << " (" << width << "x" << height << ")" << endl;
}