#include <iostream>
#include <string>
#include <math/Vec3.h>
#include <core/Camera.h>
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
    const int imageWidth = 600;
    const int imageHeight = 600;
    const int samplesPerPixel = 512;
    const int maxBounceDepth = 8;
    const int progressInterval = 4;

    string usdFilePath = (argc >= 2) ? argv[1] : "";
    string outputPath = (argc >= 3) ? argv[2] : "output/cornellBox.exr";
    string cameraFile = (argc >= 4) ? argv[3] : "";

    Scene scene;

    if (!usdFilePath.empty())
    {
#ifdef ENABLE_USD
        cout << "Loading USD scene: " << usdFilePath << endl;
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

    cout << "\nBuilding BVH..." << endl;
    scene.buildAccelerator();
    cout << "BVH built successfully\n" << endl;

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
         << " samples"
         << " | depth " << maxBounceDepth << endl;

    WavefrontRenderer renderer(samplesPerPixel, maxBounceDepth, SchedulingPolicy::MaterialAware);

    double shadingTimeMs =
        renderer.renderScene(scene, camera, image, previewPath, progressInterval);

    cout << "Shading time: " << shadingTimeMs << "ms" << endl;

    image.write(outputPath);

    return 0;
}