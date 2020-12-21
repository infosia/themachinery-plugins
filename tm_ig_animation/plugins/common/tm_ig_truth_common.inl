static void set_vec3(tm_the_truth_o *tt, tm_the_truth_object_o *o, tm_vec3_t* a)
{
    tm_the_truth_api->set_float(tt, o, TM_TT_PROP__VEC3__X, a->x);
    tm_the_truth_api->set_float(tt, o, TM_TT_PROP__VEC3__Y, a->y);
    tm_the_truth_api->set_float(tt, o, TM_TT_PROP__VEC3__Z, a->z);
}

static void set_vec4(tm_the_truth_o *tt, tm_the_truth_object_o *o, tm_vec4_t *a)
{
    tm_the_truth_api->set_float(tt, o, TM_TT_PROP__VEC4__X, a->x);
    tm_the_truth_api->set_float(tt, o, TM_TT_PROP__VEC4__Y, a->y);
    tm_the_truth_api->set_float(tt, o, TM_TT_PROP__VEC4__Z, a->z);
    tm_the_truth_api->set_float(tt, o, TM_TT_PROP__VEC4__W, a->w);
}

static void read_vec3(const tm_the_truth_o *tt, const tm_the_truth_object_o *o, tm_vec3_t *a)
{
    a->x = tm_the_truth_api->get_float(tt, o, TM_TT_PROP__VEC3__X);
    a->y = tm_the_truth_api->get_float(tt, o, TM_TT_PROP__VEC3__Y);
    a->z = tm_the_truth_api->get_float(tt, o, TM_TT_PROP__VEC3__Z);
}

static void read_vec4(const tm_the_truth_o *tt, const tm_the_truth_object_o *o, tm_vec4_t *a)
{
    a->x = tm_the_truth_api->get_float(tt, o, TM_TT_PROP__VEC4__X);
    a->y = tm_the_truth_api->get_float(tt, o, TM_TT_PROP__VEC4__Y);
    a->z = tm_the_truth_api->get_float(tt, o, TM_TT_PROP__VEC4__Z);
    a->w = tm_the_truth_api->get_float(tt, o, TM_TT_PROP__VEC4__W);
}