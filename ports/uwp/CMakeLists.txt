cmake_minimum_required(VERSION 3.21)

project(Doom64EXClassicUWP LANGUAGES C CXX)

if(NOT CMAKE_SYSTEM_NAME STREQUAL "WindowsStore")
    message(FATAL_ERROR "Configure this project with one of the uwp-* presets.")
endif()

if(CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_CONFIGURATION_TYPES "Release;RelWithDebInfo" CACHE STRING "Supported configurations" FORCE)
endif()

set(CMAKE_C_STANDARD 17)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

get_filename_component(DOOM64EX_ROOT "${CMAKE_CURRENT_LIST_DIR}/../.." ABSOLUTE)
set(DOOM64EX_SOURCE_DIR "${DOOM64EX_ROOT}/src/kex")
set(DOOM64EX_DEPS_ROOT "${CMAKE_CURRENT_LIST_DIR}/third_party/uwp-dep-sdl2")
set(DOOM64EX_DEPS_X64 "${DOOM64EX_DEPS_ROOT}/x64")
set(DOOM64EX_DEPS_BIN "${DOOM64EX_DEPS_X64}/bin")
set(DOOM64EX_DEPS_LIB "${DOOM64EX_DEPS_X64}/lib")

foreach(required_file
    "${DOOM64EX_DEPS_BIN}/SDL2.dll"
    "${DOOM64EX_DEPS_LIB}/SDL2.lib"
    "${DOOM64EX_DEPS_BIN}/opengl32.dll"
    "${DOOM64EX_DEPS_BIN}/libgallium_wgl.dll")
    if(NOT EXISTS "${required_file}")
        message(FATAL_ERROR "The vendored SDL2/Mesa UWP depot is incomplete: ${required_file}")
    endif()
endforeach()

find_package(ZLIB REQUIRED)
find_package(fluidlite CONFIG REQUIRED)

add_library(Doom64EXSDL2 SHARED IMPORTED GLOBAL)
set_target_properties(Doom64EXSDL2 PROPERTIES
    IMPORTED_LOCATION "${DOOM64EX_DEPS_BIN}/SDL2.dll"
    IMPORTED_IMPLIB "${DOOM64EX_DEPS_LIB}/SDL2.lib"
    INTERFACE_INCLUDE_DIRECTORIES "${DOOM64EX_DEPS_X64}/include")
add_library(SDL2::SDL2 ALIAS Doom64EXSDL2)

set(PNG_SHARED OFF CACHE BOOL "Build libpng 1.5.14 statically" FORCE)
set(PNG_STATIC ON CACHE BOOL "Build libpng 1.5.14 statically" FORCE)
set(PNG_TESTS OFF CACHE BOOL "Do not build libpng test programs" FORCE)
add_subdirectory("${DOOM64EX_ROOT}/lib/lpng1514" "${CMAKE_BINARY_DIR}/libpng15")
target_compile_definitions(png_objects PRIVATE PNG_USE_CRT_STRING_FUNCTIONS _CRT_SECURE_NO_WARNINGS lstrlenA=strlen)

file(GLOB DOOM64EX_GAME_SOURCES CONFIGURE_DEPENDS "${DOOM64EX_SOURCE_DIR}/*.c")
list(REMOVE_ITEM DOOM64EX_GAME_SOURCES
    "${DOOM64EX_SOURCE_DIR}/i_exception.c"
    "${DOOM64EX_SOURCE_DIR}/i_opndir.c"
    "${DOOM64EX_SOURCE_DIR}/i_cpu.c"
    "${DOOM64EX_SOURCE_DIR}/i_cpu_posix.c")

add_library(Doom64EXGame STATIC
    ${DOOM64EX_GAME_SOURCES}
    "${CMAKE_CURRENT_LIST_DIR}/compat/fluidlite_sdl_audio.c"
    "${CMAKE_CURRENT_LIST_DIR}/compat/uwp_glu.c"
    "${CMAKE_CURRENT_LIST_DIR}/compat/uwp_storage_root.c"
    "${CMAKE_CURRENT_LIST_DIR}/compat/uwp_video.c")
