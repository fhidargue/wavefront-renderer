#pragma once

#include <vector>
#include <core/Scene.h>
#include <core/Camera.h>
#include <core/Image.h>
#include <core/EnvironmentMap.h>
#include <scheduling/RayQueue.h>
#include <scheduling/ShadingQueue.h>
#include <render/CostTracker.h>
#include <render/AdaptiveSampler.h>

// Called every N samples during rendering with current progress
using ProgressCallback = std::function<void(int currentSample, int totalSamples)>;

class WavefrontRenderer
{
  public:
    int samplesPerPixel;
    int maxDepth;
    int rrMinDepth = 3;
    const float PI = 3.14159265f;
    SchedulingPolicy policy;

    // CLI flags
    // Set an empty environment map variable in case we want an HDRI
    std::string environmentMapPath;
    bool enableSampleLogging = true;
    bool enableCostAwareRR = true;
    bool enableRaySort = true;
    bool enableAdaptiveSampling = true;
    bool enableMemoryCoherenceStats = false;
    float adaptiveSamplingThreshold = AdaptiveSamplingConstants::defaultRelativeThreshold;
    float fireflyThreshold = 10.0f;

    WavefrontRenderer(int samples, int maxDepth, SchedulingPolicy policy, int rrMinDepth = 3)
        : samplesPerPixel(samples), maxDepth(maxDepth), policy(policy)
    {
    }

    double renderScene(const Scene& scene, const Camera& camera, Image& image,
                       const std::string& previewPath = "", int progressInterval = 4,
                       ProgressCallback progressCallback = nullptr);

  private:
    CostTracker materialCostTracker{0};
    CostTracker textureCostTracker{0};
    EnvironmentMap environmentMap;

    void generatePrimaryRays(const Camera& camera, int width, int height, RayQueue& queue,
                             const std::vector<bool>* activePixels = nullptr);
    void intersectAll(const RayQueue& inputQueue, const Scene& scene,
                      ShadingQueue& outputShadingQueue, RayQueue& outputMissQueue);
    void shadeAll(ShadingQueue& shadingQueue, const Scene& scene, std::vector<Color>& accumulator,
                  RayQueue& outputNextQueue);
    Color getSkyColor(const Ray& ray) const;
};