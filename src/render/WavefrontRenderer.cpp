#include <render/WavefrontRenderer.h>
#include <render/CostTracker.h>
#include <core/PrintUtils.h>
#include <shading/MIS.h>
#include <math/Random.h>
#include <chrono>
#include <cstdio>
#include <algorithm>
#include <cmath>
#include <csignal>
#include <cstdlib>
#include <cstring>
#include <tbb/parallel_for.h>
#include <tbb/blocked_range.h>
#include <tbb/combinable.h>

using std::cerr;
using std::cout;
using std::endl;
using std::min;
using std::signal;
using std::string;
using std::to_string;
using std::vector;

static char previewPathBuffer[4096];

static void onSignal(int)
{
    if (previewPathBuffer[0] != '\0')
        std::remove(previewPathBuffer);
    std::exit(1);
}

static void removePreviewImage(const string& previewPath)
{
    std::strncpy(previewPathBuffer, previewPath.c_str(), sizeof(previewPathBuffer) - 1);
    previewPathBuffer[sizeof(previewPathBuffer) - 1] = '\0';
    signal(SIGTERM, onSignal);
    signal(SIGINT, onSignal);
}

static void logCostStats(const CostTracker& tracker, const string& title,
                         const std::vector<string>& names)
{
    std::vector<std::string> costLines;

    for (size_t i = 0; i < tracker.averageCostNanoseconds.size(); ++i)
    {
        std::string name = (i < names.size()) ? names[i] : ("item " + to_string(i));

        if (!tracker.initialized[i])
        {
            costLines.push_back(name + " : no samples yet");
            continue;
        }

        costLines.push_back(name + " | avg cost: " + to_string(tracker.averageCostNanoseconds[i]) +
                            "ns" +
                            " | relative: " + to_string(tracker.relativeCost(static_cast<int>(i))) +
                            " | samples: " + to_string(tracker.sampleCount[i]));
    }

    printStatsBlock(title, costLines);
}

static double computeAverageRunLength(const vector<int>& ids, const vector<int>& textureIDs)
{
    if (ids.empty())
        return 0.0;

    int runCount = 0;
    int filteredSize = 0;
    int prevID = -2; // sentinel that matches nothing

    for (size_t i = 0; i < ids.size(); ++i)
    {
        if (textureIDs[i] < 0)
            continue;

        if (ids[i] != prevID)
            ++runCount;

        prevID = ids[i];
        ++filteredSize;
    }

    if (filteredSize == 0)
        return 0.0;

    return static_cast<double>(filteredSize) / runCount;
}

