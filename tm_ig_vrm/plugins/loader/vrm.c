#include "vrm.h"
#include "vrm_loader.h"

#include <foundation/api_registry.h>
#include <foundation/asset_io.h>
#include <foundation/buffer.h>
#include <foundation/buffer_format.h>
#include <foundation/carray_print.inl>
#include <foundation/hash.inl>
#include <foundation/log.h>
#include <foundation/math.h>
#include <foundation/math.inl>
#include <foundation/murmurhash64a.inl>
#include <foundation/os.h>
#include <foundation/path.h>
#include <foundation/profiler.h>
#include <foundation/progress_report.h>
#include <foundation/string.inl>
#include <foundation/task_system.h>
#include <foundation/the_truth.h>
#include <foundation/the_truth_assets.h>
#include <foundation/the_truth_types.h>
#include <plugins/dcc_asset/dcc_asset_component.h>
#include <plugins/dcc_asset/dcc_asset_truth.h>
#include <plugins/editor_views/asset_browser.h>
#include <plugins/entity/entity.h>

#include "mikktspace.h"

TM_DISABLE_PADDING_WARNINGS

#define CGLTF_IMPLEMENTATION
#define CGLTF_VRM_v0_0
#define CGLTF_VRM_v0_0_IMPLEMENTATION

#include <cgltf.h>

TM_RESTORE_PADDING_WARNINGS

#include <float.h>

#define VRM_CONVERT_COORD

#ifdef VRM_CONVERT_COORD
static void vrm_vec3_convert_coord(cgltf_float* data, cgltf_size count)
{
	for (cgltf_size i = 0; i < count; i++) {
		data[i] = -data[i];
		data[i + 2] = -data[i + 2];
		i += 2;
	}
}
#endif

extern const struct dcc_asset_type_info_t *dcc_asset_ti;

typedef struct import_vrm_task_t
{
	uint64_t bytes;
	struct tm_asset_io_import args;
	char filename[1]; // will allocate string data together with the rest of the struct.
} import_vrm_task_t;

typedef struct tm_bone_weight_t
{
	uint32_t bone_idx;
	float weight;
} tm_bone_weight_t;

typedef struct smikktspace_data_t
{
	cgltf_size  face_count;
	cgltf_float *normals;
	cgltf_float *vertices;
	cgltf_float *texcoord;
	uint8_t     *buffer;
} smikktspace_data_t;

typedef struct TM_HASH_T(uint64_t, tm_tt_id_t) name_to_id_t;

static cgltf_skin *vrm_get_skin_for_mesh(const cgltf_data *data, const cgltf_mesh *mesh)
{
	for (cgltf_size i = 0; i < data->nodes_count; ++i) {
		const cgltf_node *node = &data->nodes[i];
		if (node->skin != NULL && node->mesh != NULL && node->mesh->name != NULL && strcmp(node->mesh->name, mesh->name) == 0) {
			return node->skin;
		}
	}
	return NULL;
}

static void vrm_to_tm_vec4(const cgltf_float *in, tm_vec4_t *out)
{
	out->x = in[0];
	out->y = in[1];
	out->z = in[2];
	out->w = in[3];
}

static void vrm_to_tm_vec3(const cgltf_float *in, tm_vec3_t *out)
{
	out->x = in[0];
	out->y = in[1];
	out->z = in[2];
}

static void tm_set_float_array(tm_the_truth_o *tt, tm_the_truth_object_o *o, uint32_t prop, float *a, uint32_t n)
{
	for (uint32_t i = 0; i < n; ++i)
		tm_the_truth_api->set_float(tt, o, prop + i, a[i]);
}

// callbacks for mikktspace
static int tm_mikk_getNumFaces(const SMikkTSpaceContext *pContext)
{
	smikktspace_data_t *data = (smikktspace_data_t *)pContext->m_pUserData;
	return (int)data->face_count;
}

static int tm_mikk_getNumVerticesOfFace(const SMikkTSpaceContext *pContext, const int iFace)
{
	return 3;
}

static void tm_mikk_getPosition(const SMikkTSpaceContext *pContext, float* fvPosOut, const int iFace, const int iVert)
{
	smikktspace_data_t *data = (smikktspace_data_t *)pContext->m_pUserData;
	memcpy(fvPosOut, (data->vertices + (iFace * 3) ), 3 * sizeof(float));
}

static void tm_mikk_getNormal(const SMikkTSpaceContext *pContext, float* fvNormOut, const int iFace, const int iVert)
{
	smikktspace_data_t *data = (smikktspace_data_t *)pContext->m_pUserData;
	memcpy(fvNormOut, (data->normals + (iFace * 3)), 3 * sizeof(float));
}

static void tm_mikk_getTexCoord(const SMikkTSpaceContext *pContext, float* fvTexcOut, const int iFace, const int iVert)
{
	smikktspace_data_t *data = (smikktspace_data_t *)pContext->m_pUserData;
	memcpy(fvTexcOut, (data->texcoord + (iFace * 2)), 2 * sizeof(float));
}

static void tm_mikk_setTSpace(const SMikkTSpaceContext *pContext, const float fvTangent[], const float fvBiTangent[], const float fMagS, const float fMagT,
	const tbool bIsOrientationPreserving, const int iFace, const int iVert)
{
	smikktspace_data_t *data = (smikktspace_data_t *)pContext->m_pUserData;
	cgltf_float *buffer = (cgltf_float*)data->buffer;
	memcpy(buffer + (iFace * 4), fvTangent, 3 * sizeof(float));

	tm_vec3_t tangent = { fvTangent[0], fvTangent[1], fvTangent[2] };
	tm_vec3_t normal = { data->normals[iFace*3], data->normals[iFace * 3 + 1], data->normals[iFace * 3 + 2] };
	tm_vec3_t bitangent = { fvBiTangent[0], fvBiTangent[1], fvBiTangent[2] };

	cgltf_float bi = tm_vec3_dot(tm_vec3_cross(normal, tangent), bitangent) > 0.f ? 1.f : -1.f;
	memcpy(buffer + (iFace * 4) + 3, &bi, sizeof(float));
}

static inline uint32_t vrm_to_tm_primitive_type(uint32_t cgltf_primitive_type, struct tm_error_i *error)
{
	switch (cgltf_primitive_type) {
	case cgltf_primitive_type_points:
		return TM_TT_VALUE__DCC_ASSET_MESH__PRIMITIVE_TYPE__POINTS;
	case cgltf_primitive_type_lines:
		return TM_TT_VALUE__DCC_ASSET_MESH__PRIMITIVE_TYPE__LINES;
	case cgltf_primitive_type_triangles:
		return TM_TT_VALUE__DCC_ASSET_MESH__PRIMITIVE_TYPE__TRIANGLES;
	default:
		return TM_TT_VALUE__DCC_ASSET_MESH__PRIMITIVE_TYPE__POLYGON;
	}
	TM_ERROR(error, "Mixed or unknown mesh primitive type: %u", cgltf_primitive_type);
	return TM_TT_VALUE__DCC_ASSET_MESH__PRIMITIVE_TYPE__MIXED_OR_UNKNOWN;
}

#ifndef VRM_CONVERT_COORD
// Rotate root node by 180 degrees around the y axis to convert from VRM's to cgltf's coordinate system
static inline void vrm_normalize_axis(cgltf_node* node)
{
	node->has_rotation = true;
	tm_vec4_t lhs = (tm_vec4_t){ node->rotation[0], node->rotation[1], node->rotation[2], node->rotation[3] };
	tm_vec4_t res = tm_quaternion_mul(lhs, (tm_vec4_t) {0, 1, 0, 0});
	node->rotation[0] = res.x;
	node->rotation[1] = res.y;
	node->rotation[2] = res.z;
	node->rotation[3] = res.w;
}
#endif

static inline tm_vec3_t v3_min(tm_vec3_t a, float* b)
{
	return (tm_vec3_t)
	{
		.x = a.x < b[0] ? a.x : b[0],
			.y = a.y < b[1] ? a.y : b[1],
			.z = a.z < b[2] ? a.z : b[2],
	};
}

static inline tm_vec3_t v3_max(tm_vec3_t a, float* b)
{
	return (tm_vec3_t)
	{
		.x = a.x > b[0] ? a.x : b[0],
			.y = a.y > b[1] ? a.y : b[1],
			.z = a.z > b[2] ? a.z : b[2],
	};
}

