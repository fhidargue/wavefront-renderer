#pragma once

#include "Vec3.h"
#include "Ray.h"

struct HitRecord {
    Point3 point;
    Vec3 normal;
    float distance;
    int materialID;
    bool frontFace;

    void setFaceNormal(const Ray& ray, const Vec3& outwardNormal) {
        frontFace = ray.direction.dot(outwardNormal) < 0.0f;
        normal = frontFace ? outwardNormal : outwardNormal * -1.0f;
    }
};