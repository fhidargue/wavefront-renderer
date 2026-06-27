#include <core/Scene.h>

int Scene::addMaterial(const Material& material)
{
    materials.push_back(material);

    return static_cast<int>(materials.size() - 1);
}

int Scene::addTexture(const Texture& texture)
{
    textures.push_back(texture);

    return static_cast<int>(textures.size() - 1);
}

void Scene::addSphere(const Sphere& sphere)
{
    spheres.push_back(sphere);
}

void Scene::addMesh(const Mesh& mesh)
{
    meshes.push_back(mesh);

    // Append to DOD flat arrays
    int vertexOffset = static_cast<int>(geometry.allPositionsX.size());
    int indexOffset = static_cast<int>(geometry.allIndices.size());

    // Append vertex positions by component
    for (const Point3& position : mesh.vertexPositions)
    {
        geometry.allPositionsX.push_back(position.x);
        geometry.allPositionsY.push_back(position.y);
        geometry.allPositionsZ.push_back(position.z);
    }

    // Append indices with global vertex offset applied
    for (int index : mesh.triangleIndices)
        geometry.allIndices.push_back(index + vertexOffset);

    // Record metadata per mesh
    geometry.meshVertexOffsets.push_back(vertexOffset);
    geometry.meshVertexCounts.push_back(static_cast<int>(mesh.vertexPositions.size()));
    geometry.meshIndexOffsets.push_back(indexOffset);
    geometry.meshIndexCounts.push_back(static_cast<int>(mesh.triangleIndices.size()));
    geometry.meshMaterialIDs.push_back(mesh.materialID);
    geometry.meshUUIDs.push_back(mesh.uuid);
}

void Scene::buildAccelerator()
{
    accelerator.build(*this);
    accelerator.printStats();
    acceleratorBuilt = true;
}

const Material& Scene::getMaterial(const HitRecord& record) const
{
    return materials[record.materialID];
}

bool Scene::hit(const Ray& ray, float minDistance, float maxDistance, HitRecord& record) const
{
    HitRecord tempRecord;
    bool hitAnything = false;
    float closestHit = maxDistance;

    for (const Sphere& sphere : spheres)
    {
        if (sphere.hit(ray, minDistance, closestHit, tempRecord))
        {
            hitAnything = true;
            closestHit = tempRecord.distance;
            record = tempRecord;
        }
    }

    if (acceleratorBuilt)
    {
        if (accelerator.intersect(ray, minDistance, closestHit, tempRecord))
        {
            hitAnything = true;
            closestHit = tempRecord.distance;
            record = tempRecord;
        }
    }
    else
    {
        // Fallback if the accelerator is not built
        for (const Mesh& mesh : meshes)
        {
            if (mesh.hit(ray, minDistance, closestHit, tempRecord))
            {
                hitAnything = true;
                closestHit = tempRecord.distance;
                record = tempRecord;
            }
        }
    }

    return hitAnything;
}