# This is a copy of the Raspberry Pi Pico SDK import helper, trimmed to
# the path this project needs. Set PICO_SDK_PATH to the SDK checkout.

if (DEFINED ENV{PICO_SDK_PATH} AND (NOT PICO_SDK_PATH))
    set(PICO_SDK_PATH $ENV{PICO_SDK_PATH})
    message("Using PICO_SDK_PATH from environment ('${PICO_SDK_PATH}')")
endif()

set(PICO_SDK_PATH "${PICO_SDK_PATH}" CACHE PATH "Path to the Raspberry Pi Pico SDK")

if (NOT PICO_SDK_PATH)
    message(FATAL_ERROR
        "PICO_SDK_PATH is not set. Point it at a checkout of raspberrypi/pico-sdk.")
endif()

get_filename_component(PICO_SDK_PATH "${PICO_SDK_PATH}" REALPATH BASE_DIR "${CMAKE_BINARY_DIR}")
if (NOT EXISTS ${PICO_SDK_PATH}/pico_sdk_init.cmake)
    message(FATAL_ERROR "Directory '${PICO_SDK_PATH}' does not contain the Pico SDK")
endif()

include(${PICO_SDK_PATH}/pico_sdk_init.cmake)
