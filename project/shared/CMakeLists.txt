add_library(rocket_shared INTERFACE)
add_library(rocket::shared ALIAS rocket_shared)

target_include_directories(rocket_shared
    INTERFACE
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>"
)
