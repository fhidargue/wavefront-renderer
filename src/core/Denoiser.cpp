#include <core/Denoiser.h>

#include <iostream>
#include <vector>

#include <OpenImageDenoise/oidn.hpp>

using std::cerr;
using std::cout;
using std::endl;
using std::vector;

bool Denoiser::denoise(Image& image)
{
    oidn::DeviceRef device = oidn::newDevice();
    device.commit();

    const char* deviceError;

    if (device.getError(deviceError) != oidn::Error::None)
    {
        cerr << "Could not create device: " << deviceError << endl;
        return false;
    }

    const size_t pixelCount = static_cast<size_t>(image.width) * image.height;

    // We need a float3 buffer for OIDN
    vector<float> colorBuffer(pixelCount * 3);

    for (size_t i = 0; i < pixelCount; ++i)
    {
        colorBuffer[i * 3 + 0] = image.pixels[i].x;
        colorBuffer[i * 3 + 1] = image.pixels[i].y;
        colorBuffer[i * 3 + 2] = image.pixels[i].z;
    }

    oidn::FilterRef filter = device.newFilter("RT");
    filter.setImage("color", colorBuffer.data(), oidn::Format::Float3, image.width, image.height);
    filter.setImage("output", colorBuffer.data(), oidn::Format::Float3, image.width, image.height);
    filter.set("hdr", true);
    filter.commit();

    filter.execute();

    const char* filterError;

    if (device.getError(filterError) != oidn::Error::None)
    {
        cerr << "OIDN Denoise failed: " << filterError << endl;
        return false;
    }

    for (size_t i = 0; i < pixelCount; ++i)
    {
        image.pixels[i].x = colorBuffer[i * 3 + 0];
        image.pixels[i].y = colorBuffer[i * 3 + 1];
        image.pixels[i].z = colorBuffer[i * 3 + 2];
    }

    cout << "OIDN Denoise complete" << endl;

    return true;
}
