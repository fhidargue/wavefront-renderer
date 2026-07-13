#pragma once

#include <vector>
#include <cstdint>
#include <algorithm>
#include <numeric>
#include <type_traits>
#include <tbb/parallel_sort.h>
#include <core/Ray.h>
#include <math/Vec3.h>

// TODO: add references for Morton z-ordering encoding for spatial ray sorting.
// Pharr, Kolb, Gershbein &
// Hanrahan (1997) "Rendering Complex Scenes with Memory-Coherent Ray
// Tracing" and Wald, Slusallek, Benthin & Wagner
// (2001) "Interactive Distributed Ray Tracing of Highly Complex Models"
namespace RaySortConstants
{
constexpr int mortonBitsPerAxis = 10;
constexpr uint32_t mortonGridResolution = 1u << mortonBitsPerAxis; // 1024 cells per axis
constexpr int mortonCodeBits = mortonBitsPerAxis * 3;

// Ray direction sign bits that make up the 3-bit octant key
constexpr uint64_t negativeXBit = 1u;
constexpr uint64_t negativeYBit = 2u;
constexpr uint64_t negativeZBit = 4u;

// Smallest bounding box extent
constexpr float minimumBoundsExtent = 1e-6f;

// Standard Morton bit-spread masks
constexpr uint32_t mortonLow10BitsMask = 0x3FFu;
constexpr uint32_t mortonSpreadMask1 = 0x30000FFu;
constexpr uint32_t mortonSpreadMask2 = 0x300F00Fu;
constexpr uint32_t mortonSpreadMask3 = 0x30C30C3u;
constexpr uint32_t mortonSpreadMask4 = 0x9249249u;
constexpr int mortonSpreadShift1 = 16;
constexpr int mortonSpreadShift2 = 8;
constexpr int mortonSpreadShift3 = 4;
constexpr int mortonSpreadShift4 = 2;
} // namespace RaySortConstants

// Pads two zero bits between each of the low 10 bits of value, so three spread values can
// be organized together without colliding
inline uint32_t spreadBits10(uint32_t value)
{
    using namespace RaySortConstants;

    value &= mortonLow10BitsMask;
    value = (value | (value << mortonSpreadShift1)) & mortonSpreadMask1;
    value = (value | (value << mortonSpreadShift2)) & mortonSpreadMask2;
    value = (value | (value << mortonSpreadShift3)) & mortonSpreadMask3;
    value = (value | (value << mortonSpreadShift4)) & mortonSpreadMask4;
    return value;
}

inline uint32_t mortonEncode3(uint32_t gridX, uint32_t gridY, uint32_t gridZ)
{
    return spreadBits10(gridX) | (spreadBits10(gridY) << 1) | (spreadBits10(gridZ) << 2);
}

// DOD Structure of Arrays RayQueue
struct RayQueue
{
    // Intersection stage reads these
    std::vector<float> originsX;
    std::vector<float> originsY;
    std::vector<float> originsZ;
    std::vector<float> dirsX;
    std::vector<float> dirsY;
    std::vector<float> dirsZ;

    // Shading stage reads these
    std::vector<float> throughputsR;
    std::vector<float> throughputsG;
    std::vector<float> throughputsB;
    std::vector<float> accumLightR;
    std::vector<float> accumLightG;
    std::vector<float> accumLightB;

    // Both stages need these
    std::vector<int> pixelIndices;
    std::vector<int> depths;
    std::vector<bool> alive;

    // PDF the ray direction that was sampled
    std::vector<float> pdfs;

    // Sort order for traversal coherence
    std::vector<int> sortedIndices;

    void add(const Ray& ray, const Color& throughput, const Color& accumLight, int pixelIndex,
             int depth, bool isAlive, float pdf)
    {
        originsX.push_back(ray.origin.x);
        originsY.push_back(ray.origin.y);
        originsZ.push_back(ray.origin.z);
        dirsX.push_back(ray.direction.x);
        dirsY.push_back(ray.direction.y);
        dirsZ.push_back(ray.direction.z);
        throughputsR.push_back(throughput.x);
        throughputsG.push_back(throughput.y);
        throughputsB.push_back(throughput.z);
        accumLightR.push_back(accumLight.x);
        accumLightG.push_back(accumLight.y);
        accumLightB.push_back(accumLight.z);
        pixelIndices.push_back(pixelIndex);
        depths.push_back(depth);
        alive.push_back(isAlive);
        pdfs.push_back(pdf);
    }

