#pragma once
#include <cmath>
#include <ostream>

struct Vec3
{
    float x, y, z;

    // Constructors
    constexpr Vec3() : x(0), y(0), z(0) {}
    constexpr Vec3(float x_, float y_, float z_) : x(x_), y(y_), z(z_) {}

    // Addition
    constexpr Vec3 operator+(const Vec3& v) const
    {
        return Vec3(x + v.x, y + v.y, z + v.z);
    }

    // Subtraction
    constexpr Vec3 operator-(const Vec3& v) const
    {
        return Vec3(x - v.x, y - v.y, z - v.z);
    }

    // Scalar multiply
    constexpr Vec3 operator*(float s) const
    {
        return Vec3(x * s, y * s, z * s);
    }

    // Scalar divide
    constexpr Vec3 operator/(float s) const
    {
        return Vec3(x / s, y / s, z / s);
    }

    // Compound operators
    Vec3& operator+=(const Vec3& v)
    {
        x += v.x; y += v.y; z += v.z;
        return *this;
    }

    Vec3& operator-=(const Vec3& v)
    {
        x -= v.x; y -= v.y; z -= v.z;
        return *this;
    }

    Vec3& operator*=(float s)
    {
        x *= s; y *= s; z *= s;
        return *this;
    }

    Vec3& operator/=(float s)
    {
        x /= s; y /= s; z /= s;
        return *this;
    }

    // Length
    float length() const
    {
        return std::sqrt(x*x + y*y + z*z);
    }

    // Length squared (faster)
    float lengthSquared() const
    {
        return x*x + y*y + z*z;
    }

    // Normalize
    Vec3 normalized() const
    {
        float len = length();
        return Vec3(x/len, y/len, z/len);
    }

    // Dot product
    static float dot(const Vec3& a, const Vec3& b)
    {
        return a.x*b.x + a.y*b.y + a.z*b.z;
    }

    // Cross product
    static Vec3 cross(const Vec3& a, const Vec3& b)
    {
        return Vec3(
            a.y*b.z - a.z*b.y,
            a.z*b.x - a.x*b.z,
            a.x*b.y - a.y*b.x
        );
    }
};

// Scalar * vector
inline Vec3 operator*(float s, const Vec3& v)
{
    return v * s;
}

// Printing
inline std::ostream& operator<<(std::ostream& os, const Vec3& v)
{
    os << "(" << v.x << ", " << v.y << ", " << v.z << ")";
    return os;
}