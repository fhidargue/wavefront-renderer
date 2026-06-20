#pragma once

#include <vector>
#include <core/Ray.h>
#include <math/Vec3.h>

using std::vector;

struct RayState {
    Ray ray;
    Color throughtput;
    Color accumLight;
    int rayPixelIndex;
    int rayDepth;
    bool isRayAlive;

    RayState() {}

    RayState(const Ray& ray, int rayPixelIndex)
        : ray(ray),
        throughtput(1.0f, 1.0f, 1.0f),
        accumLight(0.0f, 0.0f, 0.0f),
        rayPixelIndex(rayPixelIndex),
        rayDepth(0),
        isRayAlive(true)
    {}
};

struct RayQueue {
    vector<RayState> rays;

    void add(const RayState& rayState) {
        rays.push_back(rayState);
    }

    void clear() {
        rays.clear();
    }

    int size() const {
        return static_cast<int>(rays.size());
    }

    bool empty() const {
        return rays.empty();
    }
};