#include <core/ImageFileLoader.h>

#include <iostream>

#include <OpenImageIO/imageio.h>

using std::cerr;
using std::endl;
using std::string;

bool loadImageRGB(const string& filePath, LoadedImageRGB& outImage, const string& loggingLabel)
{
    auto input = OIIO::ImageInput::open(filePath);

    if (!input)
    {
        cerr << "WARNING: Could not open " << loggingLabel << ": " << filePath << endl;
        return false;
    }

    const OIIO::ImageSpec& specification = input->spec();
    outImage.width = specification.width;
    outImage.height = specification.height;
    outImage.isEightBitSource = (specification.format == OIIO::TypeDesc::UINT8);

    outImage.pixels.resize(static_cast<size_t>(outImage.width) * outImage.height * 3);
    bool ok = input->read_image(0, 0, 0, 3, OIIO::TypeDesc::FLOAT, outImage.pixels.data());
    input->close();

    if (!ok)
        cerr << "WARNING: Failed to read " << loggingLabel << " pixels: " << filePath << endl;

    return ok;
}