double WavefrontRenderer::renderScene(const Scene& scene, const Camera& camera, Image& image,
                                      const string& previewPath, int progressInterval,
                                      ProgressCallback progressCallback)
{
    // Delete the preview image is the process is terminated
    removePreviewImage(previewPath);

    materialCostTracker = CostTracker(static_cast<int>(scene.materials.size()));
    textureCostTracker = CostTracker(static_cast<int>(scene.textures.size()));

    if (!environmentMapPath.empty())
        environmentMap.load(environmentMapPath);

    const int pixelCount = image.width * image.height;
    double totalShadeOnlyMs = 0.0;
    double totalRaySortMs = 0.0;
    double totalIntersectMs = 0.0;

    // Weighted by batch size
    double totalMaterialRunLengthWeighted = 0.0;
    double totalTextureRunLengthWeighted = 0.0;
    long long totalCoherenceSampleWeight = 0;

    // Cache line homogeneity: fraction of 64-byte cache lines that contain
    // only one material/texture ID (1.0 = perfect, 0.0 = fully mixed)
    double totalMaterialHomogeneityWeighted = 0.0;
    double totalTextureHomogeneityWeighted = 0.0;

    vector<Color> accumulator(pixelCount, Color(0.0f, 0.0f, 0.0f));
    vector<Color> accumulatorBeforeSample(pixelCount, Color(0.0f, 0.0f, 0.0f));
    AdaptiveSampler adaptiveSampler(pixelCount, adaptiveSamplingThreshold);

    // Writes each pixel as its own accumulated total divided by its own sample count
    auto writeAveragedImage = [&](int samplesCompleted)
    {
        for (int i = 0; i < pixelCount; ++i)
        {
            int sampleCountForPixel =
                enableAdaptiveSampling ? adaptiveSampler.getSampleCount(i) : samplesCompleted;
            sampleCountForPixel = std::max(sampleCountForPixel, 1);

            image.pixels[i] = accumulator[i] / static_cast<float>(sampleCountForPixel);
        }
    };

    int samplesActuallyRun = samplesPerPixel;

    for (int sample = 0; sample < samplesPerPixel; ++sample)
    {
        if (enableAdaptiveSampling)
            accumulatorBeforeSample = accumulator;

        RayQueue currentQueue;

        if (enableAdaptiveSampling)
            generatePrimaryRays(camera, image.width, image.height, currentQueue,
                                &adaptiveSampler.pixelActive);
        else
            generatePrimaryRays(camera, image.width, image.height, currentQueue);

        for (int depth = 0; depth < maxDepth; ++depth)
        {
            if (currentQueue.empty())
                break;

            if (enableRaySort)
            {
                auto raySortStart = std::chrono::high_resolution_clock::now();
                currentQueue.schedule(scene.boundsMin, scene.boundsMax);
                currentQueue.materialize();
                auto raySortEnd = std::chrono::high_resolution_clock::now();

                totalRaySortMs +=
                    std::chrono::duration<double, std::milli>(raySortEnd - raySortStart).count();
            }

            ShadingQueue shadingQueue(policy);
            RayQueue missQueue;

            auto intersectStart = std::chrono::high_resolution_clock::now();
            intersectAll(currentQueue, scene, shadingQueue, missQueue);
            auto intersectEnd = std::chrono::high_resolution_clock::now();

            totalIntersectMs +=
                std::chrono::duration<double, std::milli>(intersectEnd - intersectStart).count();

            if (enableMemoryCoherenceStats && sample == 0 && depth == 0)
                shadingQueue.printQueueComposition(depth, scene.materials, scene.textures);

            shadingQueue.schedule(scene.materials, scene.textures, &materialCostTracker);
            shadingQueue.materialize();

            // Shading batch tracking
            int shadingBatchSize = shadingQueue.size();

            if (shadingBatchSize > 0)
            {
                double materialRunLength =
                    computeAverageRunLength(shadingQueue.materialIDs, shadingQueue.textureIDs);
                double textureRunLength =
                    computeAverageRunLength(shadingQueue.textureIDs, shadingQueue.textureIDs);

                totalMaterialRunLengthWeighted += materialRunLength * shadingBatchSize;
                totalTextureRunLengthWeighted += textureRunLength * shadingBatchSize;
                totalCoherenceSampleWeight += shadingBatchSize;

                if (enableMemoryCoherenceStats)
                {
                    static constexpr int INTS_PER_CACHE_LINE = 64 / sizeof(int); // 16

                    auto cacheLineHomogeneity = [](const std::vector<int>& ids,
                                                   int elemsPerLine) -> double
                    {
                        if (ids.empty())
                            return 0.0;

                        int monomorphicLines = 0;
                        int totalLines = 0;

                        for (int lineStart = 0; lineStart < (int)ids.size();
                             lineStart += elemsPerLine)
                        {
                            int lineEnd = std::min(lineStart + elemsPerLine, (int)ids.size());
                            int firstID = ids[lineStart];
                            bool isMono = true;

                            for (int j = lineStart + 1; j < lineEnd; ++j)
                            {
                                if (ids[j] != firstID)
                                {
                                    isMono = false;
                                    break;
                                }
                            }

                            if (isMono)
                                ++monomorphicLines;
                            ++totalLines;
                        }

                        return static_cast<double>(monomorphicLines) / totalLines;
                    };

                    double materialHomogeneity =
                        cacheLineHomogeneity(shadingQueue.materialIDs, INTS_PER_CACHE_LINE);
                    double textureHomogeneity =
                        cacheLineHomogeneity(shadingQueue.textureIDs, INTS_PER_CACHE_LINE);

                    totalMaterialHomogeneityWeighted += materialHomogeneity * shadingBatchSize;
                    totalTextureHomogeneityWeighted += textureHomogeneity * shadingBatchSize;
                }
            }

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

        if (enableAdaptiveSampling)
        {
            for (int pixelIndex = 0; pixelIndex < pixelCount; ++pixelIndex)
            {
                if (!adaptiveSampler.isActive(pixelIndex))
                    continue;

                Color sampleContribution =
                    accumulator[pixelIndex] - accumulatorBeforeSample[pixelIndex];
                adaptiveSampler.addSample(pixelIndex, luminance(sampleContribution));
            }

            bool isCheckSample =
                (sample + 1) % AdaptiveSamplingConstants::convergenceCheckInterval == 0;

            if (isCheckSample)
            {
                adaptiveSampler.updateConvergence();

                if (adaptiveSampler.activePixelCount() == 0)
                {
                    samplesActuallyRun = sample + 1;
                    break;
                }
            }
        }

        // Write preview EXR every progressInterval samples
        // Python UI polls this file and updates the display
        if (!previewPath.empty() && progressInterval > 0 && (sample + 1) % progressInterval == 0)
        {
            int samplesCompleted = sample + 1;

            writeAveragedImage(samplesCompleted);
            image.write(previewPath, enableSampleLogging);

            if (!enableSampleLogging)
                cout << "Sample: " << samplesCompleted << "/" << samplesPerPixel << endl;

            if (progressCallback)
                progressCallback(samplesCompleted, samplesPerPixel);
        }
    }

    // Final image write with gamma correction
    writeAveragedImage(samplesActuallyRun);

    double totalPipelineMs = totalRaySortMs + totalIntersectMs + totalShadeOnlyMs;
    double sortOverheadPercent =
        (totalPipelineMs > 0.0) ? (totalRaySortMs / totalPipelineMs) * 100.0 : 0.0;

    if (enableRaySort)
    {
        printStatsBlock("Ray Sort Stats",
                        {
                            "Total ray sort time (ms)      : " + to_string(totalRaySortMs),
                            "Total intersect time (ms)     : " + to_string(totalIntersectMs),
                            "Total shade time (ms)         : " + to_string(totalShadeOnlyMs),
                            "Sort overhead (% of pipeline) : " + to_string(sortOverheadPercent),
                        });
    }

    if (totalCoherenceSampleWeight > 0)
    {
        double averageMaterialRunLength =
            totalMaterialRunLengthWeighted / totalCoherenceSampleWeight;
        double averageTextureRunLength = totalTextureRunLengthWeighted / totalCoherenceSampleWeight;

        printStatsBlock(
            "Coherence Metrics",
            {
                "Average material run length  : " + to_string(averageMaterialRunLength),
                "Average texture run length   : " + to_string(averageTextureRunLength),
                "Total shaded hits            : " + to_string(totalCoherenceSampleWeight),
            });
    }

    if (enableMemoryCoherenceStats && totalCoherenceSampleWeight > 0)
    {
        double averageMaterialHomogeneity =
            totalMaterialHomogeneityWeighted / totalCoherenceSampleWeight;
        double averageTextureHomogeneity =
            totalTextureHomogeneityWeighted / totalCoherenceSampleWeight;

        printStatsBlock(
            "Memory Coherence Stats",
            {
                "Material ID cache line homogeneity : " + to_string(averageMaterialHomogeneity),
                "Texture  ID cache line homogeneity : " + to_string(averageTextureHomogeneity),
            });
    }

    if (enableAdaptiveSampling)
    {
        printStatsBlock(
            "Adaptive Sampling Stats",
            {
                "Samples budgeted           : " + to_string(samplesPerPixel),
                "Samples actually run       : " + to_string(samplesActuallyRun),
                "Pixels still active at end : " + to_string(adaptiveSampler.activePixelCount()),
            });
    }

    // Print material and texture cost stats
    vector<string> materialNames;
    for (const auto& material : scene.materials)
        materialNames.push_back(material.uuid);

    vector<string> textureNames;
    for (size_t i = 0; i < scene.textures.size(); ++i)
    {
        string name = scene.textures[i].name;
        textureNames.push_back(name.empty() ? ("Texture " + to_string(i)) : name);
    }

    if (!materialNames.empty())
        logCostStats(materialCostTracker, "Material Cost Tracker Stats", materialNames);

    if (!textureNames.empty())
        logCostStats(textureCostTracker, "Texture Cost Tracker Stats", textureNames);

    // Delete preview file once the render is complete
    if (!previewPath.empty())
    {
        if (std::remove(previewPath.c_str()) != 0)
            cerr << "WARNING: Could not delete preview: " << previewPath << endl;
    }

    return totalShadeOnlyMs;
}

void WavefrontRenderer::generatePrimaryRays(const Camera& camera, int width, int height,
                                            RayQueue& outputQueue,
                                            const std::vector<bool>* activePixels)
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
                                  int pixelIndex = y * width + x;

                                  if (activePixels && !(*activePixels)[pixelIndex])
                                      continue;

                                  float u = (static_cast<float>(x) + randomFloat()) / (width - 1);
                                  float v =
                                      1.0f - (static_cast<float>(y) + randomFloat()) / (height - 1);

                                  Ray ray = camera.getRay(u, v);
                                  local.addPrimary(ray, pixelIndex);
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
            append(outputQueue.pdfs, local.pdfs);
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
                                        true, inputQueue.pdfs[i + j]);
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
                                    inputQueue.pixelIndices[i], inputQueue.depths[i], true,
                                    inputQueue.pdfs[i]);
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
            append(outputShadingQueue.hitDistances, local.hitDistances);
            append(outputShadingQueue.hitU, local.hitU);
            append(outputShadingQueue.hitV, local.hitV);
            append(outputShadingQueue.hitTriangleIndex, local.hitTriangleIndex);
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
            append(outputShadingQueue.pdfs, local.pdfs);
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
            append(outputMissQueue.pdfs, local.pdfs);
        });
}

