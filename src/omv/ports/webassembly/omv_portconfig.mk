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

CFLAGS_EXTMOD += -DCMSIS_MCU_H='<cmsis_compiler.h>'
CFLAGS_EXTMOD += -I$(TOP_DIR)/$(OMV_DIR)/boards/$(TARGET)
CFLAGS_EXTMOD += -I$(TOP_DIR)/$(OMV_DIR)/ports/$(PORT)
CFLAGS_EXTMOD += -I$(TOP_DIR)/$(OMV_DIR)/ports/$(PORT)/cmsis
CFLAGS_EXTMOD += -I$(TOP_DIR)/$(OMV_DIR)/ports/$(PORT)/lib/oofatfs
CFLAGS_EXTMOD += -I$(TOP_DIR)/$(OMV_DIR)/modules
CFLAGS_EXTMOD += -I$(TOP_DIR)/$(OMV_DIR)/imlib
CFLAGS_EXTMOD += -I$(TOP_DIR)/$(OMV_DIR)/alloc
CFLAGS_EXTMOD += -I$(TOP_DIR)/$(OMV_DIR)/common
CFLAGS_EXTMOD += -I$(TOP_DIR)/$(MICROPY_DIR)/py


SRC_EXTMOD_C += $(addprefix $(TOP_DIR)/$(MICROPY_DIR)/,\
	extmod/modasyncio.c \
	extmod/modbinascii.c \
	extmod/modbtree.c \
	extmod/modcryptolib.c \
	extmod/moddeflate.c \
	extmod/modframebuf.c \
	extmod/modhashlib.c \
	extmod/modheapq.c \
	extmod/modjson.c \
	extmod/modlwip.c \
	extmod/modmachine.c \
	extmod/modnetwork.c \
	extmod/modonewire.c \
	extmod/modopenamp.c \
	extmod/modopenamp_remoteproc.c \
	extmod/modopenamp_remoteproc_store.c \
	extmod/modos.c \
	extmod/modplatform.c\
	extmod/modrandom.c \
	extmod/modre.c \
	extmod/modselect.c \
	extmod/modsocket.c \
	extmod/modtls_axtls.c \
	extmod/modtls_mbedtls.c \
	extmod/mbedtls/mbedtls_alt.c \
	extmod/modtime.c \
	extmod/moductypes.c \
	extmod/modvfs.c \
	extmod/modwebrepl.c \
	extmod/modwebsocket.c \
	extmod/os_dupterm.c \
	extmod/vfs.c \
	extmod/vfs_posix.c \
	extmod/vfs_posix_file.c \
	extmod/vfs_reader.c \
	extmod/virtpin.c \
	shared/libc/abort_.c \
	shared/libc/printf.c \
	)

SRC_EXTMOD_C += $(addprefix $(TOP_DIR)/$(OMV_DIR)/,\
	modules/py_clock.c \
	modules/py_helper.c \
	modules/py_image.c \
	modules/py_imageio.c \
	)

SRC_THIRDPARTY_C += $(addprefix $(TOP_DIR)/$(OMV_DIR)/,\
	ports/webassembly/lib/oofatfs/ff.c \
	common/array.c \
	common/file_utils.c \
	common/mutex.c \
	alloc/fb_alloc.c \
	alloc/umm_malloc.c \
	alloc/unaligned_memcpy.c \
	alloc/xalloc.c \
	)

SRC_THIRDPARTY_C += $(addprefix $(TOP_DIR)/$(OMV_DIR)/imlib/,\
	apriltag.c \
	bayer.c \
	binary.c \
	blob.c \
	bmp.c \
	clahe.c \
	collections.c \
	dmtx.c \
	draw.c \
	edge.c \
	eye.c \
	fast.c \
	fft.c \
	filter.c \
	fmath.c \
	font.c \
	framebuffer.c \
	fsort.c \
	gif.c \
	haar.c \
	hog.c \
	hough.c \
	imlib.c \
	integral_mw.c \
	integral.c \
	isp.c \
	jpegd.c \
	jpege.c \
	kmeans.c \
	lab_tab.c \
	lbp.c \
	line.c \
	lodepng.c \
	lsd.c \
	mathop.c \
	mjpeg.c \
	orb.c \
	phasecorrelation.c \
	png.c \
	point.c \
	ppm.c \
	qrcode.c \
	rainbow_tab.c \
	rectangle.c \
	selective_search.c \
	sincos_tab.c \
	stats.c \
	stereo.c \
	template.c \
	xyz_tab.c \
	yuv.c \
	zbar.c \
	)

###################################################
all: $(OPENMV)

$(FIRMWARE):
	$(MAKE) -C $(MICROPY_DIR)/ports/$(PORT) BUILD=$(BUILD)/$(TARGET) min CFLAGS_EXTMOD="$(CFLAGS_EXTMOD)" SRC_EXTMOD_C="$(SRC_EXTMOD_C)" SRC_THIRDPARTY_C="$(SRC_THIRDPARTY_C)"

# This target generates the firmware image.
$(OPENMV): $(FIRMWARE)
	wasm-opt -Oz -o $(BUILD)/$(TARGET)/micropython.wasm $(BUILD)/$(TARGET)/micropython.wasm
