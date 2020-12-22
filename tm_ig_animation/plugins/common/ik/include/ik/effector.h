#pragma once

#include "ik/config.h"
#include "ik/attachment.h"
#include "ik/vec3.h"
#include "ik/quat.h"

/*!
 * WEIGHT_NLERP Causes intermediary weight values to rotate the target around the
 * chain's base instead of linearly interpolating the target. Can be more
 * appealing if the solved tree diverges a lot from the original tree
 * during weight transitions.
 */
#define IK_EFFECTOR_FEATURES_LIST \
    X(WEIGHT_NLERP,            weight_nlerp,            0x01)

C_BEGIN

enum ik_effector_feature
{
#define X(upper, lower, value) IK_EFFECTOR_##upper = value,
    IK_EFFECTOR_FEATURES_LIST
#undef X

    IK_EFFECTOR_FLAGS_COUNT
};

/*!
 * @brief Specifies how a chain of nodes should be solved. The effector can
 * be attached to any node in a tree using ik_node_attach_effector(). The
 * effector specifies the target position and rotation of that node, as well
 * as how much influence the algorithm has on the tree (weight) and how many
 * child nodes are affected (chain_length).
 */
struct ik_effector
{
    IK_ATTACHMENT_HEAD

    /*!
     * @brief Can be set at any point, and should be updated whenever you have
     * a new target position to solve for. Specifies the global (world)
     * position where the node it is attached to should head for.
     * @note Default value is (0, 0, 0).
     */
    union ik_vec3 target_position;

    /*!
     * @brief Can be set at any point, and should be updated whenever you have
     * a new target rotation to solve for. Specifies the global (world)
     * rotation where the node it is attached to should head for.
     * @note Default value is the identity quaternion.
     */
    union ik_quat target_rotation;

    /*!
     * @brief Specifies how much influence the algorithm has on the chain of
     * nodes. A value of 0.0 will cause the algorithm to completely ignore the
     * chain, while a value of 1.0 will cause the algorithm to try to place the
     * target node directly at target_position/target_rotation.
     *
     * This is useful for blending the algorithm in and out. For instance, if you
     * wanted to ground the legs of an animated character, you would want the
     * algorithm to do nothing during the time when the foot is in the air
     * (weight=0.0) and be fully active when the foot is on the ground
     * (weight=1.0).
     */
    ikreal weight;

    ikreal rotation_weight;
    ikreal rotation_decay;

    /*!
     * @brief Specifies how many parent nodes should be affected. A value of
     * 0 means all of the parents, including the base node.
     * @note Changing the chain length requires the algorithm tree to be rebuilt
     * with ik_algorithm_rebuild_tree().
     */
    uint16_t chain_length;

    /*!
     * @brief Various behavioral settings. Check the enum effector_flags_e for
     * more information.
     */
    uint16_t features;
};

/*!
 * @brief Creates a new effector object. It can be attached to any node in the
 * tree using ik_node_attach_effector().
 */
IK_PUBLIC_API struct ik_effector*
ik_effector_create(void);

IK_PUBLIC_API struct ik_effector*
ik_effector_duplicate(const struct ik_effector* effector);

IK_PUBLIC_API int
ik_effector_duplicate_from_tree(struct ik_tree_object* dst, const struct ik_tree_object* src);

C_END
