#pragma once

#include <math/Vec3.h>
#include <core/Ray.h>

struct HitRecord
{
    Point3 point;
    Vec3 normal;
    float distance;
    int materialID;
    int textureID;
    float u, v;
    bool frontFace;

    void setFaceNormal(const Ray& ray, const Vec3& outwardNormal)
    {
        frontFace = ray.direction.dot(outwardNormal) < 0.0f;
        normal = frontFace ? outwardNormal : outwardNormal * -1.0f;
    }
};