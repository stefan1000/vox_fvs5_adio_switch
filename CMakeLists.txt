cmake_minimum_required(VERSION 3.13)
set(BOARD pico_sdk)
include(pico_sdk_import.cmake)

project(vox_vfs5_adio_switch C CXX ASM)
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)
pico_sdk_init()

add_executable(vox_vfs5_adio_switch
    vox_vfs5_adio_switch.c vfs5_input.c
)

target_include_directories(vox_vfs5_adio_switch PUBLIC ${CMAKE_CURRENT_LIST_DIR})

target_link_libraries(vox_vfs5_adio_switch tinyusb_host tinyusb_board pico_stdlib hardware_adc hardware_dma)

pico_add_extra_outputs(vox_vfs5_adio_switch)

