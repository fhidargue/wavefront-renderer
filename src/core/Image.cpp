#include <core/Image.h>

#include <iostream>
#include <vector>

#include <OpenImageIO/imageio.h>

using std::cerr;
using std::cout;
using std::endl;
using std::string;
using std::vector;

void Image::write(const string& filePath, bool enableSampleLogging) const
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

    if (!enableSampleLogging)
        cout << "Image written: " << filePath << " (" << width << "x" << height << ")" << endl;
}

void Image::writePreview(const string& filePath) const
{
    // Preview PNG
    vector<uint8_t> buffer(width * height * 3);

    for (int i = 0; i < width * height; ++i)
    {
        // Gamma 2.2 correction
        auto gammaCorrect = [](float v) -> uint8_t
        {
            v = std::max(0.0f, std::min(1.0f, v));
            return static_cast<uint8_t>(std::pow(v, 1.0f / 2.2f) * 255.0f + 0.5f);
        };

        buffer[i * 3 + 0] = gammaCorrect(pixels[i].x);
        buffer[i * 3 + 1] = gammaCorrect(pixels[i].y);
        buffer[i * 3 + 2] = gammaCorrect(pixels[i].z);
    }

    auto out = OIIO::ImageOutput::create(filePath);
    if (!out)
        return;

    OIIO::ImageSpec spec(width, height, 3, OIIO::TypeDesc::UINT8);
    out->open(filePath, spec);
    out->write_image(OIIO::TypeDesc::UINT8, buffer.data());
    out->close();
}