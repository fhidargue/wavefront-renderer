#pragma once

#include <vector>
#include <core/Scene.h>
#include <core/Camera.h>
#include <core/Image.h>
#include <scheduling/RayQueue.h>
#include <scheduling/ShadingQueue.h>

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

    WavefrontRenderer(int samples, int maxDepth, SchedulingPolicy policy, int rrMinDepth = 3)
        : samplesPerPixel(samples), maxDepth(maxDepth), policy(policy)
    {
    }

    double renderScene(const Scene& scene, const Camera& camera, Image& image,
                       const std::string& previewPath = "", int progressInterval = 4,
                       ProgressCallback progressCallback = nullptr);

  private:
    void generatePrimaryRays(const Camera& camera, int width, int height, RayQueue& queue);
    void intersectAll(const RayQueue& inputQueue, const Scene& scene,
                      ShadingQueue& outputShadingQueue, RayQueue& outputMissQueue);
    void shadeAll(ShadingQueue& shadingQueue, const Scene& scene, std::vector<Color>& accumulator,
                  RayQueue& outputNextQueue);
    Color getSkyColor(const Ray& ray) const;
};