# This file is part of the OpenMV project.
#
# Copyright (c) 2013-2024 Ibrahim Abdelkader <iabdalkader@openmv.io>
# Copyright (c) 2013-2024 Kwabena W. Agyeman <kwagyeman@openmv.io>
#
# This work is licensed under the MIT license, see the file LICENSE for details.
#
# Top level Makefile

# Set verbosity
ifeq ($(V), 1)
Q =
else
Q = @
MAKEFLAGS += --silent
endif

# Default path to LLVM toolchain.
LLVM_PATH ?=/opt/LLVM-ET-Arm-18.1.3-Linux-x86_64/bin/

# Commands
CC      = $(Q)arm-none-eabi-gcc
CLANG   = $(Q)$(LLVM_PATH)/clang
CXX     = $(Q)arm-none-eabi-g++
AS      = $(Q)arm-none-eabi-as
LD      = $(Q)arm-none-eabi-ld
AR      = $(Q)arm-none-eabi-ar
RM      = $(Q)rm
CPP     = $(Q)arm-none-eabi-cpp
SIZE    = $(Q)arm-none-eabi-size
STRIP   = $(Q)arm-none-eabi-strip -s
OBJCOPY = $(Q)arm-none-eabi-objcopy
OBJDUMP = $(Q)arm-none-eabi-objdump
PYTHON  = $(Q)python3
MKDFU   = micropython/tools/dfu.py
PYDFU   = $(Q)../tools/pydfu.py
MKDIR   = $(Q)mkdir
ECHO    = $(Q)@echo
MAKE    = $(Q)make
CAT     = $(Q)cat
TFLITE2C = tflite2c.py
MKROMFS = mkromfs.py

# Targets
OPENMV = openmv
FIRMWARE = firmware

ifeq ($(TARGET),)
  ifneq ($(MAKECMDGOALS),clean)
    $(error Invalid or no TARGET specified)
  endif
  TARGET=OPENMV4
endif

# Directories
TOP_DIR=$(shell pwd)
BUILD=$(TOP_DIR)/build
TOOLS=$(TOP_DIR)/../tools
FW_DIR=$(BUILD)/bin
OMV_DIR=omv
BOOT_DIR=boot
CUBEAI_DIR=stm32cubeai
CMSIS_DIR=hal/cmsis
MICROPY_DIR=micropython
GENX320_DIR=drivers/genx320
BOSON_DIR=drivers/boson
LEPTON_DIR=drivers/lepton
LSM6DS3_DIR=drivers/lsm6ds3
LSM6DSM_DIR=drivers/lsm6dsm
LSM6DSOX_DIR=drivers/lsm6dsox
WINC1500_DIR=drivers/winc1500
MLX90621_DIR=drivers/mlx90621
MLX90640_DIR=drivers/mlx90640
MLX90641_DIR=drivers/mlx90641
VL53L5CX_DIR=drivers/vl53l5cx
PIXART_DIR=drivers/pixart
DISPLAY_DIR=drivers/display
DAVE2D_DIR=drivers/dave2d
NEMA_DIR=drivers/nema
LIBPDM_DIR=lib/libpdm
TENSORFLOW_DIR=lib/tflm
CYW4343_FW_DIR=drivers/cyw4343/firmware/
OMV_BOARD_CONFIG_DIR=$(TOP_DIR)/$(OMV_DIR)/boards/$(TARGET)/
OMV_PORT_DIR=$(TOP_DIR)/$(OMV_DIR)/ports/$(PORT)
MP_BOARD_CONFIG_DIR=$(TOP_DIR)/$(MICROPY_DIR)/ports/$(PORT)/boards/$(TARGET)/
OMV_LIB_DIR=$(TOP_DIR)/../scripts/libraries
FROZEN_MANIFEST=$(OMV_BOARD_CONFIG_DIR)/manifest.py
OMV_COMMON_DIR=$(TOP_DIR)/$(OMV_DIR)/common

# Debugging/Optimization
ifeq ($(DEBUG), 1)
ROM_TEXT_COMPRESSION = 0
CFLAGS += -Og -ggdb3
else
DEBUG=0
ROM_TEXT_COMPRESSION = 1
CFLAGS += -O2 -DNDEBUG
USERMOD_OPT = -O2
MPY_CFLAGS += -DMICROPY_ROM_TEXT_COMPRESSION=1
endif

# Enable debug printf
ifeq ($(DEBUG_PRINTF), 1)
CFLAGS += -DOMV_DEBUG_PRINTF
endif

# Enable stack protection
ifeq ($(STACK_PROTECTOR), 1)
CFLAGS += -fstack-protector-all -DSTACK_PROTECTOR
endif

# Enable debug printf
ifeq ($(FB_ALLOC_STATS), 1)
CFLAGS += -DFB_ALLOC_STATS
endif

