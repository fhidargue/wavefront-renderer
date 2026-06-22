#include <render/WavefrontRenderer.h>
#include <math/Random.h>
#include <chrono>
#include <algorithm>
#include <cmath>
#include <mutex>
#include <tbb/parallel_for.h>
#include <tbb/blocked_range.h>
#include <tbb/enumerable_thread_specific.h>
#include <tbb/combinable.h>

using std::vector;

double WavefrontRenderer::renderScene(const Scene& scene, const Camera& camera, Image& image) {
    const int pixelCount = image.width * image.height;
    double totalShadeOnlyMs = 0.0;

    vector<Color> accumulator(pixelCount, Color(0.0f, 0.0f, 0.0f));

    for (int sample = 0; sample < samplesPerPixel; ++sample) {
        RayQueue currentQueue;
        generatePrimaryRays(camera, image.width, image.height, currentQueue);

        for (int depth = 0; depth < maxDepth; ++depth) {
            if (currentQueue.empty()) break;

            ShadingQueue shadingQueue(policy);
            RayQueue missQueue;

            intersectAll(currentQueue, scene, shadingQueue, missQueue);
            shadingQueue.schedule();

            RayQueue nextQueue;
            auto shadeStart = std::chrono::high_resolution_clock::now();
            shadeAll(shadingQueue, scene, accumulator, nextQueue);
            auto shadeEnd = std::chrono::high_resolution_clock::now();

            totalShadeOnlyMs += std::chrono::duration<double, std::milli>(shadeEnd - shadeStart).count();

            for (const RayState& miss : missQueue.rays) {
                Color skyColor = getSkyColor(miss.ray);
                accumulator[miss.rayPixelIndex] =
                    accumulator[miss.rayPixelIndex] + miss.throughtput * skyColor;
            }

            currentQueue = nextQueue;
        }
    }

    // Final image write with gamma correction
    for (int i = 0; i < pixelCount; ++i) {
        Color averaged = accumulator[i] / static_cast<float>(samplesPerPixel);

        averaged.x = std::sqrt(std::max(0.0f, averaged.x));
        averaged.y = std::sqrt(std::max(0.0f, averaged.y));
        averaged.z = std::sqrt(std::max(0.0f, averaged.z));

        int x = i % image.width;
        int y = i / image.width;
        image.setPixel(x, y, averaged);
    }

    return totalShadeOnlyMs;
}

void WavefrontRenderer::generatePrimaryRays(const Camera& camera, int width, int height, RayQueue& outputQueue) {
    // Thread local ray buffer creation
    tbb::combinable<vector<RayState>> threadLocalRays;
    
    tbb::parallel_for(tbb::blocked_range<int>(0, height),
        [&](const tbb::blocked_range<int>& rowRange) {
            auto& localRays = threadLocalRays.local();

            for (int y = rowRange.begin(); y < rowRange.end(); ++y) {
                for (int x = 0; x < width; ++x) {
                    float u = (static_cast<float>(x) + randomFloat()) / (width  - 1);
                    float v = 1.0f - (static_cast<float>(y) + randomFloat()) / (height - 1);

                    Ray ray = camera.getRay(u, v);
                    int pixelIndex = y * width + x;

                    localRays.push_back(RayState(ray, pixelIndex));
                }
            }
        });

    // Merge all thread local ray buffers into an output queue
    threadLocalRays.combine_each([&](const vector<RayState>& localRays) {
        for (const RayState& ray : localRays)
            outputQueue.add(ray);
    });
}

void WavefrontRenderer::intersectAll(const RayQueue& inputQueue, const Scene& scene, 
    ShadingQueue& outputShadingQueue, RayQueue& outputMissQueue) {
    const int rayCount = inputQueue.size();

    // Each thread gets its own hit/miss buckets
    tbb::combinable<vector<ShadingWork>> threadLocalHits;
    tbb::combinable<vector<RayState>> threadLocalMisses;

    tbb::parallel_for(tbb::blocked_range<int>(0, rayCount),
        [&](const tbb::blocked_range<int>& range) {
            auto& localHits = threadLocalHits.local();
            auto& localMisses = threadLocalMisses.local();

            for (int i = range.begin(); i < range.end(); ++i) {
                const RayState& rayState = inputQueue.rays[i];
                HitRecord record;

                if (scene.hit(rayState.ray, 0.001f, 1e9f, record))
                    localHits.push_back({rayState, record});
                else
                    localMisses.push_back(rayState);
            }
        });

    // Merge thread local results into the shared queues
    threadLocalHits.combine_each([&](const vector<ShadingWork>& localHits) {
        for (const ShadingWork& hit : localHits)
            outputShadingQueue.add(hit.rayState, hit.hitRecord);
    });

    threadLocalMisses.combine_each([&](const vector<RayState>& localMisses) {
        for (const RayState& miss : localMisses)
            outputMissQueue.add(miss);
    });
}

void WavefrontRenderer::shadeAll(ShadingQueue& shadingQueue, const Scene& scene, 
    vector<Color>& accumulator, RayQueue& outputNextQueue) {
    const int workCount = shadingQueue.size();

    tbb::combinable<vector<RayState>> threadLocalNextRays;
    tbb::combinable<vector<std::pair<int, Color>>> threadLocalAccumContribs;

    tbb::parallel_for(tbb::blocked_range<int>(0, workCount),
        [&](const tbb::blocked_range<int>& range) {
            auto& localNextRays = threadLocalNextRays.local();
            auto& localAccumContribs = threadLocalAccumContribs.local();

            for (int i = range.begin(); i < range.end(); ++i) {
                const ShadingWork& work = shadingQueue.getSorted(i);
                const Material& material = scene.getMaterial(work.hitRecord);

                // Accumulate emitted light
                Color emitted = material.emitted();
                if (emitted.x > 0.0f || emitted.y > 0.0f || emitted.z > 0.0f) {
                    localAccumContribs.push_back({work.rayState.rayPixelIndex,
                         work.rayState.throughtput * emitted});
                }

                // Scatter the ray
                Color attenuation;
                Ray scattered;

                if (material.scatter(work.rayState.ray, work.hitRecord, 
                    scene.textures, attenuation, scattered)) {
                    RayState nextRay = work.rayState;

                    nextRay.ray = scattered;
                    nextRay.throughtput = work.rayState.throughtput * attenuation;
                    nextRay.rayDepth += 1;

                    localNextRays.push_back(nextRay);
                }
            }
        });

    // Merge accumulator contributions
    threadLocalAccumContribs.combine_each(
        [&](const vector<std::pair<int, Color>>& contribs) {
            for (const auto& contrib : contribs)
                accumulator[contrib.first] = accumulator[contrib.first] + contrib.second;
        });

    // Merge next bounce rays
    threadLocalNextRays.combine_each([&](const vector<RayState>& localRays) {
        for (const RayState& ray : localRays)
            outputNextQueue.add(ray);
    });
}

Color WavefrontRenderer::getSkyColor(const Ray& ray) const {
    float blendFactor = 0.5f * (ray.direction.normalized().y + 1.0f);
    Color white(1.0f, 1.0f, 1.0f);
    Color skyBlue(0.5f, 0.7f, 1.0f);
    
    return white * (1.0f - blendFactor) + skyBlue * blendFactor;
}