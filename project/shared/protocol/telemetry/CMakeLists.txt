add_library(protocol_telemetry INTERFACE)

target_include_directories(protocol_telemetry
    INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)