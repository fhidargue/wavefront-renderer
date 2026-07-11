#pragma once

#include <cmath>
#include <math/Vec3.h>
#include <shading/Texture.h>

// TODO: Include better APA citing - Teschner et al
namespace SpatialHashConstants
{
constexpr unsigned int primeX = 73856093u;
constexpr unsigned int primeY = 19349663u;
constexpr unsigned int primeZ = 83492791u;

// Knuth's multiplicative hash constant
constexpr unsigned int knuthMultiplier = 2654435761u;
} // namespace SpatialHashConstants

inline unsigned int hashGridCell(int cellX, int cellY, int cellZ)
{
    unsigned int hash = static_cast<unsigned int>(cellX) * SpatialHashConstants::primeX ^
                        static_cast<unsigned int>(cellY) * SpatialHashConstants::primeY ^
                        static_cast<unsigned int>(cellZ) * SpatialHashConstants::primeZ;

    return hash * SpatialHashConstants::knuthMultiplier;
}

inline Color colorFromHash(unsigned int hash, float reduceContrast)
{
    float r = ((hash >> 16) & 0xFF) / 255.0f;
    float g = ((hash >> 8) & 0xFF) / 255.0f;
    float b = (hash & 0xFF) / 255.0f;

    r = r + (1.0f - r) * reduceContrast;
    g = g + (1.0f - g) * reduceContrast;
    b = b + (1.0f - b) * reduceContrast;

    return Color(r, g, b);
}

inline void hashToUV(unsigned int hash, float& outU, float& outV)
{
    // Split the 32bit hash into two independent 16bit fields
    outU = ((hash >> 16) & 0xFFFF) / 65535.0f;
    outV = (hash & 0xFFFF) / 65535.0f;
}