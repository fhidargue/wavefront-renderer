#include <render/WavefrontRenderer.h>
#include <math/Random.h>
#include <chrono>
#include <algorithm>
#include <cmath>
#include <tbb/parallel_for.h>
#include <tbb/blocked_range.h>
#include <tbb/combinable.h>

using std::vector;
using std::cout;
using std::endl;
using std::min;

double WavefrontRenderer::renderScene(const Scene& scene, const Camera& camera,
                                       Image& image, const std::string& previewPath,
                                       int progressInterval,
                                       ProgressCallback progressCallback)
{
    const int pixelCount = image.width * image.height;
    double totalShadeOnlyMs = 0.0;

    vector<Color> accumulator(pixelCount, Color(0.0f, 0.0f, 0.0f));

    for (int sample = 0; sample < samplesPerPixel; ++sample)
    {
        RayQueue currentQueue;
        generatePrimaryRays(camera, image.width, image.height, currentQueue);

        for (int depth = 0; depth < maxDepth; ++depth)
        {
            if (currentQueue.empty())
                break;

            ShadingQueue shadingQueue(policy);
            RayQueue missQueue;

            intersectAll(currentQueue, scene, shadingQueue, missQueue);
            shadingQueue.schedule();

            RayQueue nextQueue;
            auto shadeStart = std::chrono::high_resolution_clock::now();
            shadeAll(shadingQueue, scene, accumulator, nextQueue);
            auto shadeEnd = std::chrono::high_resolution_clock::now();

            totalShadeOnlyMs +=
                std::chrono::duration<double, std::milli>(shadeEnd - shadeStart).count();

            int missCount = missQueue.size();

            for (int i = 0; i < missCount; ++i)
            {
                Ray ray = missQueue.getRay(i);
                Color sky = getSkyColor(ray);
                Color throughput = missQueue.getThroughput(i);
                int pixelIndex = missQueue.pixelIndices[i];
                accumulator[pixelIndex] = accumulator[pixelIndex] + throughput * sky;
            }

            currentQueue = nextQueue;
        }

        // Write preview EXR every progressInterval samples
        // Python UI polls this file and updates the display
        if (!previewPath.empty() &&
            progressInterval > 0 &&
            (sample + 1) % progressInterval == 0)
        {
            int samplesCompleted = sample + 1;

            // Average accumulator into image pixels
            for (int i = 0; i < pixelCount; ++i)
                image.pixels[i] =
                    accumulator[i] / static_cast<float>(samplesCompleted);

            image.write(previewPath);

            cout << "Sample: " << samplesCompleted
                      << "/" << samplesPerPixel << endl;

            if (progressCallback)
                progressCallback(samplesCompleted, samplesPerPixel);
        }
    }

    // Final image write with gamma correction
    for (int i = 0; i < pixelCount; ++i)
        image.pixels[i] = accumulator[i] / static_cast<float>(samplesPerPixel);

    return totalShadeOnlyMs;
}

void WavefrontRenderer::generatePrimaryRays(const Camera& camera, int width, int height,
                                            RayQueue& outputQueue)
{
    // Thread local ray buffer creation
    tbb::combinable<RayQueue> threadLocalQueues;

    tbb::parallel_for(tbb::blocked_range<int>(0, height),
                      [&](const tbb::blocked_range<int>& rowRange)
                      {
                          auto& local = threadLocalQueues.local();

                          for (int y = rowRange.begin(); y < rowRange.end(); ++y)
                          {
                              for (int x = 0; x < width; ++x)
                              {
                                  float u = (static_cast<float>(x) + randomFloat()) / (width - 1);
                                  float v =
                                      1.0f - (static_cast<float>(y) + randomFloat()) / (height - 1);

                                  Ray ray = camera.getRay(u, v);
                                  local.addPrimary(ray, y * width + x);
                              }
                          }
                      });

    // Merge all thread local ray buffers into an output queue
    threadLocalQueues.combine_each(
        [&](const RayQueue& local)
        {
            // Merge all SoA arrays
            auto append = [](auto& destination, const auto& src)

            { destination.insert(destination.end(), src.begin(), src.end()); };
            append(outputQueue.originsX, local.originsX);
            append(outputQueue.originsY, local.originsY);
            append(outputQueue.originsZ, local.originsZ);
            append(outputQueue.dirsX, local.dirsX);
            append(outputQueue.dirsY, local.dirsY);
            append(outputQueue.dirsZ, local.dirsZ);
            append(outputQueue.throughputsR, local.throughputsR);
            append(outputQueue.throughputsG, local.throughputsG);
            append(outputQueue.throughputsB, local.throughputsB);
            append(outputQueue.accumLightR, local.accumLightR);
            append(outputQueue.accumLightG, local.accumLightG);
            append(outputQueue.accumLightB, local.accumLightB);
            append(outputQueue.pixelIndices, local.pixelIndices);
            append(outputQueue.depths, local.depths);
            append(outputQueue.alive, local.alive);
        });
}

