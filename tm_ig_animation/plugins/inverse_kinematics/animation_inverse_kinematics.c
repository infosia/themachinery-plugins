#include "animation_inverse_kinematics.h"

#include <plugins/entity/entity.h>
#include <plugins/entity/scene_tree_component.h>
#include <plugins/the_machinery_shared/component_interfaces/editor_ui_interface.h>
#include <plugins/entity/tag_component.h>

#include <foundation/api_registry.h>
#include <foundation/carray.inl>
#include <foundation/feature_flags.h>
#include <foundation/localizer.h>
#include <foundation/math.inl>
#include <foundation/the_truth.h>
#include <foundation/the_truth_types.h>
#include <foundation/log.h>

#include "ik/ik.h"

static struct tm_entity_api *tm_entity_api;
static struct tm_temp_allocator_api *tm_temp_allocator_api;
static struct tm_the_truth_api *tm_the_truth_api;
static struct tm_the_truth_common_types_api *tm_the_truth_common_types_api;
static struct tm_localizer_api *tm_localizer_api;
static struct tm_logger_api *tm_logger_api;
static struct tm_scene_tree_component_api *tm_scene_tree_component_api;
static struct tm_tag_component_api *tm_tag_component_api;
struct tm_tag_component_manager_o* tm_tag_component_manager;

typedef struct ik_solver_t ik_solver_t;
typedef struct ik_effector_t ik_effector_t;
typedef struct ik_node_t ik_node_t;

static uint32_t ik_retain_count = 0;

#define NODE_NOT_FOUND UINT32_MAX
#define ANIMATION_ROOT_TAG_HASH TM_STATIC_HASH("animation_root", 0x83cfc2072683b940ULL)

#define LEFT_UP_LEG TM_STATIC_HASH("mixamorig:LeftUpLeg", 0x90e5274920e6bcbdULL)
#define RIGHT_UP_LEG TM_STATIC_HASH("mixamorig:RightUpLeg", 0xfbcd8c04e5ae0ee5ULL)
#define LEFT_LEG TM_STATIC_HASH("mixamorig:LeftLeg", 0x60a337ed3124d2cULL)
#define RIGHT_LEG TM_STATIC_HASH("mixamorig:RightLeg", 0xa18d2aab53d4aa4bULL)
#define LEFT_FOOT TM_STATIC_HASH("mixamorig:LeftFoot", 0xc1bd774f86dd8ebfULL)
#define RIGHT_FOOT TM_STATIC_HASH("mixamorig:RightFoot", 0x580df98c5729fcf8ULL)

typedef struct tm_animation_ik_component_t
{
    uint32_t max_iterations;
    float tolerance;

    ik_node_t* upLeg_L;
    ik_node_t* upLeg_R;
    ik_node_t* leg_L;
    ik_node_t* leg_R;
    ik_node_t* foot_L;
    ik_node_t* foot_R;

    tm_scene_tree_component_t* stc;
    uint32_t upLeg_L_node;
    uint32_t upLeg_R_node;
    uint32_t leg_L_node;
    uint32_t leg_R_node;
    uint32_t foot_L_node;
    uint32_t foot_R_node;

} tm_animation_ik_component_t;

typedef struct tm_animation_ik_component_manager_t
{
    tm_entity_context_o *ctx;
    tm_allocator_i allocator;
} tm_animation_ik_component_manager_t;

static tm_animation_ik_component_t default_values = {
    .max_iterations = 20,
    .tolerance = 1e-3f
};

static const char *component__category(void)
{
    return TM_LOCALIZE("Animation");
}

static tm_ci_editor_ui_i *editor_aspect = &(tm_ci_editor_ui_i){
    .category = component__category
};

#include "tm_ig_truth_common.inl"