# Enable timing for some functions.
ifeq ($(PROFILE), 1)
CFLAGS += -DOMV_PROFILE_ENABLE=1
endif

# Include OpenMV board config first to set the port.
include $(OMV_BOARD_CONFIG_DIR)/omv_boardconfig.mk

# Include MicroPython board config.
#include $(MP_BOARD_CONFIG_DIR)/mpconfigboard.mk

# Additional qstr definitions for OpenMV
#OMV_SRC_QSTR := $(wildcard $(TOP_DIR)/$(OMV_DIR)/modules/*.c)

# The following command line args are passed to MicroPython's top Makefile.
MPY_MKARGS = PORT=$(PORT) BOARD=$(TARGET) DEBUG=$(DEBUG) MICROPY_MANIFEST_OMV_LIB_DIR=$(OMV_LIB_DIR)\
             FROZEN_MANIFEST=$(FROZEN_MANIFEST) OMV_SRC_QSTR="$(OMV_SRC_QSTR)"\
             MICROPY_ROM_TEXT_COMPRESSION=$(ROM_TEXT_COMPRESSION) USER_C_MODULES=$(TOP_DIR)/$(OMV_DIR)

# Configure additional built-in modules. Note must define both the CFLAGS and the Make command line args.
ifeq ($(MICROPY_PY_CSI), 1)
MPY_CFLAGS += -DMICROPY_PY_CSI=1
MPY_MKARGS += MICROPY_PY_CSI=1
endif

ifeq ($(MICROPY_PY_WINC1500), 1)
MPY_CFLAGS += -DMICROPY_PY_WINC1500=1
MPY_MKARGS += MICROPY_PY_WINC1500=1
MPY_PENDSV_ENTRIES += PENDSV_DISPATCH_WINC,
endif

ifeq ($(MICROPY_PY_IMU), 1)
MPY_CFLAGS += -DMICROPY_PY_IMU=1
MPY_MKARGS += MICROPY_PY_IMU=1
endif

ifeq ($(MICROPY_PY_BTREE), 1)
MPY_CFLAGS += -DMICROPY_PY_BTREE=1
MPY_MKARGS += MICROPY_PY_BTREE=1
endif

ifeq ($(MICROPY_PY_TOF), 1)
MPY_CFLAGS += -DMICROPY_PY_TOF=1
MPY_MKARGS += MICROPY_PY_TOF=1
endif

ifeq ($(MICROPY_PY_ULAB), 1)
MPY_CFLAGS += -DMICROPY_PY_ULAB=1
MPY_CFLAGS += -DULAB_CONFIG_FILE="\"$(OMV_BOARD_CONFIG_DIR)/ulab_config.h\""
MPY_MKARGS += MICROPY_PY_ULAB=1
endif

ifeq ($(MICROPY_PY_AUDIO), 1)
MPY_CFLAGS += -DMICROPY_PY_AUDIO=1
MPY_MKARGS += MICROPY_PY_AUDIO=1
endif

ifeq ($(MICROPY_PY_DISPLAY), 1)
MPY_CFLAGS += -DMICROPY_PY_DISPLAY=1
MPY_MKARGS += MICROPY_PY_DISPLAY=1
endif

ifeq ($(MICROPY_PY_TV), 1)
MPY_CFLAGS += -DMICROPY_PY_TV=1
MPY_MKARGS += MICROPY_PY_TV=1
endif

ifeq ($(MICROPY_PY_BUZZER), 1)
MPY_CFLAGS += -DMICROPY_PY_BUZZER=1
MPY_MKARGS += MICROPY_PY_BUZZER=1
endif

ifeq ($(CUBEAI), 1)
MPY_CFLAGS += -DMICROPY_PY_CUBEAI=1
MPY_MKARGS += MICROPY_PY_CUBEAI=1
endif

ifeq ($(MICROPY_PY_ML), 1)
MPY_CFLAGS += -DMICROPY_PY_ML=1
MPY_MKARGS += MICROPY_PY_ML=1
endif

ifeq ($(MICROPY_PY_ML_TFLM), 1)
MPY_CFLAGS += -DMICROPY_PY_ML_TFLM=1
MPY_MKARGS += MICROPY_PY_ML_TFLM=1
endif

MPY_PENDSV_ENTRIES := $(shell echo $(MPY_PENDSV_ENTRIES) | tr -d '[:space:]')
MPY_CFLAGS += -DMICROPY_BOARD_PENDSV_ENTRIES="$(MPY_PENDSV_ENTRIES)"
MPY_CFLAGS += -DMP_CONFIGFILE=\<$(OMV_PORT_DIR)/omv_mpconfigport.h\>

# Include the port Makefile.
include $(OMV_PORT_DIR)/omv_portconfig.mk

# Export all common variables
include $(OMV_COMMON_DIR)/export.mk

clean:
	$(RM) -fr $(BUILD)
