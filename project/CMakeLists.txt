include(ExternalProject)

function(add_rocket_project name source_dir build_dir)
    ExternalProject_Add(${name}
        SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/${source_dir}"
        BINARY_DIR "${CMAKE_CURRENT_BINARY_DIR}/${build_dir}"
        CMAKE_ARGS
            -DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE}
            -DROOT_DIR=${CMAKE_SOURCE_DIR}
            -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
        INSTALL_COMMAND ""
        BUILD_ALWAYS TRUE
    )
endfunction()

if(BUILD_AIR)
    add_rocket_project(air_firmware air/firmware air-firmware-build)
endif()

if(BUILD_GROUND)
    add_rocket_project(ground_firmware ground/firmware ground-firmware-build)
endif()

if(BUILD_APP)
    add_rocket_project(ground_app ground/app ground-app-build)
endif()
