#pragma once

#include <vector>
#include <cmath>
#include <math/Vec3.h>

namespace AdaptiveSamplingConstants
{
constexpr int minimumSamplesBeforeConvergenceCheck = 32;
constexpr int convergenceCheckInterval = 16;
constexpr float defaultRelativeThreshold = 0.05f;
constexpr float relativeThresholdMeanFloor = 0.01f;
constexpr int besselCorrection = 1;

// Luma coefficients, used to collapse RGB into the single scalar
constexpr float luminanceWeightRed = 0.2126f;
constexpr float luminanceWeightGreen = 0.7152f;
constexpr float luminanceWeightBlue = 0.0722f;
} // namespace AdaptiveSamplingConstants

inline float luminance(const Color& color)
{
    using namespace AdaptiveSamplingConstants;

    return luminanceWeightRed * color.x + luminanceWeightGreen * color.y +
           luminanceWeightBlue * color.z;
}

class AdaptiveSampler
{
  public:
    explicit AdaptiveSampler(
        int pixelCount,
        float relativeThreshold = AdaptiveSamplingConstants::defaultRelativeThreshold)
        : pixelMean(pixelCount, 0.0f), pixelM2(pixelCount, 0.0f), pixelSampleCount(pixelCount, 0),
          pixelActive(pixelCount, true), relativeThreshold(relativeThreshold)
    {
    }

    // Feeds one sample luminance contribution into pixel
    void addSample(int pixelIndex, float sampleLuminance)
    {
        pixelSampleCount[pixelIndex] += 1;
        int sampleCountForPixel = pixelSampleCount[pixelIndex];

        float meanBeforeUpdate = pixelMean[pixelIndex];
        float deviationFromOldMean = sampleLuminance - meanBeforeUpdate;

        pixelMean[pixelIndex] += deviationFromOldMean / static_cast<float>(sampleCountForPixel);

        float deviationFromNewMean = sampleLuminance - pixelMean[pixelIndex];
        pixelM2[pixelIndex] += deviationFromOldMean * deviationFromNewMean;
    }

    void updateConvergence()
    {
        using namespace AdaptiveSamplingConstants;

        for (int pixelIndex = 0; pixelIndex < static_cast<int>(pixelActive.size()); ++pixelIndex)
        {
            if (!pixelActive[pixelIndex])
                continue;

            int sampleCountForPixel = pixelSampleCount[pixelIndex];

            if (sampleCountForPixel < minimumSamplesBeforeConvergenceCheck)
                continue;

            float sampleVariance =
                pixelM2[pixelIndex] / static_cast<float>(sampleCountForPixel - besselCorrection);
            float standardErrorOfMean =
                std::sqrt(sampleVariance / static_cast<float>(sampleCountForPixel));

            float convergenceThreshold =
                relativeThreshold * (pixelMean[pixelIndex] + relativeThresholdMeanFloor);

            if (standardErrorOfMean < convergenceThreshold)
                pixelActive[pixelIndex] = false;
        }
    }

    bool isActive(int pixelIndex) const
    {
        return pixelActive[pixelIndex];
    }

    int getSampleCount(int pixelIndex) const
    {
        return pixelSampleCount[pixelIndex];
    }

    int activePixelCount() const
    {
        int count = 0;
        for (bool active : pixelActive)
            count += active ? 1 : 0;

        return count;
    }

    std::vector<float> pixelMean;
    std::vector<float> pixelM2;
    std::vector<int> pixelSampleCount;
    std::vector<bool> pixelActive;
    float relativeThreshold;
};