static void import_node(struct tm_the_truth_o *tt, struct tm_the_truth_object_o *asset, struct tm_the_truth_object_o *scene, struct tm_the_truth_object_o *parent, const struct cgltf_node *node,
	name_to_id_t *node_by_name, struct tm_error_i *error)
{
	const tm_tt_id_t id = tm_the_truth_api->create_object_of_type(tt, dcc_asset_ti->node_type, TM_TT_NO_UNDO_SCOPE);
	tm_the_truth_object_o *tm_node = tm_the_truth_api->write(tt, id);

	const char *node_name = node->name == NULL ? "node.*" : node->name;

	tm_the_truth_api->set_string(tt, tm_node, TM_TT_PROP__DCC_ASSET_NODE__NAME, node_name);
	const uint64_t name_hash = tm_murmur_hash_string_inline(node_name);
	tm_hash_add(node_by_name, name_hash, id);

	tm_vec3_t p = { 0, 0, 0 };
	tm_vec4_t r = { 0, 0, 0, 1 };
	tm_vec3_t s = { 1, 1, 1 };

	if (node->has_translation) {
		vrm_to_tm_vec3(node->translation, &p);
#ifdef VRM_CONVERT_COORD
		p.x = -p.x;
		p.z = -p.z;
#endif
	}
	if (node->has_rotation) {
		vrm_to_tm_vec4(node->rotation, &r);
	}
	if (node->has_scale) {
		vrm_to_tm_vec3(node->scale, &s);
	}

	tm_tt_id_t pos_id = tm_the_truth_api->create_object_of_type(tt, dcc_asset_ti->position_type, TM_TT_NO_UNDO_SCOPE);
	tm_the_truth_object_o *pos_w = tm_the_truth_api->write(tt, pos_id);
	tm_set_float_array(tt, pos_w, TM_TT_PROP__DCC_ASSET_POSITION__X, &p.x, 3);
	tm_the_truth_api->set_subobject(tt, tm_node, TM_TT_PROP__DCC_ASSET_NODE__POSITION, pos_w);
	tm_the_truth_api->commit(tt, pos_w, TM_TT_NO_UNDO_SCOPE);

	tm_tt_id_t rot_id = tm_the_truth_api->create_object_of_type(tt, dcc_asset_ti->rotation_type, TM_TT_NO_UNDO_SCOPE);
	tm_the_truth_object_o *rot_w = tm_the_truth_api->write(tt, rot_id);
	tm_set_float_array(tt, rot_w, TM_TT_PROP__DCC_ASSET_ROTATION__X, &r.x, 4);
	tm_the_truth_api->set_subobject(tt, tm_node, TM_TT_PROP__DCC_ASSET_NODE__ROTATION, rot_w);
	tm_the_truth_api->commit(tt, rot_w, TM_TT_NO_UNDO_SCOPE);

	tm_tt_id_t scl_id = tm_the_truth_api->create_object_of_type(tt, dcc_asset_ti->scale_type, TM_TT_NO_UNDO_SCOPE);
	tm_the_truth_object_o *scl_w = tm_the_truth_api->write(tt, scl_id);
	tm_set_float_array(tt, scl_w, TM_TT_PROP__DCC_ASSET_SCALE__X, &s.x, 3);
	tm_the_truth_api->set_subobject(tt, tm_node, TM_TT_PROP__DCC_ASSET_NODE__SCALE, scl_w);
	tm_the_truth_api->commit(tt, scl_w, TM_TT_NO_UNDO_SCOPE);

	for (uint32_t c = 0; c != node->children_count; ++c)
		import_node(tt, asset, scene, tm_node, node->children[c], node_by_name, error);

	tm_the_truth_api->add_to_subobject_set(tt, asset, TM_TT_PROP__DCC_ASSET__NODES, &tm_node, 1);

	if (node->mesh != NULL) {
		TM_INIT_TEMP_ALLOCATOR(ta);
		const tm_tt_id_t *meshes = tm_the_truth_api->get_subobject_set(tt, asset, TM_TT_PROP__DCC_ASSET__MESHES, ta);
		const uint32_t num_meshes = (uint32_t)tm_carray_size(meshes);

		for (cgltf_size i = 0; i < node->mesh->primitives_count; i++) {
			const uint32_t mesh_idx = (uint32_t)(node->mesh->ext_0 + i);
			if (TM_ASSERT(mesh_idx < num_meshes, error, "Node mesh index out of bounds: %u Num meshes in scene: %u", mesh_idx, num_meshes)) {
				tm_the_truth_api->add_to_reference_set(tt, tm_node, TM_TT_PROP__DCC_ASSET_NODE__MESHES, &meshes[mesh_idx], 1);
			}
		}

		TM_SHUTDOWN_TEMP_ALLOCATOR(ta);
	}

	tm_the_truth_api->commit(tt, tm_node, TM_TT_NO_UNDO_SCOPE);

	if (parent)
		tm_the_truth_api->add_to_reference_set(tt, parent, TM_TT_PROP__DCC_ASSET_NODE__CHILDREN, &id, 1);
	else
		tm_the_truth_api->add_to_reference_set(tt, scene, TM_TT_PROP__DCC_ASSET_SCENE__ROOT_NODES, &id, 1);

}

static tm_tt_id_t extract_texture(struct tm_the_truth_o *tt, name_to_id_t *image_lookup, struct tm_the_truth_object_o *obj, 
	const struct cgltf_material *material, uint32_t type,
	struct tm_temp_allocator_i *ta, struct tm_error_i *error)
{
	const cgltf_texture *texture = NULL;

	if (type == TM_TT_PROP__DCC_ASSET_MATERIAL__NORMAL_TEXTURE && material->normal_texture.texture != NULL) {
		texture = material->normal_texture.texture;
	} else if (type == TM_TT_PROP__DCC_ASSET_MATERIAL__EMISSIVE_TEXTURE && material->emissive_texture.texture != NULL) {
		texture = material->emissive_texture.texture;
	} else if (type == TM_TT_PROP__DCC_ASSET_MATERIAL__BASE_COLOR_TEXTURE
		&& material->has_pbr_metallic_roughness && material->pbr_metallic_roughness.base_color_texture.texture != NULL) {
		texture = material->pbr_metallic_roughness.base_color_texture.texture;
	} else if (type == TM_TT_PROP__DCC_ASSET_MATERIAL_PBR_MR__METALLIC_ROUGHNESS_TEXTURE && material->pbr_metallic_roughness.metallic_roughness_texture.texture != NULL) {
		texture = material->pbr_metallic_roughness.metallic_roughness_texture.texture;
	} else {
		return (tm_tt_id_t) { 0 };
	}

	char *image_name = texture->image->name;

	if (image_name == NULL || strlen(image_name) == 0) {
		image_name = tm_temp_allocator_api->printf(ta, "*.%d", texture->image_index);
	}

	uint64_t path_hash = tm_murmur_hash_string_inline(image_name);
	tm_tt_id_t *image = tm_hash_add_reference(image_lookup, path_hash);
	if (!image->u64) {
		tm_buffers_i *buffers = tm_the_truth_api->buffers(tt);
		tm_tt_id_t tm_image_id = tm_the_truth_api->create_object_of_type(tt, dcc_asset_ti->image_type, TM_TT_NO_UNDO_SCOPE);
		tm_the_truth_object_o *tm_image = tm_the_truth_api->write(tt, tm_image_id);

		tm_the_truth_api->set_string(tt, tm_image, TM_TT_PROP__DCC_ASSET_IMAGE__NAME, image_name);

		const char *mime_type = texture->image->mime_type;
		if (strcmp(mime_type, "image\\/png") == 0) {
			tm_the_truth_api->set_uint32_t(tt, tm_image, TM_TT_PROP__DCC_ASSET_IMAGE__TYPE, TM_TT_VALUE__DCC_ASSET_IMAGE__TYPE__PNG);
		} else {
			tm_the_truth_api->set_uint32_t(tt, tm_image, TM_TT_PROP__DCC_ASSET_IMAGE__TYPE, TM_TT_VALUE__DCC_ASSET_IMAGE__TYPE__UNKNOWN);
		}

		const cgltf_buffer_view* buffer_view = texture->image->buffer_view;
		uint8_t *buffer_data = buffers->allocate(buffers->inst, buffer_view->size, 0);
		memcpy(buffer_data, (uint8_t*)(buffer_view->buffer->data) + buffer_view->offset, buffer_view->size);
		const uint32_t buffer_id = buffers->add(buffers->inst, buffer_data, buffer_view->size, 0);

		const tm_tt_id_t buf_id = tm_the_truth_api->create_object_of_type(tt, dcc_asset_ti->buffer_type, TM_TT_NO_UNDO_SCOPE);
		tm_the_truth_object_o *buf_o = tm_the_truth_api->write(tt, buf_id);
		tm_the_truth_api->set_string(tt, buf_o, TM_TT_PROP__DCC_ASSET_BUFFER__NAME, tm_temp_allocator_api->printf(ta, "image.%s", image_name));
		tm_the_truth_api->set_buffer(tt, buf_o, TM_TT_PROP__DCC_ASSET_BUFFER__DATA, buffer_id);
		tm_the_truth_api->add_to_subobject_set(tt, obj, TM_TT_PROP__DCC_ASSET__BUFFERS, &buf_o, 1);
		tm_the_truth_api->commit(tt, buf_o, TM_TT_NO_UNDO_SCOPE);

		tm_the_truth_api->set_reference(tt, tm_image, TM_TT_PROP__DCC_ASSET_IMAGE__BUFFER, buf_id);

		tm_the_truth_api->add_to_subobject_set(tt, obj, TM_TT_PROP__DCC_ASSET__IMAGES, &tm_image, 1);
		tm_the_truth_api->commit(tt, tm_image, TM_TT_NO_UNDO_SCOPE);
		*image = tm_image_id;
	}

