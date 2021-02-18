#pragma once

#include <foundation/api_types.h>

struct tm_allocator_i;
struct tm_the_truth_o;
struct tm_ui_o;
struct tm_asset_io_import;

struct tm_ig_glb_api
{
    // Creates the types used to represent ASSIMP objects in The Truth.
    void (*create_truth_types)(struct tm_the_truth_o *truth);

    // Imports the specified 'file' into The Truth.
    void (*import)(const char *file, const struct tm_asset_io_import *import);

    // Returns the asset io interface which can be registered to the `tm_asset_io_api`.
    struct tm_asset_io_i *(*io_interface)();
};

#if defined(TM_LINKS_IG_GLB)
extern struct tm_ig_glb_api* tm_ig_glb_api;
#endif
