#pragma once

#include <unordered_set>
#include <vector>
#include <algorithm>
#include <numeric>
#include <type_traits>
#include <cstdint>
#include <tbb/parallel_sort.h>
#include <scheduling/RayQueue.h>
#include <core/HitRecord.h>
#include <core/PrintUtils.h>
#include <render/CostTracker.h>
#include <render/AdaptiveSampler.h>
#include <shading/Material.h>
#include <shading/Texture.h>

enum class SchedulingPolicy
{
    None,
    MaterialAware,
    TextureAware,
    CostBenefitAware,
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

    void schedule(const std::vector<Material>& materials, const std::vector<Texture>& textures,
                  const CostTracker* costTracker = nullptr,
                  const CostTracker* textureCostTracker = nullptr)
    {
        const int n = size();
        sortedIndices.resize(n);
        std::iota(sortedIndices.begin(), sortedIndices.end(), 0);

        if (policy == SchedulingPolicy::None)
            return;

        if (policy == SchedulingPolicy::CostBenefitAware)
        {
            scheduleCostBenefitAware(materials, costTracker, textureCostTracker);
            return;
        }

        std::vector<int> typeOrder = computeTypeOrder(materials, textures, costTracker);
        std::unordered_map<int, int> typeRank;
        for (int rank = 0; rank < static_cast<int>(typeOrder.size()); ++rank)
            typeRank[typeOrder[rank]] = rank;

        tbb::parallel_sort(
            sortedIndices.begin(), sortedIndices.end(),
            [this, &materials, &textures, &typeRank, costTracker](int a, int b)
            {
                int rankA = typeRank[static_cast<int>(materials[materialIDs[a]].type)];
                int rankB = typeRank[static_cast<int>(materials[materialIDs[b]].type)];

                if (rankA != rankB)
                    return rankA < rankB;

                if (policy == SchedulingPolicy::TextureAware)
                {
                    int tidA = textureIDs[a];
                    int tidB = textureIDs[b];
                    size_t sizeA = (tidA >= 0 && tidA < static_cast<int>(textures.size()))
                                       ? textures[tidA].memoryBytes()
                                       : 0;
                    size_t sizeB = (tidB >= 0 && tidB < static_cast<int>(textures.size()))
                                       ? textures[tidB].memoryBytes()
                                       : 0;

                    return sizeA > sizeB;
                }

                double costA = costTracker ? costTracker->relativeCost(materialIDs[a]) : 1.0;
                double costB = costTracker ? costTracker->relativeCost(materialIDs[b]) : 1.0;

                return costA > costB;
            });
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

    std::vector<int> computeTypeOrder(const std::vector<Material>& materials,
                                      const std::vector<Texture>& textures,
                                      const CostTracker* costTracker) const
    {
        std::unordered_map<int, int> rayCountByType;
        std::unordered_map<int, size_t> memoryByType;
        std::unordered_map<int, double> costByType;

        for (int i = 0; i < size(); ++i)
        {
            int typeIndex = static_cast<int>(materials[materialIDs[i]].type);
            rayCountByType[typeIndex]++;

            int tid = textureIDs[i];

            if (tid >= 0 && tid < static_cast<int>(textures.size()))
                memoryByType[typeIndex] += textures[tid].memoryBytes();

            double cost = costTracker ? costTracker->relativeCost(materialIDs[i]) : 1.0;
            costByType[typeIndex] += cost;
        }

        std::vector<int> typeOrder;
        for (auto& [typeIndex, count] : rayCountByType)
            typeOrder.push_back(typeIndex);

        switch (policy)
        {
        case SchedulingPolicy::MaterialAware:
            std::sort(typeOrder.begin(), typeOrder.end(),
                      [&](int a, int b)
                      {
                          if (rayCountByType[a] != rayCountByType[b])
                              return rayCountByType[a] > rayCountByType[b];
                          return a > b;
                      });
            break;
        case SchedulingPolicy::TextureAware:
            std::sort(typeOrder.begin(), typeOrder.end(),
                      [&](int a, int b) { return memoryByType[a] > memoryByType[b]; });
            break;

        case SchedulingPolicy::CostBenefitAware:
            std::sort(typeOrder.begin(), typeOrder.end(),
                      [&](int a, int b) { return costByType[a] > costByType[b]; });
            break;

        default:
            break;
        }

        return typeOrder;
    }

    void printQueueComposition(int depth, const std::vector<Material>& materials,
                               const std::vector<Texture>& textures) const
    {
        static const char* typeNames[] = {"Diffuse",   "Metal", "Emissive",
                                          "SpotLight", "Glass", "Plastic"};

        std::unordered_map<int, int> countByMaterial;

        for (int i = 0; i < size(); ++i)
            countByMaterial[materialIDs[i]]++;

        auto materialLine = [&](int mid, int count) -> std::string
        {
            const Material& mat = materials[mid];
            std::string materialName = mat.uuid;
            int tid = mat.textureID;
            std::string texName =
                (tid >= 0 && tid < static_cast<int>(textures.size())) ? textures[tid].name : "none";

            return "  materialID=" + std::to_string(mid) +
                   "  materialName=" + materialName.substr(materialName.rfind('/') + 1) +
                   "  textureID=" + std::to_string(tid) +
                   "  textureName=" + texName.substr(texName.rfind('|') + 1) +
                   "  rays=" + std::to_string(count);
        };

        std::vector<std::string> lines;

        if (policy == SchedulingPolicy::None)
        {
            std::vector<int> seenOrder;
            std::unordered_set<int> seen;
            for (int i = 0; i < size(); ++i)
                if (seen.insert(materialIDs[i]).second)
                    seenOrder.push_back(materialIDs[i]);

            for (int mid : seenOrder)
                lines.push_back(materialLine(mid, countByMaterial[mid]));
        }
        else
        {
            std::unordered_map<int, std::vector<std::pair<int, int>>> raysByType;
            for (auto& [mid, count] : countByMaterial)
            {
                int typeIndex = static_cast<int>(materials[mid].type);
                raysByType[typeIndex].push_back({mid, count});
            }

            std::vector<int> typeOrder = computeTypeOrder(materials, textures, nullptr);

            for (int typeIndex : typeOrder)
            {
                auto& entries = raysByType[typeIndex];
                int totalRaysForType = 0;
                for (auto& [mid, count] : entries)
                    totalRaysForType += count;

                const char* typeName =
                    (typeIndex >= 0 && typeIndex < 6) ? typeNames[typeIndex] : "Unknown";
                lines.push_back(std::string(typeName) + " queue (" +
                                std::to_string(totalRaysForType) + " rays)");

                for (auto& [mid, count] : entries)
                    lines.push_back(materialLine(mid, count));
            }
        }

        printStatsBlock("Shading Queue Composition (" + std::to_string(size()) + " rays)", lines);
    }

  private:
    void scheduleCostBenefitAware(const std::vector<Material>& materials,
                                  const CostTracker* costTracker,
                                  const CostTracker* textureCostTracker)
    {
        const int n = size();

        std::unordered_map<int, int> rayCountByMaterial;
        std::unordered_map<int, double> throughputSumByMaterial;

        for (int i = 0; i < n; ++i)
        {
            int materialID = materialIDs[i];
            Color throughput(throughputsR[i], throughputsG[i], throughputsB[i]);

            rayCountByMaterial[materialID] += 1;
            throughputSumByMaterial[materialID] += luminance(throughput);
        }

        constexpr double minimumCost = 1e-6; // guards against divisions by zero
        std::unordered_map<int, double> priorityByMaterial;

        for (const auto& entry : rayCountByMaterial)
        {
            int materialID = entry.first;
            double benefit = throughputSumByMaterial[materialID];

            double shadingCost = costTracker ? costTracker->relativeCost(materialID) : 1.0;
            int textureID = materials[materialID].textureID;
            double textureCost = (textureCostTracker && textureID >= 0)
                                     ? textureCostTracker->relativeCost(textureID)
                                     : 0.0;
            double combinedCost = shadingCost + textureCost;

            priorityByMaterial[materialID] = benefit / std::max(combinedCost, minimumCost);
        }

        // Compute scene bounds of hit points for Morton quantisation
        float minX = *std::min_element(hitPointsX.begin(), hitPointsX.end());
        float minY = *std::min_element(hitPointsY.begin(), hitPointsY.end());
        float minZ = *std::min_element(hitPointsZ.begin(), hitPointsZ.end());
        float maxX = *std::max_element(hitPointsX.begin(), hitPointsX.end());
        float maxY = *std::max_element(hitPointsY.begin(), hitPointsY.end());
        float maxZ = *std::max_element(hitPointsZ.begin(), hitPointsZ.end());

        // Compute Morton codes once upfront so sorting only compares precomputed values
        std::vector<uint64_t> mortonCodes(n);

        for (int i = 0; i < n; ++i)
            mortonCodes[i] = computeMortonCode(hitPointsX[i], hitPointsY[i], hitPointsZ[i], minX,
                                               minY, minZ, maxX, maxY, maxZ);

        // Sorting happens in three ways:
        // 1. Shader type: groups Diffuse / Metal / Glass / etc
        // 2. Priority: within each type, high throughput/cost benefit first
        // 3. Morton code: within same priority, spatially close hits together (BVH locality
        // enhancement)
        std::stable_sort(sortedIndices.begin(), sortedIndices.end(),
                         [&](int a, int b)
                         {
                             int typeA = static_cast<int>(materials[materialIDs[a]].type);
                             int typeB = static_cast<int>(materials[materialIDs[b]].type);

                             if (typeA != typeB)
                                 return typeA < typeB;

                             double priorityA = priorityByMaterial[materialIDs[a]];
                             double priorityB = priorityByMaterial[materialIDs[b]];

                             if (priorityA != priorityB)
                                 return priorityA > priorityB;

                             return mortonCodes[a] < mortonCodes[b];
                         });
    }

    // Encodes a 3D hit point into a 30bit Morton code by inserting 10 bits per axis
    static uint64_t computeMortonCode(float x, float y, float z, float minX, float minY, float minZ,
                                      float maxX, float maxY, float maxZ)
    {
        static constexpr uint32_t MORTON_RESOLUTION = 1023;
        static constexpr uint64_t MASK_10_BITS = 0x3ff;
        static constexpr uint64_t MASK_SPREAD_16 = 0x30000ffULL;
        static constexpr uint64_t MASK_SPREAD_8 = 0x300f00fULL;
        static constexpr uint64_t MASK_SPREAD_4 = 0x30c30c3ULL;
        static constexpr uint64_t MASK_SPREAD_2 = 0x9249249ULL;

        auto quantise = [](float value, float lo, float hi) -> uint32_t
        {
            float normalizedPosition = (hi > lo) ? (value - lo) / (hi - lo) : 0.0f;

            return static_cast<uint32_t>(std::clamp(normalizedPosition, 0.0f, 1.0f) *
                                         MORTON_RESOLUTION);
        };

        // Spreads a 10bit integer across 30 bits by inserting two zero bits between each bit
        auto spread = [](uint32_t value) -> uint64_t
        {
            uint64_t expandedBits = value & MASK_10_BITS;
            expandedBits = (expandedBits | expandedBits << 16) & MASK_SPREAD_16;
            expandedBits = (expandedBits | expandedBits << 8) & MASK_SPREAD_8;
            expandedBits = (expandedBits | expandedBits << 4) & MASK_SPREAD_4;
            expandedBits = (expandedBits | expandedBits << 2) & MASK_SPREAD_2;

            return expandedBits;
        };

        static constexpr int X_BIT_OFFSET = 0;
        static constexpr int Y_BIT_OFFSET = 1;
        static constexpr int Z_BIT_OFFSET = 2;

        return (spread(quantise(x, minX, maxX)) << X_BIT_OFFSET) |
               (spread(quantise(y, minY, maxY)) << Y_BIT_OFFSET) |
               (spread(quantise(z, minZ, maxZ)) << Z_BIT_OFFSET);
    }
};
