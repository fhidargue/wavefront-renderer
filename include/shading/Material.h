#pragma once

#include <vector>
#include <math/Vec3.h>
#include <core/Ray.h>
#include <core/HitRecord.h>
#include <shading/Texture.h>

enum class MaterialType
{
    Diffuse,
    Metal,
    Emissive,
    SpotLight
};

struct Material
{
    MaterialType type;
    Color albedo;
    float roughness;
    float emission;
    int textureID;
    std::string uuid;

    // Restrictions for Spotlight emissive materials
    float spotOuterAngle;
    float spotFalloffAngle;

    static Material makeDiffuse(const Color& albedo, int textureID = -1);
    static Material makeMetal(const Color& albedo, float roughness, int textureID = -1);
    static Material makeEmissive(const Color& albedo, float strength);
    static Material makeSpotLight(const Color& albedo, float strength, float spotOuterAngleDeg,
                                  float spotFalloffAngleDeg);

    Color getSurfaceColor(const HitRecord& record, const std::vector<Texture>& textures) const;

    bool scatter(const Ray& incoming, const HitRecord& record, const std::vector<Texture>& textures,
                 Color& attenuation, Ray& scattered, float& outPdf) const;

    Color emitted(const Vec3& lightNormal = Vec3(0.0f, 0.0f, 1.0f),
                  const Vec3& directionFromLight = Vec3(0.0f, 0.0f, 1.0f)) const;
};

Vec3 randomInUnitSphere();
Vec3 reflect(const Vec3& incoming, const Vec3& normal);
Vec3 cosineSampleHemisphere(const Vec3& normal);