    void addPrimary(const Ray& ray, int pixelIndex)
    {
        add(ray, Color(1.0f, 1.0f, 1.0f), Color(0.0f, 0.0f, 0.0f), pixelIndex, 0, true, -1.0f);
    }

    Ray getRay(int i) const
    {
        return Ray(Point3(originsX[i], originsY[i], originsZ[i]),
                   Vec3(dirsX[i], dirsY[i], dirsZ[i]));
    }

    Color getThroughput(int i) const
    {
        return Color(throughputsR[i], throughputsG[i], throughputsB[i]);
    }

    Color getAccumLight(int i) const
    {
        return Color(accumLightR[i], accumLightG[i], accumLightB[i]);
    }

    // Creates a sorting key that groups nearby rays with similar directions
    uint64_t sortKey(int rayIndex, const Point3& sceneMin, const Point3& sceneMax) const
    {
        using namespace RaySortConstants;

        uint64_t octant = (dirsX[rayIndex] < 0.0f ? negativeXBit : 0u) |
                          (dirsY[rayIndex] < 0.0f ? negativeYBit : 0u) |
                          (dirsZ[rayIndex] < 0.0f ? negativeZBit : 0u);

        Vec3 boundsExtent(std::max(sceneMax.x - sceneMin.x, minimumBoundsExtent),
                          std::max(sceneMax.y - sceneMin.y, minimumBoundsExtent),
                          std::max(sceneMax.z - sceneMin.z, minimumBoundsExtent));

        auto quantizeToGrid = [](float value, float minValue, float extentValue)
        {
            float normalized = (value - minValue) / extentValue;
            normalized = std::clamp(normalized, 0.0f, 1.0f);

            return static_cast<uint32_t>(normalized * (mortonGridResolution - 1));
        };

        uint32_t gridX = quantizeToGrid(originsX[rayIndex], sceneMin.x, boundsExtent.x);
        uint32_t gridY = quantizeToGrid(originsY[rayIndex], sceneMin.y, boundsExtent.y);
        uint32_t gridZ = quantizeToGrid(originsZ[rayIndex], sceneMin.z, boundsExtent.z);

        uint64_t mortonCode = mortonEncode3(gridX, gridY, gridZ);

        return (octant << mortonCodeBits) | mortonCode;
    }

    // Sorts indices only; call materialize() to apply the reorder
    void schedule(const Point3& sceneMin, const Point3& sceneMax)
    {
        const int rayCount = size();

        sortedIndices.resize(rayCount);
        std::iota(sortedIndices.begin(), sortedIndices.end(), 0);

        // Precomputed once so the sort comparator isn't redoing the Morton math on every comparison
        std::vector<uint64_t> keys(rayCount);
        for (int rayIndex = 0; rayIndex < rayCount; ++rayIndex)
            keys[rayIndex] = sortKey(rayIndex, sceneMin, sceneMax);

        tbb::parallel_sort(sortedIndices.begin(), sortedIndices.end(),
                           [&keys](int indexA, int indexB) { return keys[indexA] < keys[indexB]; });
    }

    // Reorders all arrays into sorted order for coherent memory access
    void materialize()
    {
        const int rayCount = size();

        auto permuteArray = [&](auto& array)
        {
            std::decay_t<decltype(array)> reordered(rayCount);

            for (int position = 0; position < rayCount; ++position)
                reordered[position] = array[sortedIndices[position]];

            array = std::move(reordered);
        };

        permuteArray(originsX);
        permuteArray(originsY);
        permuteArray(originsZ);
        permuteArray(dirsX);
        permuteArray(dirsY);
        permuteArray(dirsZ);
        permuteArray(throughputsR);
        permuteArray(throughputsG);
        permuteArray(throughputsB);
        permuteArray(accumLightR);
        permuteArray(accumLightG);
        permuteArray(accumLightB);
        permuteArray(pixelIndices);
        permuteArray(depths);
        permuteArray(alive);
        permuteArray(pdfs);

        for (int position = 0; position < rayCount; ++position)
            sortedIndices[position] = position;
    }

    void clear()
    {
        originsX.clear();
        originsY.clear();
        originsZ.clear();
        dirsX.clear();
        dirsY.clear();
        dirsZ.clear();
        throughputsR.clear();
        throughputsG.clear();
        throughputsB.clear();
        accumLightR.clear();
        accumLightG.clear();
        accumLightB.clear();
        pixelIndices.clear();
        depths.clear();
        alive.clear();
        pdfs.clear();
    }

    int size() const
    {
        return static_cast<int>(originsX.size());
    }

    bool empty() const
    {
        return originsX.empty();
    }
};