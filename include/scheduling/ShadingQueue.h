#pragma once

#include <vector>
#include <algorithm>
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

    std::vector<int> materialIDs;
    std::vector<int> textureIDs;

    // Ray state at hit point
    std::vector<float> originsX, originsY, originsZ;
    std::vector<float> dirsX, dirsY, dirsZ;
    std::vector<float> throughputsR, throughputsG, throughputsB;
    std::vector<int> pixelIndices;
    std::vector<int> depths;

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
    }

    void schedule()
    {
        int n = size();
        sortedIndices.resize(n);

        for (int i = 0; i < n; ++i)
            sortedIndices[i] = i;

        if (policy == SchedulingPolicy::None)
            return;

        if (policy == SchedulingPolicy::MaterialAware)
        {
            std::sort(sortedIndices.begin(), sortedIndices.end(),
                      [this](int a, int b) { return materialIDs[a] < materialIDs[b]; });

            return;
        }

        if (policy == SchedulingPolicy::MaterialAwareParallel)
        {
            tbb::parallel_sort(sortedIndices.begin(), sortedIndices.end(),
                               [this](int a, int b) { return materialIDs[a] < materialIDs[b]; });

            return;
        }

        if (policy == SchedulingPolicy::TextureAware)
        {
            tbb::parallel_sort(sortedIndices.begin(), sortedIndices.end(),
                               [this](int a, int b)
                               {
                                   if (materialIDs[a] != materialIDs[b])
                                       return materialIDs[a] < materialIDs[b];

                                   return textureIDs[a] < textureIDs[b];
                               });

            return;
        }
    }

    // Reconstruct HitRecord at sorted position
    HitRecord getHitRecord(int sortedPos) const
    {
        int i = sortedIndices[sortedPos];
        HitRecord record;
        record.point = Point3(hitPointsX[i], hitPointsY[i], hitPointsZ[i]);
        record.normal = Vec3(hitNormalsX[i], hitNormalsY[i], hitNormalsZ[i]);
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

    void clear()
    {
        hitPointsX.clear();
        hitPointsY.clear();
        hitPointsZ.clear();
        hitNormalsX.clear();
        hitNormalsY.clear();
        hitNormalsZ.clear();
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
};