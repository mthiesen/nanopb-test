cmake_minimum_required(VERSION 3.13)

include(../pico_sdk_import.cmake)

project(v1-added_fields LANGUAGES C CXX ASM)
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

pico_sdk_init()

add_subdirectory(../lib lib)

add_executable(${PROJECT_NAME}
main.cpp
../common/settings.cpp
proto/settings.pb.c
)

pico_enable_stdio_usb(${PROJECT_NAME} 1)
pico_enable_stdio_uart(${PROJECT_NAME} 0)

target_include_directories(${PROJECT_NAME} PUBLIC
../common/
proto/
)

target_link_libraries(${PROJECT_NAME}
  pico_stdlib
  FlashPROM
  nanopb
)

pico_add_extra_outputs(${PROJECT_NAME})
