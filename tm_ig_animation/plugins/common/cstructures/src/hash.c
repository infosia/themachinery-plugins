#include "cstructures/hash.h"
#include <assert.h>

/* ------------------------------------------------------------------------- */
cs_hash32
hash32_jenkins_oaat(const void* key, uintptr_t len)
{
    cs_hash32 hash, i;
    for(hash = i = 0; i != len; ++i)
    {
        hash += *((uint8_t*)key + i);
        hash += (hash << 10);
        hash ^= (hash >> 6);
    }
    hash += (hash << 3);
    hash ^= (hash >> 1);
    hash += (hash << 15);
    return hash;
}

/* ------------------------------------------------------------------------- */
#if CSTRUCTURES_SIZEOF_VOID_P == 8
cs_hash32
hash32_ptr(const void* ptr, uintptr_t len)
{
    assert(len == sizeof(void*));
    assert(sizeof(uintptr_t) == sizeof(void*));

    return hash32_combine(
           (cs_hash32)(*(uintptr_t*)ptr & 0xFFFFFFFF),
           (cs_hash32)(*(uintptr_t*)ptr >> 32)
    );
}
#elif CSTRUCTURES_SIZEOF_VOID_P == 4
hash32_t
hash32_ptr(const void* ptr, uintptr_t len)
{
    assert(len == sizeof(void*));
    assert(sizeof(uintptr_t) == sizeof(void*));

    return (hash32_t)*(uintptr_t*)ptr;
}
#endif

/* ------------------------------------------------------------------------- */
#if CSTRUCTURES_SIZEOF_VOID_P == 8
cs_hash32
hash32_aligned_ptr(const void* ptr, uintptr_t len)
{
    assert(len == sizeof(void*));
    assert(sizeof(uintptr_t) == sizeof(void*));

    return (cs_hash32)((*(uintptr_t*)ptr / sizeof(void*)) & 0xFFFFFFFF);
}
#elif CSTRUCTURES_SIZEOF_VOID_P == 4
hash32_t
hash32_aligned_ptr(const void* ptr, uintptr_t len)
{
    assert(len == sizeof(void*));
    assert(sizeof(uintptr_t) == sizeof(void*));

    return (hash32_t)(*(uintptr_t*)ptr / sizeof(void*));
}
#endif

/* ------------------------------------------------------------------------- */
cs_hash32
hash32_combine(cs_hash32 lhs, cs_hash32 rhs)
{
    lhs ^= rhs + 0x9e3779b9 + (lhs << 6) + (lhs >> 2);
    return lhs;
}
