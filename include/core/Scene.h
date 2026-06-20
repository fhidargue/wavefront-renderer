#pragma once

#include <vector>
#include <geometry/Sphere.h>
#include <geometry/Mesh.h>
#include <core/HitRecord.h>
#include <shading/Material.h>
#include <core/Ray.h>

using std::vector;

struct Scene {
    vector<Sphere> spheres;
    vector<Material> materials;
    vector<Texture> textures;
    vector<Mesh> meshes;

    int addMaterial(const Material& material) {
        materials.push_back(material);

        return static_cast<int>(materials.size() - 1);
    }

    int addTexture(const Texture& texture) {
        textures.push_back(texture);
        return static_cast<int>(textures.size() - 1);
    }
    
    void addSphere(const Sphere& sphere) {
        spheres.push_back(sphere);
    }

    void addMesh(const Mesh& mesh) {
        meshes.push_back(mesh);
    }

    const Material& getMaterial(const HitRecord& record) const {
        return materials[record.materialID];
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

        for (const Mesh& mesh : meshes) {
            if (mesh.hit(ray, minDistance, closestHit, tempRecord)) {
                hitAnything = true;
                closestHit = tempRecord.distance;
                record = tempRecord;
            }
        }

        return hitAnything;
    }
};
