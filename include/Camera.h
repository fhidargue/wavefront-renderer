#pragma once

#include "Ray.h"

// Simple camera that sits at an origin and look through a viewport

struct Camera {
    Point3 origin;
    Point3 topLeftCorner;
    Vec3 horizontal;
    Vec3 vertical;

    Camera(int imageWidth, int imageHeight) {
        float aspectRatio = static_cast<float>(imageWidth) / imageHeight;

        // Viewport dimensions in the world
        float viewportHeight = 2.0f;
        float viewportWidth = aspectRatio * viewportHeight;
        float focalLength = 1.0f;

        origin = Point3(0, 0, 0);
        horizontal = Vec3(viewportWidth, 0 ,0);
        vertical = Vec3(0, viewportHeight, 0);

        topLeftCorner = origin - horizontal / 2.0f - vertical / 2.0f - Vec3(0, 0, focalLength);
    }

    Ray getRay(float u, float v) const {
        Vec3 direction = topLeftCorner + horizontal * u + vertical * v - origin;
        return Ray(origin, direction.normalized());
    }
};