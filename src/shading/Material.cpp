#include <shading/Material.h>
#include <math/Random.h>
#include <cmath>

Vec3 randomInUnitSphere() {
    while (true) {
        Vec3 candidate(
            randomFloat() * 2.0f - 1.0f,
            randomFloat() * 2.0f - 1.0f,
            randomFloat() * 2.0f - 1.0f
        );
        if (candidate.dot(candidate) < 1.0f)
            return candidate;
    }
}

Vec3 reflect(const Vec3& incoming, const Vec3& normal) {
    return incoming - normal * 2.0f * incoming.dot(normal);
}

Material Material::makeDiffuse(const Color& albedo, int textureID) {
    Material material;
    material.type = MaterialType::Diffuse;
    material.albedo = albedo;
    material.roughness = 1.0f;
    material.emission = 0.0f;
    material.textureID = textureID;

    return material;
}

Material Material::makeMetal(const Color& albedo, float roughness, int textureID) {
    Material material;
    material.type = MaterialType::Metal;
    material.albedo = albedo;
    material.roughness = roughness;
    material.emission = 0.0f;
    material.textureID = textureID;

    return material;
}

Material Material::makeEmissive(const Color& albedo, float strength) {
    Material material;
    material.type = MaterialType::Emissive;
    material.albedo = albedo;
    material.roughness = 0.0f;
    material.emission = strength;
    material.textureID = -1;

    return material;
}

Color Material::getSurfaceColor(const HitRecord& record, const std::vector<Texture>& textures) const {
    if (textureID >= 0 && textureID < static_cast<int>(textures.size()))
        return textures[textureID].sample(record.u, record.v);

    return albedo;
}

bool Material::scatter(const Ray& incoming, const HitRecord& record, const std::vector<Texture>& textures, 
    Color& attenuation, Ray& scattered) const {
    Color surfaceColor = getSurfaceColor(record, textures);

    if (type == MaterialType::Diffuse) {
        Vec3 scatterDirection = record.normal + randomInUnitSphere().normalized();
        scattered = Ray(record.point, scatterDirection.normalized());
        attenuation = surfaceColor;

        return true;
    }

    if (type == MaterialType::Metal) {
        Vec3 reflected = reflect(incoming.direction, record.normal);
        Vec3 fuzz = randomInUnitSphere() * roughness;
        scattered = Ray(record.point, (reflected + fuzz).normalized());
        attenuation = surfaceColor;

        return scattered.direction.dot(record.normal) > 0.0f;
    }

    if (type == MaterialType::Emissive) {
        attenuation = surfaceColor * emission;

        return false;
    }

    return false;
}

Color Material::emitted() const {
    if (type == MaterialType::Emissive)
        return albedo * emission;
        
    return Color(0.0f, 0.0f, 0.0f);
}