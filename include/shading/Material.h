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
    Emissive
};

struct Material
{
    MaterialType type;
    Color albedo;
    float roughness;
    float emission;
    int textureID;
    std::string uuid;

    static Material makeDiffuse(const Color& albedo, int textureID = -1);
    static Material makeMetal(const Color& albedo, float roughness, int textureID = -1);
    static Material makeEmissive(const Color& albedo, float strength);

    Color getSurfaceColor(const HitRecord& record, const std::vector<Texture>& textures) const;

    bool scatter(const Ray& incoming, const HitRecord& record, const std::vector<Texture>& textures,
                 Color& attenuation, Ray& scattered) const;

    Color emitted() const;
};

Vec3 randomInUnitSphere();
Vec3 reflect(const Vec3& incoming, const Vec3& normal);