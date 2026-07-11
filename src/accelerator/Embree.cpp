#include <accelerator/Embree.h>
#include <core/Scene.h>
#include <geometry/Mesh.h>
#include <cmath>

using std::cout;
using std::endl;

EmbreeAccelerator::EmbreeAccelerator()
    : m_device(nullptr), m_scene(nullptr), m_sourceScene(nullptr), m_meshCount(0),
      m_totalTriangles(0), m_totalVertices(0), m_buildTimeMs(0.0)
{
    m_device = rtcNewDevice(nullptr);
}

EmbreeAccelerator::~EmbreeAccelerator()
{
    if (m_scene)
        rtcReleaseScene(m_scene);
    if (m_device)
        rtcReleaseDevice(m_device);
}

EmbreeAccelerator::EmbreeAccelerator(EmbreeAccelerator&& other) noexcept
    : m_device(other.m_device), m_scene(other.m_scene), m_sourceScene(other.m_sourceScene),
      m_built(other.m_built), m_meshCount(other.m_meshCount),
      m_totalTriangles(other.m_totalTriangles), m_totalVertices(other.m_totalVertices),
      m_buildTimeMs(other.m_buildTimeMs)
{
    // Null the source so its destructor does not release our handles
    other.m_device = nullptr;
    other.m_scene = nullptr;
    other.m_sourceScene = nullptr;
    other.m_built = false;
}

EmbreeAccelerator& EmbreeAccelerator::operator=(EmbreeAccelerator&& other) noexcept
{
    if (this != &other)
    {
        // Release what we currently own before taking the new handles
        if (m_scene)
            rtcReleaseScene(m_scene);
        if (m_device)
            rtcReleaseDevice(m_device);

        m_device = other.m_device;
        m_scene = other.m_scene;
        m_sourceScene = other.m_sourceScene;
        m_built = other.m_built;
        m_meshCount = other.m_meshCount;
        m_totalTriangles = other.m_totalTriangles;
        m_totalVertices = other.m_totalVertices;
        m_buildTimeMs = other.m_buildTimeMs;

        other.m_device = nullptr;
        other.m_scene = nullptr;
        other.m_sourceScene = nullptr;
        other.m_built = false;
    }

    return *this;
}

// Build the BVH from all meshes
void EmbreeAccelerator::build(const Scene& scene)
{
    m_sourceScene = &scene;
    m_scene = rtcNewScene(m_device);

    auto buildStart = std::chrono::high_resolution_clock::now();

    m_meshCount = 0;
    m_totalTriangles = 0;
    m_totalVertices = 0;

    for (const Mesh& mesh : scene.meshes)
    {
        RTCGeometry geometry = rtcNewGeometry(m_device, RTC_GEOMETRY_TYPE_TRIANGLE);

        int vertexCount = static_cast<int>(mesh.vertexPositions.size());
        int triangleCount = mesh.triangleCount();

        // Vertex buffer
        float* vertices = static_cast<float*>(
            rtcSetNewGeometryBuffer(geometry, RTC_BUFFER_TYPE_VERTEX, 0, RTC_FORMAT_FLOAT3,
                                    3 * sizeof(float), vertexCount));

        for (int i = 0; i < vertexCount; ++i)
        {
            vertices[i * 3 + 0] = mesh.vertexPositions[i].x;
            vertices[i * 3 + 1] = mesh.vertexPositions[i].y;
            vertices[i * 3 + 2] = mesh.vertexPositions[i].z;
        }

        // Index buffer
        unsigned* indices = static_cast<unsigned*>(
            rtcSetNewGeometryBuffer(geometry, RTC_BUFFER_TYPE_INDEX, 0, RTC_FORMAT_UINT3,
                                    3 * sizeof(unsigned), triangleCount));

        for (int i = 0; i < triangleCount * 3; ++i)
        {
            indices[i] = static_cast<unsigned>(mesh.triangleIndices[i]);
        }

        rtcCommitGeometry(geometry);
        rtcAttachGeometry(m_scene, geometry);
        rtcReleaseGeometry(geometry);

        m_meshCount++;
        m_totalTriangles += triangleCount;
        m_totalVertices += vertexCount;
    }

    rtcCommitScene(m_scene);

    auto buildEnd = std::chrono::high_resolution_clock::now();
    m_buildTimeMs = std::chrono::duration<double, std::milli>(buildEnd - buildStart).count();
}

