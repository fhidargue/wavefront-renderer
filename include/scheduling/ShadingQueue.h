#pragma once

#include <vector>
#include <algorithm>
#include <numeric>
#include <type_traits>
#include <tbb/parallel_sort.h>
#include <scheduling/RayQueue.h>
#include <core/HitRecord.h>

enum class SchedulingPolicy
{
    None,
    MaterialAware,
    MaterialAwareParallel,
    TextureAware
};

// DOD Structure of Arrays ShadingQueue
struct ShadingQueue
{
    // Hit geometry
    std::vector<float> hitPointsX;
    std::vector<float> hitPointsY;
    std::vector<float> hitPointsZ;
    std::vector<float> hitNormalsX;
    std::vector<float> hitNormalsY;
    std::vector<float> hitNormalsZ;
    std::vector<float> hitDistances;
    std::vector<float> hitU;
    std::vector<float> hitV;
    std::vector<int> hitTriangleIndex;

    std::vector<int> materialIDs;
    std::vector<int> textureIDs;

    // Ray state at hit point
    std::vector<float> originsX, originsY, originsZ;
    std::vector<float> dirsX, dirsY, dirsZ;
    std::vector<float> throughputsR, throughputsG, throughputsB;
    std::vector<int> pixelIndices;
    std::vector<int> depths;
    std::vector<float> pdfs;

    // Sort order
    std::vector<int> sortedIndices;

    SchedulingPolicy policy;

    explicit ShadingQueue(SchedulingPolicy p = SchedulingPolicy::None) : policy(p)
    {
    }

    void add(const RayQueue& rays, int rayIndex, const HitRecord& hit)
    {
        hitPointsX.push_back(hit.point.x);
        hitPointsY.push_back(hit.point.y);
        hitPointsZ.push_back(hit.point.z);
        hitNormalsX.push_back(hit.normal.x);
        hitNormalsY.push_back(hit.normal.y);
        hitNormalsZ.push_back(hit.normal.z);
        hitDistances.push_back(hit.distance);
        hitU.push_back(hit.u);
        hitV.push_back(hit.v);
        hitTriangleIndex.push_back(hit.triangleIndex);
        materialIDs.push_back(hit.materialID);
        textureIDs.push_back(hit.textureID);

        originsX.push_back(rays.originsX[rayIndex]);
        originsY.push_back(rays.originsY[rayIndex]);
        originsZ.push_back(rays.originsZ[rayIndex]);
        dirsX.push_back(rays.dirsX[rayIndex]);
        dirsY.push_back(rays.dirsY[rayIndex]);
        dirsZ.push_back(rays.dirsZ[rayIndex]);
        throughputsR.push_back(rays.throughputsR[rayIndex]);
        throughputsG.push_back(rays.throughputsG[rayIndex]);
        throughputsB.push_back(rays.throughputsB[rayIndex]);
        pixelIndices.push_back(rays.pixelIndices[rayIndex]);
        depths.push_back(rays.depths[rayIndex]);
        pdfs.push_back(rays.pdfs[rayIndex]);
    }

    void schedule()
    {
        const int n = size();

        sortedIndices.resize(n);
        std::iota(sortedIndices.begin(), sortedIndices.end(), 0);

        auto materialCompare = [this](int a, int b) { return materialIDs[a] < materialIDs[b]; };

        auto textureCompare = [this](int a, int b)
        {
            if (materialIDs[a] != materialIDs[b])
                return materialIDs[a] < materialIDs[b];

            return textureIDs[a] < textureIDs[b];
        };

        switch (policy)
        {
        case SchedulingPolicy::None:
            break;

        case SchedulingPolicy::MaterialAware:
            std::sort(sortedIndices.begin(), sortedIndices.end(), materialCompare);
            break;

        case SchedulingPolicy::MaterialAwareParallel:
            tbb::parallel_sort(sortedIndices.begin(), sortedIndices.end(), materialCompare);
            break;

        case SchedulingPolicy::TextureAware:
            tbb::parallel_sort(sortedIndices.begin(), sortedIndices.end(), textureCompare);
            break;

        default:
            break;
        }
    }

    // Reconstruct HitRecord at sorted position
    HitRecord getHitRecord(int sortedPos) const
    {
        int i = sortedIndices[sortedPos];
        HitRecord record;
        record.point = Point3(hitPointsX[i], hitPointsY[i], hitPointsZ[i]);
        record.normal = Vec3(hitNormalsX[i], hitNormalsY[i], hitNormalsZ[i]);
        record.distance = hitDistances[i];
        record.u = hitU[i];
        record.v = hitV[i];
        record.triangleIndex = hitTriangleIndex[i];
        record.materialID = materialIDs[i];
        record.textureID = textureIDs[i];

        return record;
    }

    // Reconstruct Ray at sorted position
    Ray getRay(int sortedPos) const
    {
        int i = sortedIndices[sortedPos];

        return Ray(Point3(originsX[i], originsY[i], originsZ[i]),
                   Vec3(dirsX[i], dirsY[i], dirsZ[i]));
    }

    // Get throughput at sorted position
    Color getThroughput(int sortedPos) const
    {
        int i = sortedIndices[sortedPos];

        return Color(throughputsR[i], throughputsG[i], throughputsB[i]);
    }

    int getPixelIndex(int sortedPos) const
    {
        return pixelIndices[sortedIndices[sortedPos]];
    }

    int getDepth(int sortedPos) const
    {
        return depths[sortedIndices[sortedPos]];
    }

    float getPdf(int sortedPos) const
    {
        return pdfs[sortedIndices[sortedPos]];
    }

    void clear()
    {
        hitPointsX.clear();
        hitPointsY.clear();
        hitPointsZ.clear();
        hitNormalsX.clear();
        hitNormalsY.clear();
        hitNormalsZ.clear();
        hitDistances.clear();
        hitU.clear();
        hitV.clear();
        hitTriangleIndex.clear();
        materialIDs.clear();
        textureIDs.clear();
        originsX.clear();
        originsY.clear();
        originsZ.clear();
        dirsX.clear();
        dirsY.clear();
        dirsZ.clear();
        throughputsR.clear();
        throughputsG.clear();
        throughputsB.clear();
        pixelIndices.clear();
        depths.clear();
        pdfs.clear();
        sortedIndices.clear();
    }

    int size() const
    {
        return static_cast<int>(materialIDs.size());
    }

    bool empty() const
    {
        return materialIDs.empty();
    }

    // Physically reorders every ray array into sorted order, turning the
    // logical sort in sortedIndices into real memory coherent layout
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

        permuteArray(hitPointsX);
        permuteArray(hitPointsY);
        permuteArray(hitPointsZ);
        permuteArray(hitNormalsX);
        permuteArray(hitNormalsY);
        permuteArray(hitNormalsZ);
        permuteArray(hitDistances);
        permuteArray(hitU);
        permuteArray(hitV);
        permuteArray(hitTriangleIndex);
        permuteArray(materialIDs);
        permuteArray(textureIDs);
        permuteArray(originsX);
        permuteArray(originsY);
        permuteArray(originsZ);
        permuteArray(dirsX);
        permuteArray(dirsY);
        permuteArray(dirsZ);
        permuteArray(throughputsR);
        permuteArray(throughputsG);
        permuteArray(throughputsB);
        permuteArray(pixelIndices);
        permuteArray(depths);
        permuteArray(pdfs);

        // Data will now be physically sorted
        for (int position = 0; position < rayCount; ++position)
            sortedIndices[position] = position;
    }
};
