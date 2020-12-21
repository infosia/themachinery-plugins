#include "animation_puppet.h"

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

// Tha bone count that puppet is interested in
#define BONE_COUNT 65

// default bone mappings
#include "mapping/bones-1.h"
#include "mapping/bones-2.h"

static struct tm_entity_api *tm_entity_api;
static struct tm_temp_allocator_api *tm_temp_allocator_api;
static struct tm_the_truth_api *tm_the_truth_api;
static struct tm_the_truth_common_types_api *tm_the_truth_common_types_api;
static struct tm_localizer_api *tm_localizer_api;
static struct tm_logger_api *tm_logger_api;
static struct tm_scene_tree_component_api *tm_scene_tree_component_api;
static struct tm_tag_component_api *tm_tag_component_api;
static struct tm_tag_component_manager_o *tm_tag_component_manager;
static  struct tm_link_component_api *tm_link_component_api;

#define NODE_NOT_FOUND UINT32_MAX
#define ANIMATION_ROOT_TAG_HASH TM_STATIC_HASH("tm_ig_animation_puppet_root",   0xc88a0edba8d16200ULL)

typedef struct tm_animation_puppet_component_t
{
    tm_vec3_t transform_factor;
    tm_vec4_t rotation_factor;
    tm_vec3_t scale_factor;
} tm_animation_puppet_component_t;

typedef struct tm_animation_puppet_component_manager_t
{
    tm_entity_context_o *ctx;
    tm_allocator_i allocator;
} tm_animation_puppet_component_manager_t;

static tm_animation_puppet_component_t default_values = {
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
    tm_the_truth_property_definition_t animation_puppet_component_properties[] = {
        { "transform_factor", TM_THE_TRUTH_PROPERTY_TYPE_SUBOBJECT, .type_hash = TM_TT_TYPE_HASH__POSITION },
        { "rotation_factor", TM_THE_TRUTH_PROPERTY_TYPE_SUBOBJECT, .type_hash = TM_TT_TYPE_HASH__ROTATION },
        { "scale_factor",    TM_THE_TRUTH_PROPERTY_TYPE_SUBOBJECT, .type_hash = TM_TT_TYPE_HASH__SCALE },
    };

    const uint64_t object_type = tm_the_truth_api->create_object_type(tt, TM_TT_TYPE__IG_ANIMATION_PUPPET, animation_puppet_component_properties, TM_ARRAY_COUNT(animation_puppet_component_properties));

    const tm_tt_id_t component = tm_the_truth_api->create_object_of_type(tt, tm_the_truth_api->object_type_from_name_hash(tt, TM_TT_TYPE_HASH__IG_ANIMATION_PUPPET), TM_TT_NO_UNDO_SCOPE);
    tm_the_truth_object_o *component_w = tm_the_truth_api->write(tt, component);

    tm_tt_id_t position_id = tm_the_truth_api->create_object_of_type(tt, tm_the_truth_api->object_type_from_name_hash(tt, TM_TT_TYPE_HASH__VEC3), TM_TT_NO_UNDO_SCOPE);
    tm_the_truth_object_o * pos_w = tm_the_truth_api->write(tt, position_id);
    set_vec3(tt, pos_w, &default_values.transform_factor);
    tm_the_truth_api->set_subobject(tt, component_w, TM_TT_PROP__IG_ANIMATION_PUPPET__POSITION_FACTOR, pos_w);
    tm_the_truth_api->commit(tt, pos_w, TM_TT_NO_UNDO_SCOPE);

    tm_tt_id_t rotation_id = tm_the_truth_api->create_object_of_type(tt, tm_the_truth_api->object_type_from_name_hash(tt, TM_TT_TYPE_HASH__VEC4), TM_TT_NO_UNDO_SCOPE);
    tm_the_truth_object_o *rotation_w = tm_the_truth_api->write(tt, rotation_id);
    set_vec4(tt, rotation_w, &default_values.rotation_factor);
    tm_the_truth_api->set_subobject(tt, component_w, TM_TT_PROP__IG_ANIMATION_PUPPET__ROTATION_FACTOR, rotation_w);
    tm_the_truth_api->commit(tt, rotation_w, TM_TT_NO_UNDO_SCOPE);

    tm_tt_id_t scale_id = tm_the_truth_api->create_object_of_type(tt, tm_the_truth_api->object_type_from_name_hash(tt, TM_TT_TYPE_HASH__VEC3), TM_TT_NO_UNDO_SCOPE);
    tm_the_truth_object_o *scale_w = tm_the_truth_api->write(tt, scale_id);
    set_vec3(tt, scale_w, &default_values.scale_factor);
    tm_the_truth_api->set_subobject(tt, component_w, TM_TT_PROP__IG_ANIMATION_PUPPET__SCALE_FACTOR, scale_w);
    tm_the_truth_api->commit(tt, scale_w, TM_TT_NO_UNDO_SCOPE);

    tm_the_truth_api->commit(tt, component_w, TM_TT_NO_UNDO_SCOPE);
    tm_the_truth_api->set_default_object(tt, object_type, component);

    tm_the_truth_api->set_aspect(tt, object_type, TM_CI_EDITOR_UI, editor_aspect);
//    tm_the_truth_api->set_aspect(tt, object_type, TM_TT_ASPECT__PROPERTIES, properties_aspect);
//    tm_the_truth_api->set_aspect(tt, object_type, TM_TT_ASPECT__VALIDATE, properties__validate);
}

