/* WARNING: This file was auto-generated by D:/workspace/inverse_kinematics/ik/ik/ik_gen_vtable.py */
#ifndef IK_VTABLE_VEC3_STATIC_H
#define IK_VTABLE_VEC3_STATIC_H

#include "ik/config.h"

C_BEGIN

#include "ik/vec3.h"

IK_PRIVATE_API ik_vec3_t ik_vec3_static_vec3(ikreal_t x, ikreal_t y, ikreal_t z);
IK_PRIVATE_API void ik_vec3_static_set(ikreal_t v[3], const ikreal_t src[3]);
IK_PRIVATE_API void ik_vec3_static_set_zero(ikreal_t v[3]);
IK_PRIVATE_API void ik_vec3_static_add_scalar(ikreal_t v1[3], ikreal_t scalar);
IK_PRIVATE_API void ik_vec3_static_add_vec3(ikreal_t v1[3], const ikreal_t v2[3]);
IK_PRIVATE_API void ik_vec3_static_sub_scalar(ikreal_t v1[3], ikreal_t scalar);
IK_PRIVATE_API void ik_vec3_static_sub_vec3(ikreal_t v1[3], const ikreal_t v2[3]);
IK_PRIVATE_API void ik_vec3_static_mul_scalar(ikreal_t v1[3], ikreal_t scalar);
IK_PRIVATE_API void ik_vec3_static_mul_vec3(ikreal_t v1[3], const ikreal_t v2[3]);
IK_PRIVATE_API void ik_vec3_static_div_scalar(ikreal_t v[3], ikreal_t scalar);
IK_PRIVATE_API void ik_vec3_static_div_vec3(ikreal_t v[3], const ikreal_t v2[3]);
IK_PRIVATE_API ikreal_t ik_vec3_static_length_squared(const ikreal_t v[3]);
IK_PRIVATE_API ikreal_t ik_vec3_static_length(const ikreal_t v[3]);
IK_PRIVATE_API void ik_vec3_static_normalize(ikreal_t v[3]);
IK_PRIVATE_API ikreal_t ik_vec3_static_dot(const ikreal_t v1[3], const ikreal_t v2[3]);
IK_PRIVATE_API void ik_vec3_static_cross(ikreal_t v1[3], const ikreal_t v2[3]);
IK_PRIVATE_API void ik_vec3_static_rotate(ikreal_t v[3], const ikreal_t q[4]);
#define IK_VEC3_STATIC_IMPL \
    ik_vec3_static_vec3, \
    ik_vec3_static_set, \
    ik_vec3_static_set_zero, \
    ik_vec3_static_add_scalar, \
    ik_vec3_static_add_vec3, \
    ik_vec3_static_sub_scalar, \
    ik_vec3_static_sub_vec3, \
    ik_vec3_static_mul_scalar, \
    ik_vec3_static_mul_vec3, \
    ik_vec3_static_div_scalar, \
    ik_vec3_static_div_vec3, \
    ik_vec3_static_length_squared, \
    ik_vec3_static_length, \
    ik_vec3_static_normalize, \
    ik_vec3_static_dot, \
    ik_vec3_static_cross, \
    ik_vec3_static_rotate



















C_END

#endif /* IK_VTABLE_VEC3_STATIC_H */