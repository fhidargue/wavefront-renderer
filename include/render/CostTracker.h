#pragma once

#include <vector>
#include <atomic>
#include <algorithm>

// Exponential Moving Average (EMA) based cost tracker
class CostTracker
{
  public:
    explicit CostTracker(int itemCount)
        : averageCostNanoseconds(itemCount, 0.0), initialized(itemCount, false),
          sampleCount(itemCount, 0)
    {
    }

    // Called after timing a single shading operation for the given ID
    void record(int id, double costNanoseconds)
    {
        const double emaWeight = 0.05;

        sampleCount[id] += 1;

        if (!initialized[id])
        {
            averageCostNanoseconds[id] = costNanoseconds;
            initialized[id] = true;
            trackedItemCount += 1;
            cachedTotalCost += costNanoseconds;
        }
        else
        {
            double oldCost = averageCostNanoseconds[id];
            double newCost = (1.0 - emaWeight) * oldCost + emaWeight * costNanoseconds;

            averageCostNanoseconds[id] = newCost;
            cachedTotalCost += (newCost - oldCost);
        }
    }

    // Returns this item's average cost relative to the average cost across all items
    double relativeCost(int id) const
    {
        const int minimumSamplesForConfidence = 50;

        if (sampleCount[id] < minimumSamplesForConfidence || trackedItemCount == 0)
            return 1.0;

        double globalAverage = cachedTotalCost / trackedItemCount;

        if (globalAverage <= 0.0)
            return 1.0;

        return averageCostNanoseconds[id] / globalAverage;
    }

    std::vector<double> averageCostNanoseconds;
    std::vector<bool> initialized;
    std::vector<int> sampleCount;

  private:
    double cachedTotalCost = 0.0;
    int trackedItemCount = 0;
};