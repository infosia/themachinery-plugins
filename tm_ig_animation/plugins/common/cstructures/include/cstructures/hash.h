#pragma once

#include "cstructures/config.h"
#include <stdint.h>

C_BEGIN

typedef uint32_t cs_hash32;
typedef cs_hash32 (*hash32_func)(const void*, uintptr_t);

CSTRUCTURES_PUBLIC_API cs_hash32
hash32_jenkins_oaat(const void* key, uintptr_t len);

CSTRUCTURES_PUBLIC_API cs_hash32
hash32_ptr(const void* ptr, uintptr_t len);

CSTRUCTURES_PUBLIC_API cs_hash32
hash32_aligned_ptr(const void* ptr, uintptr_t len);

/*!
 * @brief Taken from boost::hash_combine. Combines two hash values into a
 * new hash value.
 */
CSTRUCTURES_PUBLIC_API cs_hash32
hash32_combine(cs_hash32 lhs, cs_hash32 rhs);

C_END
