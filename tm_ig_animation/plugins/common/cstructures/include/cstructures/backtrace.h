#pragma once

#define CSTRUCTURES_BACKTRACE_SIZE 64

#include "cstructures/config.h"

C_BEGIN

/*!
 * @brief Generates a backtrace.
 * @param[in] size The maximum number of frames to walk.
 * @return Returns an array of char* arrays.
 * @note The returned array must be freed manually with FREE(returned_array).
 */
CSTRUCTURES_PRIVATE_API char**
get_backtrace(int* size);

C_END
