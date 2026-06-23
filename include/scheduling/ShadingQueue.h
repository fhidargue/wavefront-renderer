#pragma once

#include <vector>
#include <algorithm>
#include <tbb/parallel_sort.h>
#include <scheduling/RayQueue.h>
#include <core/HitRecord.h>

using std::vector;
using tbb::parallel_sort;

struct ShadingWork
{
    RayState rayState;
    HitRecord hitRecord;
};

enum class SchedulingPolicy
{
    None,
    MaterialAware,
    MaterialAwareParallel,
    TextureAware
};

struct ShadingQueue
{
    vector<ShadingWork> work;
    vector<int> sortedIndices;
    SchedulingPolicy policy;

    ShadingQueue(const SchedulingPolicy& policy = SchedulingPolicy::None) : policy(policy)
    {
    }

    void add(const RayState& rayState, const HitRecord& hitRecord)
    {
        work.push_back({rayState, hitRecord});
    }

    // Scheduler
    void schedule()
    {
        sortedIndices.resize(work.size());
        for (int i = 0; i < static_cast<int>(work.size()); ++i)
            sortedIndices[i] = i;

        if (policy == SchedulingPolicy::None)
            return;

        if (policy == SchedulingPolicy::MaterialAware)
        {
            std::sort(sortedIndices.begin(), sortedIndices.end(), [this](int a, int b)
                      { return work[a].hitRecord.materialID < work[b].hitRecord.materialID; });
            return;
        }

        if (policy == SchedulingPolicy::MaterialAwareParallel)
        {
            parallel_sort(sortedIndices.begin(), sortedIndices.end(), [this](int a, int b)
                          { return work[a].hitRecord.materialID < work[b].hitRecord.materialID; });
            return;
        }

        if (policy == SchedulingPolicy::TextureAware)
        {
            parallel_sort(sortedIndices.begin(), sortedIndices.end(),
                          [this](int a, int b)
                          {
                              if (work[a].hitRecord.materialID != work[b].hitRecord.materialID)
                                  return work[a].hitRecord.materialID <
                                         work[b].hitRecord.materialID;

                              return work[a].hitRecord.textureID < work[b].hitRecord.textureID;
                          });

            return;
        }
    }

    const ShadingWork& getSorted(int i) const
    {
        return work[sortedIndices[i]];
    }

    void clear()
    {
        work.clear();
    }

    int size() const
    {
        return static_cast<int>(work.size());
    }
};