	tm_tt_id_t tm_texture = tm_the_truth_api->create_object_of_type(tt, dcc_asset_ti->texture_type, TM_TT_NO_UNDO_SCOPE);
	tm_the_truth_object_o *tm_texture_w = tm_the_truth_api->write(tt, tm_texture);

	tm_the_truth_api->set_reference(tt, tm_texture_w, TM_TT_PROP__DCC_ASSET_TEXTURE__IMAGE, *image);
	tm_the_truth_api->set_uint32_t(tt, tm_texture_w, TM_TT_PROP__DCC_ASSET_TEXTURE__MAPPING, TM_TT_VALUE__DCC_ASSET_TEXTURE__MAPPING__UV);
	tm_the_truth_api->set_uint32_t(tt, tm_texture_w, TM_TT_PROP__DCC_ASSET_TEXTURE__ADDRESS_MODE, TM_TT_VALUE__DCC_ASSET_TEXTURE__ADDRESS_MODE__WRAP);
	tm_the_truth_api->set_uint32_t(tt, tm_texture_w, TM_TT_PROP__DCC_ASSET_TEXTURE__UV_SET, 0);

	tm_the_truth_api->commit(tt, tm_texture_w, TM_TT_NO_UNDO_SCOPE);
	return tm_texture;
}

