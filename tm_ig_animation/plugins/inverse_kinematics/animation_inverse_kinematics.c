#include "animation_inverse_kinematics.h"

#include <foundation/api_registry.h>
#include <foundation/carray.inl>
#include <foundation/feature_flags.h>
#include <foundation/localizer.h>
#include <foundation/math.inl>
#include <foundation/the_truth.h>
#include <foundation/the_truth_types.h>
#include <foundation/log.h>

#include <plugins/entity/entity.h>
#include <plugins/entity/scene_tree_component.h>
#include <plugins/the_machinery_shared/component_interfaces/editor_ui_interface.h>
#include <plugins/entity/tag_component.h>

#include "simbody_component.h"

static struct tm_entity_api *tm_entity_api;
static struct tm_temp_allocator_api *tm_temp_allocator_api;
static struct tm_the_truth_api *tm_the_truth_api;
static struct tm_the_truth_common_types_api *tm_the_truth_common_types_api;
static struct tm_localizer_api *tm_localizer_api;
static struct tm_logger_api *tm_logger_api;
static struct tm_scene_tree_component_api *tm_scene_tree_component_api;
static struct tm_tag_component_api *tm_tag_component_api;
static struct tm_tag_component_manager_o *tm_tag_component_manager;
static struct tm_link_component_api *tm_link_component_api;

typedef struct tm_animation_ik_component_t
{
    tm_vec3_t transform_factor;
    tm_vec4_t rotation_factor;
    tm_vec3_t scale_factor;
} tm_animation_ik_component_t;

typedef struct tm_animation_ik_component_manager_t
{
    tm_entity_context_o *ctx;
    tm_allocator_i allocator;
} tm_animation_ik_component_manager_t;

static tm_animation_ik_component_t default_values = {
    .transform_factor = {1, 1, 1},
    .rotation_factor = {1, 1, 1, 1},
    .scale_factor = {1, 1, 1 },
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
        { "transform_factor", TM_THE_TRUTH_PROPERTY_TYPE_SUBOBJECT, .type_hash = TM_TT_TYPE_HASH__POSITION },
        { "rotation_factor", TM_THE_TRUTH_PROPERTY_TYPE_SUBOBJECT, .type_hash = TM_TT_TYPE_HASH__ROTATION },
        { "scale_factor",    TM_THE_TRUTH_PROPERTY_TYPE_SUBOBJECT, .type_hash = TM_TT_TYPE_HASH__SCALE },
    };

    const uint64_t object_type = tm_the_truth_api->create_object_type(tt, TM_TT_TYPE__IG_ANIMATION_INVERSE_KINEMATICS, animation_ik_component_properties, TM_ARRAY_COUNT(animation_ik_component_properties));

    const tm_tt_id_t component = tm_the_truth_api->create_object_of_type(tt, tm_the_truth_api->object_type_from_name_hash(tt, TM_TT_TYPE_HASH__IG_ANIMATION_INVERSE_KINEMATICS), TM_TT_NO_UNDO_SCOPE);
    tm_the_truth_object_o *component_w = tm_the_truth_api->write(tt, component);

    tm_tt_id_t position_id = tm_the_truth_api->create_object_of_type(tt, tm_the_truth_api->object_type_from_name_hash(tt, TM_TT_TYPE_HASH__VEC3), TM_TT_NO_UNDO_SCOPE);
    tm_the_truth_object_o * pos_w = tm_the_truth_api->write(tt, position_id);
    set_vec3(tt, pos_w, &default_values.transform_factor);
    tm_the_truth_api->set_subobject(tt, component_w, TM_TT_PROP__IG_ANIMATION_IK__POSITION_FACTOR, pos_w);
    tm_the_truth_api->commit(tt, pos_w, TM_TT_NO_UNDO_SCOPE);

    tm_tt_id_t rotation_id = tm_the_truth_api->create_object_of_type(tt, tm_the_truth_api->object_type_from_name_hash(tt, TM_TT_TYPE_HASH__VEC4), TM_TT_NO_UNDO_SCOPE);
    tm_the_truth_object_o *rotation_w = tm_the_truth_api->write(tt, rotation_id);
    set_vec4(tt, rotation_w, &default_values.rotation_factor);
    tm_the_truth_api->set_subobject(tt, component_w, TM_TT_PROP__IG_ANIMATION_IK__ROTATION_FACTOR, rotation_w);
    tm_the_truth_api->commit(tt, rotation_w, TM_TT_NO_UNDO_SCOPE);

    tm_tt_id_t scale_id = tm_the_truth_api->create_object_of_type(tt, tm_the_truth_api->object_type_from_name_hash(tt, TM_TT_TYPE_HASH__VEC3), TM_TT_NO_UNDO_SCOPE);
    tm_the_truth_object_o *scale_w = tm_the_truth_api->write(tt, scale_id);
    set_vec3(tt, scale_w, &default_values.scale_factor);
    tm_the_truth_api->set_subobject(tt, component_w, TM_TT_PROP__IG_ANIMATION_IK__SCALE_FACTOR, scale_w);
    tm_the_truth_api->commit(tt, scale_w, TM_TT_NO_UNDO_SCOPE);

    tm_the_truth_api->commit(tt, component_w, TM_TT_NO_UNDO_SCOPE);
    tm_the_truth_api->set_default_object(tt, object_type, component);

    tm_the_truth_api->set_aspect(tt, object_type, TM_CI_EDITOR_UI, editor_aspect);
