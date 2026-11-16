# SDL Game Editor - CMake build.
#
# Works with any IDE that opens CMake projects (Visual Studio, VS Code, CLion, Qt Creator...)
# or from the command line:
#
#   cmake -S . -B out/build/release -DCMAKE_BUILD_TYPE=Release
#   cmake --build out/build/release
#
# The SDL3 family (SDL3, SDL3_image, SDL3_ttf, SDL3_mixer) is looked up first among the
# installed packages (CMAKE_PREFIX_PATH, SGE_DEPS_DIR, the system, vcpkg, Homebrew, MSYS2...).
# Whatever is not found is downloaded (SGE_FETCH_SDL, ON by default): on Windows, the
# official prebuilt packages into deps/ (tools/get_sdl.ps1, the same ones the .vcxproj
# uses); elsewhere, or if that fails, the sources from GitHub, built together with the
# project (the first configure takes a few minutes). See "Building and running" in
# README.md.
#
# sdl-game-editor.vcxproj stays as an alternative for Visual Studio 2026.

cmake_minimum_required(VERSION 3.24)
project(sdl-game-editor LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT CMAKE_CONFIGURATION_TYPES AND NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Debug CACHE STRING "Build type (Debug, Release, RelWithDebInfo)" FORCE)
endif()

# The executable and every DLL / shared library it needs end up in the same folder, so it
# can be started with a double click.
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
if(NOT WIN32)
    set(CMAKE_BUILD_RPATH "$ORIGIN")
    if(APPLE)
        set(CMAKE_BUILD_RPATH "@loader_path")
    endif()
endif()

# ---------------------------------------------------------------------------------------
# Dependencies: the SDL3 family
# ---------------------------------------------------------------------------------------

# Versions the project is tested with (also the ones downloaded when they are missing).
set(SGE_SDL_TAG       "release-3.4.14")
set(SGE_SDL_IMAGE_TAG "release-3.4.4")
set(SGE_SDL_TTF_TAG   "release-3.2.2")
set(SGE_SDL_MIXER_TAG "release-3.2.4")

option(SGE_FETCH_SDL "Download the SDL3 libraries that are not installed" ON)

# A folder with the unzipped SDL development packages (for example the VC or mingw zips
# from GitHub: deps/SDL3-3.4.14, deps/SDL3_image-3.4.4...). By default, deps/ in the repo.
if(NOT DEFINED SGE_DEPS_DIR AND DEFINED ENV{SGE_DEPS_DIR})
    set(SGE_DEPS_DIR "$ENV{SGE_DEPS_DIR}")
endif()
set(SGE_DEPS_DIR "${CMAKE_CURRENT_SOURCE_DIR}/deps" CACHE PATH
    "Folder that holds the unzipped SDL3, SDL3_image, SDL3_ttf and SDL3_mixer packages")
if(EXISTS "${SGE_DEPS_DIR}")
    # find_package also looks in <prefix>/<Name>*/cmake, so the package folders can keep
    # their version in the name.
    list(APPEND CMAKE_PREFIX_PATH "${SGE_DEPS_DIR}")
endif()

# Windows: if something is missing, the official prebuilt packages are downloaded into
# SGE_DEPS_DIR (much faster than building SDL; they also work with MinGW).
if(WIN32 AND SGE_FETCH_SDL AND CMAKE_SIZEOF_VOID_P EQUAL 8)
    find_package(SDL3 3.4 CONFIG QUIET COMPONENTS SDL3)
    find_package(SDL3_image 3.4 CONFIG QUIET)
    find_package(SDL3_ttf 3.2 CONFIG QUIET)
    find_package(SDL3_mixer 3.2 CONFIG QUIET)
    if(NOT (SDL3_FOUND AND SDL3_image_FOUND AND SDL3_ttf_FOUND AND SDL3_mixer_FOUND))
        message(STATUS "Downloading the prebuilt SDL3 packages into ${SGE_DEPS_DIR}")
        execute_process(
            COMMAND powershell -NoProfile -ExecutionPolicy Bypass
                    -File "${CMAKE_CURRENT_SOURCE_DIR}/tools/get_sdl.ps1" -Dest "${SGE_DEPS_DIR}"
            RESULT_VARIABLE _get_sdl_result)
        if(_get_sdl_result EQUAL 0)
            list(APPEND CMAKE_PREFIX_PATH "${SGE_DEPS_DIR}")
        else()
            message(STATUS "Could not download them: the sources will be built instead")
        endif()
    endif()
endif()

include(FetchContent)
set(FETCHCONTENT_QUIET OFF)

