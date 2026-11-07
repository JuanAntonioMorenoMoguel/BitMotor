cmake_minimum_required(VERSION 3.25)

if(NOT DEFINED CMAKE_TOOLCHAIN_FILE AND DEFINED ENV{VCPKG_ROOT})
    set(CMAKE_TOOLCHAIN_FILE "$ENV{VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake" CACHE STRING "")
endif()

project(BitMotor VERSION 0.0.1 LANGUAGES C CXX)

# BitMotor: a C++ library to make simple games.
# Using it from a game:
#     add_subdirectory(${BITMOTOR_PATH} BitMotor)
#     target_link_libraries(MyGame PRIVATE BitMotor::BitMotor)
#     BitMotor_Prepare(MyGame COPY Data Assets)
# Dependencies come from the game's vcpkg.json (copy the one in this folder).

set(BITMOTOR_TOP_LEVEL OFF)
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    set(BITMOTOR_TOP_LEVEL ON)
    set(CMAKE_CXX_STANDARD 20)
    set(CMAKE_CXX_STANDARD_REQUIRED ON)
    set(CMAKE_CXX_EXTENSIONS OFF)
    set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
endif()

option(BITMOTOR_TESTS "Build the BitMotor tests" ${BITMOTOR_TOP_LEVEL})
option(BITMOTOR_EXAMPLES "Build the BitMotor examples" ${BITMOTOR_TOP_LEVEL})
option(BITMOTOR_WARNINGS_AS_ERRORS "Treat warnings in BitMotor's own code as errors" OFF)
set(BITMOTOR_STEAM_SDK "" CACHE PATH "Path to the Steamworks SDK (empty: local platform only). Used from phase 6")

find_package(SDL3 CONFIG REQUIRED)
find_package(nlohmann_json CONFIG REQUIRED)
# stb (images): header-only. vcpkg puts it in include/, Linux distributions in include/stb/.
find_path(BITMOTOR_STB_INCLUDE_DIR stb_image.h PATH_SUFFIXES stb REQUIRED)
# miniaudio (audio device; WAV, FLAC and MP3): one header, compiled inside BitMotor. stb_vorbis (OGG) comes with stb.
find_path(BITMOTOR_MINIAUDIO_INCLUDE_DIR miniaudio.h PATH_SUFFIXES miniaudio REQUIRED)
find_package(Threads REQUIRED)

file(GLOB_RECURSE BITMOTOR_SOURCES CONFIGURE_DEPENDS
    ${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/*.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/*.vert
    ${CMAKE_CURRENT_SOURCE_DIR}/src/*.frag
    ${CMAKE_CURRENT_SOURCE_DIR}/include/BitMotor/*.h)
add_library(BitMotor STATIC ${BITMOTOR_SOURCES})
add_library(BitMotor::BitMotor ALIAS BitMotor)
target_compile_features(BitMotor PUBLIC cxx_std_20)
target_include_directories(BitMotor
    PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src ${BITMOTOR_STB_INCLUDE_DIR} ${BITMOTOR_MINIAUDIO_INCLUDE_DIR})
target_link_libraries(BitMotor PRIVATE SDL3::SDL3 nlohmann_json::nlohmann_json Threads::Threads ${CMAKE_DL_LIBS})
if(UNIX AND NOT APPLE)
    target_link_libraries(BitMotor PRIVATE m)
endif()
if(WIN32)
    target_link_libraries(BitMotor PRIVATE shell32) # CommandLineToArgvW
endif()
target_compile_definitions(BitMotor PRIVATE BITMOTOR_VERSION="${PROJECT_VERSION}")

# Warnings only for BitMotor's own code: they are never imposed on the game.
include(cmake/BitMotor.cmake)
_BitMotor_Warnings(BitMotor)

if(BITMOTOR_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
if(BITMOTOR_EXAMPLES)
    add_subdirectory(examples)
endif()
