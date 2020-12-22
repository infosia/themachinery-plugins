/* -------------------------------------------------------------------------
 * Configures prerequisite for this library
 * ------------------------------------------------------------------------- */

#ifndef IK_CONFIG_H
    #define IK_CONFIG_H

    /* ---------------------------------------------------------------------
     * Build settings
     * --------------------------------------------------------------------- */

/* #undef IK_BENCHMARKS */
/* #undef IK_DOT_EXPORT */
    #define IK_HAVE_STDINT_H
/* #undef IK_MEMORY_DEBUGGING */
/* #undef IK_MEMORY_BACKTRACE */
/* #undef IK_HASHMAP_STATS */
    #define IK_LOGGING
    #define IK_PIC
/* #undef IK_PROFILING */
/* #undef IK_PYTHON */
/* #undef IK_PYTHON_REFCOUNT_DEBUGGING */
/* #undef IK_PYTHON_REFCOUNT_BACKTRACES */
/* #undef IK_TESTS */

#   define IK_PRECISION_FLOAT

#   if !defined(IK_MEMORY_DEBUGGING)
#       if defined(IK_MEMORY_BACKTRACE)
#           undef IK_MEMORY_BACKTRACE
#       endif
#   endif

#   if !defined(IK_PYTHON_REFCOUNT_DEBUGGING)
#       if defined(IK_PYTHON_REFCOUNT_BACKTRACES)
#           undef IK_PYTHON_REFCOUNT_BACKTRACES
#       endif
#   endif

    /* ---------------------------------------------------------------------
     * Helpers
     * --------------------------------------------------------------------- */

    /* Helpful for making sure functions are being used */
#   define IK_WARN_UNUSED 

    /* Visibility macros */
#   define IK_HELPER_API_EXPORT 
#   define IK_HELPER_API_IMPORT 
#   define IK_HELPER_API_LOCAL  
#   if defined (IK_BUILDING)  /* defined only when the library is being compiled using a command line option, e.g. -DIK_BUILDING */
#       define IK_PUBLIC_API IK_HELPER_API_EXPORT
#   else
#       define IK_PUBLIC_API IK_HELPER_API_IMPORT
#   endif
#   define IK_PRIVATE_API IK_HELPER_API_LOCAL

    /* C linkage */
#   ifdef __cplusplus
#       define C_BEGIN extern "C" {
#       define C_END }
#   else
#       define C_BEGIN
#       define C_END
#   endif

    /* --------------------------------------------------------------
     * Common include files
     * --------------------------------------------------------------*/

    #ifdef IK_HAVE_STDINT_H
        #include <stdint.h>
    #else
        #include "ik/pstdint.h"
    #endif

    /* --------------------------------------------------------------
     * Types
     * --------------------------------------------------------------*/

    /* The "real" datatype to be used throughout the library */
typedef float ikreal;

    /* 2 to the power of CPU word size. E.g. on 64-bit machines, this would
     * be 3 because 2^3 = 8.
     */
#   define IK_CPU_WORD_SIZE 8

    /* Define epsilon depending on the type of "real" */
#   include <float.h>
#   if defined(IK_PRECISION_LONG_DOUBLE)
#       define IK_EPSILON DBL_EPSILON
#   elif defined(IK_PRECISION_DOUBLE)
#       define IK_EPSILON DBL_EPSILON
#   elif defined(IK_PRECISION_FLOAT)
#       define IK_EPSILON FLT_EPSILON
#   else
#       error Unknown precision. Are you sure you defined IK_PRECISION and IK_PRECISION_CAPS_AND_NO_SPACES?
#   endif

#endif /* IK_CONFIG_H */
