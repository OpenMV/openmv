# This file is part of the OpenMV project.
#
# Copyright (c) 2013-2021 Ibrahim Abdelkader <iabdalkader@openmv.io>
# Copyright (c) 2013-2021 Kwabena W. Agyeman <kwagyeman@openmv.io>
#
# This work is licensed under the MIT license, see the file LICENSE for details.
#
# OMV Makefile

SRCS += $(addprefix alloc/,     \
	xalloc.c                    \
	fb_alloc.c                  \
	umm_malloc.c                \
	dma_alloc.c                 \
	unaligned_memcpy.c          \
   )

SRCS += $(addprefix common/,    \
	array.c                     \
	ringbuf.c                   \
	trace.c                     \
	mutex.c                     \
	vospi.c                     \
	pendsv.c                    \
	usbdbg.c                    \
	tinyusb_debug.c             \
	file_utils.c                \
	mp_utils.c                  \
	nosys_stubs.c               \
	omv_csi.c                   \
   )

SRCS += $(addprefix sensors/,   \
	ov2640.c                    \
	ov5640.c                    \
	ov7670.c                    \
	ov7690.c                    \
	ov7725.c                    \
	ov9650.c                    \
	mt9v0xx.c                   \
	mt9m114.c                   \
	boson.c                     \
	lepton.c                    \
	hm01b0.c                    \
	hm0360.c                    \
	gc2145.c                    \
	genx320.c                   \
	pag7920.c                   \
	pag7936.c                   \
	paj6100.c                   \
	frogeye2020.c               \
   )

SRCS += $(addprefix imlib/,     \
	agast.c                     \
	apriltag.c                  \
	bayer.c                     \
	binary.c                    \
	blob.c                      \
	bmp.c                       \
	clahe.c                     \
	collections.c               \
	dmtx.c                      \
	draw.c                      \
	edge.c                      \
	eye.c                       \
	fast.c                      \
	fft.c                       \
	filter.c                    \
	fmath.c                     \
	font.c                      \
	framebuffer.c               \
	fsort.c                     \
	gif.c                       \
	haar.c                      \
	hog.c                       \
	hough.c                     \
	imlib.c                     \
	integral.c                  \
	integral_mw.c               \
	isp.c                       \
	jpegd.c                     \
	jpege.c                     \
	lodepng.c                   \
	png.c                       \
	kmeans.c                    \
	lab_tab.c                   \
	lbp.c                       \
	line.c                      \
	lsd.c                       \
	mathop.c                    \
	mjpeg.c                     \
	orb.c                       \
	phasecorrelation.c          \
	point.c                     \
	ppm.c                       \
	qrcode.c                    \
	qsort.c                     \
	rainbow_tab.c               \
	rectangle.c                 \
	selective_search.c          \
	sincos_tab.c                \
	stats.c                     \
	stereo.c                    \
	template.c                  \
	xyz_tab.c                   \
	yuv.c                       \
	zbar.c                      \
   )

SRCS += $(wildcard ports/$(PORT)/*.c)

OBJS = $(addprefix $(BUILD)/, $(SRCS:.c=.o))
ifeq ($(USE_CLANG),1)
CLANG_OBJS = $(BUILD)/imlib/bayer.o \
			 $(BUILD)/imlib/binary.o \
			 $(BUILD)/imlib/filter.o \
			 $(BUILD)/imlib/mathop.o
endif

OBJ_DIRS = $(sort $(dir $(OBJS)))

all: | $(OBJ_DIRS) $(OBJS)
$(OBJ_DIRS):
	$(MKDIR) -p $@

$(BUILD)/%.o : %.c
	$(ECHO) "CC $<"
	$(CC) $(CFLAGS) -c -o $@ $<

$(BUILD)/%.o : %.s
	$(ECHO) "AS $<"
	$(AS) $(AFLAGS) $< -o $@

# Rule for compiling with clang
$(CLANG_OBJS): $(BUILD)/%.o : %.c
	$(ECHO) "CL $<"
	$(CLANG) $(CLANG_FLAGS) -c -o $@ $<

$(BUILD)/imlib/fmath.o: override CFLAGS += -fno-strict-aliasing

-include $(OBJS:%.o=%.d)
