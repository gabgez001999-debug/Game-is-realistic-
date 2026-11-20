cmake_minimum_required(VERSION 3.20)
project(GameIsRealistic VERSION 0.1.0 LANGUAGES C CXX)

# ---------------------------------------------------------------------------
#  Game-is-realistic  -  built on the Veritas Engine (C++20, OpenGL 4.1 core)
# ---------------------------------------------------------------------------

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

option(VERITAS_STREAMING "Build the cloud streaming server (Linux, needs FFmpeg + EGL)" ON)

find_package(PkgConfig QUIET)

# --- SDL2 (window, input, audio device) -------------------------------------
find_package(SDL2 QUIET CONFIG)
if(NOT SDL2_FOUND AND PKG_CONFIG_FOUND)
  pkg_check_modules(SDL2 REQUIRED IMPORTED_TARGET sdl2)
  add_library(SDL2::SDL2 ALIAS PkgConfig::SDL2)
endif()

# --- GLM (math) --------------------------------------------------------------
find_package(glm QUIET CONFIG)
if(NOT glm_FOUND)
  find_path(GLM_INCLUDE_DIR glm/glm.hpp PATHS /opt/homebrew/include /usr/local/include)
  if(NOT GLM_INCLUDE_DIR)
    include(FetchContent)
    FetchContent_Declare(glm GIT_REPOSITORY https://github.com/g-truc/glm.git GIT_TAG 1.0.1)
    FetchContent_MakeAvailable(glm)
  else()
    add_library(glm_headers INTERFACE)
    target_include_directories(glm_headers INTERFACE ${GLM_INCLUDE_DIR})
    add_library(glm::glm ALIAS glm_headers)
  endif()
endif()

# --- Streaming deps (Linux only) --------------------------------------------
set(HAVE_STREAMING OFF)
if(VERITAS_STREAMING AND UNIX AND NOT APPLE AND PKG_CONFIG_FOUND)
  pkg_check_modules(FFMPEG IMPORTED_TARGET libavcodec libavutil)
  pkg_check_modules(EGL IMPORTED_TARGET egl)
  if(FFMPEG_FOUND AND EGL_FOUND)
    set(HAVE_STREAMING ON)
  endif()
endif()
message(STATUS "Veritas cloud streaming: ${HAVE_STREAMING}")

# --- Engine -----------------------------------------------------------------
file(GLOB VERITAS_CORE     engine/core/*.cpp)
file(GLOB VERITAS_PLATFORM engine/platform/*.cpp)
file(GLOB VERITAS_GFX      engine/gfx/*.cpp)
file(GLOB VERITAS_PHYSICS  engine/physics/*.cpp)
file(GLOB VERITAS_AUDIO    engine/audio/*.cpp)
set(VERITAS_SOURCES ${VERITAS_CORE} ${VERITAS_PLATFORM} ${VERITAS_GFX} ${VERITAS_PHYSICS} ${VERITAS_AUDIO})
if(NOT HAVE_STREAMING)
  list(FILTER VERITAS_SOURCES EXCLUDE REGEX ".*HeadlessPlatform\\.cpp$")
endif()
if(HAVE_STREAMING)
  file(GLOB VERITAS_STREAM engine/stream/*.cpp)
  list(APPEND VERITAS_SOURCES ${VERITAS_STREAM})
endif()

add_library(veritas STATIC ${VERITAS_SOURCES})
target_include_directories(veritas PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/engine)
target_link_libraries(veritas PUBLIC SDL2::SDL2 glm::glm)
target_compile_definitions(veritas PUBLIC GLM_FORCE_RADIANS GLM_ENABLE_EXPERIMENTAL
  VERITAS_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}")
if(HAVE_STREAMING)
  target_compile_definitions(veritas PUBLIC VERITAS_HAS_STREAMING=1)
  target_link_libraries(veritas PUBLIC PkgConfig::FFMPEG PkgConfig::EGL)
endif()
if(APPLE)
  target_compile_definitions(veritas PUBLIC GL_SILENCE_DEPRECATION)
  target_link_libraries(veritas PUBLIC "-framework OpenGL")
endif()
find_package(Threads REQUIRED)
target_link_libraries(veritas PUBLIC Threads::Threads)

if(MSVC)
  target_compile_options(veritas PUBLIC /W3 /permissive- /fp:fast /O2)
else()
  target_compile_options(veritas PUBLIC -Wall -Wno-unused-parameter -Wno-missing-field-initializers
    $<$<CONFIG:Release>:-O2 -ffast-math>)
endif()

# --- Game -------------------------------------------------------------------
file(GLOB GAME_SOURCES game/*.cpp)
add_executable(realistic ${GAME_SOURCES})
target_link_libraries(realistic PRIVATE veritas)
target_include_directories(realistic PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/game)