# Downloaded libraries: shared, without their samples and tests, and only with the formats
# that do not need extra libraries.
function(sge_fetch name repo tag submodules)
    message(STATUS "${name} not found: downloading ${repo} (${tag})")
    FetchContent_Declare(${name}
        GIT_REPOSITORY "https://github.com/libsdl-org/${repo}.git"
        GIT_TAG        "${tag}"
        GIT_SHALLOW    TRUE
        GIT_PROGRESS   TRUE
        GIT_SUBMODULES "${submodules}"
        EXCLUDE_FROM_ALL)
    FetchContent_MakeAvailable(${name})
endfunction()

function(sge_require name)
    if(NOT SGE_FETCH_SDL)
        message(FATAL_ERROR
            "${name} was not found and SGE_FETCH_SDL is OFF.\n"
            "Install it, unzip its development package into SGE_DEPS_DIR (${SGE_DEPS_DIR}) "
            "or point CMAKE_PREFIX_PATH at it, or configure with -DSGE_FETCH_SDL=ON.")
    endif()
endfunction()

# The libraries built here are SHARED (as the official packages are).
set(BUILD_SHARED_LIBS ON)

find_package(SDL3 3.4 CONFIG QUIET COMPONENTS SDL3)
if(SDL3_FOUND)
    message(STATUS "SDL3 found: ${SDL3_DIR}")
else()
    sge_require(SDL3)
    set(SDL_SHARED ON CACHE BOOL "" FORCE)
    set(SDL_STATIC OFF CACHE BOOL "" FORCE)
    set(SDL_TEST_LIBRARY OFF CACHE BOOL "" FORCE)
    set(SDL_EXAMPLES OFF CACHE BOOL "" FORCE)
    set(SDL_TESTS OFF CACHE BOOL "" FORCE)
    sge_fetch(SDL3 SDL "${SGE_SDL_TAG}" "")
endif()

find_package(SDL3_image 3.4 CONFIG QUIET)
if(SDL3_image_FOUND)
    message(STATUS "SDL3_image found: ${SDL3_image_DIR}")
else()
    sge_require(SDL3_image)
    # PNG and the other common formats come built in (stb_image); the ones that need
    # their own libraries are turned off.
    set(SDLIMAGE_VENDORED OFF CACHE BOOL "" FORCE)
    set(SDLIMAGE_SAMPLES OFF CACHE BOOL "" FORCE)
    set(SDLIMAGE_TESTS OFF CACHE BOOL "" FORCE)
    set(SDLIMAGE_AVIF OFF CACHE BOOL "" FORCE)
    set(SDLIMAGE_JXL OFF CACHE BOOL "" FORCE)
    set(SDLIMAGE_TIF OFF CACHE BOOL "" FORCE)
    set(SDLIMAGE_WEBP OFF CACHE BOOL "" FORCE)
    sge_fetch(SDL3_image SDL_image "${SGE_SDL_IMAGE_TAG}" "")
endif()

find_package(SDL3_ttf 3.2 CONFIG QUIET)
if(SDL3_ttf_FOUND)
    message(STATUS "SDL3_ttf found: ${SDL3_ttf_DIR}")
else()
    sge_require(SDL3_ttf)
    # FreeType is built from its copy inside SDL_ttf (the only submodule downloaded).
    set(SDLTTF_VENDORED ON CACHE BOOL "" FORCE)
    set(SDLTTF_HARFBUZZ OFF CACHE BOOL "" FORCE)
    set(SDLTTF_PLUTOSVG OFF CACHE BOOL "" FORCE)
    set(SDLTTF_SAMPLES OFF CACHE BOOL "" FORCE)
    set(SDLTTF_TESTS OFF CACHE BOOL "" FORCE)
    sge_fetch(SDL3_ttf SDL_ttf "${SGE_SDL_TTF_TAG}" "external/freetype")
endif()

find_package(SDL3_mixer 3.2 CONFIG QUIET)
if(SDL3_mixer_FOUND)
    message(STATUS "SDL3_mixer found: ${SDL3_mixer_DIR}")
else()
    sge_require(SDL3_mixer)
    # WAV, OGG Vorbis (stb_vorbis), FLAC and MP3 come built in; the formats that need their
    # own libraries (Opus, MOD, WavPack, GME, MIDI with FluidSynth) are turned off.
    set(SDLMIXER_VENDORED OFF CACHE BOOL "" FORCE)
    set(SDLMIXER_SAMPLES OFF CACHE BOOL "" FORCE)
    set(SDLMIXER_TESTS OFF CACHE BOOL "" FORCE)
    set(SDLMIXER_OPUS OFF CACHE BOOL "" FORCE)
    set(SDLMIXER_MOD_XMP OFF CACHE BOOL "" FORCE)
    set(SDLMIXER_WAVPACK OFF CACHE BOOL "" FORCE)
    set(SDLMIXER_GME OFF CACHE BOOL "" FORCE)
    set(SDLMIXER_MIDI_FLUIDSYNTH OFF CACHE BOOL "" FORCE)
    set(SDLMIXER_FLAC_LIBFLAC OFF CACHE BOOL "" FORCE)
    set(SDLMIXER_MP3_MPG123 OFF CACHE BOOL "" FORCE)
    set(SDLMIXER_VORBIS_VORBISFILE OFF CACHE BOOL "" FORCE)
    sge_fetch(SDL3_mixer SDL_mixer "${SGE_SDL_MIXER_TAG}" "")
