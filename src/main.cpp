#include <iostream>
#include <chrono>
#include <vector>
#include <numeric>
#include <math/Vec3.h>
#include <core/Camera.h>
#include <core/Image.h>
#include <core/Scene.h>
#include <shading/Material.h>
#include <render/WavefrontRenderer.h>
#include <scene_io/SceneGenerator.h>
#include <scene_io/CornellBox.h>

using std::cout;
using std::endl;
using std::srand;
using std::string;

int main() {
    const int imageWidth = 600;
    const int imageHeight = 600;
    const int samplesPerPixel = 64;
    const int maxBounceDepth = 8;

    srand(42);

    Scene scene = CornellBox::build();

    Image image(imageWidth, imageHeight);
    Camera camera(
        Point3(0.0f, 5.0f, 14.0f),
        Point3(0.0f, 5.0f, 0.0f), 
        Vec3(0.0f, 1.0f, 0.0f),   
        55.0f, 
        imageWidth, imageHeight
    );

    cout << "Rendering Cornell Box: " << imageWidth << "x" << imageHeight << endl;

    WavefrontRenderer renderer(samplesPerPixel, maxBounceDepth, SchedulingPolicy::None);
    double shadingTimeMs = renderer.renderScene(scene, camera, image);

    cout << "Shading time: " << shadingTimeMs << "ms" << endl;

    image.writePPMFile("../output/cornell_box.ppm");

    return 0;
}