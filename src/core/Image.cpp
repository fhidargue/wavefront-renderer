#include <core/Image.h>

#include <iostream>
#include <vector>

#include <OpenImageIO/imageio.h>

using std::cerr;
using std::cout;
using std::endl;
using std::string;
using std::vector;
using namespace OpenImageIO;

void Image::write(const std::string& filePath, bool applyColorTransform) const
{
    vector<float> buffer(width * height * 3);

    for (int i = 0; i < width * height; ++i)
    {
        buffer[i * 3 + 0] = pixels[i].x;
        buffer[i * 3 + 1] = pixels[i].y;
        buffer[i * 3 + 2] = pixels[i].z;
    }

    auto out = ImageOutput::create(filePath);
    
    if (!out)
    {
        cerr << "ERROR: Could not create image output for: " << filePath << endl;
        return;
    }

    ImageSpec spec(width, height, 3, TypeDesc::FLOAT);
    spec.attribute("compression", "zip");
    spec.attribute("Software", "Wavefront Renderer - MSc Thesis");

    out->open(filePath, spec);
    out->write_image(TypeDesc::FLOAT, buffer.data());
    out->close();

    cout << "Image written: " << filePath
         << " (" << width << "x" << height << ")" << endl;
}