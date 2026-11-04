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

# Reproducible build stamp.
#
# The SDK's image tool writes the app header's BLFBVERF string from the
# compiler's __DATE__ and __TIME__, so without this two builds of identical
# source differ in three bytes and no firmware digest means anything.  GCC
# honours SOURCE_DATE_EPOCH for both macros, so pinning it here makes a clean
# build byte-identical every time.
#
# The value is the one the release baseline was measured at (2026-11-04
# 00:00:00 UTC), which is why the firmware still hashes to ae318f0268c0...:
# changing it changes the build, not just its record.  `?=` so a caller can
# still choose another stamp deliberately, and it is exported because the
# compiler reads it from the environment rather than from a command line.
SOURCE_DATE_EPOCH ?= 1793750400

export BL_SDK_BASE
export SOURCE_DATE_EPOCH
export PATH := $(TOOLCHAIN_BIN):$(PATH)

CHIP ?= bl616
BOARD ?= bl616dk
CROSS_COMPILE ?= riscv64-unknown-elf-

include $(BL_SDK_BASE)/project.build
