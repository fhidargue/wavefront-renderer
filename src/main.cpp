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

using std::cerr;
using std::cout;
using std::endl;
using std::stoi;
using std::string;
using std::vector;

static string derivePreviewPath(const string& outputPath)
{
    auto dot = outputPath.rfind('.');

    if (dot != string::npos)
        return outputPath.substr(0, dot) + "_preview" + outputPath.substr(dot);

    return outputPath + "_preview";
}

int main(int argc, char* argv[])
{
    // CLI flags
    int samplesPerPixelOverride = -1;
    int maxDepthOverride = -1;
    int progressIntervalOverride = -1;
    int imageWidth = -1;
    int imageHeight = -1;
    float fireflyThresholdOverride = -1.0f;
    bool denoiseEnabled = false;
    bool enableCostAwareRR = true;

    // Adaptive sampling
    bool enableAdaptiveSampling = true;
    float adaptiveThresholdOverride = -1.0f;

    // TODO: turn on on havier scene
    bool enableRaySort = false;
    bool quiet = false;
    string policyName;
    string environmentMapPath;

    // Separate size flags from positional args
    vector<string> positional;

    for (int i = 1; i < argc; ++i)
    {
        std::string_view arg = argv[i];

        auto hasValue = [&]() { return i + 1 < argc; };

        if (arg == "--width" && hasValue())
            imageWidth = std::stoi(argv[++i]);
        else if (arg == "--height" && hasValue())
            imageHeight = std::stoi(argv[++i]);
        else if (arg == "--denoise")
            denoiseEnabled = true;
        else if (arg == "--no-cost-rr")
            enableCostAwareRR = false;
        else if (arg == "--no-ray-sort")
            enableRaySort = false;
        else if (arg == "--no-adaptive")
            enableAdaptiveSampling = false;
        else if (arg == "--quiet")
            quiet = true;
        else if (arg == "--env" && hasValue())
            environmentMapPath = argv[++i];
        else if (arg == "--samples" && hasValue())
            samplesPerPixelOverride = std::stoi(argv[++i]);
        else if (arg == "--max-depth" && hasValue())
            maxDepthOverride = std::stoi(argv[++i]);
        else if (arg == "--policy" && hasValue())
            policyName = argv[++i];
        else if (arg == "--progress-interval" && hasValue())
            progressIntervalOverride = std::stoi(argv[++i]);
        else if (arg == "--firefly-threshold" && hasValue())
            fireflyThresholdOverride = std::stof(argv[++i]);
        else
            positional.emplace_back(arg);
    }

    // Rendering variables
    int samplesPerPixel = (samplesPerPixelOverride > 0) ? samplesPerPixelOverride : 256;
    int maxBounceDepth = (maxDepthOverride > 0) ? maxDepthOverride : 8;
    int progressInterval = (progressIntervalOverride >= 0) ? progressIntervalOverride : 4;
    imageWidth = (imageWidth > 0) ? imageWidth : 600;
    imageHeight = (imageHeight > 0) ? imageHeight : 600;

    string usdFilePath = (positional.size() >= 1) ? positional[0] : "";
    string outputPath = (positional.size() >= 2) ? positional[1] : "output/cornellBox.exr";
    string cameraFile = (positional.size() >= 3) ? positional[2] : "";

    Scene scene;

    // Scheduling policy selection
    SchedulingPolicy policy = SchedulingPolicy::MaterialAware;

    if (policyName == "none")
        policy = SchedulingPolicy::None;
    else if (policyName == "material")
        policy = SchedulingPolicy::MaterialAware;
    else if (policyName == "material-parallel")
        policy = SchedulingPolicy::MaterialAwareParallel;
    else if (policyName == "texture")
        policy = SchedulingPolicy::TextureAware;
    else if (!policyName.empty())
        cerr << "WARNING: unknown --policy '" << policyName << "', using default" << endl;

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

    WavefrontRenderer renderer(samplesPerPixel, maxBounceDepth, policy);
    renderer.enableCostAwareRR = enableCostAwareRR;
    renderer.enableRaySort = enableRaySort;
    renderer.enableAdaptiveSampling = enableAdaptiveSampling;
    renderer.environmentMapPath = environmentMapPath;
    renderer.enableSampleLogging = quiet;

    if (adaptiveThresholdOverride > 0.0f)
        renderer.adaptiveSamplingThreshold = adaptiveThresholdOverride;

    if (fireflyThresholdOverride > 0.0f)
        renderer.fireflyThreshold = fireflyThresholdOverride;

    double shadingTimeMs =
        renderer.renderScene(scene, camera, image, previewPath, progressInterval);

    cout << "Rendered: " << imageWidth << "x" << imageHeight << " | " << samplesPerPixel
         << " samples" << " | depth " << maxBounceDepth << endl;

    cout << "Shading time: " << shadingTimeMs << "ms" << endl;

    if (denoiseEnabled)
    {
        if (!Denoiser::denoise(image))
            cout << "Denoise failed, writing noisy image instead." << endl;
    }

    image.write(outputPath, quiet);

    return 0;
}