endif()

# ---------------------------------------------------------------------------------------
# The executable
# ---------------------------------------------------------------------------------------

# Every .cpp in engine/ and game/ (Dear ImGui included): a file added to game/ is picked
# up on the next build without touching this file.
file(GLOB_RECURSE SGE_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/engine/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/game/*.cpp")
file(GLOB_RECURSE SGE_HEADERS CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/engine/*.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/engine/*.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/game/*.h")

add_executable(sdl-game-editor main.cpp ${SGE_SOURCES} ${SGE_HEADERS})
source_group(TREE "${CMAKE_CURRENT_SOURCE_DIR}" FILES main.cpp ${SGE_SOURCES} ${SGE_HEADERS})

target_include_directories(sdl-game-editor PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}"
    "${CMAKE_CURRENT_SOURCE_DIR}/engine"
    "${CMAKE_CURRENT_SOURCE_DIR}/engine/third_party")

target_link_libraries(sdl-game-editor PRIVATE
    SDL3_image::SDL3_image
    SDL3_ttf::SDL3_ttf
    SDL3_mixer::SDL3_mixer
    SDL3::SDL3)

if(MSVC)
    target_compile_options(sdl-game-editor PRIVATE /W3 /permissive-)
    target_compile_definitions(sdl-game-editor PRIVATE _CRT_SECURE_NO_WARNINGS)
else()
    target_compile_options(sdl-game-editor PRIVATE -Wall)
endif()

if(MINGW)
    # Without this the .exe also needs libstdc++, libgcc and libwinpthread from MinGW
    # (the SDL libraries are still DLLs).
    target_link_options(sdl-game-editor PRIVATE -static)
endif()

# The assets/ folder: the program looks for it in the working folder, next to the .exe
# and, as a last resort, in this repo (SGE_SOURCE_DIR), so running it from the build
# folder edits and saves the repo's assets/ (see findAssetsFolder in Application.cpp).
# For a folder that can be copied elsewhere, use "cmake --install" (below).
target_compile_definitions(sdl-game-editor PRIVATE
    SGE_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}")

# Visual Studio (solution generated by CMake): run from the repo.
set_property(DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}" PROPERTY VS_STARTUP_PROJECT sdl-game-editor)
set_target_properties(sdl-game-editor PROPERTIES
    VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}")

# Windows: the DLLs next to the .exe after each build. The official SDL3_image and
# SDL3_mixer packages keep the libraries of some formats (OGG, Opus, WebP...) in
# lib/<arch>/optional: they are loaded at run time, so they are copied too.
if(WIN32)
    set(SGE_OPTIONAL_DLLS "")
    if(CMAKE_SIZEOF_VOID_P EQUAL 4)
        set(_arch x86)
    else()
        set(_arch x64)
    endif()
    foreach(pkg SDL3_image SDL3_mixer)
        if(${pkg}_FOUND AND ${pkg}_DIR)
            file(GLOB _dlls "${${pkg}_DIR}/../lib/${_arch}/optional/*.dll")
            list(APPEND SGE_OPTIONAL_DLLS ${_dlls})
        endif()
    endforeach()

    add_custom_command(TARGET sdl-game-editor POST_BUILD
        COMMAND "${CMAKE_COMMAND}" -E copy_if_different
                "$<TARGET_RUNTIME_DLLS:sdl-game-editor>" ${SGE_OPTIONAL_DLLS}
                "$<TARGET_FILE_DIR:sdl-game-editor>"
        COMMAND_EXPAND_LISTS
        VERBATIM)
endif()

# cmake --install <build folder> --prefix <folder>: the program ready to zip or start with
# a double click (executable, libraries and a copy of assets/).
install(TARGETS sdl-game-editor RUNTIME DESTINATION .)
install(DIRECTORY assets DESTINATION .)
if(WIN32)
    install(FILES "$<TARGET_RUNTIME_DLLS:sdl-game-editor>" ${SGE_OPTIONAL_DLLS} DESTINATION .)
else()
    # The SDL libraries built here (the installed ones come from the system).
    if(APPLE)
        set_target_properties(sdl-game-editor PROPERTIES INSTALL_RPATH "@loader_path")
    else()
        set_target_properties(sdl-game-editor PROPERTIES INSTALL_RPATH "$ORIGIN")
    endif()
    install(DIRECTORY "${CMAKE_BINARY_DIR}/bin/" DESTINATION .
        FILES_MATCHING PATTERN "*.so*" PATTERN "*.dylib")
endif()
