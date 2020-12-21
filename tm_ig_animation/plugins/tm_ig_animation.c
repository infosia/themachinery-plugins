#include <foundation/api_types.h>
#include <foundation/api_registry.h>

extern void tm_ig_animation_puppet_load_plugin(struct tm_api_registry_api *reg, bool load);
extern void tm_ig_animation_inverse_kinematics_load_plugin(struct tm_api_registry_api* reg, bool load);

TM_DLL_EXPORT void tm_load_plugin(struct tm_api_registry_api *reg, bool load)
{
	tm_ig_animation_puppet_load_plugin(reg, load);
	tm_ig_animation_inverse_kinematics_load_plugin(reg, load);
}
