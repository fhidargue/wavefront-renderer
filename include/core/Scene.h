#pragma once

#include <vector>
#include <geometry/Sphere.h>
#include <geometry/Mesh.h>
#include <core/HitRecord.h>
#include <shading/Material.h>
#include <shading/Texture.h>
#include <core/Ray.h>

struct Scene {
    std::vector<Sphere> spheres;
    std::vector<Mesh> meshes;
    std::vector<Material> materials;
    std::vector<Texture> textures;

    int addMaterial(const Material& material);
    int addTexture(const Texture& texture);
    void addSphere(const Sphere& sphere);
    void addMesh(const Mesh& mesh);

    const Material& getMaterial(const HitRecord& record) const;

    bool hit(const Ray& ray, float minDistance, float maxDistance, HitRecord& record) const;
};