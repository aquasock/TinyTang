cmake_minimum_required(VERSION 3.15)

# Diagnostic: drop the newlib/stdio port without touching anything else.
if(DEFINED ENV{TINYTANG_MIN})
    include(proj.min.conf)
elseif(DEFINED ENV{TINYTANG_NONEWLIB})
    include(proj.nonewlib.conf)
else()
    include(proj.conf)
endif()

find_package(bouffalo_sdk REQUIRED HINTS $ENV{BL_SDK_BASE})

sdk_add_include_directories(. ports/bl616 third_party/tinydesk-shell/include)

# TinyDesk Shell's core is C11 (it uses max_align_t) while the SDK compiles C
# as gnu99, so raise the standard for this project.  This is the one flag the
# port needs; the core itself is untouched.
sdk_add_compile_options(-std=gnu11)

# Diagnostic: build without the scheduler and shell, to isolate the USB
# bring-up from everything above it.  Selected by the environment so it does
# not disturb the SDK's own CMake definitions.
if(DEFINED ENV{TINYTANG_USB_ONLY})
    sdk_add_compile_definitions(-DTINYTANG_USB_ONLY=1)
endif()

if(DEFINED ENV{TINYTANG_REF})
    # Reference build: the SDK's own device CDC example, unmodified, compiled
    # in this project with this proj.conf.  It answers whether the failure is
    # in our USB code or in the project configuration, because nothing else
    # differs.
    target_sources(app PRIVATE ref/cdc_acm_template.c)
    sdk_set_main_file(ref/ref_main.c)
else()
    # Our code only.  TinyDesk Shell is consumed as the
    # third_party/tinydesk-shell submodule: its portable core is compiled here
    # unchanged, its include tree is on the path, and its platform API,
    # filesystem and console are implemented under ports/bl616.
    file(GLOB TINYTANG_SOURCES "ports/bl616/*.c")
    file(GLOB TDSH_CORE_SOURCES "third_party/tinydesk-shell/src/core/*.c")
    target_sources(app PRIVATE ${TINYTANG_SOURCES} ${TDSH_CORE_SOURCES})
    sdk_set_main_file(main.cpp)
endif()

project(tinytang)