static void component__create_types(struct tm_the_truth_o *tt)
{
    tm_the_truth_property_definition_t animation_ik_component_properties[] = {
        { "max_iteration", TM_THE_TRUTH_PROPERTY_TYPE_UINT32_T },
        { "tolerance", TM_THE_TRUTH_PROPERTY_TYPE_FLOAT }
    };

    const uint64_t object_type = tm_the_truth_api->create_object_type(tt, TM_TT_TYPE__IG_ANIMATION_IK, animation_ik_component_properties, TM_ARRAY_COUNT(animation_ik_component_properties));

    const tm_tt_id_t component = tm_the_truth_api->create_object_of_type(tt, tm_the_truth_api->object_type_from_name_hash(tt, TM_TT_TYPE_HASH__IG_ANIMATION_IK), TM_TT_NO_UNDO_SCOPE);
    tm_the_truth_object_o *component_w = tm_the_truth_api->write(tt, component);

    tm_the_truth_api->set_uint32_t(tt, component_w, TM_TT_PROP__IG_ANIMATION_IK__IK_MAX_ITERATION, default_values.max_iterations);
    tm_the_truth_api->set_float(tt, component_w, TM_TT_PROP__IG_ANIMATION_IK__IK_TOLERANCE, default_values.tolerance);
    tm_the_truth_api->commit(tt, component_w, TM_TT_NO_UNDO_SCOPE);
    tm_the_truth_api->set_default_object(tt, object_type, component);

    tm_the_truth_api->set_aspect(tt, object_type, TM_CI_EDITOR_UI, editor_aspect);
//    tm_the_truth_api->set_aspect(tt, object_type, TM_TT_ASPECT__PROPERTIES, properties_aspect);
//    tm_the_truth_api->set_aspect(tt, object_type, TM_TT_ASPECT__VALIDATE, properties__validate);
}

static bool component__load_asset(tm_component_manager_o *manager_, tm_entity_t e, void *data, const tm_the_truth_o *tt, tm_tt_id_t asset)
{
    tm_animation_ik_component_manager_t *manager = (tm_animation_ik_component_manager_t *)manager_;
    tm_animation_ik_component_t *c = data;
    const tm_the_truth_object_o *asset_obj = tm_the_truth_api->read(tt, asset);

    (void)manager;

    c->max_iterations = tm_the_truth_api->get_uint32_t(tt, asset_obj, TM_TT_PROP__IG_ANIMATION_IK__IK_MAX_ITERATION);
    c->tolerance = tm_the_truth_api->get_float(tt, asset_obj, TM_TT_PROP__IG_ANIMATION_IK__IK_TOLERANCE);

    return true;
}

static void destroy(tm_component_manager_o *manager)
{
    tm_animation_ik_component_manager_t *man = (tm_animation_ik_component_manager_t *)manager;

    // Make sure that ik is destroyed only once
    ik_retain_count--;
    if (ik_retain_count == 0) {
        ik.deinit();
    }

    tm_entity_context_o *ctx = man->ctx;
    tm_allocator_i a = man->allocator;
    tm_free(&a, man, sizeof(*man));
    tm_entity_api->destroy_child_allocator(ctx, &a);
}

static void component__create(struct tm_entity_context_o *ctx)
{
    tm_allocator_i a;
    tm_entity_api->create_child_allocator(ctx, TM_TT_TYPE__IG_ANIMATION_IK, &a);
    tm_animation_ik_component_manager_t *manager = tm_alloc(&a, sizeof(*manager));
    *manager = (tm_animation_ik_component_manager_t) {
        .ctx = ctx,
        .allocator = a
    };

    // Make sure that ik is initialized only once
    if (ik_retain_count == 0) {
        ik.init();
    }
    ik_retain_count++;

    tm_component_i component = {
        .name = TM_TT_TYPE__IG_ANIMATION_IK,
        .bytes = sizeof(tm_animation_ik_component_t),
        .load_asset = component__load_asset,
        .destroy = destroy,
        .manager = (tm_component_manager_o*)manager
    };

    tm_entity_api->register_component(ctx, &component);
}

static void apply_ik_nodes_to_scene(struct ik_node_t* node)
{
    if (node->user_data == NULL)
        return;

    tm_animation_ik_component_t* c = node->user_data;

    if (node->guid == 0) {
        tm_transform_t trans = tm_scene_tree_component_api->local_transform(c->stc, c->upLeg_L_node);

        ik_quat_t rot = node->rotation;
        trans.rot.x = rot.x;
        trans.rot.y = rot.y;
        trans.rot.z = rot.z;
        trans.rot.w = rot.w;

        tm_scene_tree_component_api->set_local_transform(c->stc, c->upLeg_L_node, &trans);
    } else if (node->guid == 1) {
        tm_transform_t trans = tm_scene_tree_component_api->local_transform(c->stc, c->leg_L_node);

        ik_quat_t rot = node->rotation;
        trans.rot.x = rot.x;
        trans.rot.y = rot.y;
        trans.rot.z = rot.z;
        trans.rot.w = rot.w;

        tm_scene_tree_component_api->set_local_transform(c->stc, c->leg_L_node, &trans);
    }
}

