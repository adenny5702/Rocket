include(ExternalProject)


if(BUILD_AIR)
    ExternalProject_Add(air_firmware
        SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/air/firmware"
        BINARY_DIR "${CMAKE_CURRENT_BINARY_DIR}/air-firmware-build"
        CMAKE_ARGS 
            -DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE}
            -DROOT_DIR=${CMAKE_SOURCE_DIR}
            -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
        INSTALL_COMMAND ""
        BUILD_ALWAYS TRUE
    )
endif()

if(BUILD_GROUND)
    ExternalProject_Add(ground_firmware
        SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/ground/firmware"
        BINARY_DIR "${CMAKE_CURRENT_BINARY_DIR}/ground-firmware-build"
        CMAKE_ARGS 
            -DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE}
            -DROOT_DIR=${CMAKE_SOURCE_DIR}
            -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
        INSTALL_COMMAND ""
        BUILD_ALWAYS TRUE
    )
endif()

if(BUILD_APP)
    ExternalProject_Add(ground_app
        SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/ground/app"
        BINARY_DIR "${CMAKE_CURRENT_BINARY_DIR}/ground-app-build"
        CMAKE_ARGS 
            -DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE}
            -DROOT_DIR=${CMAKE_SOURCE_DIR}
            -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
        INSTALL_COMMAND ""
        BUILD_ALWAYS TRUE
    )
endif()