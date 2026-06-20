#pragma once

#include <cmath>
#include <core/Ray.h>

struct Camera {
    Point3 origin;
    Point3 topLeftCorner;
    Vec3 horizontal;
    Vec3 vertical;

    Camera(Point3 lookFrom, Point3 lookAt, Vec3 upDirection, 
        float verticalFieldOfViewDegrees, int imageWidth, int imageHeight) {
        float aspectRatio = static_cast<float>(imageWidth) / imageHeight;

        // Convert field of view from degrees to a viewport height
        float theta = verticalFieldOfViewDegrees * 3.14159265f / 180.0f;
        float viewportHeight = 2.0f * std::tan(theta / 2.0f);
        float viewportWidth = aspectRatio * viewportHeight;

        Vec3 forward = (lookFrom - lookAt).normalized();
        Vec3 right = upDirection.cross(forward).normalized();
        Vec3 up = forward.cross(right);

        origin = lookFrom;
        horizontal = right * viewportWidth;
        vertical = up * viewportHeight;

        // Top-left corner of the viewport
        topLeftCorner = origin - horizontal / 2.0f - vertical / 2.0f - forward;
    }

    Ray getRay(float u, float v) const {
        Vec3 direction = topLeftCorner + horizontal * u + vertical * v - origin;
        
        return Ray(origin, direction.normalized());
    }
};