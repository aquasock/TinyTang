cmake_minimum_required(VERSION 3.15)

include(proj.conf)

find_package(bouffalo_sdk REQUIRED HINTS $ENV{BL_SDK_BASE})

# Our code only.  TinyDesk Shell is consumed as the third_party/tinydesk-shell
# submodule: its portable core is compiled here unchanged, its include tree is
# on the path, and its platform API, filesystem and console are implemented
# under ports/bl616.
sdk_add_include_directories(. ports/bl616 third_party/tinydesk-shell/include)

# TinyDesk Shell's core is C11 (it uses max_align_t) while the SDK compiles C
# as gnu99, so raise the standard for this project.  This is the one flag the
# port needs; the core itself is untouched.
sdk_add_compile_options(-std=gnu11)

file(GLOB TINYTANG_SOURCES "ports/bl616/*.c")
file(GLOB TDSH_CORE_SOURCES "third_party/tinydesk-shell/src/core/*.c")

target_sources(app PRIVATE ${TINYTANG_SOURCES} ${TDSH_CORE_SOURCES})

sdk_set_main_file(main.cpp)

project(tinytang)
