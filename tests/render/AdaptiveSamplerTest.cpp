#include <gtest/gtest.h>
#include <render/AdaptiveSampler.h>

TEST(LuminanceTest, BlackIsZero)
{
    EXPECT_FLOAT_EQ(luminance(Color(0.0f, 0.0f, 0.0f)), 0.0f);
}

TEST(LuminanceTest, WhiteSumsToOne)
{
    EXPECT_NEAR(luminance(Color(1.0f, 1.0f, 1.0f)), 1.0f, 0.0001f);
}

TEST(LuminanceTest, GreenWeightedMoreThanBlue)
{
    float greenLuminance = luminance(Color(0.0f, 1.0f, 0.0f));
    float blueLuminance = luminance(Color(0.0f, 0.0f, 1.0f));

    EXPECT_GT(greenLuminance, blueLuminance);
}

TEST(AdaptiveSamplerTest, StartsWithAllPixelsActive)
{
    AdaptiveSampler sampler(4);

    for (int i = 0; i < 4; ++i)
        EXPECT_TRUE(sampler.isActive(i));

    EXPECT_EQ(sampler.activePixelCount(), 4);
}

TEST(AdaptiveSamplerTest, StartsWithZeroSamplesPerPixel)
{
    AdaptiveSampler sampler(3);

    for (int i = 0; i < 3; ++i)
        EXPECT_EQ(sampler.getSampleCount(i), 0);
}

TEST(AdaptiveSamplerTest, AddSampleIncrementsSampleCount)
{
    AdaptiveSampler sampler(1);
    sampler.addSample(0, 0.5f);
    sampler.addSample(0, 0.5f);

    EXPECT_EQ(sampler.getSampleCount(0), 2);
}

TEST(AdaptiveSamplerTest, SingleSampleMeanEqualsThatSample)
{
    AdaptiveSampler sampler(1);
    sampler.addSample(0, 0.7f);

    EXPECT_NEAR(sampler.pixelMean[0], 0.7f, 0.0001f);
}

TEST(AdaptiveSamplerTest, MeanOfIdenticalSamplesEqualsThatValue)
{
    AdaptiveSampler sampler(1);

    for (int i = 0; i < 50; ++i)
        sampler.addSample(0, 0.3f);

    EXPECT_NEAR(sampler.pixelMean[0], 0.3f, 0.0001f);
}

TEST(AdaptiveSamplerTest, IdenticalSamplesProduceZeroVariance)
{
    AdaptiveSampler sampler(1);

    for (int i = 0; i < 50; ++i)
        sampler.addSample(0, 0.3f);

    EXPECT_NEAR(sampler.pixelM2[0], 0.0f, 0.0001f);
}

TEST(AdaptiveSamplerTest, PixelsAreIndependentOfEachOther)
{
    AdaptiveSampler sampler(2);

    for (int i = 0; i < 10; ++i)
        sampler.addSample(0, 1.0f);

    EXPECT_EQ(sampler.getSampleCount(0), 10);
    EXPECT_EQ(sampler.getSampleCount(1), 0);
}

TEST(AdaptiveSamplerTest, StaysActiveBelowMinimumSampleFloor)
{
    AdaptiveSampler sampler(1);

    // All identical zero variance samples
    for (int i = 0; i < 10; ++i)
        sampler.addSample(0, 0.5f);

    sampler.updateConvergence();

    EXPECT_TRUE(sampler.isActive(0));
}

TEST(AdaptiveSamplerTest, ConvergesOnLowVariancePixelPastMinimumSamples)
{
    AdaptiveSampler sampler(1);

    for (int i = 0; i < 40; ++i)
        sampler.addSample(0, 0.5f);

    sampler.updateConvergence();

    EXPECT_FALSE(sampler.isActive(0));
    EXPECT_EQ(sampler.activePixelCount(), 0);
}

TEST(AdaptiveSamplerTest, StaysActiveOnHighVariancePixelPastMinimumSamples)
{
    AdaptiveSampler sampler(1);

    // Alternating high/low values
    for (int i = 0; i < 40; ++i)
        sampler.addSample(0, (i % 2 == 0) ? 0.0f : 10.0f);

    sampler.updateConvergence();

    EXPECT_TRUE(sampler.isActive(0));
}

TEST(AdaptiveSamplerTest, ConvergedPixelStaysConvergedOnRepeatedChecks)
{
    AdaptiveSampler sampler(1);

    for (int i = 0; i < 40; ++i)
        sampler.addSample(0, 0.5f);

    sampler.updateConvergence();
    EXPECT_FALSE(sampler.isActive(0));

    sampler.updateConvergence();
    EXPECT_FALSE(sampler.isActive(0));
}

TEST(AdaptiveSamplerTest, EachPixelConvergesIndependently)
{
    AdaptiveSampler sampler(2);

    for (int i = 0; i < 40; ++i)
    {
        sampler.addSample(0, 0.5f);
        sampler.addSample(1, (i % 2 == 0) ? 0.0f : 10.0f);
    }

    sampler.updateConvergence();

    EXPECT_FALSE(sampler.isActive(0));
    EXPECT_TRUE(sampler.isActive(1));
    EXPECT_EQ(sampler.activePixelCount(), 1);
}

TEST(AdaptiveSamplerTest, LooserThresholdConvergesSoonerThanStricterThreshold)
{
    AdaptiveSampler strictSampler(1, 0.001f);
    AdaptiveSampler looseSampler(1, 0.5f);

    // Small amount of noise around a stable mean
    for (int i = 0; i < 40; ++i)
    {
        float noisySample = 0.5f + ((i % 2 == 0) ? 0.02f : -0.02f);
        strictSampler.addSample(0, noisySample);
        looseSampler.addSample(0, noisySample);
    }

    strictSampler.updateConvergence();
    looseSampler.updateConvergence();

    EXPECT_TRUE(strictSampler.isActive(0));
    EXPECT_FALSE(looseSampler.isActive(0));
}