static bool component__load_asset(tm_component_manager_o *manager_, tm_entity_t e, void *data, const tm_the_truth_o *tt, tm_tt_id_t asset)
{
    tm_animation_puppet_component_manager_t *manager = (tm_animation_puppet_component_manager_t *)manager_;
    tm_animation_puppet_component_t *c = data;
    const tm_the_truth_object_o *asset_obj = tm_the_truth_api->read(tt, asset);

    (void)manager;

    tm_tt_id_t position_id = tm_the_truth_api->get_subobject(tt, asset_obj, TM_TT_PROP__IG_ANIMATION_PUPPET__POSITION_FACTOR);
    read_vec3(tt, tm_tt_read(tt, position_id), &c->transform_factor);

    tm_tt_id_t rotation_id = tm_the_truth_api->get_subobject(tt, asset_obj, TM_TT_PROP__IG_ANIMATION_PUPPET__ROTATION_FACTOR);
    read_vec4(tt, tm_tt_read(tt, rotation_id), &c->rotation_factor);

    tm_tt_id_t scale_id = tm_the_truth_api->get_subobject(tt, asset_obj, TM_TT_PROP__IG_ANIMATION_PUPPET__SCALE_FACTOR);
    read_vec3(tt, tm_tt_read(tt, scale_id), &c->scale_factor);

    return true;
}

static void destroy(tm_component_manager_o *manager)
{
    tm_animation_puppet_component_manager_t *man = (tm_animation_puppet_component_manager_t *)manager;

    tm_entity_context_o *ctx = man->ctx;
    tm_allocator_i a = man->allocator;
    tm_free(&a, man, sizeof(*man));
    tm_entity_api->destroy_child_allocator(ctx, &a);
}

static void component__create(struct tm_entity_context_o *ctx)
{
    tm_allocator_i a;
    tm_entity_api->create_child_allocator(ctx, TM_TT_TYPE__IG_ANIMATION_PUPPET, &a);
    tm_animation_puppet_component_manager_t *manager = tm_alloc(&a, sizeof(*manager));
    *manager = (tm_animation_puppet_component_manager_t){
        .ctx = ctx,
        .allocator = a
    };

    tm_component_i component = {
        .name = TM_TT_TYPE__IG_ANIMATION_PUPPET,
        .bytes = sizeof(tm_animation_puppet_component_t),
        .load_asset = component__load_asset,
        .destroy = destroy,
        .manager = (tm_component_manager_o *)manager
    };

    tm_entity_api->register_component(ctx, &component);
}

