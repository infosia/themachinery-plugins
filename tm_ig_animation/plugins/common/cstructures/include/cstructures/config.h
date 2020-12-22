#ifndef CSTRUCTURES_CONFIG_H
#define CSTRUCTURES_CONFIG_H

/* #undef CSTRUCTURES_BENCHMARKS */
/* #undef CSTRUCTURES_BTREE_64BIT_KEYS */
/* #undef CSTRUCTURES_BTREE_64BIT_CAPACITY */
/* #undef CSTRUCTURES_MEMORY_BACKTRACE */
/* #undef CSTRUCTURES_MEMORY_DEBUGGING */
#define CSTRUCTURES_PIC
/* #undef CSTRUCTURES_TESTS */
/* #undef CSTRUCTURES_VEC_64BIT */

#define CSTRUCTURES_STATIC
#define CSTRUCTURES_SIZEOF_VOID_P 8
#define CSTRUCTURES_BTREE_EXPAND_FACTOR 2
#define CSTRUCTURES_BTREE_MIN_CAPACITY  32
#define CSTRUCTURES_VEC_EXPAND_FACTOR   2
#define CSTRUCTURES_VEC_MIN_CAPACITY    32

#if defined(CSTRUCTURES_SHARED)
#   if defined(CSTRUCTURES_BUILDING)
#       define CSTRUCTURES_PUBLIC_API 
#   else
#       define CSTRUCTURES_PUBLIC_API 
#   endif
#   define CSTRUCTURES_PRIVATE_API 
#else
#   define CSTRUCTURES_PUBLIC_API
#   define CSTRUCTURES_PRIVATE_API
#endif

#ifdef __cplusplus
#   define C_BEGIN extern "C" {
#   define C_END }
#else
#   define C_BEGIN
#   define C_END
#endif

#endif /* CSTRUCTURES_CONFIG_H */