static bool import_into(struct tm_the_truth_o *tt, struct tm_the_truth_object_o *obj, const struct cgltf_data *data,
	const char *scene_name, const char *asset_path, struct tm_temp_allocator_i *ta, struct tm_error_i *error,
	uint64_t task_id)
{
	const uint64_t dcc_asset_scene_type = tm_the_truth_api->object_type_from_name_hash(tt, TM_TT_TYPE_HASH__DCC_ASSET_SCENE);
	const tm_tt_id_t scene_id = tm_the_truth_api->create_object_of_type(tt, dcc_asset_scene_type, TM_TT_NO_UNDO_SCOPE);
	tm_the_truth_object_o *scene_obj = tm_the_truth_api->write(tt, scene_id);

	tm_the_truth_api->set_string(tt, scene_obj, TM_TT_PROP__DCC_ASSET_SCENE__NAME, scene_name);

	tm_the_truth_api->add_to_subobject_set(tt, obj, TM_TT_PROP__DCC_ASSET__SCENES, &scene_obj, 1);
	tm_the_truth_api->set_reference(tt, obj, TM_TT_PROP__DCC_ASSET__SCENE, scene_id);

	tm_buffers_i *buffers = tm_the_truth_api->buffers(tt);
	TM_GET_TEMP_ALLOCATOR_ADAPTER(ta, a);

	name_to_id_t image_lookup = { .allocator = a };

	if (tm_task_system_api->is_task_canceled(task_id))
		return false;

	// Materials
	for (cgltf_size i = 0; i < data->materials_count; ++i) {
		tm_progress_report_api->set_task_progress(task_id, tm_temp_allocator_api->printf(ta, "%s - materials: %i / %i", scene_name, i, data->materials_count), (float)i / (float)data->materials_count);
		const struct cgltf_material *material = &data->materials[i];
		const tm_tt_id_t id = tm_the_truth_api->create_object_of_type(tt, dcc_asset_ti->material_type, TM_TT_NO_UNDO_SCOPE);
		tm_the_truth_object_o *tm_material = tm_the_truth_api->write(tt, id);

		// name
		if (material->name != NULL && strlen(material->name) > 0) {
			tm_the_truth_api->set_string(tt, tm_material, TM_TT_PROP__DCC_ASSET_MATERIAL__NAME, material->name);
		} else {
			tm_the_truth_api->set_string(tt, tm_material, TM_TT_PROP__DCC_ASSET_MATERIAL__NAME, tm_temp_allocator_api->printf(ta, "material_%u", i));
		}

		// double_sided
		tm_the_truth_api->set_bool(tt, tm_material, TM_TT_PROP__DCC_ASSET_MATERIAL__DOUBLE_SIDED, material->double_sided ? true : false);

		// alpha
		switch (material->alpha_mode) {
		case cgltf_alpha_mode_opaque:
			tm_the_truth_api->set_uint32_t(tt, tm_material, TM_TT_PROP__DCC_ASSET_MATERIAL__ALPHA_MODE, TM_TT_VALUE__DCC_ASSET_MATERIAL__ALPHA_MODE__OPAQUE);
			break;
		case cgltf_alpha_mode_mask:
			tm_the_truth_api->set_uint32_t(tt, tm_material, TM_TT_PROP__DCC_ASSET_MATERIAL__ALPHA_MODE, TM_TT_VALUE__DCC_ASSET_MATERIAL__ALPHA_MODE__MASK);
			break;
		case cgltf_alpha_mode_blend:
			tm_the_truth_api->set_uint32_t(tt, tm_material, TM_TT_PROP__DCC_ASSET_MATERIAL__ALPHA_MODE, TM_TT_VALUE__DCC_ASSET_MATERIAL__ALPHA_MODE__BLEND);
			break;
		}
		tm_the_truth_api->set_float(tt, tm_material, TM_TT_PROP__DCC_ASSET_MATERIAL__ALPHA_CUTOFF, material->alpha_cutoff);

		// roughness, metallic
		const float default_roughness = 0.5f;
		const float default_metallic = 0.f;
		tm_vec4_t base_color_factor = { 1.f, 1.f, 1.f, 1.f };

		tm_the_truth_object_o *tm_pbr_mr = tm_the_truth_api->write(tt, tm_the_truth_api->create_object_of_type(tt, dcc_asset_ti->pbr_mr_type, TM_TT_NO_UNDO_SCOPE));
		if (material->has_pbr_metallic_roughness) {
			// TODO: Uncomment following disables asset preview for some reason
			//tm_tt_id_t texture = extract_texture(tt, &image_lookup, obj, &material, TM_TT_PROP__DCC_ASSET_MATERIAL_PBR_MR__METALLIC_ROUGHNESS_TEXTURE, ta, error);
			//if (texture.u64) {
			//	tm_the_truth_api->set_subobject_id(tt, tm_pbr_mr, TM_TT_PROP__DCC_ASSET_MATERIAL_PBR_MR__METALLIC_ROUGHNESS_TEXTURE, texture, TM_TT_NO_UNDO_SCOPE);
			//}
			const cgltf_pbr_metallic_roughness metallic_roughness = material->pbr_metallic_roughness;
			tm_the_truth_api->set_float(tt, tm_pbr_mr, TM_TT_PROP__DCC_ASSET_MATERIAL_PBR_MR__ROUGHNESS_FACTOR, metallic_roughness.roughness_factor);
			tm_the_truth_api->set_float(tt, tm_pbr_mr, TM_TT_PROP__DCC_ASSET_MATERIAL_PBR_MR__METALLIC_FACTOR, metallic_roughness.metallic_factor);
			vrm_to_tm_vec4(metallic_roughness.base_color_factor, &base_color_factor);
		} else {
			tm_the_truth_api->set_float(tt, tm_pbr_mr, TM_TT_PROP__DCC_ASSET_MATERIAL_PBR_MR__ROUGHNESS_FACTOR, default_roughness);
			tm_the_truth_api->set_float(tt, tm_pbr_mr, TM_TT_PROP__DCC_ASSET_MATERIAL_PBR_MR__METALLIC_FACTOR, default_metallic);
		}

		tm_the_truth_api->set_subobject(tt, tm_material, TM_TT_PROP__DCC_ASSET_MATERIAL__PBR_METALLIC_ROUGHNESS, tm_pbr_mr);
		tm_the_truth_api->commit(tt, tm_pbr_mr, TM_TT_NO_UNDO_SCOPE);

		tm_the_truth_object_o *tm_color = tm_the_truth_api->write(tt, tm_the_truth_api->create_object_of_type(tt, dcc_asset_ti->color_rgba_type, TM_TT_NO_UNDO_SCOPE));
		tm_the_truth_api->set_float(tt, tm_color, TM_TT_PROP__DCC_ASSET_COLOR__R, base_color_factor.x);
		tm_the_truth_api->set_float(tt, tm_color, TM_TT_PROP__DCC_ASSET_COLOR__G, base_color_factor.y);
		tm_the_truth_api->set_float(tt, tm_color, TM_TT_PROP__DCC_ASSET_COLOR__B, base_color_factor.z);
		tm_the_truth_api->set_float(tt, tm_color, TM_TT_PROP__DCC_ASSET_COLOR__A, base_color_factor.w);
		tm_the_truth_api->set_subobject(tt, tm_material, TM_TT_PROP__DCC_ASSET_MATERIAL__BASE_COLOR_FACTOR, tm_color);
		tm_the_truth_api->commit(tt, tm_color, TM_TT_NO_UNDO_SCOPE);

		const uint32_t tm_texture_properties[] = { TM_TT_PROP__DCC_ASSET_MATERIAL__BASE_COLOR_TEXTURE, TM_TT_PROP__DCC_ASSET_MATERIAL__NORMAL_TEXTURE, TM_TT_PROP__DCC_ASSET_MATERIAL__EMISSIVE_TEXTURE };
		for (uint32_t t = 0; t != TM_ARRAY_COUNT(tm_texture_properties); ++t) {
			tm_tt_id_t texture = extract_texture(tt, &image_lookup, obj, material, tm_texture_properties[t], ta, error);
			if (texture.u64)
				tm_the_truth_api->set_subobject_id(tt, tm_material, tm_texture_properties[t], texture, TM_TT_NO_UNDO_SCOPE);
		}

		tm_the_truth_api->add_to_subobject_set(tt, obj, TM_TT_PROP__DCC_ASSET__MATERIALS, &tm_material, 1);
		tm_the_truth_api->commit(tt, tm_material, TM_TT_NO_UNDO_SCOPE);
	}

	const tm_tt_id_t *tm_materials = tm_the_truth_api->get_subobject_set(tt, obj, TM_TT_PROP__DCC_ASSET__MATERIALS, ta);
	uint32_t n_materials = (uint32_t)tm_carray_size(tm_materials);

	if (tm_task_system_api->is_task_canceled(task_id))
		return false;

	// Meshes
	size_t tt_total_mesh_count = 0;
	for (cgltf_size i = 0; i < data->meshes_count; ++i) {
		tm_progress_report_api->set_task_progress(task_id, tm_temp_allocator_api->printf(ta, "%s - meshes: %i / %i", scene_name, i, data->meshes_count), (float)i / (float)data->meshes_count);
		cgltf_mesh *mesh = &data->meshes[i];

		// Used to keep reference to tm_mesh
		mesh->ext_0 = tt_total_mesh_count;

		for (cgltf_size j = 0; j < mesh->primitives_count; ++j) {
			const tm_tt_id_t mesh_id = tm_the_truth_api->create_object_of_type(tt, dcc_asset_ti->mesh_type, TM_TT_NO_UNDO_SCOPE);
			tm_the_truth_object_o *tm_mesh = tm_the_truth_api->write(tt, mesh_id);

			tm_the_truth_api->set_string(tt, tm_mesh, TM_TT_PROP__DCC_ASSET_MESH__NAME, tm_temp_allocator_api->printf(ta, "%s.%d", mesh->name, j));

			const cgltf_primitive *primitive = &mesh->primitives[j];
			const uint32_t primitive_type = vrm_to_tm_primitive_type(primitive->type, error);
			tm_the_truth_api->set_uint32_t(tt, tm_mesh, TM_TT_PROP__DCC_ASSET_MESH__PRIMITIVE_TYPE, primitive_type);

			const uint32_t material_index = (uint32_t)primitive->material_index;
			if (material_index < n_materials)
				tm_the_truth_api->set_reference(tt, tm_mesh, TM_TT_PROP__DCC_ASSET_MESH__MATERIAL, tm_materials[primitive->material_index]);

			if (primitive_type != TM_TT_VALUE__DCC_ASSET_MESH__PRIMITIVE_TYPE__MIXED_OR_UNKNOWN) {
				const tm_tt_id_t idata_id = tm_the_truth_api->create_object_of_type(tt, dcc_asset_ti->buffer_type, TM_TT_NO_UNDO_SCOPE);
				tm_the_truth_object_o *idata = tm_the_truth_api->write(tt, idata_id);
				tm_the_truth_api->set_string(tt, idata, TM_TT_PROP__DCC_ASSET_BUFFER__NAME, tm_temp_allocator_api->printf(ta, "ibuf.%s", mesh->name));

				size_t ibuf_size = primitive->indices->count * sizeof(uint32_t);
				uint32_t *data_start = buffers->allocate(buffers->inst, ibuf_size, 0);
				uint32_t *indices_data = data_start;
				for (cgltf_size k = 0; k < primitive->indices->count; ++k) {
					indices_data[k] = (uint32_t)cgltf_accessor_read_index(primitive->indices, k);
				}
				const uint32_t ibuf_id = buffers->add(buffers->inst, data_start, ibuf_size, 0);

				tm_the_truth_api->set_buffer(tt, idata, TM_TT_PROP__DCC_ASSET_BUFFER__DATA, ibuf_id);
				tm_the_truth_api->add_to_subobject_set(tt, obj, TM_TT_PROP__DCC_ASSET__BUFFERS, &idata, 1);
				tm_the_truth_api->commit(tt, idata, TM_TT_NO_UNDO_SCOPE);

				const tm_tt_id_t access_id = tm_the_truth_api->create_object_of_type(tt, dcc_asset_ti->accessor_type, TM_TT_NO_UNDO_SCOPE);
				tm_the_truth_object_o *access = tm_the_truth_api->write(tt, access_id);
				tm_the_truth_api->set_uint32_t(tt, access, TM_TT_PROP__DCC_ASSET_ACCESSOR__OFFSET, 0);
				tm_the_truth_api->set_uint32_t(tt, access, TM_TT_PROP__DCC_ASSET_ACCESSOR__COUNT, (uint32_t)primitive->indices->count);
				tm_the_truth_api->set_bool(tt, access, TM_TT_PROP__DCC_ASSET_ACCESSOR__IS_FLOAT, false);
				tm_the_truth_api->set_bool(tt, access, TM_TT_PROP__DCC_ASSET_ACCESSOR__IS_SIGNED, false);
				tm_the_truth_api->set_bool(tt, access, TM_TT_PROP__DCC_ASSET_ACCESSOR__IS_NORMALIZED, false);
				tm_the_truth_api->set_uint32_t(tt, access, TM_TT_PROP__DCC_ASSET_ACCESSOR__BITS, 32);
				tm_the_truth_api->set_uint32_t(tt, access, TM_TT_PROP__DCC_ASSET_ACCESSOR__COMPONENT_COUNT, 1);
				tm_the_truth_api->set_reference(tt, access, TM_TT_PROP__DCC_ASSET_ACCESSOR__BUFFER, idata_id);
				tm_the_truth_api->add_to_subobject_set(tt, obj, TM_TT_PROP__DCC_ASSET__ACCESSORS, &access, 1);
				tm_the_truth_api->commit(tt, access, TM_TT_NO_UNDO_SCOPE);
				tm_the_truth_api->set_reference(tt, tm_mesh, TM_TT_PROP__DCC_ASSET_MESH__INDICES, access_id);
			}

			const tm_tt_id_t vdata_id = tm_the_truth_api->create_object_of_type(tt, dcc_asset_ti->buffer_type, TM_TT_NO_UNDO_SCOPE);
			uint32_t vbuf_size = 0;
			tm_bone_weight_t **skin_data = 0;
			uint32_t total_weights = 0;

			cgltf_accessor *acc_POSITION = NULL;
			cgltf_accessor *acc_NORMAL = NULL;
			cgltf_accessor *acc_TEXCOORD_0 = NULL;
			cgltf_accessor *acc_WEIGHTS_0 = NULL;
			cgltf_accessor *acc_JOINTS_0 = NULL;

			for (cgltf_size k = 0; k < primitive->attributes_count; ++k) {
				const cgltf_attribute *attr = &primitive->attributes[k];

				if (attr->type == cgltf_attribute_type_position) {
					acc_POSITION = attr->data;
				} else if (attr->type == cgltf_attribute_type_normal) {
					acc_NORMAL = attr->data;
				} else if (strcmp(attr->name, "TEXCOORD_0") == 0) {
					acc_TEXCOORD_0 = attr->data;
				} else if (strcmp(attr->name, "WEIGHTS_0") == 0) {
					acc_WEIGHTS_0 = attr->data;
				} else if (strcmp(attr->name, "JOINTS_0") == 0) {
					acc_JOINTS_0 = attr->data;
				}
			}

			uint32_t num_vertices = 0;

			if (acc_POSITION != NULL) {
				num_vertices = (uint32_t)acc_POSITION->count;
			}

			cgltf_skin *skin = vrm_get_skin_for_mesh(data, mesh);

			if (skin != NULL && acc_JOINTS_0 != NULL && acc_WEIGHTS_0 != NULL && num_vertices > 0) {
				tm_carray_temp_resize(skin_data, num_vertices, ta);
				memset(skin_data, 0, num_vertices * sizeof(void *));

				const cgltf_size unpack_count = acc_JOINTS_0->count * 4;

				uint8_t*  joints_data_char = NULL;
				uint16_t* joints_data_short = NULL;
				if (acc_JOINTS_0->component_type == cgltf_component_type_r_8u) {
                    tm_carray_temp_resize(joints_data_char, unpack_count, ta);
                    cgltf_buffer_view* buffer_view = acc_JOINTS_0->buffer_view;
                    memcpy(joints_data_char, (uint8_t*)buffer_view->buffer->data + buffer_view->offset, unpack_count * sizeof(uint8_t));
                } else {
                    tm_carray_temp_resize(joints_data_short, unpack_count, ta);
                    cgltf_buffer_view* buffer_view = acc_JOINTS_0->buffer_view;
                    memcpy(joints_data_short, (uint8_t*)buffer_view->buffer->data + buffer_view->offset, unpack_count * sizeof(uint16_t));                                
				}

				cgltf_float *weights_data = NULL;
				tm_carray_temp_resize(weights_data, unpack_count, ta);
				cgltf_accessor_unpack_floats(acc_WEIGHTS_0, weights_data, unpack_count);

				// collect joints that's used from this mesh
				bool *joints_used = NULL;
				tm_carray_temp_resize(joints_used, skin->joints_count, ta);
				memset(joints_used, 0, skin->joints_count * sizeof(bool));

				for (uint32_t v = 0; v < num_vertices; ++v) {
					const uint32_t v_begin = v * 4;
					for (int8_t idx = 0; idx < 4; ++idx) {
						const uint16_t joints_data_idx = joints_data_char != NULL ? joints_data_char[v_begin+idx] : joints_data_short[v_begin+idx];
						if (joints_data_idx < skin->joints_count) {
							joints_used[joints_data_idx] = true;
						}
					}
				}

				uint32_t *joints_index = NULL;
				tm_carray_temp_resize(joints_index, skin->joints_count, ta);
				memset(joints_index, 0, skin->joints_count * sizeof(uint32_t));

				uint32_t bone_idx = 0;
				for (cgltf_size b = 0; b < skin->joints_count; ++b) {
					if (joints_used[b]) {
						cgltf_node *joint = skin->joints[b];
						joints_index[b] = (uint32_t)bone_idx;
						const tm_tt_id_t bone_id = tm_the_truth_api->create_object_of_type(tt, dcc_asset_ti->bone_type, TM_TT_NO_UNDO_SCOPE);
						tm_the_truth_object_o *bone_w = tm_the_truth_api->write(tt, bone_id);

						tm_the_truth_api->set_uint32_t(tt, bone_w, TM_TT_PROP__DCC_ASSET_BONE__INDEX, bone_idx);
						tm_the_truth_api->set_string(tt, bone_w, TM_TT_PROP__DCC_ASSET_BONE__NODE_NAME, joint->name);

						cgltf_accessor *inverse_bind_matrices = skin->inverse_bind_matrices;
						if (inverse_bind_matrices->ext_0 == NULL) {
							cgltf_float *mbuf_data = NULL;
							const cgltf_size unpack_m_count = inverse_bind_matrices->count * 16; // 4x4 matrix
							tm_carray_temp_resize(mbuf_data, unpack_m_count, ta);
							cgltf_accessor_unpack_floats(inverse_bind_matrices, mbuf_data, unpack_m_count);
							inverse_bind_matrices->ext_0 = mbuf_data;
						}

						tm_vec3_t p = { 0, 0, 0 };
						tm_vec4_t r = { 0, 0, 0, 1 };
						tm_vec3_t s = { 1, 1, 1 };

						const cgltf_float *cgltf_m = (cgltf_float *)inverse_bind_matrices->ext_0;
						const cgltf_size m_start = b * 16;
						tm_mat44_t m = {
							cgltf_m[m_start],    cgltf_m[m_start + 1], cgltf_m[m_start + 2] , cgltf_m[m_start + 3],
							cgltf_m[m_start + 4],  cgltf_m[m_start + 5], cgltf_m[m_start + 6] , cgltf_m[m_start + 7],
							cgltf_m[m_start + 8],  cgltf_m[m_start + 9], cgltf_m[m_start + 10] , cgltf_m[m_start + 11],
							cgltf_m[m_start + 12], cgltf_m[m_start + 13], cgltf_m[m_start + 14] , cgltf_m[m_start + 15],
						};
						tm_math_api->mat44_to_translation_quaternion_scale(&p, &r, &s, &m);


#ifdef VRM_CONVERT_COORD
						p.x = -p.x;
						p.z = -p.z;
#endif
						tm_tt_id_t pos_id = tm_the_truth_api->create_object_of_type(tt, dcc_asset_ti->position_type, TM_TT_NO_UNDO_SCOPE);
						tm_the_truth_object_o *pos_w = tm_the_truth_api->write(tt, pos_id);
						tm_set_float_array(tt, pos_w, TM_TT_PROP__DCC_ASSET_POSITION__X, &p.x, 3);
						tm_the_truth_api->set_subobject(tt, bone_w, TM_TT_PROP__DCC_ASSET_BONE__INVERSE_BIND_POSITION, pos_w);
						tm_the_truth_api->commit(tt, pos_w, TM_TT_NO_UNDO_SCOPE);

						tm_tt_id_t rot_id = tm_the_truth_api->create_object_of_type(tt, dcc_asset_ti->rotation_type, TM_TT_NO_UNDO_SCOPE);
						tm_the_truth_object_o *rot_w = tm_the_truth_api->write(tt, rot_id);
						tm_set_float_array(tt, rot_w, TM_TT_PROP__DCC_ASSET_ROTATION__X, &r.x, 4);
						tm_the_truth_api->set_subobject(tt, bone_w, TM_TT_PROP__DCC_ASSET_BONE__INVERSE_BIND_ROTATION, rot_w);
						tm_the_truth_api->commit(tt, rot_w, TM_TT_NO_UNDO_SCOPE);

						tm_tt_id_t scl_id = tm_the_truth_api->create_object_of_type(tt, dcc_asset_ti->scale_type, TM_TT_NO_UNDO_SCOPE);
						tm_the_truth_object_o *scl_w = tm_the_truth_api->write(tt, scl_id);
						tm_set_float_array(tt, scl_w, TM_TT_PROP__DCC_ASSET_SCALE__X, &s.x, 3);
						tm_the_truth_api->set_subobject(tt, bone_w, TM_TT_PROP__DCC_ASSET_BONE__INVERSE_BIND_SCALE, scl_w);
						tm_the_truth_api->commit(tt, scl_w, TM_TT_NO_UNDO_SCOPE);

						tm_the_truth_api->add_to_subobject_set(tt, tm_mesh, TM_TT_PROP__DCC_ASSET_MESH__BONES, &bone_w, 1);
						tm_the_truth_api->commit(tt, bone_w, TM_TT_NO_UNDO_SCOPE);

						bone_idx++;
					}
				}

				total_weights = num_vertices * 4;

				for (uint32_t v = 0; v < num_vertices; ++v) {
					const uint32_t v_begin = v * 4;
					for (uint8_t idx = 0; idx < 4; idx++) {
						const uint16_t joints_data_idx = joints_data_char != NULL ? joints_data_char[v_begin + idx] : joints_data_short[v_begin + idx];
						if (joints_data_idx < skin->joints_count) {
							tm_carray_temp_push(skin_data[v], ((tm_bone_weight_t){.bone_idx = joints_index[joints_data_idx], .weight = weights_data[v_begin + idx] }), ta);
						} else {
							tm_carray_temp_push(skin_data[v], ((tm_bone_weight_t){.bone_idx = 0, .weight = 0.f }), ta);						
						}
					}
				}

				const uint32_t total_skin_data_size = (num_vertices * sizeof(uint32_t)) + (total_weights * sizeof(tm_bone_weight_t));
				if (total_skin_data_size < 64 * 1024 * 1024) {
					tm_the_truth_object_o *attr = tm_the_truth_api->write(tt, tm_the_truth_api->create_object_of_type(tt, dcc_asset_ti->attribute_type, TM_TT_NO_UNDO_SCOPE));
					tm_the_truth_api->set_uint32_t(tt, attr, TM_TT_PROP__DCC_ASSET_ATTRIBUTE__SEMANTIC, TM_TT_VALUE__DCC_ASSET_VERTEX__SEMANTIC__SKIN_DATA);
					tm_the_truth_api->set_uint32_t(tt, attr, TM_TT_PROP__DCC_ASSET_ATTRIBUTE__SET, 0);

					const tm_tt_id_t access_id = tm_the_truth_api->create_object_of_type(tt, dcc_asset_ti->accessor_type, TM_TT_NO_UNDO_SCOPE);
					tm_the_truth_object_o *access = tm_the_truth_api->write(tt, access_id);
					tm_the_truth_api->set_uint32_t(tt, access, TM_TT_PROP__DCC_ASSET_ACCESSOR__OFFSET, vbuf_size);
					tm_the_truth_api->set_uint32_t(tt, access, TM_TT_PROP__DCC_ASSET_ACCESSOR__COUNT, num_vertices);
					tm_the_truth_api->set_bool(tt, access, TM_TT_PROP__DCC_ASSET_ACCESSOR__IS_FLOAT, false);
					tm_the_truth_api->set_bool(tt, access, TM_TT_PROP__DCC_ASSET_ACCESSOR__IS_SIGNED, false);
					tm_the_truth_api->set_bool(tt, access, TM_TT_PROP__DCC_ASSET_ACCESSOR__IS_NORMALIZED, false);
					tm_the_truth_api->set_uint32_t(tt, access, TM_TT_PROP__DCC_ASSET_ACCESSOR__BITS, 32);
					tm_the_truth_api->set_uint32_t(tt, access, TM_TT_PROP__DCC_ASSET_ACCESSOR__COMPONENT_COUNT, 1);
					tm_the_truth_api->set_reference(tt, access, TM_TT_PROP__DCC_ASSET_ACCESSOR__BUFFER, vdata_id);
					tm_the_truth_api->set_reference(tt, attr, TM_TT_PROP__DCC_ASSET_ATTRIBUTE__ACCESSOR, access_id);
					tm_the_truth_api->add_to_subobject_set(tt, obj, TM_TT_PROP__DCC_ASSET__ACCESSORS, &access, 1);
					tm_the_truth_api->commit(tt, access, TM_TT_NO_UNDO_SCOPE);
					tm_the_truth_api->add_to_subobject_set(tt, tm_mesh, TM_TT_PROP__DCC_ASSET_MESH__ATTRIBUTES, &attr, 1);
					tm_the_truth_api->commit(tt, attr, TM_TT_NO_UNDO_SCOPE);

					vbuf_size += total_skin_data_size;
				} else {
					skin_data = 0;
					TM_ERROR(tm_error_api->def, "Skin data of mesh: %s exceeds 64MB, skipping!", mesh->name);
				}
			}

			if (acc_POSITION != NULL) {
				tm_the_truth_object_o *attr = tm_the_truth_api->write(tt, tm_the_truth_api->create_object_of_type(tt, dcc_asset_ti->attribute_type, TM_TT_NO_UNDO_SCOPE));
				tm_the_truth_api->set_uint32_t(tt, attr, TM_TT_PROP__DCC_ASSET_ATTRIBUTE__SEMANTIC, TM_TT_VALUE__DCC_ASSET_VERTEX__SEMANTIC__POSITION);
				tm_the_truth_api->set_uint32_t(tt, attr, TM_TT_PROP__DCC_ASSET_ATTRIBUTE__SET, 0);

				const tm_tt_id_t access_id = tm_the_truth_api->create_object_of_type(tt, dcc_asset_ti->accessor_type, TM_TT_NO_UNDO_SCOPE);
				tm_the_truth_object_o *access = tm_the_truth_api->write(tt, access_id);
				tm_the_truth_api->set_uint32_t(tt, access, TM_TT_PROP__DCC_ASSET_ACCESSOR__OFFSET, vbuf_size);
				tm_the_truth_api->set_uint32_t(tt, access, TM_TT_PROP__DCC_ASSET_ACCESSOR__COUNT, num_vertices);
				tm_the_truth_api->set_bool(tt, access, TM_TT_PROP__DCC_ASSET_ACCESSOR__IS_FLOAT, true);
				tm_the_truth_api->set_bool(tt, access, TM_TT_PROP__DCC_ASSET_ACCESSOR__IS_SIGNED, true);
				tm_the_truth_api->set_bool(tt, access, TM_TT_PROP__DCC_ASSET_ACCESSOR__IS_NORMALIZED, false);
				tm_the_truth_api->set_uint32_t(tt, access, TM_TT_PROP__DCC_ASSET_ACCESSOR__BITS, 32);
				tm_the_truth_api->set_uint32_t(tt, access, TM_TT_PROP__DCC_ASSET_ACCESSOR__COMPONENT_COUNT, 3);
				tm_the_truth_api->set_reference(tt, access, TM_TT_PROP__DCC_ASSET_ACCESSOR__BUFFER, vdata_id);
				tm_the_truth_api->set_reference(tt, attr, TM_TT_PROP__DCC_ASSET_ATTRIBUTE__ACCESSOR, access_id);
				tm_the_truth_api->add_to_subobject_set(tt, obj, TM_TT_PROP__DCC_ASSET__ACCESSORS, &access, 1);
				tm_the_truth_api->commit(tt, access, TM_TT_NO_UNDO_SCOPE);
				tm_the_truth_api->add_to_subobject_set(tt, tm_mesh, TM_TT_PROP__DCC_ASSET_MESH__ATTRIBUTES, &attr, 1);

				tm_the_truth_api->commit(tt, attr, TM_TT_NO_UNDO_SCOPE);
				vbuf_size += num_vertices * sizeof(float) * 3;
			}

			if (acc_NORMAL != NULL && num_vertices > 0) {
				tm_the_truth_object_o *attr = tm_the_truth_api->write(tt, tm_the_truth_api->create_object_of_type(tt, dcc_asset_ti->attribute_type, TM_TT_NO_UNDO_SCOPE));
				tm_the_truth_api->set_uint32_t(tt, attr, TM_TT_PROP__DCC_ASSET_ATTRIBUTE__SEMANTIC, TM_TT_VALUE__DCC_ASSET_VERTEX__SEMANTIC__NORMAL);
				tm_the_truth_api->set_uint32_t(tt, attr, TM_TT_PROP__DCC_ASSET_ATTRIBUTE__SET, 0);

				const tm_tt_id_t access_id = tm_the_truth_api->create_object_of_type(tt, dcc_asset_ti->accessor_type, TM_TT_NO_UNDO_SCOPE);
				tm_the_truth_object_o *access = tm_the_truth_api->write(tt, access_id);
				tm_the_truth_api->set_uint32_t(tt, access, TM_TT_PROP__DCC_ASSET_ACCESSOR__OFFSET, vbuf_size);
				tm_the_truth_api->set_uint32_t(tt, access, TM_TT_PROP__DCC_ASSET_ACCESSOR__COUNT, num_vertices);
				tm_the_truth_api->set_bool(tt, access, TM_TT_PROP__DCC_ASSET_ACCESSOR__IS_FLOAT, true);
				tm_the_truth_api->set_bool(tt, access, TM_TT_PROP__DCC_ASSET_ACCESSOR__IS_SIGNED, true);
				tm_the_truth_api->set_bool(tt, access, TM_TT_PROP__DCC_ASSET_ACCESSOR__IS_NORMALIZED, false);
				tm_the_truth_api->set_uint32_t(tt, access, TM_TT_PROP__DCC_ASSET_ACCESSOR__BITS, 32);
				tm_the_truth_api->set_uint32_t(tt, access, TM_TT_PROP__DCC_ASSET_ACCESSOR__COMPONENT_COUNT, 3);
				tm_the_truth_api->set_reference(tt, access, TM_TT_PROP__DCC_ASSET_ACCESSOR__BUFFER, vdata_id);
				tm_the_truth_api->set_reference(tt, attr, TM_TT_PROP__DCC_ASSET_ATTRIBUTE__ACCESSOR, access_id);
				tm_the_truth_api->add_to_subobject_set(tt, obj, TM_TT_PROP__DCC_ASSET__ACCESSORS, &access, 1);
				tm_the_truth_api->commit(tt, access, TM_TT_NO_UNDO_SCOPE);
				tm_the_truth_api->add_to_subobject_set(tt, tm_mesh, TM_TT_PROP__DCC_ASSET_MESH__ATTRIBUTES, &attr, 1);

				tm_the_truth_api->commit(tt, attr, TM_TT_NO_UNDO_SCOPE);
				vbuf_size += (uint32_t)(acc_NORMAL->count * sizeof(float) * 3);
			}

			if (acc_TEXCOORD_0 != NULL && num_vertices > 0) {
				tm_the_truth_object_o *attr = tm_the_truth_api->write(tt, tm_the_truth_api->create_object_of_type(tt, dcc_asset_ti->attribute_type, TM_TT_NO_UNDO_SCOPE));
				tm_the_truth_api->set_uint32_t(tt, attr, TM_TT_PROP__DCC_ASSET_ATTRIBUTE__SEMANTIC, TM_TT_VALUE__DCC_ASSET_VERTEX__SEMANTIC__TEXCOORD);
				tm_the_truth_api->set_uint32_t(tt, attr, TM_TT_PROP__DCC_ASSET_ATTRIBUTE__SET, 0);

				const tm_tt_id_t access_id = tm_the_truth_api->create_object_of_type(tt, dcc_asset_ti->accessor_type, TM_TT_NO_UNDO_SCOPE);
				tm_the_truth_object_o *access = tm_the_truth_api->write(tt, access_id);
				tm_the_truth_api->set_uint32_t(tt, access, TM_TT_PROP__DCC_ASSET_ACCESSOR__OFFSET, vbuf_size);
				tm_the_truth_api->set_uint32_t(tt, access, TM_TT_PROP__DCC_ASSET_ACCESSOR__COUNT, num_vertices);
				tm_the_truth_api->set_bool(tt, access, TM_TT_PROP__DCC_ASSET_ACCESSOR__IS_FLOAT, true);
				tm_the_truth_api->set_bool(tt, access, TM_TT_PROP__DCC_ASSET_ACCESSOR__IS_SIGNED, true);
				tm_the_truth_api->set_bool(tt, access, TM_TT_PROP__DCC_ASSET_ACCESSOR__IS_NORMALIZED, false);
				tm_the_truth_api->set_uint32_t(tt, access, TM_TT_PROP__DCC_ASSET_ACCESSOR__BITS, 32);
				tm_the_truth_api->set_uint32_t(tt, access, TM_TT_PROP__DCC_ASSET_ACCESSOR__COMPONENT_COUNT, 2);
				tm_the_truth_api->set_reference(tt, access, TM_TT_PROP__DCC_ASSET_ACCESSOR__BUFFER, vdata_id);
				tm_the_truth_api->set_reference(tt, attr, TM_TT_PROP__DCC_ASSET_ATTRIBUTE__ACCESSOR, access_id);
				tm_the_truth_api->add_to_subobject_set(tt, obj, TM_TT_PROP__DCC_ASSET__ACCESSORS, &access, 1);
				tm_the_truth_api->commit(tt, access, TM_TT_NO_UNDO_SCOPE);
				tm_the_truth_api->add_to_subobject_set(tt, tm_mesh, TM_TT_PROP__DCC_ASSET_MESH__ATTRIBUTES, &attr, 1);

				tm_the_truth_api->commit(tt, attr, TM_TT_NO_UNDO_SCOPE);
				vbuf_size += (uint32_t)(acc_TEXCOORD_0->count * sizeof(float) * 2);
			}

			// TANGENT
			if (acc_NORMAL != NULL && num_vertices > 0) {
				tm_the_truth_object_o *attr = tm_the_truth_api->write(tt, tm_the_truth_api->create_object_of_type(tt, dcc_asset_ti->attribute_type, TM_TT_NO_UNDO_SCOPE));
				tm_the_truth_api->set_uint32_t(tt, attr, TM_TT_PROP__DCC_ASSET_ATTRIBUTE__SEMANTIC, TM_TT_VALUE__DCC_ASSET_VERTEX__SEMANTIC__TANGENT);
				tm_the_truth_api->set_uint32_t(tt, attr, TM_TT_PROP__DCC_ASSET_ATTRIBUTE__SET, 0);

				const tm_tt_id_t access_id = tm_the_truth_api->create_object_of_type(tt, dcc_asset_ti->accessor_type, TM_TT_NO_UNDO_SCOPE);
				tm_the_truth_object_o *access = tm_the_truth_api->write(tt, access_id);
				tm_the_truth_api->set_uint32_t(tt, access, TM_TT_PROP__DCC_ASSET_ACCESSOR__OFFSET, vbuf_size);
				tm_the_truth_api->set_uint32_t(tt, access, TM_TT_PROP__DCC_ASSET_ACCESSOR__COUNT, num_vertices);
				tm_the_truth_api->set_bool(tt, access, TM_TT_PROP__DCC_ASSET_ACCESSOR__IS_FLOAT, true);
				tm_the_truth_api->set_bool(tt, access, TM_TT_PROP__DCC_ASSET_ACCESSOR__IS_SIGNED, true);
				tm_the_truth_api->set_bool(tt, access, TM_TT_PROP__DCC_ASSET_ACCESSOR__IS_NORMALIZED, false);
				tm_the_truth_api->set_uint32_t(tt, access, TM_TT_PROP__DCC_ASSET_ACCESSOR__BITS, 32);
				tm_the_truth_api->set_uint32_t(tt, access, TM_TT_PROP__DCC_ASSET_ACCESSOR__COMPONENT_COUNT, 4);
				tm_the_truth_api->set_reference(tt, access, TM_TT_PROP__DCC_ASSET_ACCESSOR__BUFFER, vdata_id);
				tm_the_truth_api->set_reference(tt, attr, TM_TT_PROP__DCC_ASSET_ATTRIBUTE__ACCESSOR, access_id);
				tm_the_truth_api->add_to_subobject_set(tt, obj, TM_TT_PROP__DCC_ASSET__ACCESSORS, &access, 1);
				tm_the_truth_api->commit(tt, access, TM_TT_NO_UNDO_SCOPE);
				tm_the_truth_api->add_to_subobject_set(tt, tm_mesh, TM_TT_PROP__DCC_ASSET_MESH__ATTRIBUTES, &attr, 1);

				tm_the_truth_api->commit(tt, attr, TM_TT_NO_UNDO_SCOPE);
				vbuf_size += num_vertices * sizeof(float) * 4;
			}

			tm_the_truth_object_o *vdata = tm_the_truth_api->write(tt, vdata_id);
			tm_the_truth_api->set_string(tt, vdata, TM_TT_PROP__DCC_ASSET_BUFFER__NAME, tm_temp_allocator_api->printf(ta, "vbuf.%s", mesh->name));
			uint8_t *vbuf_data = buffers->allocate(buffers->inst, vbuf_size, 0);
			const uint8_t *data_begins = vbuf_data;

			if (skin != NULL && acc_JOINTS_0 != NULL && acc_JOINTS_0->count > 0) {
				// Note: Currently this code assumes we can fit all skin weights for all vertices in less than 64MB
				uint32_t skin_offset = num_vertices * sizeof(uint32_t);
				for (uint32_t b = 0; b != num_vertices; ++b, vbuf_data += sizeof(uint32_t)) {
					const uint8_t n_bone_influences = (uint8_t)tm_carray_size(skin_data[b]);
					*(uint32_t *)vbuf_data = (((skin_offset / 4) & 0xffffff) << 8) | n_bone_influences;
					skin_offset += n_bone_influences * sizeof(tm_bone_weight_t);
				}

				for (uint32_t b = 0; b != num_vertices; ++b) {
					const uint8_t n_bone_influences = (uint8_t)tm_carray_size(skin_data[b]);
					// Normalize skin weights.
					float total_weight = 0.f;
					for (uint32_t bi = 0; bi != n_bone_influences; ++bi)
						total_weight += skin_data[b][bi].weight;
					if (total_weight > 0.f) {
						for (uint32_t bi = 0; bi != n_bone_influences; ++bi)
							skin_data[b][bi].weight /= total_weight;
					}
					uint32_t stored_size = n_bone_influences * sizeof(tm_bone_weight_t);
					memcpy(vbuf_data, skin_data[b], stored_size);
					vbuf_data += stored_size;
				}
			}

			cgltf_float *vertices_data = NULL;
			if (acc_POSITION != NULL && num_vertices > 0) {
				const cgltf_size unpack_count = num_vertices * 3;
				vertices_data = (cgltf_float *)vbuf_data;
				cgltf_accessor_unpack_floats(acc_POSITION, vertices_data, unpack_count);
#ifdef VRM_CONVERT_COORD
				vrm_vec3_convert_coord(vertices_data, unpack_count);
#endif

				// calc bounds
				tm_vec3_t bounds[2] = {
					{ FLT_MAX, FLT_MAX, FLT_MAX },
					{ -FLT_MAX, -FLT_MAX, -FLT_MAX }
				};

				for (cgltf_size p = 0; p != num_vertices; ++p) {
					float *v = (float *)vbuf_data + (p * 3);
					bounds[0] = v3_min(bounds[0], v);
					bounds[1] = v3_max(bounds[1], v);
				}

				tm_tt_id_t min_id = tm_the_truth_api->create_object_of_type(tt, tm_the_truth_api->object_type_from_name_hash(tt, TM_TT_TYPE_HASH__VEC3), TM_TT_NO_UNDO_SCOPE);
				tm_the_truth_object_o *min_w = tm_the_truth_api->write(tt, min_id);
				tm_set_float_array(tt, min_w, TM_TT_PROP__VEC3__X, &bounds[0].x, 3);
				tm_the_truth_api->set_subobject(tt, tm_mesh, TM_TT_PROP__DCC_ASSET_MESH__BOUNDS_MIN, min_w);
				tm_the_truth_api->commit(tt, min_w, TM_TT_NO_UNDO_SCOPE);

				tm_tt_id_t max_id = tm_the_truth_api->create_object_of_type(tt, tm_the_truth_api->object_type_from_name_hash(tt, TM_TT_TYPE_HASH__VEC3), TM_TT_NO_UNDO_SCOPE);
				tm_the_truth_object_o *max_w = tm_the_truth_api->write(tt, max_id);
				tm_set_float_array(tt, max_w, TM_TT_PROP__VEC3__X, &bounds[1].x, 3);
				tm_the_truth_api->set_subobject(tt, tm_mesh, TM_TT_PROP__DCC_ASSET_MESH__BOUNDS_MAX, max_w);
				tm_the_truth_api->commit(tt, max_w, TM_TT_NO_UNDO_SCOPE);
				vbuf_data += unpack_count * sizeof(float);
			}

			cgltf_float *normals_data = NULL;
			if (acc_NORMAL != NULL) {
				const cgltf_size unpack_count = acc_NORMAL->count * 3;
				cgltf_accessor_unpack_floats(acc_NORMAL, (cgltf_float *)vbuf_data, unpack_count);
				normals_data = (cgltf_float *)vbuf_data;
#ifdef VRM_CONVERT_COORD
				vrm_vec3_convert_coord(normals_data, unpack_count);
#endif
				vbuf_data += unpack_count * sizeof(float);
			}

			cgltf_float *texcoord_data = NULL;
			if (acc_TEXCOORD_0 != NULL) {
				const cgltf_size unpack_count = acc_TEXCOORD_0->count * 2;
				texcoord_data = (cgltf_float *)vbuf_data;
				cgltf_accessor_unpack_floats(acc_TEXCOORD_0, texcoord_data, unpack_count);
				vbuf_data += unpack_count * sizeof(float);
			}

			// Tangents
			if (normals_data != NULL && vertices_data != NULL && texcoord_data != NULL) {
				smikktspace_data_t mikk_data = {
					.normals = normals_data,
					.vertices = vertices_data,
					.texcoord = texcoord_data,
					.face_count = num_vertices,
					.buffer = vbuf_data
				};
				SMikkTSpaceInterface mikk_i = {
					.m_getNumFaces = tm_mikk_getNumFaces,
					.m_getNumVerticesOfFace = tm_mikk_getNumVerticesOfFace,
					.m_getNormal = tm_mikk_getNormal,
					.m_getPosition = tm_mikk_getPosition,
					.m_getTexCoord = tm_mikk_getTexCoord,
					.m_setTSpace = tm_mikk_setTSpace
				};
				SMikkTSpaceContext mikk_ctx = { .m_pInterface = &mikk_i, .m_pUserData = &mikk_data };
				genTangSpaceDefault(&mikk_ctx);

				vbuf_data += mikk_data.face_count * 4 * sizeof(float);
			}

			const uint32_t vbuf_id = buffers->add(buffers->inst, data_begins, vbuf_size, 0);
			tm_the_truth_api->set_buffer(tt, vdata, TM_TT_PROP__DCC_ASSET_BUFFER__DATA, vbuf_id);
			tm_the_truth_api->add_to_subobject_set(tt, obj, TM_TT_PROP__DCC_ASSET__BUFFERS, &vdata, 1);

			tm_the_truth_api->commit(tt, vdata, TM_TT_NO_UNDO_SCOPE);

			tt_total_mesh_count++;
			tm_the_truth_api->add_to_subobject_set(tt, obj, TM_TT_PROP__DCC_ASSET__MESHES, &tm_mesh, 1);
			tm_the_truth_api->commit(tt, tm_mesh, TM_TT_NO_UNDO_SCOPE);
		}
	}

	name_to_id_t node_by_name = { .allocator = a };

	if (tm_task_system_api->is_task_canceled(task_id))
		return false;

	// Scene graph
	tm_progress_report_api->set_task_progress(task_id, tm_temp_allocator_api->printf(ta, "%s - scene nodes..", scene_name), 0.5f);
	for (cgltf_size i = 0; i < data->scenes_count; ++i) {
		cgltf_scene *scene = &data->scenes[i];
		if (scene->nodes_count == 1) {
#ifndef VRM_CONVERT_COORD
			cgltf_node* rootnode = scene->nodes[0];
			vrm_normalize_axis(rootnode);
#endif
			import_node(tt, obj, scene_obj, NULL, scene->nodes[0], &node_by_name, error); // a single root
		} else {
			cgltf_node *fakeroot = &(cgltf_node){
				.name = "ROOT",
				.children_count = scene->nodes_count,
				.children = scene->nodes,
				.has_rotation = true,
				.rotation = {0, 0, 0, 1}
			};
#ifndef VRM_CONVERT_COORD
			vrm_normalize_axis(fakeroot);
#endif
			for (cgltf_size j = 0; j < scene->nodes_count; j++) {
				scene->nodes[j]->parent = fakeroot;
			}
			import_node(tt, obj, scene_obj, NULL, fakeroot, &node_by_name, error); // multiple root

		}
	}

	if (tm_task_system_api->is_task_canceled(task_id))
		return false;

	tm_the_truth_api->commit(tt, scene_obj, TM_TT_NO_UNDO_SCOPE);

	tm_progress_report_api->set_task_progress(task_id, 0, 0.99f);

	return true;
}

