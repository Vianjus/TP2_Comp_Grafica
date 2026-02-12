#ifndef GLM_HPP
#define GLM_HPP

#include <cmath>
#include <cstring>
#include <algorithm>

// Vector typesdefaults
struct vec2 {
    float x, y;
    vec2(float x = 0, float y = 0) : x(x), y(y) {}
    float& operator[](int i) { return (&x)[i]; }
};

struct vec3 {
    float x, y, z;
    vec3(float x = 0, float y = 0, float z = 0) : x(x), y(y), z(z) {}
    vec3 operator+(const vec3& v) const { return vec3(x + v.x, y + v.y, z + v.z); }
    vec3 operator-(const vec3& v) const { return vec3(x - v.x, y - v.y, z - v.z); }
    vec3 operator*(float s) const { return vec3(x * s, y * s, z * s); }
    vec3 operator/(float s) const { return vec3(x / s, y / s, z / s); }
    float dot(const vec3& v) const { return x*v.x + y*v.y + z*v.z; }
    float length() const { return std::sqrt(x*x + y*y + z*z); }
    vec3 normalize() const { float len = length(); if (len > 0) return *this / len; return *this; }
    float& operator[](int i) { return (&x)[i]; }
};

struct vec4 {
    float x, y, z, w;
    vec4(float x = 0, float y = 0, float z = 0, float w = 1) : x(x), y(y), z(z), w(w) {}
    float& operator[](int i) { return (&x)[i]; }
};

struct mat4 {
    float m[16];
    
    mat4(float val = 0) {
        std::fill(m, m + 16, val);
    }
    
    mat4(float m0, float m1, float m2, float m3,
         float m4, float m5, float m6, float m7,
         float m8, float m9, float m10, float m11,
         float m12, float m13, float m14, float m15) {
        m[0] = m0; m[1] = m1; m[2] = m2; m[3] = m3;
        m[4] = m4; m[5] = m5; m[6] = m6; m[7] = m7;
        m[8] = m8; m[9] = m9; m[10] = m10; m[11] = m11;
        m[12] = m12; m[13] = m13; m[14] = m14; m[15] = m15;
    }
    
    mat4 operator*(const mat4& other) const {
        mat4 result(0);
        for (int i = 0; i < 4; i++) {
            for (int j = 0; j < 4; j++) {
                for (int k = 0; k < 4; k++) {
                    result.m[i*4+j] += m[i*4+k] * other.m[k*4+j];
                }
            }
        }
        return result;
    }
    
    vec4 operator*(const vec4& v) const {
        return vec4(
            m[0]*v.x + m[1]*v.y + m[2]*v.z + m[3]*v.w,
            m[4]*v.x + m[5]*v.y + m[6]*v.z + m[7]*v.w,
            m[8]*v.x + m[9]*v.y + m[10]*v.z + m[11]*v.w,
            m[12]*v.x + m[13]*v.y + m[14]*v.z + m[15]*v.w
        );
    }
    
    float& operator[](int i) { return m[i]; }
    const float& operator[](int i) const { return m[i]; }
};

// Utility functions
inline float radians(float degrees) {
    return degrees * 3.14159265359f / 180.0f;
}

inline float degrees(float radians) {
    return radians * 180.0f / 3.14159265359f;
}

inline mat4 identity() {
    mat4 result(0);
    result.m[0] = result.m[5] = result.m[10] = result.m[15] = 1.0f;
    return result;
}

inline float* value_ptr(mat4& m) {
    return m.m;
}

inline const float* value_ptr(const mat4& m) {
    return m.m;
}

inline float clamp(float value, float min_val, float max_val) {
    return std::min(std::max(value, min_val), max_val);
}

#endif