target_include_directories(Doom64EXGame PRIVATE
    "${CMAKE_CURRENT_LIST_DIR}/compat"
    "${DOOM64EX_SOURCE_DIR}"
    "${DOOM64EX_SOURCE_DIR}/Ext"
    "${DOOM64EX_ROOT}/lib/lpng1514")
target_compile_definitions(Doom64EXGame PRIVATE
    DOOM64EX_UWP=1 HAVE_FLUIDSYNTH=1 FLUIDLITE_STATIC=1 FLUIDSYNTH_NOT_A_DLL=1
    SDL_MAIN_HANDLED=1 NOMINMAX WIN32_LEAN_AND_MEAN _CRT_SECURE_NO_WARNINGS _CRT_NONSTDC_NO_WARNINGS)
target_compile_options(Doom64EXGame PRIVATE
    /FIstdarg.h /FIstdint.h
    "/FI${CMAKE_CURRENT_LIST_DIR}/compat/uwp_crt_compat.h"
    "/FI${DOOM64EX_SOURCE_DIR}/psnprntf.h")
target_link_libraries(Doom64EXGame PRIVATE
    SDL2::SDL2 fluidlite::fluidlite-static png_static ZLIB::ZLIB
    opengl32.lib ws2_32.lib WindowsApp.lib OneCore.lib)

add_executable(Doom64EXClassicUWP WIN32
    "${CMAKE_CURRENT_LIST_DIR}/main.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/doom64ex-entry.cpp")
set_source_files_properties(
    "${CMAKE_CURRENT_LIST_DIR}/main.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/doom64ex-entry.cpp"
    PROPERTIES COMPILE_OPTIONS "/ZW")
target_include_directories(Doom64EXClassicUWP PRIVATE "${DOOM64EX_DEPS_X64}/include")
target_compile_definitions(Doom64EXClassicUWP PRIVATE SDL_MAIN_HANDLED=1)
target_link_libraries(Doom64EXClassicUWP PRIVATE Doom64EXGame SDL2::SDL2 WindowsApp.lib)
set_target_properties(Doom64EXClassicUWP PROPERTIES
    VS_WINDOWS_TARGET_PLATFORM_MIN_VERSION "10.0.19041.0"
    VS_SDK_REFERENCES "Microsoft.VCLibs, Version=14.0")

function(doom64ex_deploy file_path destination)
    target_sources(Doom64EXClassicUWP PRIVATE "${file_path}")
    set_source_files_properties("${file_path}" PROPERTIES
        VS_TOOL_OVERRIDE "Content" VS_COPY_TO_OUT_DIR Always VS_DEPLOYMENT_CONTENT TRUE
        VS_DEPLOYMENT_LOCATION "${destination}")
endfunction()

target_sources(Doom64EXClassicUWP PRIVATE "${CMAKE_CURRENT_LIST_DIR}/Package.appxmanifest")
set_source_files_properties("${CMAKE_CURRENT_LIST_DIR}/Package.appxmanifest" PROPERTIES
    VS_TOOL_OVERRIDE "AppxManifest" VS_COPY_TO_OUT_DIR Always VS_DEPLOYMENT_CONTENT TRUE VS_DEPLOYMENT_LOCATION ".")
file(GLOB DOOM64EX_ASSETS CONFIGURE_DEPENDS "${CMAKE_CURRENT_LIST_DIR}/Assets/*.png")
foreach(asset IN LISTS DOOM64EX_ASSETS)
    doom64ex_deploy("${asset}" "Assets")
endforeach()
file(GLOB DOOM64EX_RUNTIME_DLLS CONFIGURE_DEPENDS "${DOOM64EX_DEPS_BIN}/*.dll")
foreach(runtime IN LISTS DOOM64EX_RUNTIME_DLLS)
    doom64ex_deploy("${runtime}" ".")
endforeach()

set_property(DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}" PROPERTY VS_STARTUP_PROJECT Doom64EXClassicUWP)
