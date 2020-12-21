#pragma once

#include <foundation/api_types.h>

#define TM_TT_TYPE__IG_ANIMATION_INVERSE_KINEMATICS "tm_ig_animation_inverse_kinematics"
#define TM_TT_TYPE_HASH__IG_ANIMATION_INVERSE_KINEMATICS TM_STATIC_HASH("tm_ig_animation_inverse_kinematics",  0x725c672fece6864aULL)

enum {
    TM_TT_PROP__IG_ANIMATION_IK__POSITION_FACTOR, // subobject(TM_TT_TYPE__POSITION)
    TM_TT_PROP__IG_ANIMATION_IK__ROTATION_FACTOR, // subobject(TM_TT_TYPE__ROTATION)
    TM_TT_PROP__IG_ANIMATION_IK__SCALE_FACTOR,    // subobject(TM_TT_TYPE__SCALE)
};
