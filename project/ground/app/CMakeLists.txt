cmake_minimum_required(VERSION 3.20)

project(GroundApp LANGUAGES C CXX ASM)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(BUILD_GAMES    OFF CACHE BOOL "" FORCE)

add_subdirectory(${ROOT_DIR}/third_party/raylib raylib)

add_executable(GroundApp src/main.cpp)
target_link_libraries(GroundApp PRIVATE raylib)