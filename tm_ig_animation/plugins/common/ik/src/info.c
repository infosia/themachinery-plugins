#include "ik/info.h"
#include "ik/build_info.h"

#define QUOTE(str) #str
#define EXPAND_AND_QUOTE(str) QUOTE(str)

#define IK_VERSION "0.2.0." EXPAND_AND_QUOTE(IK_BUILD_NUMBER)
#define IK_CMAKE_CONFIG \
    "    -DCMAKE_BUILD_TYPE=Release \\\n" \
    "    -DCMAKE_INSTALL_PREFIX=\"C:/Program Files (x86)/Inverse Kinematics\" \\\n" \
    "    -DCMAKE_PREFIX_PATH=\"\" \\\n" \
    "    -DIK_API_NAME=\"\" \\\n" \
    "    -DIK_BENCHMARKS=OFF \\\n" \
    "    -DIK_BUILDINFO=ON \\\n" \
    "    -DIK_DOT_EXPORT= \\\n" \
    "    -DIK_LIB_TYPE=STATIC \\\n" \
    "    -DIK_MEMORY_DEBUGGING= \\\n" \
    "    -DIK_MEMORY_BACKTRACE= \\\n" \
    "    -DIK_HASHMAP_STATS= \\\n" \
    "    -DIK_PIC=ON \\\n" \
    "    -DIK_PRECISION=float \\\n" \
    "    -DIK_PROFILING=OFF \\\n" \
    "    -DIK_PYTHON=OFF \\\n" \
    "    -DIK_PYTHON_VERSION=3 \\\n" \
    "    -DIK_TESTS=OFF \\\n" \
    "    -DIK_VECTOR_64BIT="
#define IK_AUTHOR \
    "TheComet <alex.murray@gmx.ch>"

/* ------------------------------------------------------------------------- */
const char*
ik_info_author(void)
{
    return IK_AUTHOR;
}

/* ------------------------------------------------------------------------- */
const char*
ik_info_version(void)
{
    return IK_VERSION;
}

/* ------------------------------------------------------------------------- */
int
ik_info_build_number(void)
{
    return IK_BUILD_NUMBER;
}

/* ------------------------------------------------------------------------- */
const char*
ik_info_host(void)
{
    return "3.0.7-338.x86_64 x86_64 unknown unknown Msys";
}

/* ------------------------------------------------------------------------- */
const char*
ik_info_date(void)
{
    return IK_BUILD_TIME;
}

/* ------------------------------------------------------------------------- */
const char*
ik_info_commit(void)
{
    return "v1.1-140-gc09c972 (c09c972)";
}

/* ------------------------------------------------------------------------- */
const char*
ik_info_compiler(void)
{
    return "MSVC";
}

/* ------------------------------------------------------------------------- */
const char*
ik_info_cmake(void)
{
    return IK_CMAKE_CONFIG;
}

/* ------------------------------------------------------------------------- */
const char*
ik_info_all(void)
{
    return
    "This is IK " IK_VERSION " (major,minor,patch,build number)\n"
    "Created by " IK_AUTHOR "\n"
    "Build host: 3.0.7-338.x86_64 x86_64 unknown unknown Msys\n"
    "Time compiled: " IK_BUILD_TIME "\n"
    "Commit: v1.1-140-gc09c972 (c09c972)\n"
    "Compiler: MSVC\n"
    "CMake configuration:\n"
    IK_CMAKE_CONFIG "\n"
    "Other interesting variables:\n"
    "    IK_HAVE_STDINT_H=1\n"
    "    IK_WARN_UNUSED=\n"
    ;
}
