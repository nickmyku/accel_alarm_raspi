cmake_minimum_required(VERSION 3.13)

# Generic RP2040. The binary is far smaller than the 2 MB link region.
# The 1.28-B board itself has 16 MB of flash.
set(PICO_BOARD pico)

include(pico_sdk_import.cmake)

project(accel_graph C CXX ASM)
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

pico_sdk_init()

add_executable(accel_graph
    src/main.c
    src/trace.c
    src/ui.c
    src/font5x7.c
    src/gc9a01.c
    src/qmi8658.c
    src/cst816s.c
    src/settings.c
)

target_include_directories(accel_graph PRIVATE
    ${CMAKE_CURRENT_LIST_DIR}/src
    ${CMAKE_CURRENT_LIST_DIR}
)

target_compile_definitions(accel_graph PRIVATE
    PICO_STDIO_USB_CONNECT_WAIT_TIMEOUT_MS=0
)

target_compile_options(accel_graph PRIVATE -Wall -Wextra -Werror)

target_link_libraries(accel_graph
    pico_stdlib
    pico_multicore
    hardware_spi
    hardware_i2c
    hardware_pwm
    hardware_flash
    hardware_sync
)

# USB console only. UART0 would claim GPIO0, which is the alarm pin.
pico_enable_stdio_usb(accel_graph 1)
pico_enable_stdio_uart(accel_graph 0)

pico_add_extra_outputs(accel_graph)
