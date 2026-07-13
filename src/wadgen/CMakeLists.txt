cmake_minimum_required(VERSION 3.16)

project(WadGen LANGUAGES C)

set(CMAKE_C_STANDARD 17)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

find_package(ZLIB REQUIRED)

set(PNG_SHARED OFF CACHE BOOL "" FORCE)
set(PNG_STATIC ON CACHE BOOL "" FORCE)
set(PNG_TESTS OFF CACHE BOOL "" FORCE)
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../../lib/lpng1514
                 ${CMAKE_CURRENT_BINARY_DIR}/lpng1514
                 EXCLUDE_FROM_ALL)

add_executable(WadGen WIN32
  DeflateN64.c
  Files.c
  Gfx.c
  Level.c
  Mem.c
  Png.c
  MD5.c
  Rom.c
  SndFont.c
  Sound.c
  Sprite.c
  Texture.c
  Wad.c
  WadGen.c
)

if(MSVC)
  target_compile_definitions(WadGen PRIVATE _CRT_SECURE_NO_WARNINGS NOMINMAX)
else()
  target_compile_options(WadGen PRIVATE -Wall -Wextra)
endif()

target_link_libraries(WadGen PRIVATE png_static ZLIB::ZLIB)

if(WIN32)
  target_link_libraries(WadGen PRIVATE comctl32)
elseif(UNIX AND NOT APPLE)
  find_library(M_LIB m)
  if(M_LIB)
    target_link_libraries(WadGen PRIVATE ${M_LIB})
  endif()
endif()

set_target_properties(WadGen PROPERTIES
  RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)
