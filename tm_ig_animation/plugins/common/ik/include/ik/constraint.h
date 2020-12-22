#pragma once

#include "ik/config.h"
#include "ik/attachment.h"
#include "ik/vec3.h"
#include "ik/quat.h"

C_BEGIN

struct ik_constraint;
struct ik_node_data_t;

typedef void (*ik_constraint_apply_func)(struct ik_constraint* constraint,
                                         ikreal rotation[4]);

struct ik_constraint
{
    IK_ATTACHMENT_HEAD
    struct ik_constraint* next;

    /*!
     * Called by the solver whenever a joint needs to be constrained. The
     * function receives the current (absolute local rotation) of the bone,
     * and should write back a new rotation if the current one is outside of
     * the constraint's range.
     */
    ik_constraint_apply_func apply;

    union {
        struct {
            union ik_quat rotation;
        } stiff;
        struct {
            union ik_vec3 axis;
            ikreal min_angle;
            ikreal max_angle;
        } hinge;
        struct {
            union ik_quat angle;
            ikreal min_angle;
            ikreal max_angle;
        } cone;
        struct {
            void* data;
        } custom;
    } data;
};

/*!
 * @brief Creates a new constraint object. It can be attached to any node in the
 * tree using ik_node_attach_constraint().
 */
IK_PUBLIC_API struct ik_constraint*
ik_constraint_create(void);

IK_PUBLIC_API void
ik_constraint_append(struct ik_constraint* first_constraint,
                     struct ik_constraint* constraint);

IK_PUBLIC_API void
ik_constraint_set_stiff(struct ik_constraint* constraint,
                        ikreal qx, ikreal qy, ikreal qz, ikreal qw);

IK_PUBLIC_API void
ik_constraint_set_hinge(struct ik_constraint* constraint,
                        ikreal axis_x, ikreal axis_y, ikreal axis_z,
                        ikreal min_angle, ikreal max_angle);

IK_PUBLIC_API void
ik_constraint_set_cone(struct ik_constraint* constraint,
                       ikreal qx, ikreal qy, ikreal qz, ikreal qw,
                       ikreal min_angle, ikreal max_angle);

IK_PUBLIC_API void
ik_constraint_set_roll(struct ik_constraint* constraint,
                       ikreal min_angle, ikreal max_angle);

/*!
 * @brief Allows the user to specify a custom callback function for enforcing
 * a constraint.
 */
IK_PUBLIC_API void
ik_constraint_set_custom(struct ik_constraint* constraint,
                         ik_constraint_apply_func callback,
                         void* data);

IK_PUBLIC_API struct ik_constraint*
ik_constraint_duplicate(const struct ik_constraint* constraint);

IK_PUBLIC_API struct ik_constraint*
ik_constraint_duplicate_chain(const struct ik_constraint* constraint);

IK_PUBLIC_API int
ik_constraint_duplicate_from_tree(struct ik_tree_object* dst, const struct ik_tree_object* src);

C_END
