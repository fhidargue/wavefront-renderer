#pragma once

#include <embree4/rtcore.h>
#include <vector>
#include <core/Ray.h>
#include <core/HitRecord.h>

struct Scene;

class EmbreeAccelerator
{
  public:
    EmbreeAccelerator();
    ~EmbreeAccelerator();

    // Handles cannot be copied
    EmbreeAccelerator(const EmbreeAccelerator&) = delete;
    EmbreeAccelerator& operator=(const EmbreeAccelerator&) = delete;

    // Moving ownership of the handles
    EmbreeAccelerator(EmbreeAccelerator&& other) noexcept;
    EmbreeAccelerator& operator=(EmbreeAccelerator&& other) noexcept;

    void build(const Scene& scene);
    void printStats() const;

    bool intersect(const Ray& ray, float minDistance, float maxDistance, HitRecord& record) const;

    const Scene* m_sourceScene = nullptr;
    bool m_built = false;

  private:
    RTCDevice m_device;
    RTCScene m_scene;

    // Stats recorded during build
    int m_meshCount;
    int m_totalTriangles;
    int m_totalVertices;
    double m_buildTimeMs;
};