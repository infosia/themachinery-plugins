/* WARNING: This file was auto-generated by D:/workspace/inverse_kinematics/ik/ik/ik_gen_vtable.py */
#ifndef IK_VTABLE_SOLVER_TWO_BONE_H
#define IK_VTABLE_SOLVER_TWO_BONE_H

#include "ik/config.h"

C_BEGIN

#include "ik/solver_base.h"

IK_PRIVATE_API uintptr_t ik_solver_TWO_BONE_type_size(void);
IK_PRIVATE_API ikret_t ik_solver_TWO_BONE_construct(struct ik_solver_t* solver);
IK_PRIVATE_API void ik_solver_TWO_BONE_destruct(struct ik_solver_t* solver);
IK_PRIVATE_API ikret_t ik_solver_TWO_BONE_solve(struct ik_solver_t* solver);
static inline ikret_t ik_solver_TWO_BONE_harness_solve_return_value(ikret_t, ikret_t);
#define IK_SOLVER_TWO_BONE_IMPL \
    ik_solver_TWO_BONE_type_size, \
    ik_solver_base_create, \
    ik_solver_base_destroy, \
    ik_solver_TWO_BONE_harness_construct, \
    ik_solver_TWO_BONE_harness_destruct, \
    ik_solver_base_rebuild, \
    ik_solver_base_update_distances, \
    ik_solver_TWO_BONE_harness_solve, \
    ik_solver_base_set_tree, \
    ik_solver_base_unlink_tree, \
    ik_solver_base_destroy_tree, \
    ik_solver_base_iterate_all_nodes, \
    ik_solver_base_iterate_affected_nodes, \
    ik_solver_base_iterate_base_nodes


static inline ikret_t ik_solver_TWO_BONE_harness_construct(struct ik_solver_t* solver)
{
    ikret_t result;
    if ((result = ik_solver_base_construct(solver)) != IK_OK) goto solver_base_failed;
    if ((result = ik_solver_TWO_BONE_construct(solver)) != IK_OK) goto solver_TWO_BONE_failed;
    return IK_OK;
    solver_TWO_BONE_failed : ik_solver_base_destruct(solver);
    solver_base_failed : return result;
}
static inline void ik_solver_TWO_BONE_harness_destruct(struct ik_solver_t* solver)
{
    ik_solver_TWO_BONE_destruct(solver);
    ik_solver_base_destruct(solver);
}
static inline ikret_t ik_solver_TWO_BONE_harness_solve(struct ik_solver_t* solver)
{
    ikret_t a = ik_solver_base_solve(solver);
    ikret_t b = ik_solver_TWO_BONE_solve(solver);
    return ik_solver_TWO_BONE_harness_solve_return_value(a, b);
}


/*
 * Because we use X macros to fill in the ik interface struct, we have to
 * generate the implementation defines for the node, effector and constraint
 * interfaces as well. These don't actually override anything.
 */

#define IK_NODE_TWO_BONE_IMPL \
    ik_node_base_create, \
    ik_node_base_construct, \
    ik_node_base_destruct, \
    ik_node_base_destroy, \
    ik_node_base_create_child, \
    ik_node_base_add_child, \
    ik_node_base_unlink, \
    ik_node_base_find_child, \
    ik_node_base_duplicate, \
    ik_node_base_dump_to_dot




#define IK_EFFECTOR_TWO_BONE_IMPL \
    ik_effector_base_create, \
    ik_effector_base_destroy, \
    ik_effector_base_attach, \
    ik_effector_base_detach




#define IK_CONSTRAINT_TWO_BONE_IMPL \
    ik_constraint_base_create, \
    ik_constraint_base_set_type, \
    ik_constraint_base_set_custom, \
    ik_constraint_base_destroy, \
    ik_constraint_base_attach, \
    ik_constraint_base_detach




/*
 * Need to combine multiple ikret_t return values from the various before/after
 * functions.
 */
static inline ikret_t ik_solver_TWO_BONE_harness_rebuild_return_value(ikret_t a, ikret_t b) {
    if (a != IK_OK) return a;
    return b;
}
static inline ikret_t ik_solver_TWO_BONE_harness_solve_return_value(ikret_t a, ikret_t b) {
    if (a != IK_OK) return a;
    return b;
}
C_END

#endif /* IK_VTABLE_SOLVER_TWO_BONE_H */