void WavefrontRenderer::shadeAll(ShadingQueue& shadingQueue, const Scene& scene,
                                 vector<Color>& accumulator, RayQueue& outputNextQueue)
{
    const int workCount = shadingQueue.size();
    const int materialCount = static_cast<int>(scene.materials.size());
    const int textureCount = static_cast<int>(scene.textures.size());

    tbb::combinable<RayQueue> threadLocalNextRays;
    tbb::combinable<vector<std::pair<int, Color>>> threadLocalAccumContribs;

    // Material cost threads
    tbb::combinable<vector<double>> threadLocalCostSum(
        [materialCount] { return vector<double>(materialCount, 0.0); });
    tbb::combinable<vector<int>> threadLocalCostCount([materialCount]
                                                      { return vector<int>(materialCount, 0); });

    // Texture cost threads
    tbb::combinable<vector<double>> threadLocalTextureCostSum(
        [textureCount] { return vector<double>(textureCount, 0.0); });
    tbb::combinable<vector<int>> threadLocalTextureCostCount(
        [textureCount] { return vector<int>(textureCount, 0); });

    tbb::parallel_for(
        tbb::blocked_range<int>(0, workCount),
        [&](const tbb::blocked_range<int>& range)
        {
            auto& localNextRays = threadLocalNextRays.local();
            auto& localAccumContribs = threadLocalAccumContribs.local();
            auto& localCostSum = threadLocalCostSum.local();
            auto& localCostCount = threadLocalCostCount.local();
            auto& localTextureCostSum = threadLocalTextureCostSum.local();
            auto& localTextureCostCount = threadLocalTextureCostCount.local();

            for (int i = range.begin(); i < range.end(); ++i)
            {
                HitRecord record = shadingQueue.getHitRecord(i);
                Ray incomingRay = shadingQueue.getRay(i);
                Color throughput = shadingQueue.getThroughput(i);
                int pixelIndex = shadingQueue.getPixelIndex(i);
                int depth = shadingQueue.getDepth(i);
                float incomingPdf = shadingQueue.getPdf(i);

                const Material& material = scene.getMaterial(record);
                const bool shouldTimeThisRay = (randomFloat() < 0.25f);

                // Texture cost tracking
                if (shouldTimeThisRay && material.textureID >= 0)
                {
                    auto textureStart = std::chrono::high_resolution_clock::now();
                    material.getSurfaceColor(record,
                                             scene.textures); // result discarded - timing only
                    auto textureEnd = std::chrono::high_resolution_clock::now();

                    double textureCostNs =
                        std::chrono::duration<double, std::nano>(textureEnd - textureStart).count();

                    localTextureCostSum[material.textureID] += textureCostNs;
                    localTextureCostCount[material.textureID] += 1;
                }

                // Accumulate emitted light
                Color emitted = material.emitted(record.normal, incomingRay.direction * -1.0f);

                if (emitted.x > 0.0f || emitted.y > 0.0f || emitted.z > 0.0f)
                {
                    // Calculate the weight of MIS
                    float misWeight = 1.0f;

                    if (depth > 0 && incomingPdf > 0.0f)
                    {
                        float cosAtLight =
                            std::max(0.0f, (incomingRay.direction * -1.0f).dot(record.normal));

                        if (cosAtLight > 0.0f)
                        {
                            float lightPdfArea = lightAreaPdf(scene);
                            float lightPdfSolidAngle =
                                areaToAngleProbability(lightPdfArea, record.distance, cosAtLight);

                            misWeight = misBalance(incomingPdf, lightPdfSolidAngle);
                        }
                    }

                    // Prevent firefly pixels
                    emitted.x = min(emitted.x, fireflyThreshold);
                    emitted.y = min(emitted.y, fireflyThreshold);
                    emitted.z = min(emitted.z, fireflyThreshold);

                    localAccumContribs.push_back({pixelIndex, throughput * emitted * misWeight});
                }

                // NEE direct light sample
                if (material.type == MaterialType::Diffuse ||
                    material.type == MaterialType::Plastic)
                {
                    LightSample light = scene.sampleLight(record.point);

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
                                Color surfaceColor =
                                    material.getSurfaceColor(record, scene.textures);
                                float geometry = (cosAtSurface * cosAtLight) /
                                                 (distanceToLight * distanceToLight);

                                Color brdf = surfaceColor * (1.0f / PI);

                                float lightPdfArea = lightAreaPdf(scene);
                                float lightPdfSolidAngle = areaToAngleProbability(
                                    lightPdfArea, distanceToLight, cosAtLight);
                                float bsdfPdfSolidAngle = cosAtSurface / PI;

                                float misWeight = misBalance(lightPdfSolidAngle, bsdfPdfSolidAngle);

                                // NEE contribution, weighted by MIS
                                Color direct = throughput * brdf * light.emission * geometry *
                                               light.area * misWeight;

                                // Prevent firefly pixels
                                direct.x = min(direct.x, fireflyThreshold);
                                direct.y = min(direct.y, fireflyThreshold);
                                direct.z = min(direct.z, fireflyThreshold);

                                localAccumContribs.push_back({pixelIndex, direct});
                            }
                        }
                    }
                }

                // Directional light NEE
                if (scene.directionalLight.intensity > 0.0f)
                {
                    Vec3 toLightDirection = scene.directionalLight.direction * -1.0f;
                    float cosAtSurface = record.normal.dot(toLightDirection);

                    if (cosAtSurface > 0.0f)
                    {
                        bool shadowed =
                            scene.accelerator.occluded(record.point, toLightDirection, 1e6f);

                        if (!shadowed)
                        {
                            Color surfaceColor = material.getSurfaceColor(record, scene.textures);
                            Color brdf = surfaceColor * (1.0f / PI);
                            Color direct = throughput * brdf * scene.directionalLight.color *
                                           scene.directionalLight.intensity * cosAtSurface;

                            direct.x = min(direct.x, fireflyThreshold);
                            direct.y = min(direct.y, fireflyThreshold);
                            direct.z = min(direct.z, fireflyThreshold);

                            localAccumContribs.push_back({pixelIndex, direct});
                        }
                    }
                }

                // Scatter the ray for the next bounce
                Color attenuation;
                Ray scattered;

                bool didScatter;
                float scatterPdf;

                if (shouldTimeThisRay)
                {
                    auto scatterStart = std::chrono::high_resolution_clock::now();
                    didScatter = material.scatter(incomingRay, record, scene.textures, attenuation,
                                                  scattered, scatterPdf);
                    auto scatterEnd = std::chrono::high_resolution_clock::now();

                    double scatterCostNs =
                        std::chrono::duration<double, std::nano>(scatterEnd - scatterStart).count();

                    localCostSum[record.materialID] += scatterCostNs;
                    localCostCount[record.materialID] += 1;
                }
                else
                {
                    didScatter = material.scatter(incomingRay, record, scene.textures, attenuation,
                                                  scattered, scatterPdf);
                }

                if (didScatter)
                {
                    Color nextThroughput = throughput * attenuation;

                    // Wavefront Russian Roulette
                    // Rays with low throughput are terminated probabilistically
                    if (depth + 1 >= rrMinDepth)
                    {
                        float baseSurvivalProbability =
                            min(0.95f,
                                std::max({nextThroughput.x, nextThroughput.y, nextThroughput.z}));

                        float survivalProbability = baseSurvivalProbability;

                        if (enableCostAwareRR)
                        {
                            float relativeCost = static_cast<float>(
                                materialCostTracker.relativeCost(record.materialID));

                            survivalProbability =
                                std::clamp(baseSurvivalProbability / relativeCost, 0.05f, 0.95f);
                        }

                        if (randomFloat() > survivalProbability)
                            continue;

                        nextThroughput = nextThroughput / survivalProbability;
                    }

                    localNextRays.add(scattered, nextThroughput, Color(0.0f, 0.0f, 0.0f),
                                      pixelIndex, depth + 1, true, scatterPdf);
                }
            }
        });

    // Merge material and texture shading cost samples into the tracker
    vector<double> totalCostSum(materialCount, 0.0);
    vector<int> totalCostCount(materialCount, 0);
    vector<double> totalTextureCostSum(textureCount, 0.0);
    vector<int> totalTextureCostCount(textureCount, 0);

    threadLocalCostSum.combine_each(
        [&](const vector<double>& local)
        {
            for (int i = 0; i < materialCount; ++i)
                totalCostSum[i] += local[i];
        });

    threadLocalCostCount.combine_each(
        [&](const vector<int>& local)
        {
            for (int i = 0; i < materialCount; ++i)
                totalCostCount[i] += local[i];
        });

    threadLocalTextureCostSum.combine_each(
        [&](const vector<double>& local)
        {
            for (int i = 0; i < textureCount; ++i)
                totalTextureCostSum[i] += local[i];
        });

    threadLocalTextureCostCount.combine_each(
        [&](const vector<int>& local)
        {
            for (int i = 0; i < textureCount; ++i)
                totalTextureCostCount[i] += local[i];
        });

    for (int i = 0; i < materialCount; ++i)
    {
        if (totalCostCount[i] > 0)
            materialCostTracker.record(i, totalCostSum[i] / totalCostCount[i]);
    }

    for (int i = 0; i < textureCount; ++i)
    {
        if (totalTextureCostCount[i] > 0)
            textureCostTracker.record(i, totalTextureCostSum[i] / totalTextureCostCount[i]);
    }

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
            append(outputNextQueue.pdfs, local.pdfs);
        });
}

Color WavefrontRenderer::getSkyColor(const Ray& ray) const
{
    if (environmentMap.isLoaded())
        return environmentMap.sample(ray.direction);

    float blendFactor = 0.5f * (ray.direction.normalized().y + 1.0f);
    Color darkSky(0.02f, 0.02f, 0.05f);
    Color horizon(0.05f, 0.04f, 0.03f);

    return horizon * (1.0f - blendFactor) + darkSky * blendFactor;
}