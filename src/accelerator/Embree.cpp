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

void EmbreeAccelerator::printStats() const
{
    cout << "\n========================================" << endl;
    cout << "  Embree BVH Stats" << endl;
    cout << "========================================" << endl;
    cout << "  Meshes registered : " << m_meshCount << endl;
    cout << "  Total triangles   : " << m_totalTriangles << endl;
    cout << "  Total vertices    : " << m_totalVertices << endl;
    cout << "  BVH build time    : " << m_buildTimeMs << "ms" << endl;
    cout << "========================================\n" << endl;
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
    record.u = rayHit.hit.u;
    record.v = rayHit.hit.v;

    // Recompute the geometry normal from the triangle edges
    Vec3 edge1 = vertex1 - vertex0;
    Vec3 edge2 = vertex2 - vertex0;
    Vec3 outwardNormal = edge1.cross(edge2).normalized();
    record.setFaceNormal(ray, outwardNormal);

    return true;
}