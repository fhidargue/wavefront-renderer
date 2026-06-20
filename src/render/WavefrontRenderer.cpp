#include <render/WavefrontRenderer.h>
#include <math/Random.h>
#include <chrono>
#include <algorithm>
#include <cmath>

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

void WavefrontRenderer::generatePrimaryRays(const Camera& camera, int width, int height, RayQueue& queue) {
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            float u = (static_cast<float>(x) + randomFloat()) / (width  - 1);
            float v = 1.0f - (static_cast<float>(y) + randomFloat()) / (height - 1);

            Ray ray = camera.getRay(u, v);
            int pixelIndex = y * width + x;

            queue.add(RayState(ray, pixelIndex));
        }
    }
}

void WavefrontRenderer::intersectAll(const RayQueue& queue, const Scene& scene, 
    ShadingQueue& shadingQueue, RayQueue& missQueue) {
    for (const RayState& rayState : queue.rays) {
        HitRecord record;

        if (scene.hit(rayState.ray, 0.001f, 1e9f, record))
            shadingQueue.add(rayState, record);
        else
            missQueue.add(rayState);
    }
}

void WavefrontRenderer::shadeAll(ShadingQueue& shadingQueue, const Scene& scene,
    vector<Color>& accumulator, RayQueue& nextQueue) {
    for (int i = 0; i < shadingQueue.size(); ++i) {
        const ShadingWork& work = shadingQueue.getSorted(i);
        const Material& material = scene.getMaterial(work.hitRecord);

        Color emitted = material.emitted();
        accumulator[work.rayState.rayPixelIndex] =
            accumulator[work.rayState.rayPixelIndex] + work.rayState.throughtput * emitted;

        Color attenuation;
        Ray scattered;

        if (material.scatter(work.rayState.ray, work.hitRecord, scene.textures, attenuation, scattered)) {
            RayState nextRay = work.rayState;
            nextRay.ray = scattered;
            nextRay.throughtput = work.rayState.throughtput * attenuation;
            nextRay.rayDepth += 1;
            nextQueue.add(nextRay);
        }
    }
}

Color WavefrontRenderer::getSkyColor(const Ray& ray) const {
    float blendFactor = 0.5f * (ray.direction.normalized().y + 1.0f);
    Color white(1.0f, 1.0f, 1.0f);
    Color skyBlue(0.5f, 0.7f, 1.0f);
    
    return white * (1.0f - blendFactor) + skyBlue * blendFactor;
}