// Trace one ray against the BVH
bool EmbreeAccelerator::intersect(const Ray& ray, float minDistance, float maxDistance,
                                  HitRecord& record) const
{
    // Embree packs a ray and the hit result in one struct
    RTCRayHit rayHit;

    rayHit.ray.org_x = ray.origin.x;
    rayHit.ray.org_y = ray.origin.y;
    rayHit.ray.org_z = ray.origin.z;

    rayHit.ray.dir_x = ray.direction.x;
    rayHit.ray.dir_y = ray.direction.y;
    rayHit.ray.dir_z = ray.direction.z;

    rayHit.ray.tnear = minDistance;
    rayHit.ray.tfar = maxDistance;
    rayHit.ray.mask = 0xFFFFFFFF;
    rayHit.ray.flags = 0;
    rayHit.ray.time = 0.0f;

    // Create empty hits before they happen - Null state
    rayHit.hit.geomID = RTC_INVALID_GEOMETRY_ID;
    rayHit.hit.primID = RTC_INVALID_GEOMETRY_ID;

    // Traverse the BVH Intersection
    rtcIntersect1(m_scene, &rayHit);

    // Return if there are no hits
    if (rayHit.hit.geomID == RTC_INVALID_GEOMETRY_ID)
        return false;

    // Map the BVH result into the HitRecord
    const Mesh& mesh = m_sourceScene->meshes[rayHit.hit.geomID];

    int i0 = mesh.triangleIndices[rayHit.hit.primID * 3 + 0];
    int i1 = mesh.triangleIndices[rayHit.hit.primID * 3 + 1];
    int i2 = mesh.triangleIndices[rayHit.hit.primID * 3 + 2];

    const Point3& vertex0 = mesh.vertexPositions[i0];
    const Point3& vertex1 = mesh.vertexPositions[i1];
    const Point3& vertex2 = mesh.vertexPositions[i2];

    record.distance = rayHit.ray.tfar;
    record.point = ray.at(rayHit.ray.tfar);
    record.materialID = mesh.materialID;
    record.triangleIndex = static_cast<int>(rayHit.hit.primID);
    mesh.interpolateUV(rayHit.hit.primID, rayHit.hit.u, rayHit.hit.v, record.u, record.v);

    // Recompute the geometry normal from the triangle edges
    Vec3 edge1 = vertex1 - vertex0;
    Vec3 edge2 = vertex2 - vertex0;
    Vec3 outwardNormal = edge1.cross(edge2).normalized();
    record.setFaceNormal(ray, outwardNormal);

    return true;
}

