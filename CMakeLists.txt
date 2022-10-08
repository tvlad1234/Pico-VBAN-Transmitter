cmake_minimum_required(VERSION 3.12)

include(pico_sdk_import.cmake)
project(Pico-VBAN-Transmitter)
set(target_name Pico-VBAN-Transmitter)

pico_sdk_init()

add_subdirectory(Pico-10BASE-T-Library)

add_executable(${target_name})

target_sources(${target_name} PRIVATE
    main.c
    vban.c
    )

target_link_libraries(${target_name} PRIVATE
    pico_stdlib
    hardware_irq
    hardware_adc
    hardware_dma
    hardware_vreg
    10baset
    )

# enable usb output, disable uart output
pico_enable_stdio_usb(${target_name} 0)
pico_enable_stdio_uart(${target_name} 0)

pico_set_binary_type(${target_name} copy_to_ram)

pico_add_extra_outputs(${target_name})