static void import_vrm_task(void *task_data, uint64_t task_id)
{
	TM_PROFILER_BEGIN_FUNC_SCOPE();

	if (tm_task_system_api->is_task_canceled(task_id)) {
		TM_PROFILER_END_FUNC_SCOPE();
		return;
	}

	TM_INIT_TEMP_ALLOCATOR(ta);

	import_vrm_task_t *task = (import_vrm_task_t *)task_data;
	const struct tm_asset_io_import *args = &task->args;
	const char *filename = task->filename;

	tm_progress_report_api->set_task_progress(task_id, tm_temp_allocator_api->printf(ta, "Loading: %s ...", filename), 0.5f);

	cgltf_options options = { 0 };
	cgltf_data *vrm_data = NULL;
	cgltf_result result = cgltf_parse_file(&options, filename, &vrm_data);

	if (result != cgltf_result_success) {
		tm_progress_report_api->set_task_progress(task_id, 0, 1.f);
		tm_logger_api->printf(TM_LOG_TYPE_ERROR, "Import of file %s failed", filename);
		tm_free(args->allocator, task, task->bytes);
		TM_SHUTDOWN_TEMP_ALLOCATOR(ta);
		TM_PROFILER_END_FUNC_SCOPE();
		return;
	}

	result = cgltf_load_buffers(&options, (cgltf_data *)vrm_data, filename);
	if (result != cgltf_result_success) {
		tm_progress_report_api->set_task_progress(task_id, 0, 1.f);
		tm_logger_api->printf(TM_LOG_TYPE_ERROR, "Import of buffers from %s failed", filename);
		tm_free(args->allocator, task, task->bytes);
		TM_SHUTDOWN_TEMP_ALLOCATOR(ta);
		TM_PROFILER_END_FUNC_SCOPE();
		return;
	}

	tm_the_truth_o *tt = args->tt;

	const uint64_t dcc_asset_type = tm_the_truth_api->object_type_from_name_hash(tt, TM_TT_TYPE_HASH__DCC_ASSET);
	const tm_tt_id_t asset_id = tm_the_truth_api->create_object_of_type(tt, dcc_asset_type, TM_TT_NO_UNDO_SCOPE);
	tm_the_truth_object_o *asset_obj = tm_the_truth_api->write(tt, asset_id);

	const char *ext;
	const char *name = tm_path_api->split(task->filename, &ext);
	const char *asset_path = tm_path_api_dir(task->filename, name, ta);
	const char *name_without_ext = tm_temp_allocator_api->printf(ta, "%.*s", ext - name, name);
	const char *asset_name = name_without_ext;

	tm_progress_report_api->set_task_progress(task_id, 0, 0.99f);

	if (import_into(tt, asset_obj, vrm_data, asset_name, asset_path, ta, tm_error_api->def, task_id)) {
		if (args->reimport_into.u64) {
			tm_the_truth_api->retarget_write(tt, asset_obj, args->reimport_into);
			tm_the_truth_api->commit(tt, asset_obj, args->undo_scope);
			tm_the_truth_api->destroy_object(tt, asset_id, args->undo_scope);
		}
		else {
			tm_the_truth_api->commit(tt, asset_obj, args->undo_scope);

			tm_asset_browser_add_asset_api *add_asset = tm_global_api_registry->get(TM_ASSET_BROWSER_ADD_ASSET_API_NAME);
			const bool should_select = args->asset_browser.u64 && tm_the_truth_api->version(tt, args->asset_browser) == args->asset_browser_version_at_start;
			add_asset->add(add_asset->inst, args->target_dir, asset_id, asset_name, args->undo_scope, should_select, args->ui);
		}
	}

	tm_progress_report_api->set_task_progress(task_id, 0, 1.f);

	cgltf_free(vrm_data);
	tm_free(args->allocator, task, task->bytes);

	TM_SHUTDOWN_TEMP_ALLOCATOR(ta);
	TM_PROFILER_END_FUNC_SCOPE();
}

