cmake_minimum_required(VERSION 3.20)

set(PICO_SDK_PATH "${ROOT_DIR}/third_party/pico-sdk")
include("${PICO_SDK_PATH}/pico_sdk_init.cmake")

project(AirFirmware LANGUAGES C CXX ASM)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

pico_sdk_init()

add_subdirectory(${ROOT_DIR}/third_party/RadioLib RadioLib)
add_subdirectory(${ROOT_DIR}/third_party/pico-ICM45686 pico-ICM45686)
add_subdirectory(${ROOT_DIR}/project/shared/protocol protocol)

add_executable(AirFirmware 
    src/main.cpp
    "${ROOT_DIR}/third_party/RadioLib/src/hal/RPiPico/PicoHal.cpp"
)

pico_enable_stdio_uart(AirFirmware 0)
pico_enable_stdio_usb(AirFirmware 1)

target_compile_definitions(AirFirmware PUBLIC RADIOLIB_BUILD_RPI_PICO)
target_link_libraries(AirFirmware 
    PRIVATE 
    pico_stdlib
    pico_multicore
    hardware_spi
    hardware_gpio
    hardware_timer
    pico_multicore
    hardware_pwm
    RadioLib
    pico_icm45686
    protocol
)

pico_add_uf2_output(AirFirmware)