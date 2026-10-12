# TinyTang — a TangCore drop-in replacement powered by TinyDesk Shell.
#
# The BL616 firmware is built with the Bouffalo SDK's own project.build, the
# same mechanism the vendor examples use, so nothing here is inherited from a
# previous firmware.  BL_SDK_BASE and the RISC-V toolchain default to the
# local tangcore development cache and can be overridden in the environment.
# The default SDK is upstream Bouffalo SDK 2.3.36; the older fork at
# ~/.cache/tangcore-dev/sdk is kept only for rollback.

SDK_DEMO_PATH ?= .
BL_SDK_BASE   ?= $(HOME)/.cache/tangcore-dev/sdk-2.3.36
TOOLCHAIN_BIN ?= $(HOME)/.cache/tangcore-dev/toolchain/bin

export BL_SDK_BASE
export PATH := $(TOOLCHAIN_BIN):$(PATH)

CHIP ?= bl616
BOARD ?= bl616dk
CROSS_COMPILE ?= riscv64-unknown-elf-

include $(BL_SDK_BASE)/project.build
