#pragma once

#include <vector>
#include <math/Vec3.h>
#include <core/Ray.h>
#include <core/HitRecord.h>

struct Mesh {
    std::vector<Point3> vertexPositions;
    std::vector<int> triangleIndices;
    int materialID;

    int triangleCount() const;
    bool hit(const Ray& ray, float minDistance, float maxDistance, HitRecord& record) const;
};