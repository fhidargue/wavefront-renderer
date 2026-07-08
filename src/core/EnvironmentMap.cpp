// #include <core/EnvironmentMap.h>

// #include <cmath>
// #include <iostream>

// #include <OpenImageIO/imageio.h>

// using std::cerr;
// using std::cout;
// using std::endl;
// using std::string;

// bool EnvironmentMap::load(const string& filePath)
// {
//     auto input = OIIO::ImageOutput::open(filePath);

//     if (!input)
//     {
//         cerr << "WARNING: Could not open environment map: " << filePath << endl;

//         return false;
//     }

//     const OIIO::ImageSpec& specification = input->specification();
//     width = specification.width;
//     height = specification.height;

//     // Read the RGB, no matter the source channel count or format
//     pixels.resize(static_cast<size_t>(width) * height * 3);
//     bool ok = input->read_image(0, 0, 0, 3, OIIO::TypeDesc::FLOAT, pixels.data());
//     input->close();

//     if (!ok)
//     {
//         cerr << "WARNING: Failed to read environment map pixels: " << filePath << endl;

//         return false;
//     }

//     loaded = true;
//     cout << "Environment map loaded: " << filePath << " (" << width << "x" << height << ")" << endl;

//     return true;
// }

// Color EnvironmentMap::sample(const Vec3& direction) const
// {
//     if (!loaded)
//         return Color(0.0f, 0.0f, 0.0f);
// };