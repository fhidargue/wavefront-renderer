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

    std::vector<double> averageCostNanoseconds;
    std::vector<bool> initialized;
    std::vector<int> sampleCount;

  private:
    double cachedTotalCost = 0.0;
    int trackedMaterialCount = 0;
};