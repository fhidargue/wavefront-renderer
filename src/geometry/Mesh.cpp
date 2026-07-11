#include <geometry/Mesh.h>
#include <geometry/Triangle.h>

int Mesh::triangleCount() const
{
    return static_cast<int>(triangleIndices.size() / 3);
}

bool Mesh::hit(const Ray& ray, float minDistance, float maxDistance, HitRecord& record) const
{
    bool hitAnything = false;
    float closestSoFar = maxDistance;

    for (int triangleIndex = 0; triangleIndex < triangleCount(); ++triangleIndex)
    {
        const Point3& vertex0 = vertexPositions[triangleIndices[triangleIndex * 3 + 0]];
        const Point3& vertex1 = vertexPositions[triangleIndices[triangleIndex * 3 + 1]];
        const Point3& vertex2 = vertexPositions[triangleIndices[triangleIndex * 3 + 2]];

        float hitDistance, barycentricU, barycentricV;
        if (intersectTriangle(ray, vertex0, vertex1, vertex2, minDistance, closestSoFar,
                              hitDistance, barycentricU, barycentricV))
        {
            hitAnything = true;
            closestSoFar = hitDistance;

            record.distance = hitDistance;
            record.point = ray.at(hitDistance);
            record.materialID = materialID;

            record.triangleIndex = triangleIndex;
            interpolateUV(triangleIndex, barycentricU, barycentricV, record.u, record.v);
            record.u = barycentricU;
            record.v = barycentricV;

            Vec3 edge1 = vertex1 - vertex0;
            Vec3 edge2 = vertex2 - vertex0;
            Vec3 outwardNormal = edge1.cross(edge2).normalized();
            record.setFaceNormal(ray, outwardNormal);
        }
    }

    return hitAnything;
}

void Mesh::interpolateUV(int triangleIndex, float barycentricU, float barycentricV, float& outU,
                         float& outV) const
{
    if (triangleUVsU.empty())
    {
        outU = barycentricU;
        outV = barycentricV;
        return;
    }

    int cornerBase = triangleIndex * 3;
    float weight0 = 1.0f - barycentricU - barycentricV;

    outU = weight0 * triangleUVsU[cornerBase + 0] + barycentricU * triangleUVsU[cornerBase + 1] +
           barycentricV * triangleUVsU[cornerBase + 2];
    outV = weight0 * triangleUVsV[cornerBase + 0] + barycentricU * triangleUVsV[cornerBase + 1] +
           barycentricV * triangleUVsV[cornerBase + 2];
}