// Trace 4 rays against the BVH (SIMD)
int EmbreeAccelerator::intersect4(const RayQueue& queue, int startIndex, int count,
                                  HitRecord* hitRecords) const
{
    alignas(16) int valid[4] = {0, 0, 0, 0};
    alignas(16) RTCRayHit4 rayHit4;

    // Fill the packet up with 4 rays from the SoA queue
    for (int i = 0; i < count; ++i)
    {
        int queueIndex = startIndex + i;

        // Mark flag as active before filling ray data
        valid[i] = -1;

        rayHit4.ray.org_x[i] = queue.originsX[queueIndex];
        rayHit4.ray.org_y[i] = queue.originsY[queueIndex];
        rayHit4.ray.org_z[i] = queue.originsZ[queueIndex];
        rayHit4.ray.dir_x[i] = queue.dirsX[queueIndex];
        rayHit4.ray.dir_y[i] = queue.dirsY[queueIndex];
        rayHit4.ray.dir_z[i] = queue.dirsZ[queueIndex];
        rayHit4.ray.tnear[i] = 0.001f;
        rayHit4.ray.tfar[i] = 1e9f;
        rayHit4.ray.mask[i] = 0xFFFFFFFF;
        rayHit4.ray.flags[i] = 0;
        rayHit4.ray.time[i] = 0.0f;
        rayHit4.hit.geomID[i] = RTC_INVALID_GEOMETRY_ID;
        rayHit4.hit.primID[i] = RTC_INVALID_GEOMETRY_ID;
    }

    // Single BVH traversal for all 4 rays at the same time
    RTCIntersectArguments args;
    rtcInitIntersectArguments(&args);
    rtcIntersect4(valid, m_scene, &rayHit4, &args);

    // Unpack results inside HitRecords
    int hitMask = 0;
    for (int i = 0; i < count; ++i)
    {
        if (rayHit4.hit.geomID[i] == RTC_INVALID_GEOMETRY_ID)
            continue;

        // Set bit i in the hit mask to indicate this ray intersected geometry
        hitMask |= (1 << i);

        const Mesh& mesh = m_sourceScene->meshes[rayHit4.hit.geomID[i]];

        int index0 = mesh.triangleIndices[rayHit4.hit.primID[i] * 3 + 0];
        int index1 = mesh.triangleIndices[rayHit4.hit.primID[i] * 3 + 1];
        int index2 = mesh.triangleIndices[rayHit4.hit.primID[i] * 3 + 2];

        const Point3& vertex0 = mesh.vertexPositions[index0];
        const Point3& vertex1 = mesh.vertexPositions[index1];
        const Point3& vertex2 = mesh.vertexPositions[index2];

        int queueIndex = startIndex + i;

        Ray ray(Point3(queue.originsX[queueIndex], queue.originsY[queueIndex],
                       queue.originsZ[queueIndex]),
                Vec3(queue.dirsX[queueIndex], queue.dirsY[queueIndex], queue.dirsZ[queueIndex]));

        hitRecords[i].distance = rayHit4.ray.tfar[i];
        hitRecords[i].point = ray.at(rayHit4.ray.tfar[i]);
        hitRecords[i].materialID = mesh.materialID;
        hitRecords[i].triangleIndex = static_cast<int>(rayHit4.hit.primID[i]);
        hitRecords[i].textureID = m_sourceScene->materials[mesh.materialID].textureID;

        mesh.interpolateUV(rayHit4.hit.primID[i], rayHit4.hit.u[i], rayHit4.hit.v[i],
                           hitRecords[i].u, hitRecords[i].v);

        Vec3 edge1 = vertex1 - vertex0;
        Vec3 edge2 = vertex2 - vertex0;
        Vec3 outwardNormal = edge1.cross(edge2).normalized();
        hitRecords[i].setFaceNormal(ray, outwardNormal);
    }

    return hitMask;
}

bool EmbreeAccelerator::occluded(const Point3& origin, const Vec3& direction,
                                 float maxDistance) const
{
    RTCRay ray;
    ray.org_x = origin.x;
    ray.org_y = origin.y;
    ray.org_z = origin.z;
    ray.dir_x = direction.x;
    ray.dir_y = direction.y;
    ray.dir_z = direction.z;
    ray.tnear = 0.001f;
    ray.tfar = maxDistance - 0.001f;
    ray.mask = 0xFFFFFFFF;
    ray.flags = 0;
    ray.time = 0.0f;

    RTCOccludedArguments args;
    rtcInitOccludedArguments(&args);
    rtcOccluded1(m_scene, &ray, &args);

    // Embree sets tfar to -inf when occlusion happens
    return ray.tfar < 0.0f;
}