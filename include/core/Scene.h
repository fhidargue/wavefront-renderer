#pragma once

#include <vector>
#include <geometry/Sphere.h>
#include <geometry/Mesh.h>
#include <core/HitRecord.h>
#include <shading/Material.h>
#include <shading/Texture.h>
#include <core/Ray.h>
#include <accelerator/Embree.h>

struct Scene {
    std::vector<Sphere> spheres;
    std::vector<Mesh> meshes;
    std::vector<Material> materials;
    std::vector<Texture> textures;

    EmbreeAccelerator accelerator;
    bool acceleratorBuilt = false;

    int addMaterial(const Material& material);
    int addTexture(const Texture& texture);
    void addSphere(const Sphere& sphere);
    void addMesh(const Mesh& mesh);

    void buildAccelerator();
    
    const Material& getMaterial(const HitRecord& record) const;

    bool hit(const Ray& ray, float minDistance, float maxDistance, HitRecord& record) const;
};