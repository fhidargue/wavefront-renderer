#include <iostream>
#include <string>
#include <math/Vec3.h>
#include <core/Camera.h>
#include <core/Image.h>
#include <core/Scene.h>
#include <shading/Material.h>
#include <render/WavefrontRenderer.h>
#include <scene/CornellBox.h>

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
    const int samplesPerPixel = 256;
    const int maxBounceDepth = 8;

    Scene scene;

    if (argc >= 2)
    {
#ifdef ENABLE_USD
        string path = argv[1];
        cout << "Loading USD scene: " << path << endl;
        scene = UsdSceneLoader::load(path);
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

    Camera camera(Point3(0.0f, 5.0f, 15.0f), Point3(0.0f, 5.0f, -2.0f), Vec3(0.0f, 1.0f, 0.0f),
                  53.0f, imageWidth, imageHeight);

    Image image(imageWidth, imageHeight);

    cout << "Rendering: " << imageWidth << "x" << imageHeight << " | " << samplesPerPixel
         << " samples | depth " << maxBounceDepth << endl;

    string outputPath  = (argc >= 3) ? argv[2] : "output/cornellBox.exr";
    string previewPath = derivePreviewPath(outputPath);

    WavefrontRenderer renderer(samplesPerPixel, maxBounceDepth, SchedulingPolicy::MaterialAware);

    double shadingTimeMs = renderer.renderScene(scene, camera, image, previewPath, 4);

    cout << "Shading time: " << shadingTimeMs << "ms" << endl;

    image.write(outputPath);

    return 0;
}