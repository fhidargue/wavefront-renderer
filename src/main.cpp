#include <iostream>
#include <string>
#include <math/Vec3.h>
#include <core/Camera.h>
#include <core/Denoiser.h>
#include <core/Image.h>
#include <core/Scene.h>
#include <shading/Material.h>
#include <render/WavefrontRenderer.h>
#include <scene/CornellBox.h>
#include <scene/CameraLoader.h>

#ifdef ENABLE_USD
#include <scene/UsdSceneLoader.h>
#endif

using std::cout;
using std::endl;
using std::stoi;
using std::string;

static string derivePreviewPath(const string& outputPath)
{
    auto dot = outputPath.rfind('.');

    if (dot != string::npos)
        return outputPath.substr(0, dot) + "_preview" + outputPath.substr(dot);

    return outputPath + "_preview";
}

int main(int argc, char* argv[])
{
    const int samplesPerPixel = 256;
    const int maxBounceDepth = 8;
    const int progressInterval = 4;
    int imageWidth = 600;
    int imageHeight = 600;
    bool denoiseEnabled = false;
    bool useCostAwareRR = true;
    string environmentMapPath;

    // Separate size flags from positional args
    std::vector<string> positional;

    for (int i = 1; i < argc; ++i)
    {
        string arg = argv[i];
        if (arg == "--width" && i + 1 < argc)
            imageWidth = std::stoi(argv[++i]);
        else if (arg == "--height" && i + 1 < argc)
            imageHeight = std::stoi(argv[++i]);
        else if (arg == "--denoise")
            denoiseEnabled = true;
        else if (arg == "--no-cost-rr")
            useCostAwareRR = false;
        else if (arg == "--env" && i + 1 < argc)
            environmentMapPath = argv[++i];
        else
            positional.push_back(arg);
    }

    string usdFilePath = (positional.size() >= 1) ? positional[0] : "";
    string outputPath = (positional.size() >= 2) ? positional[1] : "output/cornellBox.exr";
    string cameraFile = (positional.size() >= 3) ? positional[2] : "";

    Scene scene;

    if (!usdFilePath.empty())
    {
#ifdef ENABLE_USD
        cout << "\nLoading USD scene: " << usdFilePath << endl;
        scene = UsdSceneLoader::load(usdFilePath);
#else
        cout << "USD support not compiled in. Rendering Cornell Box." << endl;
        scene = CornellBox::build();
#endif
    }
    else
    {
        cout << "No scene file provided. Rendering Cornell Box" << endl;
        scene = CornellBox::build();
    }

    // Build Embree BVH
    scene.buildAccelerator();

    Camera camera = [&]() -> Camera
    {
        if (!cameraFile.empty())
            return CameraLoader::load(cameraFile, imageWidth, imageHeight);

        bool kitchenSet = usdFilePath.find("Kitchen") != string::npos ||
                          usdFilePath.find("kitchen") != string::npos;

        if (kitchenSet)
            return CameraLoader::defaultKitchenSet(imageWidth, imageHeight);

        return CameraLoader::defaultCornellBox(imageWidth, imageHeight);
    }();

    Image image(imageWidth, imageHeight);
    string previewPath = derivePreviewPath(outputPath);

    cout << "Rendering: " << imageWidth << "x" << imageHeight << " | " << samplesPerPixel
         << " samples" << " | depth " << maxBounceDepth << endl;

    WavefrontRenderer renderer(samplesPerPixel, maxBounceDepth, SchedulingPolicy::MaterialAware);
    renderer.useCostAwareRR = useCostAwareRR;
    renderer.environmentMapPath = environmentMapPath;

    double shadingTimeMs =
        renderer.renderScene(scene, camera, image, previewPath, progressInterval);

    cout << "Shading time: " << shadingTimeMs << "ms" << endl;

    if (denoiseEnabled)
    {
        if (!Denoiser::denoise(image))
            cout << "Denoise failed, writing noisy image instead." << endl;
    }

    image.write(outputPath);

    return 0;
}