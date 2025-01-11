# SPDX-License-Identifier: MIT
#
# Copyright (C) 2013-2024 OpenMV, LLC.
#
# Permission is hereby granted, free of charge, to any person obtaining a copy
# of this software and associated documentation files (the "Software"), to deal
# in the Software without restriction, including without limitation the rights
# to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
# copies of the Software, and to permit persons to whom the Software is
# furnished to do so, subject to the following conditions:
#
# The above copyright notice and this permission notice shall be included in
# all copies or substantial portions of the Software.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
# AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
# OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
# THE SOFTWARE.
#
# Tools, directories and common variables that need to be exported when calling
# sub-Makefiles.

# Export Commands
export Q
export CC
export CLANG
export CXX
export AS
export LD
export AR
export RM
export CPP
export SIZE
export STRIP
export OBJCOPY
export OBJDUMP
export PYTHON
export MKDIR
export ECHO
export MAKE
export CAT
export TFLITE2C
export MKROMFS

# Export Flags
export CFLAGS
export AFLAGS
export LDFLAGS
export MPY_CFLAGS
export USERMOD_OPT

# Export variables
export TARGET
export FIRMWARE
export SYSTEM
export STARTUP
export MPY_MKARGS
export VELA_ARGS
export FROZEN_MANIFEST

# Export board config variables
export PORT
export MCU

# Export Directories
export TOP_DIR
export BUILD
export TOOLS
export FW_DIR
export BOOT_DIR
export OMV_DIR
export CMSIS_DIR
export MICROPY_DIR
export BOSON_DIR
export LEPTON_DIR
export LSM6DS3_DIR
export LSM6DSM_DIR
export LSM6DSOX_DIR
export WINC1500_DIR
export MLX90621_DIR
export MLX90640_DIR
export MLX90641_DIR
export VL53L5CX_DIR
export PIXART_DIR
export DISPLAY_DIR
export LIBPDM_DIR
export TENSORFLOW_DIR
export OMV_BOARD_CONFIG_DIR
export OMV_PORT_DIR
export MP_BOARD_CONFIG_DIR
export OMV_LIB_DIR
export OMV_COMMON_DIR
