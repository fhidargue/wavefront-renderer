#pragma once

#include <math/Vec3.h>
#include <core/Ray.h>

inline bool intersectTriangle(const Ray& ray, const Point3& vertex0, const Point3& vertex1, const Point3& vertex2,
    float minDistance, float maxDistance, float& outDistance, float& outBarycentricU, float& outBarycentricV)
{
    const float EPSILON = 0.0000001f;

    Vec3 edge1 = vertex1 - vertex0;
    Vec3 edge2 = vertex2 - vertex0;
    Vec3 rayCrossEdge2 = ray.direction.cross(edge2);
    float determinant  = edge1.dot(rayCrossEdge2);

    if (determinant > -EPSILON && determinant < EPSILON)
        return false;

    float inverseDeterminant = 1.0f / determinant;
    Vec3  originToVertex0 = ray.origin - vertex0;
    float barycentricU = inverseDeterminant * originToVertex0.dot(rayCrossEdge2);

    if (barycentricU < 0.0f || barycentricU > 1.0f)
        return false;

    Vec3  originCrossEdge1 = originToVertex0.cross(edge1);
    float barycentricV = inverseDeterminant * ray.direction.dot(originCrossEdge1);

    if (barycentricV < 0.0f || barycentricU + barycentricV > 1.0f)
        return false;

    float hitDistance = inverseDeterminant * edge2.dot(originCrossEdge1);

    if (hitDistance < minDistance || hitDistance > maxDistance)
        return false;

    outDistance = hitDistance;
    outBarycentricU = barycentricU;
    outBarycentricV = barycentricV;
    
    return true;
}