void WavefrontRenderer::intersectAll(const RayQueue& inputQueue, const Scene& scene,
                                     ShadingQueue& outputShadingQueue, RayQueue& outputMissQueue)
{
    const int rayCount = inputQueue.size();

    // Each thread gets its own hit/miss buckets
    tbb::combinable<ShadingQueue> threadLocalHits([this] { return ShadingQueue(policy); });
    tbb::combinable<RayQueue> threadLocalMisses;

    // Process rays in packets of 4 (SIMD) packet tracing
    tbb::parallel_for(
        tbb::blocked_range<int>(0, rayCount, 4),
        [&](const tbb::blocked_range<int>& range)
        {
            auto& localHits = threadLocalHits.local();
            auto& localMisses = threadLocalMisses.local();

            int i = range.begin();

            while (i + 4 <= range.end())
            {
                HitRecord records[4];
                int hitMask = scene.accelerator.intersect4(inputQueue, i, 4, records);

                for (int j = 0; j < 4; ++j)
                {
                    if (hitMask & (1 << j))
                        localHits.add(inputQueue, i + j, records[j]);
                    else
                        localMisses.add(inputQueue.getRay(i + j), inputQueue.getThroughput(i + j),
                                        inputQueue.getAccumLight(i + j),
                                        inputQueue.pixelIndices[i + j], inputQueue.depths[i + j],
                                        true);
                }

                i += 4;
            }

            // Handle remaining rays with scalar intersection
            while (i < range.end())
            {
                Ray ray = inputQueue.getRay(i);
                HitRecord record;

                if (scene.hit(ray, 0.001f, 1e9f, record))
                    localHits.add(inputQueue, i, record);
                else
                    localMisses.add(ray, inputQueue.getThroughput(i), inputQueue.getAccumLight(i),
                                    inputQueue.pixelIndices[i], inputQueue.depths[i], true);
                ++i;
            }
        });

    // Merge thread local results into the shared queues
    threadLocalHits.combine_each(
        [&](const ShadingQueue& local)
        {
            auto append = [](auto& dst, const auto& src)
            { dst.insert(dst.end(), src.begin(), src.end()); };
            append(outputShadingQueue.hitPointsX, local.hitPointsX);
            append(outputShadingQueue.hitPointsY, local.hitPointsY);
            append(outputShadingQueue.hitPointsZ, local.hitPointsZ);
            append(outputShadingQueue.hitNormalsX, local.hitNormalsX);
            append(outputShadingQueue.hitNormalsY, local.hitNormalsY);
            append(outputShadingQueue.hitNormalsZ, local.hitNormalsZ);
            append(outputShadingQueue.materialIDs, local.materialIDs);
            append(outputShadingQueue.textureIDs, local.textureIDs);
            append(outputShadingQueue.originsX, local.originsX);
            append(outputShadingQueue.originsY, local.originsY);
            append(outputShadingQueue.originsZ, local.originsZ);
            append(outputShadingQueue.dirsX, local.dirsX);
            append(outputShadingQueue.dirsY, local.dirsY);
            append(outputShadingQueue.dirsZ, local.dirsZ);
            append(outputShadingQueue.throughputsR, local.throughputsR);
            append(outputShadingQueue.throughputsG, local.throughputsG);
            append(outputShadingQueue.throughputsB, local.throughputsB);
            append(outputShadingQueue.pixelIndices, local.pixelIndices);
            append(outputShadingQueue.depths, local.depths);
        });

    threadLocalMisses.combine_each(
        [&](const RayQueue& local)
        {
            auto append = [](auto& dst, const auto& src)
            { dst.insert(dst.end(), src.begin(), src.end()); };
            append(outputMissQueue.originsX, local.originsX);
            append(outputMissQueue.originsY, local.originsY);
            append(outputMissQueue.originsZ, local.originsZ);
            append(outputMissQueue.dirsX, local.dirsX);
            append(outputMissQueue.dirsY, local.dirsY);
            append(outputMissQueue.dirsZ, local.dirsZ);
            append(outputMissQueue.throughputsR, local.throughputsR);
            append(outputMissQueue.throughputsG, local.throughputsG);
            append(outputMissQueue.throughputsB, local.throughputsB);
            append(outputMissQueue.accumLightR, local.accumLightR);
            append(outputMissQueue.accumLightG, local.accumLightG);
            append(outputMissQueue.accumLightB, local.accumLightB);
            append(outputMissQueue.pixelIndices, local.pixelIndices);
            append(outputMissQueue.depths, local.depths);
            append(outputMissQueue.alive, local.alive);
        });
}

