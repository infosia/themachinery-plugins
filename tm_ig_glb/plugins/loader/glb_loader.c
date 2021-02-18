#include "glb_loader.h"
#include "glb.h"

#include <foundation/api_registry.h>
#include <foundation/asset_io.h>
#include <foundation/buffer_format.h>
#include <foundation/carray.inl>
#include <foundation/error.h>
#include <foundation/image_loader.h>
#include <foundation/job_system.h>
#include <foundation/localizer.h>
#include <foundation/log.h>
#include <foundation/math.h>
#include <foundation/os.h>
#include <foundation/path.h>
#include <foundation/plugin_callbacks.h>
#include <foundation/profiler.h>
#include <foundation/progress_report.h>
#include <foundation/runtime_data_repository.h>
#include <foundation/task_system.h>
#include <foundation/the_truth.h>

#include <foundation/visibility_flags.h>

#include <plugins/dcc_asset/dcc_asset.h>
#include <plugins/default_render_pipe/default_render_pipe.h>
#include <plugins/editor_views/properties.h>
#include <plugins/entity/entity.h>
#include <plugins/entity/owner_component.h>
#include <plugins/render_graph/render_graph.h>
#include <plugins/renderer/renderer.h>
#include <plugins/shader_system/shader_system.h>
#include <plugins/the_machinery_shared/render_context.h>

struct tm_logger_api *tm_logger_api;
struct tm_os_thread_api *tm_thread_api;
struct tm_os_time_api *tm_time_api;
struct tm_error_api *tm_error_api;
struct tm_api_registry_api *tm_global_api_registry;
struct tm_temp_allocator_api *tm_temp_allocator_api;
struct tm_the_truth_api *tm_the_truth_api;
struct tm_buffer_format_api *tm_buffer_format_api;
struct tm_profiler_api *tm_profiler_api;
struct tm_job_system_api *tm_job_system_api;
struct tm_ui_api *tm_ui_api;
struct tm_localizer_api *tm_localizer_api;
struct tm_os_api *tm_os_api;
struct tm_asset_io_api *tm_asset_io_api;
struct tm_path_api *tm_path_api;
struct tm_math_api *tm_math_api;
struct tm_properties_view_api *tm_properties_view_api;
struct tm_entity_api *tm_entity_api;
struct tm_shader_repository_api *tm_shader_repository_api;
struct tm_render_context_api *tm_render_context_api;
struct tm_render_graph_api *tm_render_graph_api;
struct tm_runtime_data_repository_api *tm_runtime_data_repository_api;
struct tm_shader_api *tm_shader_api;
struct tm_shader_system_api *tm_shader_system_api;
struct tm_visibility_flags_api *tm_visibility_flags_api;
struct tm_renderer_api *tm_renderer_api;
struct tm_owner_component_api *tm_owner_component_api;
struct tm_image_loader_api *tm_image_loader_api;
struct tm_default_render_pipe_api *tm_default_render_pipe_api;
struct tm_dcc_asset_api *tm_dcc_asset_api;
struct tm_progress_report_api *tm_progress_report_api;
struct tm_task_system_api *tm_task_system_api;

const struct dcc_asset_type_info_t *dcc_asset_ti;

#define TM_IG_GLB_API_NAME "tm_ig_glb_api"

static void init(struct tm_plugin_o *inst, tm_allocator_i *allocator)
{
    dcc_asset_ti = tm_dcc_asset_api->truth_type_info();
}

struct tm_plugin_init_i init_i = {
    .init = init
};

static tm_localizer_strings_t localizer__get_strings(uint64_t language)
{
    typedef struct
    {
        const char *english;
        const char *context;
        const char *swedish;
    } entry_t;
    static const entry_t entries[] = {
#include "localizer_table.inl"
    };
    const char *const *strings = //
        language == TM_LANGUAGE_ENGLISH ? &entries[0].english : //
        language == TM_PSEUDO_LANGUAGE_CONTEXT ? &entries[0].context : //
            language == TM_LANGUAGE_SWEDISH ? &entries[0].swedish : //
                0;
    return (tm_localizer_strings_t){
        .num_strings = strings ? TM_ARRAY_COUNT(entries) : 0,
        .stride_bytes = sizeof(entry_t),
        .strings = strings,
    };
}

TM_DLL_EXPORT void tm_load_plugin(struct tm_api_registry_api *reg, bool load)
{
    tm_global_api_registry = reg;

    tm_logger_api = reg->get(TM_LOGGER_API_NAME);
    tm_error_api = reg->get(TM_ERROR_API_NAME);
    tm_temp_allocator_api = reg->get(TM_TEMP_ALLOCATOR_API_NAME);
    tm_buffer_format_api = reg->get(TM_BUFFER_FORMAT_API_NAME);
    tm_profiler_api = reg->get(TM_PROFILER_API_NAME);
    tm_job_system_api = reg->get(TM_JOB_SYSTEM_API_NAME);
    tm_localizer_api = reg->get(TM_LOCALIZER_API_NAME);
    tm_thread_api = ((struct tm_os_api *)reg->get(TM_OS_API_NAME))->thread;
    tm_time_api = ((struct tm_os_api *)reg->get(TM_OS_API_NAME))->time;
    tm_os_api = reg->get(TM_OS_API_NAME);
    tm_the_truth_api = reg->get(TM_THE_TRUTH_API_NAME);
    tm_asset_io_api = reg->get(TM_ASSET_IO_API_NAME);
    tm_path_api = reg->get(TM_PATH_API_NAME);
    tm_math_api = reg->get(TM_MATH_API_NAME);
    tm_properties_view_api = reg->get(TM_PROPERTIES_VIEW_API_NAME);
    tm_entity_api = reg->get(TM_ENTITY_API_NAME);
    tm_shader_repository_api = reg->get(TM_SHADER_REPOSITORY_API_NAME);
    tm_render_context_api = reg->get(TM_RENDER_CONTEXT_API_NAME);
    tm_render_graph_api = reg->get(TM_RENDER_GRAPH_API_NAME);
    tm_runtime_data_repository_api = reg->get(TM_RUNTIME_DATA_REPOSITORY_API_NAME);
    tm_shader_api = reg->get(TM_SHADER_API_NAME);
    tm_shader_system_api = reg->get(TM_SHADER_SYSTEM_API_NAME);
    tm_visibility_flags_api = reg->get(TM_VISIBILITY_FLAGS_API_NAME);
    tm_renderer_api = reg->get(TM_RENDERER_API_NAME);
    tm_owner_component_api = reg->get(TM_OWNER_COMPONENT_API_NAME);
    tm_image_loader_api = reg->get(TM_IMAGE_LOADER_API_NAME);
    tm_default_render_pipe_api = reg->get(TM_DEFAULT_RENDER_PIPE_API_NAME);
    tm_dcc_asset_api = reg->get(TM_DCC_ASSET_API_NAME);
    tm_progress_report_api = reg->get(TM_PROGRESS_REPORT_API_NAME);
    tm_task_system_api = reg->get(TM_TASK_SYSTEM_API_NAME);

    tm_set_or_remove_api(reg, load, TM_IG_GLB_API_NAME, tm_ig_glb_api);

    tm_add_or_remove_implementation(reg, load, TM_LOCALIZER_STRINGS_INTERFACE_NAME, localizer__get_strings);
    tm_add_or_remove_implementation(reg, load, TM_PLUGIN_INIT_INTERFACE_NAME, &init_i);

    if (load)
        tm_asset_io_api->add_asset_io(tm_ig_glb_api->io_interface());
    else
        tm_asset_io_api->remove_asset_io(tm_ig_glb_api->io_interface());
}
