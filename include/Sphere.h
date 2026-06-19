#pragma once

#include <cmath>
#include "Vec3.h"
#include "Ray.h"
#include "HitRecord.h"

using std::sqrt;

struct Sphere {
    Point3 center;
    float radius;
    int materialID;

    Sphere(const Point3& center, float radius, int materialID)
        : center(center), radius(radius), materialID(materialID) {}

    // Validate if a ray hits the sphere
    bool hit(const Ray& ray, float minDistance, float maxDistance, HitRecord& record) const {
        Vec3 originToCenter = ray.origin - center;

        // Quadratic equation coefficients: at² + bt + c = 0
        float a = ray.direction.dot(ray.direction);
        float b = 2.0f * originToCenter.dot(ray.direction);
        float c = originToCenter.dot(originToCenter) - radius * radius;

        // Discriminant tells us how many solutions exist
        float discriminant = b * b - 4.0f * a * c;

        if (discriminant < 0.0f) 
            return false;
        
        float sqrtDiscriminant = sqrt(discriminant);

        // Find the nearest hit distance within our valid range
        float hitDistance = (-b - sqrtDiscriminant) / (2.0f * a);

        if (hitDistance < minDistance || hitDistance > maxDistance) {
            hitDistance = (-b + sqrtDiscriminant) / (2.0f * a);

            if (hitDistance < minDistance || hitDistance > maxDistance)
                return false;
        }

        // Fill in the hit record
        record.distance = hitDistance;
        record.point = ray.at(hitDistance);
        record.materialID = materialID;

        Vec3 outwardNormal = (record.point - center) / radius;
        record.setFaceNormal(ray, outwardNormal);

        return true;
    }  
};