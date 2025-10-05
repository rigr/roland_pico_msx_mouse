cmake_minimum_required(VERSION 3.13)
include(pico_sdk_import.cmake)
project(pico_roland_mouse C CXX ASM)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

pico_sdk_init()

add_executable(pico_roland_mouse
    src/main.cpp
)

target_compile_definitions(pico_roland_mouse PRIVATE
    PICO_TUSB_HOST=1
)

target_include_directories(pico_roland_mouse PRIVATE
    ${CMAKE_CURRENT_LIST_DIR}/src
    ${PICO_SDK_PATH}/lib/tinyusb/src   # <-- neu: TinyUSB Host Header sichtbar machen
)


target_link_libraries(pico_roland_mouse
    pico_stdlib
    hardware_gpio
    tinyusb_host
    tinyusb_board
)

pico_add_extra_outputs(pico_roland_mouse)
