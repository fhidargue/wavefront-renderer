#pragma once

#include <math/Vec3.h>

// Has an origin and a direction where it goes

struct Ray
{
    Point3 origin;
    Vec3 direction;

    Ray()
    {
    }
    Ray(const Point3& origin, const Vec3& direction) : origin(origin), direction(direction)
    {
    }

    Point3 at(float value) const
    {
        return origin + direction * value;
    }
};