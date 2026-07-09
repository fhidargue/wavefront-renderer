#include <shading/Material.h>
#include <math/Random.h>
#include <cmath>

using std::sqrt;

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
    float randomRadius = sqrt(randomFloat());
    float randomAngle = 2.0f * PI * randomFloat();
    float directionX = randomRadius * std::cos(randomAngle);
    float directionZ = randomRadius * std::sin(randomAngle);
    float directionY = sqrt(std::max(0.0f, 1.0f - randomRadius * randomRadius));

    // Build a local coordinate frame with the surface
    // normal as the Y axis, so the sampled direction is in world space
    Vec3 worldUp = std::abs(normal.x) > 0.9f ? Vec3(0, 1, 0) : Vec3(1, 0, 0);
    Vec3 tangent = normal.cross(worldUp).normalized();
    Vec3 bitangent = normal.cross(tangent);

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

Material Material::makeSpotLight(const Color& albedo, float strength, float spotOuterAngleDeg,
                                 float spotFalloffAngleDeg)
{
    Material material;
    material.type = MaterialType::SpotLight;
    material.albedo = albedo;
    material.roughness = 0.0f;
    material.emission = strength;
    material.textureID = -1;
    material.spotOuterAngle = spotOuterAngleDeg;
    material.spotFalloffAngle = spotFalloffAngleDeg;

    return material;
}

Color Material::getSurfaceColor(const HitRecord& record, const std::vector<Texture>& textures) const
{
    if (textureID >= 0 && textureID < static_cast<int>(textures.size()))
        return textures[textureID].sample(record.u, record.v);

    return albedo;
}

bool Material::scatter(const Ray& incoming, const HitRecord& record,
                       const std::vector<Texture>& textures, Color& attenuation, Ray& scattered,
                       float& outPdf) const
{
    Color surfaceColor = getSurfaceColor(record, textures);

    switch (type)
    {
    case MaterialType::Diffuse:
    {
        Vec3 scatterDirection = cosineSampleHemisphere(record.normal);
        scattered = Ray(record.point, scatterDirection);
        attenuation = surfaceColor;

        float cosAtSurface = std::max(0.0f, record.normal.dot(scatterDirection));
        outPdf = cosAtSurface / PI;

        return true;
    }

    case MaterialType::Metal:
    {
        Vec3 reflected = reflect(incoming.direction, record.normal);
        Vec3 fuzz = randomInUnitSphere() * roughness;
        scattered = Ray(record.point, (reflected + fuzz).normalized());
        attenuation = surfaceColor;

        // Metal has no clean PDF
        outPdf = -1.0f;

        return scattered.direction.dot(record.normal) > 0.0f;
    }

    case MaterialType::Emissive:
    case MaterialType::SpotLight:
    {
        attenuation = surfaceColor * emission;
        outPdf = -1.0f;

        return false;
    }

    default:
    {
        outPdf = -1.0f;
        return false;
    }
    }
}

Color Material::emitted(const Vec3& lightNormal, const Vec3& directionFromLight) const
{
    if (type == MaterialType::Emissive)
        return albedo * emission;

    if (type == MaterialType::SpotLight)
    {
        Color radiance = albedo * emission;

        float cosAngle = lightNormal.dot(directionFromLight);
        float angleDeg = std::acos(std::max(-1.0f, std::min(1.0f, cosAngle))) * (180.0f / PI);

        if (angleDeg >= spotOuterAngle)
            return Color(0.0f, 0.0f, 0.0f);

        if (angleDeg > spotFalloffAngle)
        {
            float t = (angleDeg - spotFalloffAngle) / (spotOuterAngle - spotFalloffAngle);
            float falloff = 1.0f - t;

            // Logic for the smoothstep
            falloff = falloff * falloff * (3.0f - 2.0f * falloff);
            radiance = radiance * falloff;
        }

        return radiance;
    }

    return Color(0.0f, 0.0f, 0.0f);
}