static void engine_update__bind(tm_engine_o *inst, tm_engine_update_set_t *data)
{
    struct tm_entity_context_o *ctx = (struct tm_entity_context_o *)inst;

    (void)ctx;

    float dt = 1.0f / 60.0f;
    for (const tm_entity_blackboard_value_t *bb = data->blackboard_start; bb != data->blackboard_end; ++bb) {
        if (bb->id == TM_ENTITY_BB__DELTA_TIME)
            dt = (float)bb->double_value;
    }
    
    if (tm_tag_component_manager == NULL)
        return;

    tm_entity_t root = tm_tag_component_api->find_first(tm_tag_component_manager, ANIMATION_ROOT_TAG_HASH);
    tm_scene_tree_component_t* root_stc = tm_entity_api->get_component(ctx, root, tm_entity_api->lookup_component(ctx, TM_TT_TYPE_HASH__SCENE_TREE_COMPONENT));

    if (root.u64 == 0 || root_stc == NULL) {
        return;
    }

    for (tm_engine_update_array_t* update_array = data->arrays; update_array < data->arrays + data->num_arrays; ++update_array) {
        tm_animation_ik_component_t* components = update_array->components[0];

        for (uint32_t i = 0; i < update_array->n; ++i) {
            tm_animation_ik_component_t *c = &components[i];
            tm_entity_t target_entity = update_array->entities[i];
            tm_scene_tree_component_t* stc = tm_entity_api->get_component(ctx, target_entity, tm_entity_api->lookup_component(ctx, TM_TT_TYPE_HASH__SCENE_TREE_COMPONENT));

            c->stc = stc;

            uint32_t upLeg_L_node = tm_scene_tree_component_api->node_index_from_name(root_stc, LEFT_UP_LEG, NODE_NOT_FOUND);
            uint32_t leg_L_node   = tm_scene_tree_component_api->node_index_from_name(root_stc, LEFT_LEG,    NODE_NOT_FOUND);
            uint32_t foot_L_node  = tm_scene_tree_component_api->node_index_from_name(root_stc, LEFT_FOOT,   NODE_NOT_FOUND);
            if (foot_L_node != NODE_NOT_FOUND && leg_L_node != NODE_NOT_FOUND && foot_L_node != NODE_NOT_FOUND) {
                tm_transform_t upLeg_trns = tm_scene_tree_component_api->local_transform(root_stc, upLeg_L_node);
                tm_transform_t leg_trns = tm_scene_tree_component_api->local_transform(root_stc, leg_L_node);
                tm_transform_t foot_trns = tm_scene_tree_component_api->local_transform(root_stc, foot_L_node);

                tm_vec3_t bone1 = tm_vec3_sub(upLeg_trns.pos, leg_trns.pos);
                tm_vec3_t bone2 = tm_vec3_sub(leg_trns.pos, foot_trns.pos);
                tm_vec3_t bone3 = tm_vec3_sub(upLeg_trns.pos, foot_trns.pos);

                ik_solver_t* solver = ik.solver.create(IK_FABRIK);
                solver->max_iterations = c->max_iterations;
                solver->tolerance = c->tolerance;
                solver->flags |= IK_ENABLE_CONSTRAINTS | IK_ENABLE_TARGET_ROTATIONS | IK_ENABLE_JOINT_ROTATIONS;

                c->upLeg_L = solver->node->create(0);
                c->leg_L = solver->node->create_child(c->upLeg_L, 1);
                c->foot_L = solver->node->create_child(c->leg_L, 2);

                ik_effector_t* effector = solver->effector->create();
                solver->effector->attach(effector, c->foot_L);

                effector->chain_length = 2;
                effector->flags |= IK_WEIGHT_NLERP;

                c->upLeg_L->position = ik.vec3.vec3(0, 0, 0);
                c->leg_L->position = ik.vec3.vec3(bone1.x, bone1.y, bone1.z);
                c->foot_L->position = ik.vec3.vec3(bone2.x, bone2.y, bone2.z);

                c->upLeg_L_node = upLeg_L_node;
                c->leg_L_node = leg_L_node;
                c->foot_L_node = foot_L_node;

                c->upLeg_L->user_data = c;
                c->leg_L->user_data = c;
                c->foot_L->user_data = c;

                effector->target_position = ik.vec3.vec3(bone3.x, bone3.y+2, bone3.z-2);

                ik.solver.update_distances(solver);

                ik.solver.set_tree(solver, c->upLeg_L);
                ik.solver.rebuild(solver);
                ik.solver.solve(solver);
                ik.solver.iterate_affected_nodes(solver, apply_ik_nodes_to_scene);
            }


            /*
            c->upLeg_R->position = ik.vec3.vec3(0, 0, 0);



            c->leg_L->position = ik.vec3.vec3(0, 2, 0);
            c->leg_R->position = ik.vec3.vec3(0, 2, 0);

            c->foot_L->position = ik.vec3.vec3(0, 2, 0);
            c->foot_R->position = ik.vec3.vec3(0, 2, 0);

            ik_effector_t* effector = solver->effector->create();
            solver->effector->attach(effector, c->foot_L);

            effector->chain_length = 2;
            effector->target_position = ik.vec3.vec3(4, 0, 0); // globa space
            effector->flags |= IK_WEIGHT_NLERP;
            ik.solver.update_distances(solver);

            ik.solver.set_tree(solver, c->upLeg_L);
            ik.solver.rebuild(solver);
            ik.solver.solve(solver);
            ik.solver.iterate_affected_nodes(solver, apply_ik_nodes_to_scene);
                        */

        }
    }
}

