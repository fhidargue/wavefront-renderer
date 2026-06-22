#pragma once

#include <embree4/rtcore.h>
#include <vector>
#include <core/Ray.h>
#include <core/HitRecord.h>

struct Scene;

class EmbreeAccelerator {
    public:
        EmbreeAccelerator();
        ~EmbreeAccelerator();

        void build(const Scene& scene);

        bool intersect(const Ray& ray, float minDistance, float maxDistance, HitRecord& record) const;
    
    private:
        RTCDevice m_device;
        RTCScene m_scene;

        const Scene* m_sourceScene;
};