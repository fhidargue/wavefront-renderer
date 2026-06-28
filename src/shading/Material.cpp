#include <shading/Material.h>
#include <math/Random.h>
#include <cmath>

const float PI = 3.14159265f;

Vec3 randomInUnitSphere()
{
    while (true)
    {
        Vec3 candidate(randomFloat() * 2.0f - 1.0f, randomFloat() * 2.0f - 1.0f,
                       randomFloat() * 2.0f - 1.0f);
        if (candidate.dot(candidate) < 1.0f)
            return candidate;
    }
}

Vec3 reflect(const Vec3& incoming, const Vec3& normal)
{
    return incoming - normal * 2.0f * incoming.dot(normal);
}

Vec3 cosineSampleHemisphere(const Vec3& normal)
{
    // Cosine-weighted hemisphere sampling via samples of a 2D disk
    float randomRadius = std::sqrt(randomFloat());
    float randomAngle  = 2.0f * PI * randomFloat();
    float directionX = randomRadius * std::cos(randomAngle);
    float directionZ = randomRadius * std::sin(randomAngle);
    float directionY = std::sqrt(std::max(0.0f, 1.0f - randomRadius * randomRadius));

    // Build a local coordinate frame with the surface
    // normal as the Y axis, so the sampled direction is in world space
    Vec3 worldUp    = std::abs(normal.x) > 0.9f ? Vec3(0, 1, 0) : Vec3(1, 0, 0);
    Vec3 tangent    = normal.cross(worldUp).normalized();
    Vec3 bitangent  = normal.cross(tangent);

    // Transform from tangent space to world space
    return (tangent * directionX + normal * directionY + bitangent * directionZ).normalized();
}

Material Material::makeDiffuse(const Color& albedo, int textureID)
{
    Material material;
    material.type = MaterialType::Diffuse;
    material.albedo = albedo;
    material.roughness = 1.0f;
    material.emission = 0.0f;
    material.textureID = textureID;

    return material;
}

Material Material::makeMetal(const Color& albedo, float roughness, int textureID)
{
    Material material;
    material.type = MaterialType::Metal;
    material.albedo = albedo;
    material.roughness = roughness;
    material.emission = 0.0f;
    material.textureID = textureID;

    return material;
}

Material Material::makeEmissive(const Color& albedo, float strength)
{
    Material material;
    material.type = MaterialType::Emissive;
    material.albedo = albedo;
    material.roughness = 0.0f;
    material.emission = strength;
    material.textureID = -1;

    return material;
}

Color Material::getSurfaceColor(const HitRecord& record, const std::vector<Texture>& textures) const
{
    if (textureID >= 0 && textureID < static_cast<int>(textures.size()))
        return textures[textureID].sample(record.u, record.v);

    return albedo;
}

bool Material::scatter(const Ray& incoming, const HitRecord& record,
                       const std::vector<Texture>& textures, Color& attenuation,
                       Ray& scattered) const
{
    Color surfaceColor = getSurfaceColor(record, textures);

    if (type == MaterialType::Diffuse)
    {
        Vec3 scatterDirection = cosineSampleHemisphere(record.normal);
        scattered = Ray(record.point, scatterDirection);
        attenuation = surfaceColor;

        return true;
    }

    if (type == MaterialType::Metal)
    {
        Vec3 reflected = reflect(incoming.direction, record.normal);
        Vec3 fuzz = randomInUnitSphere() * roughness;
        scattered = Ray(record.point, (reflected + fuzz).normalized());
        attenuation = surfaceColor;

        return scattered.direction.dot(record.normal) > 0.0f;
    }

    if (type == MaterialType::Emissive)
    {
        attenuation = surfaceColor * emission;

        return false;
    }

    return false;
}

Color Material::emitted() const
{
    if (type == MaterialType::Emissive)
        return albedo * emission;

    return Color(0.0f, 0.0f, 0.0f);
}