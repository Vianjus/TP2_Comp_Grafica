#ifndef GLM_MATRIX_TRANSFORM_HPP
#define GLM_MATRIX_TRANSFORM_HPP

#include "glm.hpp"
#include <cmath>

inline mat4 lookAt(vec3 eye, vec3 center, vec3 up) {
    vec3 f = (center - eye).normalize();
    vec3 s = (f).normalize();
    // Cross product
    s = vec3(
        f.y * up.z - f.z * up.y,
        f.z * up.x - f.x * up.z,
        f.x * up.y - f.y * up.x
    ).normalize();
    
    vec3 u = vec3(
        s.y * f.z - s.z * f.y,
        s.z * f.x - s.x * f.z,
        s.x * f.y - s.y * f.x
    );
    
    mat4 result = identity();
    result.m[0] = s.x;
    result.m[4] = s.y;
    result.m[8] = s.z;
    result.m[1] = u.x;
    result.m[5] = u.y;
    result.m[9] = u.z;
    result.m[2] = -f.x;
    result.m[6] = -f.y;
    result.m[10] = -f.z;
    result.m[12] = -s.dot(eye);
    result.m[13] = -u.dot(eye);
    result.m[14] = f.dot(eye);
    
    return result;
}

inline mat4 perspective(float fov_radians, float aspect, float near, float far) {
    float f = 1.0f / std::tan(fov_radians / 2.0f);
    mat4 result(0);
    result.m[0] = f / aspect;
    result.m[5] = f;
    result.m[10] = (far + near) / (near - far);
    result.m[14] = (2.0f * far * near) / (near - far);
    result.m[11] = -1.0f;
    return result;
}

inline mat4 translate(const mat4& m, const vec3& v) {
    mat4 result = m;
    result.m[12] = m.m[0]*v.x + m.m[4]*v.y + m.m[8]*v.z + m.m[12];
    result.m[13] = m.m[1]*v.x + m.m[5]*v.y + m.m[9]*v.z + m.m[13];
    result.m[14] = m.m[2]*v.x + m.m[6]*v.y + m.m[10]*v.z + m.m[14];
    result.m[15] = m.m[3]*v.x + m.m[7]*v.y + m.m[11]*v.z + m.m[15];
    return result;
}

inline mat4 rotate(const mat4& m, float angle, const vec3& axis) {
    float c = std::cos(angle);
    float s = std::sin(angle);
    vec3 a = axis.normalize();
    
    mat4 rot = identity();
    rot.m[0] = c + a.x*a.x*(1-c);
    rot.m[1] = a.x*a.y*(1-c) - a.z*s;
    rot.m[2] = a.x*a.z*(1-c) + a.y*s;
    rot.m[4] = a.y*a.x*(1-c) + a.z*s;
    rot.m[5] = c + a.y*a.y*(1-c);
    rot.m[6] = a.y*a.z*(1-c) - a.x*s;
    rot.m[8] = a.z*a.x*(1-c) - a.y*s;
    rot.m[9] = a.z*a.y*(1-c) + a.x*s;
    rot.m[10] = c + a.z*a.z*(1-c);
    
    return m * rot;
}

inline mat4 scale(const mat4& m, const vec3& v) {
    mat4 result = m;
    result.m[0] *= v.x;
    result.m[5] *= v.y;
    result.m[10] *= v.z;
    return result;
}

#endif
