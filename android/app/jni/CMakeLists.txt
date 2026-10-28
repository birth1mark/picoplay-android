cmake_minimum_required(VERSION 3.18)
project(PicoPlayAndroid C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Google Play 16KB page size compliance (Android 15 / API 35+)
add_link_options("-Wl,-z,max-page-size=16384")

# Build SDL2 shared library from bundled source
set(SDL_SHARED ON CACHE BOOL "" FORCE)
set(SDL_STATIC OFF CACHE BOOL "" FORCE)
set(SDL_TEST OFF CACHE BOOL "" FORCE)
add_subdirectory(SDL2)

set(REPO_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../../..")

file(GLOB Z8LUA_SRCS "${REPO_ROOT}/libs/z8lua/*.c")
# z8lua uses C++ fix32 fixed-point arithmetic in .c files
set_source_files_properties(${Z8LUA_SRCS} PROPERTIES LANGUAGE CXX)

file(GLOB LODEPNG_SRCS "${REPO_ROOT}/libs/lodepng/*.cpp")
file(GLOB ENGINE_SRCS "${REPO_ROOT}/source/*.cpp")

add_library(main SHARED
    ${Z8LUA_SRCS}
    ${LODEPNG_SRCS}
    ${ENGINE_SRCS}
    "${REPO_ROOT}/libs/miniz/miniz.c"
    "${REPO_ROOT}/platform/SDL2Common/source/sdl2basehost.cpp"
    "${REPO_ROOT}/platform/android/source/AndroidHost.cpp"
)

target_include_directories(main PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/SDL2/include"
    "${REPO_ROOT}/source"
    "${REPO_ROOT}/libs"
    "${REPO_ROOT}/libs/z8lua"
    "${REPO_ROOT}/libs/lodepng"
    "${REPO_ROOT}/libs/simpleini"
    "${REPO_ROOT}/libs/miniz"
    "${REPO_ROOT}/platform/SDL2Common/source"
)

target_compile_definitions(main PRIVATE
    VER_STR="v1.00"
    LUA_USE_POSIX
)

if(PICOPLAY_PLAYSTORE_EDITION)
    target_compile_definitions(main PRIVATE PICOPLAY_PLAYSTORE_EDITION=1)
endif()

target_compile_options(main PRIVATE
    -O2
    -Wno-deprecated
    -Wno-format
    -fno-rtti
    -fexceptions
)

target_link_libraries(main
    SDL2
    android
    log
    GLESv1_CM
    GLESv2
    OpenSLES
    m
)
