#include <core/Scene.h>

int Scene::addMaterial(const Material& material) {
    materials.push_back(material);

    return static_cast<int>(materials.size() - 1);
}

int Scene::addTexture(const Texture& texture) {
    textures.push_back(texture);

    return static_cast<int>(textures.size() - 1);
}

void Scene::addSphere(const Sphere& sphere) {
    spheres.push_back(sphere);
}

void Scene::addMesh(const Mesh& mesh) {
    meshes.push_back(mesh);
}

const Material& Scene::getMaterial(const HitRecord& record) const {
    return materials[record.materialID];
}

bool Scene::hit(const Ray& ray, float minDistance, float maxDistance, HitRecord& record) const {
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