void WavefrontRenderer::shadeAll(ShadingQueue& shadingQueue, const Scene& scene,
                                 vector<Color>& accumulator, RayQueue& outputNextQueue)
{
    const float fireflyThreshold = 10.0f;
    const int workCount = shadingQueue.size();

    tbb::combinable<RayQueue> threadLocalNextRays;
    tbb::combinable<vector<std::pair<int, Color>>> threadLocalAccumContribs;

    tbb::parallel_for(
        tbb::blocked_range<int>(0, workCount),
        [&](const tbb::blocked_range<int>& range)
        {
            auto& localNextRays = threadLocalNextRays.local();
            auto& localAccumContribs = threadLocalAccumContribs.local();

            for (int i = range.begin(); i < range.end(); ++i)
            {
                HitRecord record = shadingQueue.getHitRecord(i);
                Ray incomingRay = shadingQueue.getRay(i);
                Color throughput = shadingQueue.getThroughput(i);
                int pixelIndex = shadingQueue.getPixelIndex(i);
                int depth = shadingQueue.getDepth(i);

                const Material& material = scene.getMaterial(record);

                // Accumulate emitted light
                Color emitted = material.emitted();
                if (emitted.x > 0.0f || emitted.y > 0.0f || emitted.z > 0.0f)
                {
                    // Prevent firefly pixels
                    emitted.x = min(emitted.x, fireflyThreshold);
                    emitted.y = min(emitted.y, fireflyThreshold);
                    emitted.z = min(emitted.z, fireflyThreshold);

                    localAccumContribs.push_back({pixelIndex, throughput * emitted});
                }

                // NEE direct light sample
                if (material.type == MaterialType::Diffuse || material.type == MaterialType::Metal)
                {
                    LightSample light = scene.sampleLight();

                    if (light.valid)
                    {
                        Vec3 toLight = light.point - record.point;
                        float distanceToLight = toLight.length();
                        Vec3 toLightDirection = toLight / distanceToLight;

                        float cosAtSurface = record.normal.dot(toLightDirection);
                        float cosAtLight = (toLightDirection * -1.0f).dot(light.normal);

                        // Both light and surface must face each other
                        if (cosAtSurface > 0.0f && cosAtLight > 0.0f)
                        {
                            bool shadowed = scene.accelerator.occluded(
                                record.point, toLightDirection, distanceToLight);

                            if (!shadowed)
                            {
                                float geometry = (cosAtSurface * cosAtLight) /
                                                 (distanceToLight * distanceToLight);
                                Color brdf = material.albedo * (1.0f / PI);

                                // NEE contribution
                                Color direct =
                                    throughput * brdf * light.emission * geometry * light.area;

                                // Prevent firefly pixels
                                direct.x = min(direct.x, fireflyThreshold);
                                direct.y = min(direct.y, fireflyThreshold);
                                direct.z = min(direct.z, fireflyThreshold);

                                localAccumContribs.push_back({pixelIndex, direct});
                            }
                        }
                    }
                }

                // Scatter the ray for the next bounce
                Color attenuation;
                Ray scattered;

                if (material.scatter(incomingRay, record, scene.textures, attenuation, scattered))
                {
                    Color nextThroughput = throughput * attenuation;

                    // Wavefront Russian Roulette
                    // Rays with low throughput are terminated probabilistically
                    if (depth + 1 >= rrMinDepth)
                    {
                        float survivalProbability = min(
                            0.95f,
                            std::max({nextThroughput.x, nextThroughput.y, nextThroughput.z}));

                        if (randomFloat() > survivalProbability)
                            continue;

                        nextThroughput = nextThroughput / survivalProbability;
                    }

                    localNextRays.add(scattered, nextThroughput, Color(0.0f, 0.0f, 0.0f),
                                      pixelIndex, depth + 1, true);
                }
            }
        });

    // Merge accumulator contributions
    threadLocalAccumContribs.combine_each(
        [&](const vector<std::pair<int, Color>>& contribs)
        {
            for (const auto& contrib : contribs)
                accumulator[contrib.first] = accumulator[contrib.first] + contrib.second;
        });

    // Merge next bounce rays
    threadLocalNextRays.combine_each(
        [&](const RayQueue& local)
        {
            auto append = [](auto& dst, const auto& src)
            { dst.insert(dst.end(), src.begin(), src.end()); };
            append(outputNextQueue.originsX, local.originsX);
            append(outputNextQueue.originsY, local.originsY);
            append(outputNextQueue.originsZ, local.originsZ);
            append(outputNextQueue.dirsX, local.dirsX);
            append(outputNextQueue.dirsY, local.dirsY);
            append(outputNextQueue.dirsZ, local.dirsZ);
            append(outputNextQueue.throughputsR, local.throughputsR);
            append(outputNextQueue.throughputsG, local.throughputsG);
            append(outputNextQueue.throughputsB, local.throughputsB);
            append(outputNextQueue.accumLightR, local.accumLightR);
            append(outputNextQueue.accumLightG, local.accumLightG);
            append(outputNextQueue.accumLightB, local.accumLightB);
            append(outputNextQueue.pixelIndices, local.pixelIndices);
            append(outputNextQueue.depths, local.depths);
            append(outputNextQueue.alive, local.alive);
        });
}

Color WavefrontRenderer::getSkyColor(const Ray& ray) const
{
    float blendFactor = 0.5f * (ray.direction.normalized().y + 1.0f);
    Color white(1.0f, 1.0f, 1.0f);
    Color skyBlue(0.5f, 0.7f, 1.0f);

    return white * (1.0f - blendFactor) + skyBlue * blendFactor;
}