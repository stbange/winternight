cmake_minimum_required(VERSION 3.25)
project(winternight VERSION 0.1.0 LANGUAGES C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(WN_BUILD_GAME "Build the game executable (needs SDL3)" ON)
option(WN_BUILD_TESTS "Build unit tests" ON)
option(WN_COVERAGE "Instrument the core for coverage" OFF)
option(WN_SANITIZE "Build with address and undefined-behaviour sanitizers" OFF)
option(WN_WERROR "Treat warnings as errors" ON)
set(WN_VOXEL_SIZE "0.1f" CACHE STRING "Voxel edge length in metres (provisional, design D15)")
set(WN_SDL3_SOURCE_DIR "" CACHE PATH "Local SDL3 source tree (otherwise fetched from GitHub)")

# ---------------------------------------------------------------- warnings
if(MSVC)
  # /W3 with conversions silenced (float maths) and unsigned negation allowed (PCG32).
  set(WN_WARN /W3 /wd4244 /wd4267 /wd4305 /wd4996 /wd4146 /wd4018)
  if(WN_WERROR)
    list(APPEND WN_WARN /WX)
  endif()
  add_compile_options(/fp:precise)
else()
  set(WN_WARN -Wall -Wextra -Wshadow -Wno-unused-parameter -Wno-missing-field-initializers)
  if(WN_WERROR)
    list(APPEND WN_WARN -Werror)
  endif()
  # Determinism (design D7): no fused multiply-add contraction, no fast-math.
  add_compile_options(-ffp-contract=off -fno-fast-math)
endif()

if(WN_SANITIZE AND NOT MSVC)
  add_compile_options(-fsanitize=address,undefined -fno-omit-frame-pointer -fno-sanitize-recover=all)
  add_link_options(-fsanitize=address,undefined)
endif()

# ---------------------------------------------------------------- core library
# engine/ + game/ form wn_core: pure C, no SDL, no OpenGL, no Emscripten (design D1).
file(GLOB WN_ENGINE_SOURCES CONFIGURE_DEPENDS ${CMAKE_SOURCE_DIR}/engine/src/*.c)
file(GLOB WN_GAME_SOURCES CONFIGURE_DEPENDS ${CMAKE_SOURCE_DIR}/game/src/*.c)
add_library(wn_core STATIC ${WN_ENGINE_SOURCES} ${WN_GAME_SOURCES})
target_include_directories(wn_core PUBLIC engine/include game/include
                                       PRIVATE third_party/xxhash third_party/stb)
target_compile_definitions(wn_core PUBLIC WN_VOXEL_SIZE=${WN_VOXEL_SIZE})
set_target_properties(wn_core PROPERTIES C_EXTENSIONS OFF) # strict ISO C11 for the portable core
target_compile_options(wn_core PRIVATE ${WN_WARN})
if(NOT MSVC)
  target_link_libraries(wn_core PUBLIC m)
endif()
if(WN_COVERAGE AND NOT MSVC)
  target_compile_options(wn_core PRIVATE --coverage -O0 -g)
  target_link_options(wn_core PUBLIC --coverage)
endif()

# ---------------------------------------------------------------- tools
if(NOT EMSCRIPTEN)
  # GPU-free simulation runner for determinism checks (any CI machine).
  add_executable(wn_simtest tools/wn_simtest.c)
  target_link_libraries(wn_simtest PRIVATE wn_core)
  target_compile_options(wn_simtest PRIVATE ${WN_WARN})
  target_compile_definitions(wn_simtest PRIVATE WN_DATA_DIR="${CMAKE_SOURCE_DIR}/game/data")
endif()

# ---------------------------------------------------------------- game
if(WN_BUILD_GAME)
  if(EMSCRIPTEN AND NOT WN_SDL3_SOURCE_DIR)
    set(WN_SDL_TARGET "")  # Emscripten's SDL3 port (--use-port=sdl3)
  elseif(WN_SDL3_SOURCE_DIR)
    set(SDL_SHARED OFF CACHE BOOL "" FORCE)
    set(SDL_STATIC ON CACHE BOOL "" FORCE)
    set(SDL_TEST_LIBRARY OFF CACHE BOOL "" FORCE)
    add_subdirectory(${WN_SDL3_SOURCE_DIR} ${CMAKE_BINARY_DIR}/SDL3 EXCLUDE_FROM_ALL)
    set(WN_SDL_TARGET SDL3::SDL3-static)
  else()
    include(FetchContent)
    set(SDL_SHARED OFF CACHE BOOL "" FORCE)
    set(SDL_STATIC ON CACHE BOOL "" FORCE)
    set(SDL_TEST_LIBRARY OFF CACHE BOOL "" FORCE)
    FetchContent_Declare(SDL3
      GIT_REPOSITORY https://github.com/libsdl-org/SDL.git
      GIT_TAG release-3.4.20
      GIT_SHALLOW TRUE)
    FetchContent_MakeAvailable(SDL3)
    set(WN_SDL_TARGET SDL3::SDL3-static)
  endif()

  file(GLOB WN_PLATFORM_SOURCES CONFIGURE_DEPENDS
       ${CMAKE_SOURCE_DIR}/platform/sdl/*.c ${CMAKE_SOURCE_DIR}/platform/gl/*.c)
  add_executable(winternight ${WN_PLATFORM_SOURCES})
  # Platform glue may use compiler extensions (EM_JS on the web).
  set_target_properties(winternight PROPERTIES C_EXTENSIONS ON)
  target_include_directories(winternight PRIVATE platform third_party/stb)
  target_compile_options(winternight PRIVATE ${WN_WARN})
  target_link_libraries(winternight PRIVATE wn_core ${WN_SDL_TARGET})
  if(NOT EMSCRIPTEN)
    target_compile_definitions(winternight PRIVATE WN_DATA_DIR="${CMAKE_SOURCE_DIR}/game/data")
  endif()

  if(EMSCRIPTEN)
    target_compile_definitions(winternight PRIVATE WN_DATA_DIR="/data")
    if(NOT WN_SDL3_SOURCE_DIR)
      target_compile_options(winternight PRIVATE --use-port=sdl3)
      target_link_options(winternight PRIVATE --use-port=sdl3)
    endif()
    target_link_options(winternight PRIVATE
      -sMIN_WEBGL_VERSION=2 -sMAX_WEBGL_VERSION=2
      -sALLOW_MEMORY_GROWTH=1 -sMAXIMUM_MEMORY=384MB -sINITIAL_MEMORY=96MB
      -sEXPORTED_FUNCTIONS=_main,_wn_web_status,_wn_xr_session_started,_wn_xr_session_ended,_wn_xr_frame,_malloc,_free
      -sEXPORTED_RUNTIME_METHODS=ccall,cwrap,HEAPF32,UTF8ToString
      "SHELL:--js-library ${CMAKE_SOURCE_DIR}/platform/web/wn_web.js"
      "SHELL:--preload-file ${CMAKE_SOURCE_DIR}/game/data@/data"
      "SHELL:--preload-file ${CMAKE_SOURCE_DIR}/tests/game/scripts@/tests"
      -sEXIT_RUNTIME=0
      "SHELL:--shell-file ${CMAKE_SOURCE_DIR}/platform/web/shell.html"
      )
    set_target_properties(winternight PROPERTIES SUFFIX ".html")
  endif()
endif()

# ---------------------------------------------------------------- tests
if(WN_BUILD_TESTS AND NOT EMSCRIPTEN)
  enable_testing()
  add_library(unity STATIC third_party/unity/unity.c)
  target_include_directories(unity PUBLIC third_party/unity)
  target_compile_definitions(unity PUBLIC UNITY_INCLUDE_DOUBLE)
  file(GLOB WN_TEST_SOURCES CONFIGURE_DEPENDS ${CMAKE_SOURCE_DIR}/tests/unit/test_*.c)
  foreach(src ${WN_TEST_SOURCES})
    get_filename_component(name ${src} NAME_WE)
    add_executable(${name} ${src})
    target_link_libraries(${name} PRIVATE wn_core unity)
    target_include_directories(${name} PRIVATE tests/unit third_party/stb)
    target_compile_definitions(${name} PRIVATE WN_DATA_DIR="${CMAKE_SOURCE_DIR}/game/data"
                                               WN_TEST_DIR="${CMAKE_SOURCE_DIR}/tests"
                                               WN_TEST_TMP="${CMAKE_BINARY_DIR}")
    target_compile_options(${name} PRIVATE ${WN_WARN})
    set_target_properties(${name} PROPERTIES C_EXTENSIONS OFF)
    add_test(NAME ${name} COMMAND ${name})
    set_tests_properties(${name} PROPERTIES WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
  endforeach()
endif()

# Determinism check through the GPU-free runner (same checksums on every target).
if(WN_BUILD_TESTS AND NOT EMSCRIPTEN)
  add_test(NAME sim_determinism COMMAND wn_simtest ${CMAKE_SOURCE_DIR}/tests/game/scripts/determinism.wntest)
endif()
