#pragma once

#include "ik/config.h"
#include "ik/bone.h"
#include "ik/node.h"
#include "ik/quat.inl"
#include "ik/vec3.inl"

#define IK_TRANSFORM_MODE_LIST \
    X(G2L, 0x00) \
    X(L2G, 0x01) \
    X(ROTATIONS, 0x02) \
    X(TRANSLATIONS, 0x04)

C_BEGIN

struct ik_algorithm;
struct ik_bone;
struct ik_chain;
struct cs_vector;

enum ik_transform_mode
{
#define X(arg, value) IK_TRANSFORM_##arg = value,
    IK_TRANSFORM_MODE_LIST
#undef X

    IK_TRANSFORM_MODE_COUNT
};

union ik_transform
{
    struct
    {
        /*
         * WARNING: HAS to be in this order -- there's some hacking going on
         * in transform.c which relies on the order of ikreal's in transform[7].
         */
        union ik_quat rot;
        union ik_vec3 pos;
    } t;
    ikreal f[7];
};

/*!
 * Transforms a position from the space of "base" into the space of "tip"
 */
static inline void
ik_transform_node_pos_g2l(ikreal pos[3], const struct ik_node* base, const struct ik_node* tip)
{
    if (tip == base)
        return;

    assert(tip);
    ik_transform_node_pos_g2l(pos, base, ik_node_get_parent(tip));

    ik_vec3_sub_vec3(pos, tip->position.f);
    ik_vec3_rotate_quat_conj(pos, tip->rotation.f);
}

/*!
 * Transforms a position from the space of "tip" into the space of "base"
 */
static inline void
ik_transform_node_pos_l2g(ikreal pos[3], const struct ik_node* tip, const struct ik_node* base)
{
    while (tip != base)
    {
        ik_vec3_rotate_quat(pos, tip->rotation.f);
        ik_vec3_add_vec3(pos, tip->position.f);
        tip = ik_node_get_parent(tip);
    }
}

/*!
 * Transforms a position from the space of "base" into the space of "tip"
 */
static inline void
ik_transform_bone_pos_g2l_recurse(ikreal pos[3], const struct ik_bone* base, const struct ik_bone* tip)
{
    if (tip == base)
    {
        if (tip)
            pos[2] -= tip->length;
        return;
    }

    assert(tip);
    ik_transform_bone_pos_g2l_recurse(pos, base, ik_bone_get_parent(tip));

    ik_vec3_sub_vec3(pos, tip->position.f);
    ik_vec3_rotate_quat_conj(pos, tip->rotation.f);
    pos[2] -= tip->length;
}
static inline void
ik_transform_bone_pos_g2l(ikreal pos[3], const struct ik_bone* base, const struct ik_bone* tip)
{
    if (tip == base)
        return;

    assert(tip);
    ik_transform_bone_pos_g2l_recurse(pos, base, ik_bone_get_parent(tip));

    ik_vec3_sub_vec3(pos, tip->position.f);
    ik_vec3_rotate_quat_conj(pos, tip->rotation.f);
}

/*!
 * Transforms a position from the space of "tip" into the space of "base"
 */
static inline void
ik_transform_bone_pos_l2g(ikreal pos[3], const struct ik_bone* tip, const struct ik_bone* base)
{
    if (tip == base)
        return;

    ik_vec3_rotate_quat(pos, tip->rotation.f);
    ik_vec3_add_vec3(pos, tip->position.f);
    tip = ik_bone_get_parent(tip);

    while (tip != base)
    {
        pos[2] += tip->length;
        ik_vec3_rotate_quat(pos, tip->rotation.f);
        ik_vec3_add_vec3(pos, tip->position.f);
        tip = ik_bone_get_parent(tip);
    }

    if (tip)
        pos[2] += tip->length;
}

/*!
 * @brief Canonicalizes all bone transforms and converts rotations from a nodal
 * representation into a segmental representation. All solvers rely on this
 * representation.
 *
 * @param[out] intermediate_rotations
 */
IK_PRIVATE_API void
ik_transform_chain_to_segmental_representation(struct ik_chain* root,
                                               union ik_quat* intermediate_rotations,
                                               int num_intermediate_rotations);

IK_PRIVATE_API void
ik_transform_chain_to_nodal_representation(struct ik_chain* root,
                                           union ik_quat* intermediate_rotations,
                                           int num_intermediate_rotations);

IK_PUBLIC_API void
ik_transform_nodes_to_bones(const struct ik_node* node_root, struct ik_bone* bone_root);

IK_PUBLIC_API void
ik_transform_bones_to_nodes(const struct ik_bone* bone_root, struct ik_node* node_root);

C_END
