-- premake5.lua
-- version: premake-5.0.0-alpha14

-- %TM_SDK_DIR% should be set to the location of The Machinery SDK that has the headers/ and lib/
-- directories.

newoption {
    trigger     = "clang",
    description = "Force use of CLANG for Windows builds"
 }

 function lib_path(path)
    local lib_dir = os.getenv("TM_LIB_DIR")

    if lib_dir == nil then
        error("TM_LIB_DIR not set")
        return nil
    end

    return lib_dir .. "/" .. path
end

workspace "tm_ig_vrm"
    configurations {"Debug", "Release"}
    language "C++"
    cppdialect "C++11"
    flags { "FatalWarnings", "MultiProcessorCompile" }
    warnings "Extra"
    inlining "Auto"
    sysincludedirs { "" }
    targetdir "bin/%{cfg.buildcfg}"
    editandcontinue "Off"

filter "system:windows"
    platforms { "Win64" }
    systemversion("latest")

filter { "system:windows", "options:clang" }
    toolset("msc-clangcl")
    buildoptions {
        "-Wno-missing-field-initializers",   -- = {0} is OK.
        "-Wno-unused-parameter",             -- Useful for documentation purposes.
        "-Wno-unused-local-typedef",         -- We don't always use all typedefs.
        "-Wno-missing-braces",               -- = {0} is OK.
        "-Wno-microsoft-anon-tag",           -- Allow anonymous structs.
    }
    buildoptions {
        "-fms-extensions",                   -- Allow anonymous struct as C inheritance.
        "-mavx",                             -- AVX.
        "-mfma",                             -- FMA.
    }
    removeflags {"FatalLinkWarnings"}        -- clang linker doesn't understand /WX

filter { "system:macosx" }
    platforms { "MacOSX" }

filter {"system:linux"}
    platforms { "Linux" }

filter "platforms:Win64"
    defines { "TM_OS_WINDOWS", "_CRT_SECURE_NO_WARNINGS" }
    includedirs { "%TM_SDK_DIR%/headers" }
    staticruntime "On"
    architecture "x64"
    prebuildcommands {
        "if not defined TM_SDK_DIR (echo ERROR: Environment variable TM_SDK_DIR must be set)"
    }
    libdirs { "%TM_SDK_DIR%/lib/" .. _ACTION .. "/%{cfg.buildcfg}"}
    disablewarnings {
        "4057", -- Slightly different base types. Converting from type with volatile to without.
        "4100", -- Unused formal parameter. I think unusued parameters are good for documentation.
        "4152", -- Conversion from function pointer to void *. Should be ok.
        "4200", -- Zero-sized array. Valid C99.
        "4201", -- Nameless struct/union. Valid C11.
        "4204", -- Non-constant aggregate initializer. Valid C99.
        "4206", -- Translation unit is empty. Might be #ifdefed out.
        "4214", -- Bool bit-fields. Valid C99.
        "4221", -- Pointers to locals in initializers. Valid C99.
        "4702", -- Unreachable code. We sometimes want return after exit() because otherwise we get an error about no return value.
    }
    linkoptions {"/ignore:4099"}

filter { "platforms:MacOSX" }
    defines { "TM_OS_MACOSX", "TM_OS_POSIX",  "TM_NO_MAIN_FIBER" }
    architecture "x64"
    buildoptions {
        "-fms-extensions",              -- Allow anonymous struct as C inheritance.
        "-mavx",                             -- AVX.
        "-mfma",                             -- FMA.
    }
    disablewarnings {
        "missing-field-initializers",   -- = {0} is OK.
        "unused-parameter",             -- Useful for documentation purposes.
        "unused-local-typedef",         -- We don't always use all typedefs.
        "missing-braces",               -- = {0} is OK.
        "microsoft-anon-tag",           -- Allow anonymous structs.
    }
    -- Needed, because Xcode project generation does not respect `disablewarnings` (premake-5.0.0-alpha13)
    xcodebuildsettings {
    WARNING_CFLAGS = "-Wall -Wextra " ..
        "-Wno-missing-field-initializers " ..
        "-Wno-unused-parameter " ..
        "-Wno-unused-local-typedef " ..
        "-Wno-missing-braces " ..
        "-Wno-microsoft-anon-tag "
    }

filter { "platforms:MacOSX" }
    includedirs { "$(TM_SDK_DIR)/headers" }
    prebuildcommands {
        '@if [ -z "$(TM_SDK_DIR)" ]; then echo "ERROR: Environment variable TM_SDK_DIR must be set"; exit 1; fi'
    }
    libdirs { "$(TM_SDK_DIR)/lib/" .. _ACTION .. "/%{cfg.buildcfg}"}
   
filter {"platforms:Linux"}
    defines { "TM_OS_LINUX", "TM_OS_POSIX" }
    architecture "x64"
    buildoptions {
        "-fms-extensions",                   -- Allow anonymous struct as C inheritance.
        "-g",                                -- Debugging.
        "-mavx",                             -- AVX.
        "-mfma",                             -- FMA.
    }
    enablewarnings {"shadow"}
    disablewarnings {
        "missing-field-initializers",   -- = {0} is OK.
        "unused-parameter",             -- Useful for documentation purposes.
        "unused-local-typedef",         -- We don't always use all typedefs.
        "missing-braces",               -- = {0} is OK.
        "microsoft-anon-tag",           -- Allow anonymous structs.
    }

filter { "platforms:Linux" }
    includedirs { "$(TM_SDK_DIR)/headers" }
    prebuildcommands {
        '@if [ -z "$(TM_SDK_DIR)" ]; then echo "ERROR: Environment variable TM_SDK_DIR must be set"; exit 1; fi'
    }
    libdirs { "$(TM_SDK_DIR)/lib/" .. _ACTION .. "/%{cfg.buildcfg}"}

filter "configurations:Debug"
    defines { "TM_CONFIGURATION_DEBUG", "DEBUG" }
    symbols "On"

filter "configurations:Release"
    defines { "TM_CONFIGURATION_RELEASE" }
    optimize "On"

project "vrm"
    location "build/vrm"
    kind "SharedLib"
    targetname "tm_ig_vrm"
    language "C++"
    targetdir "bin/%{cfg.buildcfg}/plugins"
    defines { "TM_LINKS_IG_VRM" }
    files {"vrm_loader/**.inl", "vrm_loader/**.h", "vrm_loader/**.c"}
    sysincludedirs { "" }
    filter "platforms:Win64"
    targetdir "$(TM_SDK_DIR)/bin/plugins"
    links { }
    includedirs { "vrm_loader/include" }
