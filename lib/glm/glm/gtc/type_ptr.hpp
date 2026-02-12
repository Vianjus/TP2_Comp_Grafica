#ifndef GLM_TYPE_PTR_HPP
#define GLM_TYPE_PTR_HPP

#include "glm.hpp"

inline float* value_ptr(mat4& m) {
    return m.m;
}

inline const float* value_ptr(const mat4& m) {
    return m.m;
}

#endif
