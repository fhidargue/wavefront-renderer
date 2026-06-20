#pragma once

#include <vector>
#include <cmath>
#include <math/Vec3.h>
#include <core/Ray.h>
#include <core/HitRecord.h>
#include <geometry/Triangle.h>

using std::vector;

struct Mesh {
    vector<Point3> vertexPositions;  
    vector<int> triangleIndices; 
    int materialID;

    int triangleCount() const {
        return static_cast<int>(triangleIndices.size() / 3);
    }

    bool hit(const Ray& ray, float minDistance, float maxDistance, HitRecord& record) const {
        bool  hitAnything = false;
        float closestSoFar = maxDistance;

        for (int triangleIndex = 0; triangleIndex < triangleCount(); ++triangleIndex) {
            const Point3& vertex0 = vertexPositions[triangleIndices[triangleIndex * 3 + 0]];
            const Point3& vertex1 = vertexPositions[triangleIndices[triangleIndex * 3 + 1]];
            const Point3& vertex2 = vertexPositions[triangleIndices[triangleIndex * 3 + 2]];

            float hitDistance, barycentricU, barycentricV;
            if (intersectTriangle(ray, vertex0, vertex1, vertex2, minDistance, closestSoFar, 
                hitDistance, barycentricU, barycentricV)) {
                hitAnything = true;
                closestSoFar = hitDistance;

                record.distance = hitDistance;
                record.point = ray.at(hitDistance);
                record.materialID = materialID;
                record.u = barycentricU;
                record.v = barycentricV;

                // Flat shading
                Vec3 edge1 = vertex1 - vertex0;
                Vec3 edge2 = vertex2 - vertex0;
                Vec3 outwardNormal = edge1.cross(edge2).normalized();
                record.setFaceNormal(ray, outwardNormal);
            }
        }

        return hitAnything;
    }
};