static void engine_update__bind(tm_engine_o *inst, tm_engine_update_set_t *data)
{
    struct tm_entity_context_o *ctx = (struct tm_entity_context_o *)inst;

    float dt = 1.0f / 60.0f;
    for (const tm_entity_blackboard_value_t *bb = data->blackboard_start; bb != data->blackboard_end; ++bb) {
        if (bb->id == TM_ENTITY_BB__DELTA_TIME)
            dt = (float)bb->double_value;
    }

    tm_entity_t root = tm_tag_component_api->find_first(tm_tag_component_manager, ANIMATION_ROOT_TAG_HASH);
    tm_scene_tree_component_t *root_stc = tm_entity_api->get_component(ctx, root, tm_entity_api->lookup_component(ctx, TM_TT_TYPE_HASH__SCENE_TREE_COMPONENT));

    if (root.u64 == 0 || root_stc == NULL) {
        return;
    }

    for (tm_engine_update_array_t *update_array = data->arrays; update_array < data->arrays + data->num_arrays; ++update_array) {
        tm_animation_puppet_component_t *bind = update_array->components[0];

        for (uint32_t i = 0; i < update_array->n; ++i) {
            tm_entity_t target_entity = update_array->entities[i];
            tm_scene_tree_component_t *stc = tm_entity_api->get_component(ctx, target_entity, tm_entity_api->lookup_component(ctx, TM_TT_TYPE_HASH__SCENE_TREE_COMPONENT));

            for (uint32_t bone_idx = 0; bone_idx < BONE_COUNT; bone_idx++) {
                uint64_t root_node_name = BONES_1[bone_idx];
                uint64_t target_node_name2 = BONES_2[bone_idx];

                uint32_t node_index = tm_scene_tree_component_api->node_index_from_name(root_stc, root_node_name, NODE_NOT_FOUND);
                if (node_index != NODE_NOT_FOUND) {
                    tm_transform_t transform = tm_scene_tree_component_api->local_transform(root_stc, node_index);

                    node_index = tm_scene_tree_component_api->node_index_from_name(stc, root_node_name, NODE_NOT_FOUND);
                    if (node_index == NODE_NOT_FOUND)
                        node_index = tm_scene_tree_component_api->node_index_from_name(stc, target_node_name2, NODE_NOT_FOUND);
                    if (node_index == NODE_NOT_FOUND)
                        continue;

                    tm_transform_t target_transform = tm_scene_tree_component_api->local_transform(stc, node_index);

                    // translate Hips bone only
                    if (bone_idx == 0) {
                        target_transform.pos = tm_vec3_element_mul(bind[i].transform_factor, transform.pos);
                    }

                    target_transform.rot = tm_vec4_element_mul(bind[i].rotation_factor, transform.rot);
                    target_transform.scl = tm_vec3_element_mul(bind[i].scale_factor, transform.scl);

                    tm_scene_tree_component_api->set_local_transform(stc, node_index, &target_transform);
                }
            }
        }
    }
}

static bool engine_filter__bind(tm_engine_o *inst, const uint32_t *components, uint32_t num_components, const tm_component_mask_t *mask)
{
    return tm_entity_mask_has_component(mask, components[0]) && (tm_entity_mask_has_component(mask, components[1]) || tm_entity_mask_has_component(mask, components[2]));
}

static void component__register_engine(struct tm_entity_context_o *ctx)
{
    const uint32_t anim_component = tm_entity_api->lookup_component(ctx, TM_TT_TYPE_HASH__IG_ANIMATION_PUPPET);
    const uint32_t scene_tree_component = tm_entity_api->lookup_component(ctx, TM_TT_TYPE_HASH__SCENE_TREE_COMPONENT);
    const uint32_t tag_component = tm_entity_api->lookup_component(ctx, TM_TT_TYPE_HASH__TAG_COMPONENT);

    tm_tag_component_manager = (tm_tag_component_manager_o *)tm_entity_api->component_manager(ctx, tag_component);

    const tm_engine_i animation_puppet_engine = {
        .name = TM_LOCALIZE_LATER("Puppet Animation"),
        .num_components = 3,
        .components = { anim_component, scene_tree_component, tag_component },
        .writes = { false, true, true },
        .update = engine_update__bind,
        .filter = engine_filter__bind,
        .inst = (tm_engine_o *)ctx,
    };
    tm_entity_api->register_engine(ctx, &animation_puppet_engine);
}

void tm_ig_animation_puppet_load_plugin(struct tm_api_registry_api *reg, bool load)
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
