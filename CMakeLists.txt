cmake_minimum_required(VERSION 3.15)

# Diagnostic: drop the newlib/stdio port without touching anything else.
if(DEFINED ENV{TINYTANG_MIN})
    include(proj.min.conf)
elseif(DEFINED ENV{TINYTANG_RTOS})
    include(proj.rtos.conf)
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

# Diagnostic: skip the SD card mount, to tell card init apart from the USB
# stack and the shell.
if(DEFINED ENV{TINYTANG_NO_FS})
    sdk_add_compile_definitions(-DTINYTANG_NO_FS=1)
endif()

# Diagnostic: run the scheduler and the USB console but never initialise the
# shell, to tell a shell fault from a console fault.
if(DEFINED ENV{TINYTANG_NO_SHELL})
    sdk_add_compile_definitions(-DTINYTANG_NO_SHELL=1)
endif()

if(DEFINED ENV{TINYTANG_REF} OR DEFINED ENV{TINYTANG_RTOS})
    # Reference build: the SDK's own device CDC example, unmodified, compiled
    # in this project with this proj.conf.  It answers whether the failure is
    # in our USB code or in the project configuration, because nothing else
    # differs.
    target_sources(app PRIVATE ref/cdc_acm_template.c)
    if(DEFINED ENV{TINYTANG_RTOS})
        sdk_set_main_file(ref/ref_rtos_main.c)
    else()
        sdk_set_main_file(ref/ref_main.c)
    endif()
else()
    # Our code only.  TinyDesk Shell is consumed as the
    # third_party/tinydesk-shell submodule: its portable core is compiled here
    # unchanged, its include tree is on the path, and its platform API,
    # filesystem and console are implemented under ports/bl616.
    file(GLOB TINYTANG_SOURCES "ports/bl616/*.c")
    file(GLOB TDSH_CORE_SOURCES "third_party/tinydesk-shell/src/core/*.c")

    # TinyDesk, the desktop environment.  Its core and apps are compiled here
    # against the shell we already have -- TinyDesk pins the same revision
    # (232a39f), so there is no second copy to keep in step.  The filesystem
    # comes from its own ports/common/td_fs_stdio.c, which is written against
    # stdio, <dirent.h> and <sys/stat.h> and therefore lands on this port's
    # FatFS shim unmodified.
    #
    # The geometry is the output of TD_MAX_COLS x TD_MAX_ROWS x 8 bytes, two
    # buffers: 100x30 costs about 48 KB for the screens and 25 KB for the
    # terminal view, against the BL616's 320 KB of OCRAM.
    file(GLOB TINYDESK_CORE_SOURCES "third_party/tinydesk/src/*.c")

    # Only the local apps.  TinyDesk's own apps/apps.c registers Network,
    # MQTT, Modbus and the OTA updater as well, and those sit on
    # proto/td_sock.c and proto/td_tls.c -- POSIX sockets, esp_timer, and
    # mbedTLS with esp_crt_bundle.  None of that exists on the BL616, so
    # apps.c, mqtt.c, modbus.c, network.c and update.c stay out and this port
    # supplies td_apps_register_all() itself.  desktop.c is the window
    # manager's own app; the rest are local and use the card.
    set(TINYDESK_APP_SOURCES
        third_party/tinydesk/apps/about.c
        third_party/tinydesk/apps/counter.c
        third_party/tinydesk/apps/datetime.c
        third_party/tinydesk/apps/desktop.c
        third_party/tinydesk/apps/editor.c
        third_party/tinydesk/apps/files.c
        third_party/tinydesk/apps/logview.c
        third_party/tinydesk/apps/session.c
        third_party/tinydesk/apps/settings.c
        third_party/tinydesk/apps/sysmon.c
        third_party/tinydesk/apps/taskmgr.c
        third_party/tinydesk/apps/terminal.c)
    sdk_add_include_directories(
        third_party/tinydesk/include
        third_party/tinydesk/apps
        third_party/tinydesk/ports/common)
    sdk_add_compile_definitions(
        -DTD_MAX_COLS=80 -DTD_MAX_ROWS=45
        -DTD_VT_SCROLLBACK=12 -DTD_MAX_WIDGETS=96)

    # The shell prints with printf; with CONFIG_NEWLIB off that resolves to the
    # SDK's UART printf.  Redirect only the shell's translation units to this
    # port's console implementation (see tdsh_stdio_redirect.h).
    set_source_files_properties(${TDSH_CORE_SOURCES} PROPERTIES
        COMPILE_OPTIONS "-include;${CMAKE_CURRENT_SOURCE_DIR}/ports/bl616/tdsh_stdio_redirect.h")

    target_sources(app PRIVATE ${TINYTANG_SOURCES} ${TDSH_CORE_SOURCES}
                               ${TINYDESK_CORE_SOURCES} ${TINYDESK_APP_SOURCES}
                               third_party/tinydesk/ports/common/td_fs_stdio.c)
    sdk_set_main_file(main.cpp)

    # The firmware's build identity: the commit, plus a hash of any uncommitted
    # changes, regenerated on every build so it cannot go stale (see
    # cmake/tinytang_build_id.cmake).  `platform` and the startup banner print
    # it, which is what lets an install be confirmed from the host.
    set(TINYTANG_BUILD_ID_DIR "${CMAKE_BINARY_DIR}/tinytang")
    add_custom_target(tinytang_build_id ALL
        COMMAND ${CMAKE_COMMAND}
                -DSRC=${CMAKE_CURRENT_SOURCE_DIR}
                -DOUT=${TINYTANG_BUILD_ID_DIR}/tinytang_build_id.h
                -P ${CMAKE_CURRENT_SOURCE_DIR}/cmake/tinytang_build_id.cmake
        BYPRODUCTS ${TINYTANG_BUILD_ID_DIR}/tinytang_build_id.h
        COMMENT "TinyTang build identity")
    add_dependencies(app tinytang_build_id)
    target_include_directories(app PRIVATE ${TINYTANG_BUILD_ID_DIR})
endif()

project(tinytang)
