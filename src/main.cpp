#include <iostream>
#include <string>
#include <math/Vec3.h>
#include <core/Camera.h>
#include <core/Image.h>
#include <core/Scene.h>
#include <shading/Material.h>
#include <render/WavefrontRenderer.h>
#include <scene/CornellBox.h>
#include <scene/JsonSceneLoader.h>

using std::cout;
using std::endl;
using std::srand;
using std::string;

int main(int argc, char* argv[]) {
    const int imageWidth = 600;
    const int imageHeight = 600;
    const int samplesPerPixel = 64;
    const int maxBounceDepth = 8;

    srand(42);

    Scene scene;

    if (argc >= 2) {
        string jsonPath = argv[1];
        scene = JsonSceneLoader::load(jsonPath);
    } else {
        cout << "No scene file provided. Rendering Cornell Box" << endl;
        scene = CornellBox::build();
    }

    cout << "\nBuilding BVH..." << endl;
    scene.buildAccelerator();
    cout << "BVH built successfully\n" << endl;

    Camera camera(
        Point3(0.0f, 5.0f, 15.0f),
        Point3(0.0f, 5.0f, -2.0f),
        Vec3(0.0f, 1.0f, 0.0f),
        53.0f,
        imageWidth,
        imageHeight
    );

    Image image(imageWidth,imageHeight);

    cout << "Rendering: " << imageWidth << "x" << imageHeight
         << " | " << samplesPerPixel << " samples | depth "
         << maxBounceDepth << endl;

    WavefrontRenderer renderer(samplesPerPixel,maxBounceDepth, 
        SchedulingPolicy::MaterialAware);

    double shadingTimeMs = renderer.renderScene(scene,camera,image);

    cout << "Shading time: " << shadingTimeMs << "ms" << endl;

    string outputPath=(argc>=3) ? argv[2] : "../output/render.ppm";
    image.writePPMFile(outputPath);

    return 0;
}