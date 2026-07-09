#pragma once

#include <cmath>
#include <core/Scene.h>

inline float areaToAngleProbability(float area, float distance, float cosine)
{
    const float minCosine = 1e-4f;
    float safeCosine = std::max(std::abs(cosine), minCosine);

    return area * (distance * distance) / safeCosine;
}

inline float misBalance(float first, float second)
{
    float denominator = first + second;

    if (denominator <= 0.0f)
        return 0.0f;

    return first / denominator;
}

inline float lightAreaPdf(const Scene& scene)
{
    float totalArea = scene.totalEmissiveArea();

    if (totalArea <= 0.0f)
        return 0.0f;

    return 1.0f / totalArea;
}