cmake_minimum_required(VERSION 3.15)

include(proj.conf)

find_package(bouffalo_sdk REQUIRED HINTS $ENV{BL_SDK_BASE})

# Our code only.  TinyDesk Shell is consumed as the third_party/tinydesk-shell
# submodule; its portable core is compiled in below and its platform API is
# implemented under ports/bl616.
sdk_add_include_directories(. ports/bl616)

file(GLOB TINYTANG_SOURCES "ports/bl616/*.c")

target_sources(app PRIVATE ${TINYTANG_SOURCES})

sdk_set_main_file(main.cpp)

project(tinytang)