//    tm_the_truth_api->set_aspect(tt, object_type, TM_TT_ASPECT__PROPERTIES, properties_aspect);
//    tm_the_truth_api->set_aspect(tt, object_type, TM_TT_ASPECT__VALIDATE, properties__validate);
}

static bool component__load_asset(tm_component_manager_o* manager, tm_entity_t e, void* data, const tm_the_truth_o* tt, tm_tt_id_t asset)
{
    tm_animation_ik_component_manager_t *man = (tm_animation_ik_component_manager_t *)manager;
    tm_animation_ik_component_t *c = data;

    (void)man;

    const tm_the_truth_object_o *asset_obj = tm_the_truth_api->read(tt, asset);

    tm_tt_id_t position_id = tm_the_truth_api->get_subobject(tt, asset_obj, TM_TT_PROP__IG_ANIMATION_IK__POSITION_FACTOR);
    read_vec3(tt, tm_tt_read(tt, position_id), &c->transform_factor);

    tm_tt_id_t rotation_id = tm_the_truth_api->get_subobject(tt, asset_obj, TM_TT_PROP__IG_ANIMATION_IK__ROTATION_FACTOR);
    read_vec4(tt, tm_tt_read(tt, rotation_id), &c->rotation_factor);

    tm_tt_id_t scale_id = tm_the_truth_api->get_subobject(tt, asset_obj, TM_TT_PROP__IG_ANIMATION_IK__SCALE_FACTOR);
    read_vec3(tt, tm_tt_read(tt, scale_id), &c->scale_factor);

    return true;
}

static void destroy(tm_component_manager_o *manager)
{
    tm_animation_ik_component_manager_t *man = (tm_animation_ik_component_manager_t *)manager;

    simbody_component_destroy();

    tm_entity_context_o *ctx = man->ctx;
    tm_allocator_i a = man->allocator;
    tm_free(&a, man, sizeof(*man));
    tm_entity_api->destroy_child_allocator(ctx, &a);
}

static void component__create(struct tm_entity_context_o *ctx)
{
    tm_allocator_i a;
    tm_entity_api->create_child_allocator(ctx, TM_TT_TYPE__IG_ANIMATION_INVERSE_KINEMATICS, &a);
    tm_animation_ik_component_manager_t *manager = tm_alloc(&a, sizeof(*manager));
    *manager = (tm_animation_ik_component_manager_t){
        .ctx = ctx,
        .allocator = a
    };

    tm_component_i component = {
        .name = TM_TT_TYPE__IG_ANIMATION_INVERSE_KINEMATICS,
        .bytes = sizeof(tm_animation_ik_component_t),
        .load_asset = component__load_asset,
        .destroy = destroy,
        .manager = (tm_component_manager_o *)manager
    };

    tm_entity_api->register_component(ctx, &component);

    simbody_component_init();
}

static void engine_update__bind(tm_engine_o *inst, tm_engine_update_set_t *data)
{
    struct tm_entity_context_o *ctx = (struct tm_entity_context_o *)inst;

    float dt = 1.0f / 60.0f;
    for (const tm_entity_blackboard_value_t *bb = data->blackboard_start; bb != data->blackboard_end; ++bb) {
        if (bb->id == TM_ENTITY_BB__DELTA_TIME)
            dt = (float)bb->double_value;
    }

    (void)ctx;
}

static bool engine_filter__bind(tm_engine_o *inst, const uint32_t *components, uint32_t num_components, const tm_component_mask_t *mask)
{
    return tm_entity_mask_has_component(mask, components[0]) && (tm_entity_mask_has_component(mask, components[1]) || tm_entity_mask_has_component(mask, components[2]));
}

static void component__register_engine(struct tm_entity_context_o *ctx)
{
    const uint32_t anim_component = tm_entity_api->lookup_component(ctx, TM_TT_TYPE_HASH__IG_ANIMATION_INVERSE_KINEMATICS);
    const uint32_t scene_tree_component = tm_entity_api->lookup_component(ctx, TM_TT_TYPE_HASH__SCENE_TREE_COMPONENT);
    const uint32_t tag_component = tm_entity_api->lookup_component(ctx, TM_TT_TYPE_HASH__TAG_COMPONENT);

    tm_tag_component_manager = (tm_tag_component_manager_o *)tm_entity_api->component_manager(ctx, tag_component);

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
