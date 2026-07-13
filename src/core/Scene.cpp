#include <core/Scene.h>
#include <core/PrintUtils.h>
#include <math/Random.h>
#include <limits>

using std::max;
using std::min;
using std::numeric_limits;
using std::to_string;

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

void Scene::computeBounds()
{
    boundsMin = Point3(numeric_limits<float>::max(), numeric_limits<float>::max(),
                       numeric_limits<float>::max());
    boundsMax = Point3(numeric_limits<float>::lowest(), numeric_limits<float>::lowest(),
                       numeric_limits<float>::lowest());

    for (int i = 0; i < geometry.totalVertexCount(); ++i)
    {
        Point3 vertex = geometry.getVertex(i);

        boundsMin.x = min(boundsMin.x, vertex.x);
        boundsMin.y = min(boundsMin.y, vertex.y);
        boundsMin.z = min(boundsMin.z, vertex.z);
        boundsMax.x = max(boundsMax.x, vertex.x);
        boundsMax.y = max(boundsMax.y, vertex.y);
        boundsMax.z = max(boundsMax.z, vertex.z);
    }
}

void Scene::buildAccelerator()
{
    // Cache emissive mesh indices for fast NEE sampling
    emissiveMeshIndices.clear();

    for (int i = 0; i < static_cast<int>(meshes.size()); ++i)
    {
        MaterialType materialType = materials[meshes[i].materialID].type;

        if (materialType == MaterialType::Emissive || materialType == MaterialType::SpotLight)
            emissiveMeshIndices.push_back(i);
    }

    // Run the total emissive area cache before multi threading
    totalEmissiveArea();

    // Determine the Embree scene bounds
    computeBounds();

    accelerator.build(*this);

    printStatsBlock("Embree BVH Stats",
                    {
                        "Meshes registered : " + to_string(accelerator.m_meshCount),
                        "Total triangles   : " + to_string(accelerator.m_totalTriangles),
                        "Total vertices    : " + to_string(accelerator.m_totalVertices),
                        "BVH build time    : " + to_string(accelerator.m_buildTimeMs) + "ms",
                        "Bounds min        : (" + to_string(boundsMin.x) + ", " +
                            to_string(boundsMin.y) + ", " + to_string(boundsMin.z) + ")",
                        "Bounds max        : (" + to_string(boundsMax.x) + ", " +
                            to_string(boundsMax.y) + ", " + to_string(boundsMax.z) + ")",
                    });

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

float Scene::totalEmissiveArea() const
{
    if (cachedTotalEmissiveArea >= 0.0f)
        return cachedTotalEmissiveArea;

    float total = 0.0f;

    for (int meshIndex : emissiveMeshIndices)
    {
        const Mesh& mesh = meshes[meshIndex];
        int triangleCount = mesh.triangleCount();

        for (int t = 0; t < triangleCount; ++t)
        {
            Point3 vertex0 = mesh.vertexPositions[mesh.triangleIndices[t * 3 + 0]];
            Point3 vertex1 = mesh.vertexPositions[mesh.triangleIndices[t * 3 + 1]];
            Point3 vertex2 = mesh.vertexPositions[mesh.triangleIndices[t * 3 + 2]];

            Vec3 edge1 = vertex1 - vertex0;
            Vec3 edge2 = vertex2 - vertex0;
            total += edge1.cross(edge2).length() * 0.5f;
        }
    }

    // Cached on first use before parallel rendering, so no atomic is required
    const_cast<Scene*>(this)->cachedTotalEmissiveArea = total;

    return total;
}

LightSample Scene::sampleLight(const Point3& shadingPoint) const
{
    // Use pre-cached emissive mesh indices
    if (emissiveMeshIndices.empty())
        return {};

    int meshIndex =
        emissiveMeshIndices[static_cast<int>(randomFloat() * emissiveMeshIndices.size()) %
                            static_cast<int>(emissiveMeshIndices.size())];

    const Mesh& mesh = meshes[meshIndex];
    int triangleCount = mesh.triangleCount();

    if (triangleCount <= 0)
        return {};

    int triangleIndex = static_cast<int>(randomFloat() * triangleCount) % triangleCount;

    Point3 vertex0 = mesh.vertexPositions[mesh.triangleIndices[triangleIndex * 3 + 0]];
    Point3 vertex1 = mesh.vertexPositions[mesh.triangleIndices[triangleIndex * 3 + 1]];
    Point3 vertex2 = mesh.vertexPositions[mesh.triangleIndices[triangleIndex * 3 + 2]];

    float r1 = randomFloat();
    float r2 = randomFloat();

    if (r1 + r2 > 1.0f)
    {
        r1 = 1.0f - r1;
        r2 = 1.0f - r2;
    }

    float r3 = 1.0f - r1 - r2;

    Point3 point = vertex0 * r3 + vertex1 * r1 + vertex2 * r2;

    Vec3 edge1 = vertex1 - vertex0;
    Vec3 edge2 = vertex2 - vertex0;
    Vec3 cross = edge1.cross(edge2);

    Vec3 normal = cross.normalized();
    Vec3 directionFromLight = (shadingPoint - point).normalized();
    Color emission = materials[mesh.materialID].emitted(normal, directionFromLight);

    float totalArea = totalEmissiveArea();

    return {point, normal, emission, totalArea, true};
}