static bool engine_filter__bind(tm_engine_o *inst, const uint32_t *components, uint32_t num_components, const tm_component_mask_t *mask)
{
    return tm_entity_mask_has_component(mask, components[0]) && (tm_entity_mask_has_component(mask, components[1]) || tm_entity_mask_has_component(mask, components[2]));
}

static void component__register_engine(struct tm_entity_context_o *ctx)
{
    const uint32_t anim_component = tm_entity_api->lookup_component(ctx, TM_TT_TYPE_HASH__IG_ANIMATION_IK);
    const uint32_t scene_tree_component = tm_entity_api->lookup_component(ctx, TM_TT_TYPE_HASH__SCENE_TREE_COMPONENT);
    const uint32_t tag_component = tm_entity_api->lookup_component(ctx, TM_TT_TYPE_HASH__TAG_COMPONENT);

    tm_tag_component_manager = (tm_tag_component_manager_o*)tm_entity_api->component_manager(ctx, tag_component);

    const tm_engine_i animation_ik_engine = {
        .name = TM_LOCALIZE_LATER("Inverse Kinematics"),
        .num_components = 3,
        .components = { anim_component, scene_tree_component, tag_component },
        .writes = { false, true, true },
        .update = engine_update__bind,
        .filter = engine_filter__bind,
        .inst = (tm_engine_o *)ctx,
    };
    tm_entity_api->register_engine(ctx, &animation_ik_engine);
}

void tm_ig_animation_inverse_kinematics_load_plugin(struct tm_api_registry_api* reg, bool load)
{
    tm_entity_api = reg->get(TM_ENTITY_API_NAME);
    tm_the_truth_api = reg->get(TM_THE_TRUTH_API_NAME);
    tm_the_truth_common_types_api = reg->get(TM_THE_TRUTH_COMMON_TYPES_API_NAME);
    tm_temp_allocator_api = reg->get(TM_TEMP_ALLOCATOR_API_NAME);
    tm_localizer_api = reg->get(TM_LOCALIZER_API_NAME);
    tm_logger_api = reg->get(TM_LOGGER_API_NAME);
    tm_scene_tree_component_api = reg->get(TM_SCENE_TREE_COMPONENT_API_NAME);
    tm_tag_component_api = reg->get(TM_TAG_COMPONENT_API_NAME);

    tm_add_or_remove_implementation(reg, load, TM_THE_TRUTH_CREATE_TYPES_INTERFACE_NAME, component__create_types);
    tm_add_or_remove_implementation(reg, load, TM_ENTITY_CREATE_COMPONENT_INTERFACE_NAME, component__create);
    tm_add_or_remove_implementation(reg, load, TM_ENTITY_SIMULATION_INTERFACE_NAME, component__register_engine);
}
