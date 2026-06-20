#pragma once

#include <cmath>
#include <iostream>

using std::sqrt;

struct Vec3 {
    float x, y, z;

    Vec3() : x(0.0f), y(0.0f), z(0.0f) {}
    Vec3 (float x, float y, float z) : x(x), y(y), z(z) {}

    Vec3 operator+(const Vec3& other) const {
        return Vec3(x + other.x, y + other.y, z + other.z);
    }

    Vec3 operator-(const Vec3& other) const {
        return Vec3(x - other.x, y - other.y, z - other.z);
    }

    Vec3 operator*(float value) const {
        return Vec3(x * value, y * value, z * value);
    }

    Vec3 operator/(float value) const {
        return Vec3(x / value, y / value, z / value);
    }

    // Multiplication between vectors
    Vec3 operator*(const Vec3& other) const {
        return Vec3(x * other.x, y * other.y, z * other.z);
    }

    float length() const {
        return sqrt(x*x + y*y + z*z);
    }

    // Dot product
    float dot(const Vec3& other) const {
        return x*other.x + y*other.y + z*other.z; 
    }

    // Cross product
    Vec3 cross(const Vec3& other) const {
        return Vec3(
            y * other.z - z * other.y,
            z * other.x - x * other.z,
            x * other.y - y * other.x
        );
    }

    // Normalize
    Vec3 normalized() const {
        return *this / length();
    }
};

// Scalar on the left
inline Vec3 operator*(float value, const Vec3& other) {
    return other * value;
}

using Point3 = Vec3;
using Color = Vec3;