cmake_minimum_required(VERSION 3.16)
project(battlezero VERSION 0.3.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()
add_compile_options(-Wall -Wextra)

add_library(bzcore STATIC
  src/core/raster.cpp src/core/tilt.cpp src/core/models.cpp src/core/world.cpp
  src/core/enemy.cpp src/core/game.cpp src/core/render.cpp src/core/sound.cpp)
target_include_directories(bzcore PUBLIC src/core)

# Headless tools: tests and scripted frame dumps. Built everywhere.
enable_testing()
add_executable(bz-tests tests/test_core.cpp)
target_link_libraries(bz-tests bzcore)
add_test(NAME core COMMAND bz-tests)

add_executable(bz-frames tools/frames.cpp)
target_link_libraries(bz-frames bzcore)

if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
  # The CardputerZero build: framebuffer, evdev keys, IIO motion sensor.
  add_executable(battlezero src/platform/dev_main.cpp)
  find_package(Threads REQUIRED)
  # libstdc++ is linked in so the package only depends on the C library.
  target_link_libraries(battlezero bzcore Threads::Threads ${CMAKE_DL_LIBS} -static-libstdc++ -static-libgcc)
else()
  find_package(SDL2 QUIET)
  if(SDL2_FOUND)
    add_executable(battlezero-sim src/platform/sim_sdl.cpp)
    target_link_libraries(battlezero-sim bzcore SDL2::SDL2)
  else()
    message(STATUS "SDL2 not found: the simulator will not be built")
  endif()
endif()
