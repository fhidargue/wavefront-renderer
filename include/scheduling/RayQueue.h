#pragma once

#include <vector>
#include <core/Ray.h>
#include <math/Vec3.h>

// DOD Structure of Arrays RayQueue
struct RayQueue
{
    // Intersection stage reads these
    std::vector<float> originsX;
    std::vector<float> originsY;
    std::vector<float> originsZ;
    std::vector<float> dirsX;
    std::vector<float> dirsY;
    std::vector<float> dirsZ;

    // Shading stage reads these
    std::vector<float> throughputsR;
    std::vector<float> throughputsG;
    std::vector<float> throughputsB;
    std::vector<float> accumLightR;
    std::vector<float> accumLightG;
    std::vector<float> accumLightB;

    // Both stages need these
    std::vector<int> pixelIndices;
    std::vector<int> depths;
    std::vector<bool> alive;

    void add(const Ray& ray, const Color& throughput, const Color& accumLight, int pixelIndex,
             int depth, bool isAlive)
    {
        originsX.push_back(ray.origin.x);
        originsY.push_back(ray.origin.y);
        originsZ.push_back(ray.origin.z);
        dirsX.push_back(ray.direction.x);
        dirsY.push_back(ray.direction.y);
        dirsZ.push_back(ray.direction.z);
        throughputsR.push_back(throughput.x);
        throughputsG.push_back(throughput.y);
        throughputsB.push_back(throughput.z);
        accumLightR.push_back(accumLight.x);
        accumLightG.push_back(accumLight.y);
        accumLightB.push_back(accumLight.z);
        pixelIndices.push_back(pixelIndex);
        depths.push_back(depth);
        alive.push_back(isAlive);
    }

    void addPrimary(const Ray& ray, int pixelIndex)
    {
        add(ray, Color(1.0f, 1.0f, 1.0f), Color(0.0f, 0.0f, 0.0f), pixelIndex, 0, true);
    }

    Ray getRay(int i) const
    {
        return Ray(Point3(originsX[i], originsY[i], originsZ[i]),
                   Vec3(dirsX[i], dirsY[i], dirsZ[i]));
    }

    Color getThroughput(int i) const
    {
        return Color(throughputsR[i], throughputsG[i], throughputsB[i]);
    }

    Color getAccumLight(int i) const
    {
        return Color(accumLightR[i], accumLightG[i], accumLightB[i]);
    }

    void clear()
    {
        originsX.clear();
        originsY.clear();
        originsZ.clear();
        dirsX.clear();
        dirsY.clear();
        dirsZ.clear();
        throughputsR.clear();
        throughputsG.clear();
        throughputsB.clear();
        accumLightR.clear();
        accumLightG.clear();
        accumLightB.clear();
        pixelIndices.clear();
        depths.clear();
        alive.clear();
    }

    int size() const
    {
        return static_cast<int>(originsX.size());
    }

    bool empty() const
    {
        return originsX.empty();
    }
};