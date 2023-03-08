cmake_minimum_required(VERSION 3.13)

include(../pico_sdk_import.cmake)
include(../gen_settings_proto.cmake)

project(v0-base LANGUAGES C CXX ASM)
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

pico_sdk_init()
gen_settings_proto()

add_subdirectory(../lib lib)

add_executable(${PROJECT_NAME}
main.cpp
${PROTO_OUTPUT_DIR}/settings.pb.c
../common/settings.cpp
)

pico_enable_stdio_usb(${PROJECT_NAME} 1)
pico_enable_stdio_uart(${PROJECT_NAME} 0)

target_include_directories(${PROJECT_NAME} PUBLIC
../common/
${PROTO_OUTPUT_DIR}
)

target_link_libraries(${PROJECT_NAME}
  pico_stdlib
  FlashPROM
  nanopb
)

pico_add_extra_outputs(${PROJECT_NAME})