static uint64_t import(const char *file, const struct tm_asset_io_import *args)
{
	// allocate string data together with the rest of the struct.
	const uint64_t bytes = sizeof(import_vrm_task_t) + strlen(file);
	import_vrm_task_t *task = tm_alloc(args->allocator, bytes);
	*task = (import_vrm_task_t){
		.bytes = bytes,
		.args = *args,
	};
	strcpy(task->filename, file);
	return tm_task_system_api->run_task(import_vrm_task, task, "VRM Import");
}

static bool enabled(struct tm_asset_io_o *inst)
{
	return true;
}

static bool can_import(struct tm_asset_io_o *inst, const char *extension)
{
	return tm_strcmp_ignore_case(extension, "vrm") == 0;
}

static bool can_reimport(struct tm_asset_io_o *inst, struct tm_the_truth_o *tt, tm_tt_id_t asset)
{
	const tm_tt_id_t wav = tm_the_truth_api->get_subobject(tt, tm_tt_read(tt, asset), TM_TT_PROP__ASSET__OBJECT);
	return wav.type == tm_the_truth_api->object_type_from_name_hash(tt, TM_TT_TYPE_HASH__DCC_ASSET);
}

static void importer_extensions_string(struct tm_asset_io_o *inst, char **output, struct tm_temp_allocator_i *ta, const char *separator)
{
	tm_carray_temp_printf(output, ta, "%s%s", "vrm", separator);
}

static void importer_description_string(struct tm_asset_io_o *inst, char **output, struct tm_temp_allocator_i *ta, const char *separator)
{
	tm_carray_temp_printf(output, ta, ".%s%s", "vrm", separator);
}

static uint64_t import_asset(struct tm_asset_io_o *inst, const char *file, const struct tm_asset_io_import *args)
{
	return import(file, args);
}

static struct tm_asset_io_i *vrm_importer = &(struct tm_asset_io_i)
{
	.enabled = enabled,
	.can_import = can_import,
	.can_reimport = can_reimport,
	.importer_extensions_string = importer_extensions_string,
	.importer_description_string = importer_description_string,
	.import_asset = import_asset
};

static struct tm_asset_io_i *io_interface()
{
	return vrm_importer;
}

struct tm_ig_vrm_api *tm_ig_vrm_api = &(struct tm_ig_vrm_api)
{
	.import = import,
	.io_interface = io_interface
};
