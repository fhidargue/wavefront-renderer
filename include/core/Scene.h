#pragma once

#include <vector>
#include <geometry/Sphere.h>
#include <geometry/Mesh.h>
#include <core/HitRecord.h>
#include <shading/Material.h>
#include <shading/Texture.h>
#include <core/Ray.h>
#include <accelerator/Embree.h>

// DOD flat contiguous arrays for all scene geometry
struct SceneGeometry
{
    std::vector<float> allPositionsX;
    std::vector<float> allPositionsY;
    std::vector<float> allPositionsZ;
    std::vector<int> allIndices;

    std::vector<int> meshVertexOffsets;
    std::vector<int> meshVertexCounts;
    std::vector<int> meshIndexOffsets;
    std::vector<int> meshIndexCounts;
    std::vector<int> meshMaterialIDs;
    std::vector<std::string> meshUUIDs;

    int meshCount() const
    {
        return static_cast<int>(meshMaterialIDs.size());
    }
    int totalVertexCount() const
    {
        return static_cast<int>(allPositionsX.size());
    }
    int totalTriangleCount() const
    {
        return static_cast<int>(allIndices.size()) / 3;
    }

    Point3 getVertex(int globalIndex) const
    {
        return Point3(allPositionsX[globalIndex], allPositionsY[globalIndex],
                      allPositionsZ[globalIndex]);
    }
};

struct LightSample
{
    Point3 point;
    Vec3 normal;
    Color emission;
    float area = 0.0f;
    bool valid = false;
};

struct Scene
{
    Scene() = default;
    Scene(const Scene&) = delete;
    Scene& operator=(const Scene&) = delete;
    Scene(Scene&&) = default;
    Scene& operator=(Scene&&) = default;

    std::vector<Sphere> spheres;
    std::vector<Mesh> meshes;
    std::vector<Material> materials;
    std::vector<Texture> textures;
    std::vector<int> emissiveMeshIndices;

    SceneGeometry geometry;

    EmbreeAccelerator accelerator;
    bool acceleratorBuilt = false;

    int addMaterial(const Material& material);
    int addTexture(const Texture& texture);
    void addSphere(const Sphere& sphere);
    void addMesh(const Mesh& mesh);

    void buildAccelerator();

    const Material& getMaterial(const HitRecord& record) const;

    bool hit(const Ray& ray, float minDistance, float maxDistance, HitRecord& record) const;

    // NEE sample for a random point on emissive mesh
    LightSample sampleLight() const;
};