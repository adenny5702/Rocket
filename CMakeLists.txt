cmake_minimum_required(VERSION 3.20)

project(Rocket LANGUAGES NONE)

set(CMAKE_PROJECT_VERSION_MAJOR 0)
set(CMAKE_PROJECT_VERSION_MINOR 0)
set(CMAKE_PROJECT_VERSION_PATCH 0)
set(CMAKE_PROJECT_VERSION ${CMAKE_PROJECT_VERSION_MAJOR}.${CMAKE_PROJECT_VERSION_MINOR}.${CMAKE_PROJECT_VERSION_PATCH})

option(BUILD_AIR "Build the firmware for the rocket" OFF)
option(BUILD_GROUND "Build the firmware for the ground station" OFF)
option(BUILD_APP "Build the app for the ground station" OFF)

add_subdirectory(project)
