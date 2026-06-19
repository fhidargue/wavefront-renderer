# pragma once

#include <cmath>
#include <cstdlib>
#include "Vec3.h"
#include "Ray.h"
#include "HitRecord.h"

using std::rand;

inline float randomFloat() {
    return static_cast<float>(rand()) / static_cast<float>(RAND_MAX);
}

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

    static Material makeDiffuse(const Color& albedo) {
        Material material;
        material.type = MaterialType::Diffuse;
        material.albedo = albedo;
        material.roughness = 1.0f;
        material.emission = 0.0f;

        return material;
    }

    static Material makeMetal(const Color& albedo, float roughness) {
        Material material;
        material.type = MaterialType::Metal;
        material.albedo = albedo;
        material.roughness = roughness;
        material.emission = 0.0f;

        return material;
    }

    static Material makeEmissive(const Color& albedo, float strength) {
        Material material;
        material.type = MaterialType::Emissive;
        material.albedo = albedo;
        material.roughness = 0.0f;
        material.emission = strength;

        return material;
    }

    bool scatter(const Ray& incoming, const HitRecord& record,
                 Color& attenuation, Ray& scattered) const {
        if (type == MaterialType::Diffuse)
        {
            // Scatter in a random direction around the normal
            Vec3 scatterDirection = record.normal + randomInUnitSphere().normalized();
            scattered = Ray(record.point, scatterDirection.normalized());
            attenuation = albedo;

            return true;
        }

        if (type == MaterialType::Metal)
        {
            // Reflect the ray around the normal, add roughness
            Vec3 reflected = reflect(incoming.direction, record.normal);
            Vec3 fuzz = randomInUnitSphere() * roughness;
            scattered = Ray(record.point, (reflected + fuzz).normalized());
            attenuation = albedo;

            // Absorb if scattered is below the surface
            return scattered.direction.dot(record.normal) > 0.0f;
        }

        if (type == MaterialType::Emissive)
        {
            // Just emit light
            attenuation = albedo * emission;

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