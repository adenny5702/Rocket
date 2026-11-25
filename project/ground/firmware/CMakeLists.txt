cmake_minimum_required(VERSION 3.20)

set(PICO_SDK_PATH "${ROOT_DIR}/third_party/pico-sdk")
include("${PICO_SDK_PATH}/pico_sdk_init.cmake")

project(GroundFirmware LANGUAGES C CXX ASM)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

pico_sdk_init()

add_subdirectory(${ROOT_DIR}/third_party/RadioLib RadioLib)
add_subdirectory(${ROOT_DIR}/project/shared/protocol protocol)

add_executable(GroundFirmware 
    src/main.cpp
    "${ROOT_DIR}/third_party/RadioLib/src/hal/RPiPico/PicoHal.cpp"
)

pico_enable_stdio_uart(GroundFirmware 0)
pico_enable_stdio_usb(GroundFirmware 1)

target_compile_definitions(GroundFirmware PUBLIC RADIOLIB_BUILD_RPI_PICO)
target_link_libraries(GroundFirmware 
    PRIVATE 
    pico_stdlib 
    hardware_spi
    hardware_gpio
    hardware_timer
    pico_multicore
    hardware_pwm
    RadioLib
    protocol
)

pico_add_uf2_output(GroundFirmware)