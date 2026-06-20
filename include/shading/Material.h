# pragma once

#include <cmath>
#include <cstdlib>
#include <math/Vec3.h>
#include <core/Ray.h>
#include <core/HitRecord.h>
#include <shading/Texture.h>
#include <math/Random.h>

using std::rand;
using std::vector;

// Used for diffuse scattering, simulates rough surfaces
inline Vec3 randomInUnitSphere() {
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

// Used for metal reflections
inline Vec3 reflect(const Vec3& incoming, const Vec3& normal)
{
    return incoming - normal * 2.0f * incoming.dot(normal);
}

enum class MaterialType {
    Diffuse, 
    Metal,
    Emissive
};

struct Material {
    MaterialType type;
    Color albedo;
    float roughness;
    float emission;
    int textureID;

    static Material makeDiffuse(const Color& albedo, int textureID = -1) {
        Material material;
        material.type = MaterialType::Diffuse;
        material.albedo = albedo;
        material.roughness = 1.0f;
        material.emission = 0.0f;
        material.textureID = textureID;

        return material;
    }

    static Material makeMetal(const Color& albedo, float roughness, int textureID = -1) {
        Material material;
        material.type = MaterialType::Metal;
        material.albedo = albedo;
        material.roughness = roughness;
        material.emission = 0.0f;
        material.textureID = textureID;

        return material;
    }

    static Material makeEmissive(const Color& albedo, float strength) {
        Material material;
        material.type = MaterialType::Emissive;
        material.albedo = albedo;
        material.roughness = 0.0f;
        material.emission = strength;
        material.textureID = -1;

        return material;
    }

    Color getSurfaceColor(const HitRecord& record, const vector<Texture>& textures) const {
        if (textureID >= 0 && textureID < static_cast<int>(textures.size())) {
            return textures[textureID].sample(record.u, record.v);
        }

        return albedo;
    }

    bool scatter(const Ray& incoming, const HitRecord& record, const vector<Texture>& textures, 
            Color& attenuation, Ray& scattered) const {
        Color surfaceColor = getSurfaceColor(record, textures);

        if (type == MaterialType::Diffuse) {
            // Scatter in a random direction around the normal
            Vec3 scatterDirection = record.normal + randomInUnitSphere().normalized();
            scattered = Ray(record.point, scatterDirection.normalized());
            attenuation = surfaceColor;

            return true;
        }

        if (type == MaterialType::Metal)
        {
            // Reflect the ray around the normal, add roughness
            Vec3 reflected = reflect(incoming.direction, record.normal);
            Vec3 fuzz = randomInUnitSphere() * roughness;
            scattered = Ray(record.point, (reflected + fuzz).normalized());
            attenuation = surfaceColor;

            // Absorb if scattered is below the surface
            return scattered.direction.dot(record.normal) > 0.0f;
        }

        if (type == MaterialType::Emissive)
        {
            // Just emit light
            attenuation = surfaceColor * emission;

            return false;
        }

        return false;
    }

    Color emitted() const {
        if (type == MaterialType::Emissive)
            return albedo * emission;
            
        return Color(0.0f, 0.0f, 0.0f);
    }
};