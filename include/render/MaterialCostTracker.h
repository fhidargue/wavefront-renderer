#pragma once

#include <vector>
#include <atomic>
#include <algorithm>
#include <shading/Material.h>

class MaterialCostTracker
{
  public:
    explicit MaterialCostTracker(int materialCount)
        : averageCostNanoseconds(materialCount, 0.0), initialized(materialCount, false),
          sampleCount(materialCount, 0)
    {
    }

    // Called after timing a single scatter() call for a ray that hit
    // materialID, taking time to shade
    void record(int materialID, double costNanoseconds)
    {
        const double emaWeight = 0.05;

        sampleCount[materialID] += 1;

        if (!initialized[materialID])
        {
            averageCostNanoseconds[materialID] = costNanoseconds;
            initialized[materialID] = true;
            trackedMaterialCount += 1;
            cachedTotalCost += costNanoseconds;
        }
        else
        {
            double oldCost = averageCostNanoseconds[materialID];
            double newCost = (1.0 - emaWeight) * oldCost + emaWeight * costNanoseconds;

            averageCostNanoseconds[materialID] = newCost;
            cachedTotalCost += (newCost - oldCost);
        }
    }

    // Returns this material's average cost relative to the average cost
    // across all materials seen so far
    double relativeCost(int materialID) const
    {
        const int minimumSamplesForConfidence = 50;

        if (sampleCount[materialID] < minimumSamplesForConfidence || trackedMaterialCount == 0)
            return 1.0;

        double globalAverage = cachedTotalCost / trackedMaterialCount;

        if (globalAverage <= 0.0)
            return 1.0;

        return averageCostNanoseconds[materialID] / globalAverage;
    }

    void printStats(const std::vector<Material>& materials) const
    {
        std::cout << "\n========================================" << std::endl;
        std::cout << "  Material Cost Tracker Stats" << std::endl;
        std::cout << "========================================" << std::endl;

        for (size_t i = 0; i < averageCostNanoseconds.size(); ++i)
        {
            std::string materialName = (i < materials.size()) ? materials[i].uuid : "unknown";

            if (!initialized[i])
            {
                std::cout << "  Material " << i << " (" << materialName << ") : no samples yet"
                          << std::endl;
                continue;
            }

            std::cout << "  Material " << i << " (" << materialName << ")"
                      << " | avg cost: " << averageCostNanoseconds[i] << "ns"
                      << " | relative: " << relativeCost(static_cast<int>(i))
                      << " | samples: " << sampleCount[i] << std::endl;
        }

        std::cout << "========================================\n" << std::endl;
    }

  private:
    double cachedTotalCost = 0.0;
    int trackedMaterialCount = 0;

    std::vector<double> averageCostNanoseconds;
    std::vector<bool> initialized;
    std::vector<int> sampleCount;
};