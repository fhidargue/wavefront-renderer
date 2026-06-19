#pragma once

#include <vector>
#include "Sphere.h"
#include "HitRecord.h"
#include "Material.h"
#include "Ray.h"

using std::vector;

struct Scene
{
    vector<Sphere> spheres;
    vector<Material> materials;

    int addMaterial(const Material& material) {
        materials.push_back(material);

        return static_cast<int>(materials.size() - 1);
    }

    const Material& getMaterial(const HitRecord& record) const {
        return materials[record.materialID];
    }

    void addSphere(const Sphere& sphere) {
        spheres.push_back(sphere);
    }

    bool hit(const Ray& ray, float minDistance, float maxDistance, HitRecord& record) const {
        HitRecord tempRecord;
        bool hitAnything = false;
        float closestHit = maxDistance;

        for (const Sphere& sphere : spheres) {
            if (sphere.hit(ray, minDistance, closestHit, tempRecord)) {
                hitAnything = true;
                closestHit = tempRecord.distance;
                record = tempRecord;
            }
        }

        return hitAnything;
    }
};
