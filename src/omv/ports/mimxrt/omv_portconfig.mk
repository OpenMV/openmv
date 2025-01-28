# SPDX-License-Identifier: MIT
#
# Copyright (C) 2023 OpenMV, LLC.
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

# Set startup and system files for CMSIS Makefile.
MCU_SERIES := $(shell echo $(MCU) | cut -c1-10)
LDSCRIPT ?= mimxrt
SYSTEM   ?= mimxrt/system_$(MCU_SERIES)
STARTUP  ?= mimxrt/startup_$(MCU_SERIES)
HAL_DIR  ?= hal/mimxrt/$(MCU_SERIES)

ROMFS_IMAGE := $(FW_DIR)/romfs.stamp
ROMFS_CONFIG := $(OMV_BOARD_CONFIG_DIR)/romfs.json

# Compiler Flags
# TODO: -Wdouble-promotion
CFLAGS += -std=gnu99 \
          -Wall \
          -Werror \
          -Warray-bounds \
          -nostartfiles \
          -fdata-sections \
          -ffunction-sections \
          -fno-inline-small-functions \
          -mfloat-abi=$(FABI) \
          -mthumb \
          -mcpu=$(CPU) \
          -mtune=$(CPU) \
          -mfpu=$(FPU)

# TODO: FIX HSE
CFLAGS += -DCPU_$(MCU) \
          -D$(TARGET) \
          -DARM_NN_TRUNCATE \
          -D__FPU_PRESENT=1 \
          -D__VFP_FP__ \
          -DHSE_VALUE=$(OMV_HSE_VALUE) \
          -DMICROPY_PY_MACHINE_SDCARD=1 \
          -DXIP_EXTERNAL_FLASH=1 \
	      -DXIP_BOOT_HEADER_ENABLE=1 \
	      -DFSL_SDK_ENABLE_DRIVER_CACHE_CONTROL=1 \
	      -DCFG_TUSB_MCU=OPT_MCU_MIMXRT1XXX \
	      -DCPU_HEADER_H='<$(MCU_SERIES).h>' \
	      -DCMSIS_MCU_H='<$(MCU_SERIES).h>' \
	      -DCLOCK_CONFIG_H='<boards/$(MCU_SERIES)_clock_config.h>' \
          -DCSI_DRIVER_FRAG_MODE=1 \
          -D__START=main \
          -D__STARTUP_CLEAR_BSS \
          -D__STARTUP_INITIALIZE_RAMFUNCTION \
          $(OMV_BOARD_CFLAGS)

# Linker Flags
LDFLAGS = -mthumb \
          -mcpu=$(CPU) \
          -mfpu=$(FPU) \
          -mfloat-abi=hard \
          -mabi=aapcs-linux \
          -Wl,--print-memory-usage \
          -Wl,--gc-sections \
          -Wl,--wrap=mp_usbd_task \
          -Wl,--wrap=tud_cdc_rx_cb \
          -Wl,--wrap=mp_hal_stdio_poll \
          -Wl,--wrap=mp_hal_stdout_tx_strn \
          -Wl,-T$(BUILD)/$(LDSCRIPT).lds \
          -Wl,-Map=$(BUILD)/$(FIRMWARE).map

HAL_CFLAGS += -I$(TOP_DIR)/$(CMSIS_DIR)/include
HAL_CFLAGS += -I$(TOP_DIR)/$(CMSIS_DIR)/include/mimxrt
HAL_CFLAGS += -I$(TOP_DIR)/$(HAL_DIR)
HAL_CFLAGS += -I$(TOP_DIR)/$(HAL_DIR)/drivers

MPY_CFLAGS += -I$(MP_BOARD_CONFIG_DIR)
MPY_CFLAGS += -I$(BUILD)/$(MICROPY_DIR)
MPY_CFLAGS += -I$(TOP_DIR)/$(MICROPY_DIR)
MPY_CFLAGS += -I$(TOP_DIR)/$(MICROPY_DIR)/py
MPY_CFLAGS += -I$(TOP_DIR)/$(MICROPY_DIR)/lib/oofatfs
MPY_CFLAGS += -I$(TOP_DIR)/$(MICROPY_DIR)/lib/tinyusb/src
MPY_CFLAGS += -I$(TOP_DIR)/$(MICROPY_DIR)/lib/lwip/src/include
MPY_CFLAGS += -I$(TOP_DIR)/$(MICROPY_DIR)/shared/tinyusb
MPY_CFLAGS += -I$(TOP_DIR)/$(MICROPY_DIR)/ports/mimxrt
MPY_CFLAGS += -I$(TOP_DIR)/$(MICROPY_DIR)/ports/mimxrt/lwip_inc
MPY_CFLAGS += -I$(TOP_DIR)/$(MICROPY_DIR)/shared/runtime

MPY_CFLAGS += -DMICROPY_PY_LWIP=$(MICROPY_PY_LWIP)
MPY_CFLAGS += -DMICROPY_PY_SSL=$(MICROPY_PY_SSL)
MPY_CFLAGS += -DMICROPY_SSL_MBEDTLS=$(MICROPY_SSL_MBEDTLS)
MPY_CFLAGS += -DMICROPY_PY_NETWORK_CYW43=$(MICROPY_PY_NETWORK_CYW43)
MPY_CFLAGS += -DMICROPY_PY_BLUETOOTH=$(MICROPY_PY_BLUETOOTH)
MPY_CFLAGS += -DMICROPY_BLUETOOTH_NIMBLE=$(MICROPY_BLUETOOTH_NIMBLE)
MPY_CFLAGS += -DMICROPY_PY_BLUETOOTH_USE_SYNC_EVENTS=1
MPY_CFLAGS += -DMICROPY_VFS_FAT=1

MPY_MKARGS += MCU_DIR=$(TOP_DIR)/$(HAL_DIR)
MPY_MKARGS += CMSIS_DIR=$(TOP_DIR)/$(CMSIS_DIR)
MPY_MKARGS += SUPPORTS_HARDWARE_FP_SINGLE=1
MPY_MKARGS += MICROPY_VFS_LFS2=0
MPY_MKARGS += MICROPY_PY_LWIP=$(MICROPY_PY_LWIP)
MPY_MKARGS += MICROPY_PY_SSL=$(MICROPY_PY_SSL)
MPY_MKARGS += MICROPY_SSL_MBEDTLS=$(MICROPY_SSL_MBEDTLS)
MPY_MKARGS += MICROPY_PY_NETWORK_CYW43=$(MICROPY_PY_NETWORK_CYW43)
MPY_MKARGS += MICROPY_PY_BLUETOOTH=$(MICROPY_PY_BLUETOOTH)
MPY_MKARGS += MICROPY_BLUETOOTH_NIMBLE=$(MICROPY_BLUETOOTH_NIMBLE)
MPY_MKARGS += MICROPY_PY_OPENAMP=$(MICROPY_PY_OPENAMP)
MPY_MKARGS += MICROPY_PY_OPENAMP_REMOTEPROC=$(MICROPY_PY_OPENAMP_REMOTEPROC)

OMV_CFLAGS += -I$(OMV_BOARD_CONFIG_DIR)
OMV_CFLAGS += -I$(TOP_DIR)/$(OMV_DIR)
OMV_CFLAGS += -I$(TOP_DIR)/$(OMV_DIR)/alloc
OMV_CFLAGS += -I$(TOP_DIR)/$(OMV_DIR)/common
OMV_CFLAGS += -I$(TOP_DIR)/$(OMV_DIR)/imlib
OMV_CFLAGS += -I$(TOP_DIR)/$(OMV_DIR)/modules
OMV_CFLAGS += -I$(TOP_DIR)/$(OMV_DIR)/sensors
OMV_CFLAGS += -I$(TOP_DIR)/$(OMV_DIR)/templates
OMV_CFLAGS += -I$(TOP_DIR)/$(OMV_DIR)/ports/$(PORT)
OMV_CFLAGS += -I$(TOP_DIR)/$(OMV_DIR)/ports/$(PORT)/modules

OMV_CFLAGS += -I$(TOP_DIR)/$(GENX320_DIR)/include
OMV_CFLAGS += -I$(TOP_DIR)/$(BOSON_DIR)/include
OMV_CFLAGS += -I$(TOP_DIR)/$(LEPTON_DIR)/include
OMV_CFLAGS += -I$(TOP_DIR)/$(LSM6DS3_DIR)/include
OMV_CFLAGS += -I$(TOP_DIR)/$(LSM6DSOX_DIR)/include
OMV_CFLAGS += -I$(TOP_DIR)/$(VL53L5CX_DIR)/include
OMV_CFLAGS += -I$(TOP_DIR)/$(WINC1500_DIR)/include
OMV_CFLAGS += -I$(TOP_DIR)/$(MLX90621_DIR)/include
OMV_CFLAGS += -I$(TOP_DIR)/$(MLX90640_DIR)/include
OMV_CFLAGS += -I$(TOP_DIR)/$(MLX90641_DIR)/include
OMV_CFLAGS += -I$(TOP_DIR)/$(PIXART_DIR)/include

CFLAGS += $(HAL_CFLAGS) $(MPY_CFLAGS) $(OMV_CFLAGS)

#------------- Firmware Objects ----------------#
FIRM_OBJ += $(wildcard $(BUILD)/$(CMSIS_DIR)/src/dsp/*/*.o)
FIRM_OBJ += $(wildcard $(BUILD)/$(HAL_DIR)/drivers/*.o)
FIRM_OBJ += $(wildcard $(BUILD)/$(GENX320_DIR)/src/*.o)
FIRM_OBJ += $(wildcard $(BUILD)/$(BOSON_DIR)/src/*.o)
FIRM_OBJ += $(wildcard $(BUILD)/$(LEPTON_DIR)/src/*.o)
ifeq ($(MICROPY_PY_IMU), 1)
FIRM_OBJ += $(wildcard $(BUILD)/$(LSM6DS3_DIR)/src/*.o)
FIRM_OBJ += $(wildcard $(BUILD)/$(LSM6DSOX_DIR)/src/*.o)
endif
FIRM_OBJ += $(wildcard $(BUILD)/$(MLX90621_DIR)/src/*.o)
FIRM_OBJ += $(wildcard $(BUILD)/$(MLX90640_DIR)/src/*.o)
FIRM_OBJ += $(wildcard $(BUILD)/$(MLX90641_DIR)/src/*.o)
FIRM_OBJ += $(wildcard $(BUILD)/$(VL53L5CX_DIR)/src/*.o)
FIRM_OBJ += $(wildcard $(BUILD)/$(PIXART_DIR)/src/*.o)

#------------- OpenMV Objects ----------------#
FIRM_OBJ += $(addprefix $(BUILD)/$(CMSIS_DIR)/src/, \
	$(STARTUP).o                \
	$(SYSTEM).o                 \
	)

FIRM_OBJ += $(addprefix $(BUILD)/$(OMV_DIR)/alloc/, \
	xalloc.o                    \
	fb_alloc.o                  \
	umm_malloc.o                \
	unaligned_memcpy.o          \
   )

FIRM_OBJ += $(addprefix $(BUILD)/$(OMV_DIR)/common/, \
	array.o                     \
	ringbuf.o                   \
	trace.o                     \
	mutex.o                     \
	vospi.o                     \
	pendsv.o                    \
	usbdbg.o                    \
	tinyusb_debug.o             \
	file_utils.o                \
	mp_utils.o                  \
	omv_csi.o                   \
   )

FIRM_OBJ += $(addprefix $(BUILD)/$(OMV_DIR)/sensors/,   \
	ov2640.o                    \
	ov5640.o                    \
	ov7670.o                    \
	ov7690.o                    \
	ov7725.o                    \
	ov9650.o                    \
	mt9v0xx.o                   \
	mt9m114.o                   \
	boson.o                     \
	lepton.o                    \
	hm01b0.o                    \
	hm0360.o                    \
	gc2145.o                    \
	genx320.o                   \
	pag7920.o                   \
	paj6100.o                   \
	frogeye2020.o               \
   )

FIRM_OBJ += $(addprefix $(BUILD)/$(OMV_DIR)/imlib/, \
	agast.o                     \
	apriltag.o                  \
	bayer.o                     \
	binary.o                    \
	blob.o                      \
	bmp.o                       \
	clahe.o                     \
	collections.o               \
	dmtx.o                      \
	draw.o                      \
	edge.o                      \
	eye.o                       \
	fast.o                      \
	fft.o                       \
	filter.o                    \
	fmath.o                     \
	font.o                      \
	framebuffer.o               \
	fsort.o                     \
	gif.o                       \
	haar.o                      \
	hog.o                       \
	hough.o                     \
	imlib.o                     \
	integral.o                  \
	integral_mw.o               \
	isp.o                       \
	jpegd.o                     \
	jpege.o                     \
	lodepng.o                   \
	png.o                       \
	kmeans.o                    \
	lab_tab.o                   \
	lbp.o                       \
	line.o                      \
	lsd.o                       \
	mathop.o                    \
	mjpeg.o                     \
	orb.o                       \
	phasecorrelation.o          \
	point.o                     \
	ppm.o                       \
	qrcode.o                    \
	qsort.o                     \
	rainbow_tab.o               \
	rectangle.o                 \
	selective_search.o          \
	sincos_tab.o                \
	stats.o                     \
	stereo.o                    \
	template.o                  \
	xyz_tab.o                   \
	yuv.o                       \
	zbar.o                      \
   )

FIRM_OBJ += $(wildcard $(BUILD)/$(OMV_DIR)/ports/$(PORT)/*.o)

#------------- MicroPy Objects -------------------#
FIRM_OBJ += $(wildcard $(BUILD)/$(MICROPY_DIR)/modules/*.o)
FIRM_OBJ += $(wildcard $(BUILD)/$(MICROPY_DIR)/ports/$(PORT)/modules/*.o)
FIRM_OBJ += $(wildcard $(BUILD)/$(MICROPY_DIR)/py/*.o)
FIRM_OBJ += $(wildcard $(BUILD)/$(MICROPY_DIR)/boards/$(TARGET)/*.o)

FIRM_OBJ += $(addprefix $(BUILD)/$(MICROPY_DIR)/,\
	boards/$(MCU_SERIES)_clock_config.o \
	dma_manager.o                       \
	eth.o                               \
	fatfs_port.o                        \
	frozen_content.o                    \
	flash.o                             \
	led.o                               \
	machine_bitstream.o                 \
	machine_can.o                       \
	machine_i2c.o                       \
	machine_led.o                       \
	machine_pin.o                       \
	machine_rtc.o                       \
	machine_sdcard.o                    \
	machine_spi.o                       \
	mbedtls/mbedtls_port.o              \
	mimxrt_flash.o                      \
	mimxrt_sdram.o                      \
	modmimxrt.o                         \
	mpnetworkport.o                     \
	msc_disk.o                          \
	network_lan.o                       \
	mphalport.o                         \
	pin.o                               \
	pins_gen.o                          \
	sdcard.o                            \
	sdio.o                              \
	systick.o                           \
	ticks.o                             \
	usbd.o                              \
	)

FIRM_OBJ += $(addprefix $(BUILD)/$(MICROPY_DIR)/hal/,\
	pwm_backport.o                  \
	flexspi_nor_flash.o             \
	qspi_nor_flash_config.o         \
	)

ifeq ($(MICROPY_PY_LWIP), 1)
FIRM_OBJ += $(addprefix $(BUILD)/$(MICROPY_DIR)/hal/,\
	phy/mdio/enet/fsl_enet_mdio.o           \
	phy/device/phydp83825/fsl_phydp83825.o  \
	phy/device/phydp83848/fsl_phydp83848.o  \
	phy/device/phyksz8081/fsl_phyksz8081.o  \
	phy/device/phylan8720/fsl_phylan8720.o  \
	phy/device/phyrtl8211f/fsl_phyrtl8211f.o\
	)
endif


FIRM_OBJ += $(addprefix $(BUILD)/$(MICROPY_DIR)/shared/,\
	libc/printf.o               \
	libc/string0.o              \
	netutils/netutils.o         \
	netutils/trace.o            \
	netutils/dhcpserver.o       \
	runtime/gchelper_thumb2.o   \
	runtime/gchelper_native.o   \
	runtime/interrupt_char.o    \
	runtime/mpirq.o             \
	runtime/pyexec.o            \
	runtime/stdout_helpers.o    \
	runtime/sys_stdio_mphal.o   \
	runtime/softtimer.o         \
	timeutils/timeutils.o       \
	readline/readline.o         \
	tinyusb/mp_usbd.o           \
	tinyusb/mp_usbd_cdc.o       \
	tinyusb/mp_usbd_descriptor.o \
	)

FIRM_OBJ += $(wildcard $(BUILD)/$(MICROPY_DIR)/lib/mbedtls/library/*.o)
FIRM_OBJ += $(wildcard $(BUILD)/$(MICROPY_DIR)/lib/mbedtls_errors/*.o)

FIRM_OBJ += $(addprefix $(BUILD)/$(MICROPY_DIR)/lib/tinyusb/src/, \
	class/cdc/cdc_device.o          \
	class/dfu/dfu_rt_device.o       \
	class/hid/hid_device.o          \
	class/midi/midi_device.o        \
	class/msc/msc_device.o          \
	class/usbtmc/usbtmc_device.o    \
	class/vendor/vendor_device.o    \
	common/tusb_fifo.o              \
	device/usbd.o                   \
	device/usbd_control.o           \
	tusb.o                          \
	portable/chipidea/ci_hs/dcd_ci_hs.o \
	)

FIRM_OBJ += $(addprefix $(BUILD)/$(MICROPY_DIR)/extmod/,\
	machine_adc.o           \
	machine_adc_block.o     \
	machine_bitstream.o     \
	machine_i2c.o           \
	machine_i2s.o           \
	machine_mem.o           \
	machine_pinbase.o       \
	machine_pulse.o         \
	machine_pwm.o           \
	machine_signal.o        \
	machine_spi.o           \
	machine_timer.o         \
	machine_uart.o          \
	machine_usb_device.o    \
	machine_wdt.o           \
	modasyncio.o            \
	modbinascii.o           \
	modbtree.o              \
	modcryptolib.o          \
	moddeflate.o            \
	modframebuf.o           \
	modhashlib.o            \
	modheapq.o              \
	modjson.o               \
	modmachine.o            \
	modnetwork.o            \
	modonewire.o            \
	modopenamp.o            \
	modopenamp_remoteproc.o \
	modopenamp_remoteproc_store.o \
	modos.o                 \
	modplatform.o           \
	modrandom.o             \
	modre.o                 \
	modselect.o             \
	modsocket.o             \
	modtls_axtls.o          \
	modtls_mbedtls.o        \
	modtime.o               \
	moductypes.o            \
	modvfs.o                \
	network_esp_hosted.o    \
	network_ninaw10.o       \
	network_wiznet5k.o      \
	os_dupterm.o            \
	vfs.o                   \
	vfs_blockdev.o          \
	vfs_fat.o               \
	vfs_fat_diskio.o        \
	vfs_fat_file.o          \
	vfs_lfs.o               \
	vfs_rom.o               \
	vfs_rom_file.o          \
	vfs_posix.o             \
	vfs_posix_file.o        \
	vfs_reader.o            \
	virtpin.o               \
	)

FIRM_OBJ += $(addprefix $(BUILD)/$(MICROPY_DIR)/lib/oofatfs/,\
	ff.o                \
	ffunicode.o         \
	)

FIRM_OBJ += $(addprefix $(BUILD)/$(MICROPY_DIR)/drivers/,\
	bus/softspi.o       \
	dht/dht.o           \
	)

ifeq ($(MICROPY_PY_ULAB), 1)
FIRM_OBJ += $(addprefix $(BUILD)/$(MICROPY_DIR)/modules/ulab/code/,\
	ndarray.o                       \
	ndarray_operators.o             \
	ndarray_properties.o            \
	numpy/approx.o                  \
	numpy/bitwise.o                 \
	numpy/carray/carray.o           \
	numpy/carray/carray_tools.o     \
	numpy/compare.o                 \
	numpy/create.o                  \
	numpy/fft/fft.o                 \
	numpy/fft/fft_tools.o           \
	numpy/filter.o                  \
	numpy/io/io.o                   \
	numpy/linalg/linalg.o           \
	numpy/linalg/linalg_tools.o     \
	numpy/ndarray/ndarray_iter.o    \
	numpy/numerical.o               \
	numpy/numpy.o                   \
	numpy/poly.o                    \
	numpy/random/random.o           \
	numpy/stats.o                   \
	numpy/transform.o               \
	numpy/vector.o                  \
	scipy/integrate/integrate.o     \
	scipy/linalg/linalg.o           \
	scipy/optimize/optimize.o       \
	scipy/scipy.o                   \
	scipy/signal/signal.o           \
	scipy/special/special.o         \
	ulab.o                          \
	ulab_tools.o                    \
	user/user.o                     \
	utils/utils.o                   \
	)
endif

ifeq ($(MICROPY_PY_LWIP), 1)
FIRM_OBJ += $(addprefix $(BUILD)/$(MICROPY_DIR)/,\
	lib/lwip/src/core/*.o      \
	lib/lwip/src/core/*/*.o    \
	lib/lwip/src/netif/*.o     \
	lib/lwip/src/apps/*/*.o    \
	extmod/modlwip.o           \
	extmod/modwebsocket.o      \
	extmod/modwebrepl.o        \
	extmod/network_lwip.o      \
	)
endif

ifeq ($(MICROPY_PY_NETWORK_CYW43), 1)
FIRM_OBJ += $(addprefix $(BUILD)/$(MICROPY_DIR)/,\
	lib/cyw43-driver/src/*.o    \
	extmod/network_cyw43.o      \
	)
FIRM_OBJ += $(addprefix $(BUILD)/$(MICROPY_DIR)/drivers/,\
	cyw43/cywbt.o               \
	)
endif

ifeq ($(MICROPY_BLUETOOTH_NIMBLE),1)
FIRM_OBJ += $(addprefix $(BUILD)/$(MICROPY_DIR)/lib/mynewt-nimble/,\
	ext/tinycrypt/src/*.o                   \
	nimble/host/services/gap/src/*.o        \
	nimble/host/services/gatt/src/*.o       \
	nimble/host/src/*.o                     \
	nimble/host/util/src/*.o                \
	nimble/transport/uart/src/*.o           \
	porting/nimble/src/*.o                  \
	)

FIRM_OBJ += $(addprefix $(BUILD)/$(MICROPY_DIR)/,\
	mpbthciport.o                               \
	mpnimbleport.o                              \
	extmod/nimble/modbluetooth_nimble.o         \
	extmod/nimble/nimble/nimble_npl_os.o        \
	extmod/nimble/hal/hal_uart.o                \
	extmod/modbluetooth.o                       \
	)
endif

ifeq ($(MICROPY_PY_OPENAMP),1)
FIRM_OBJ += $(addprefix $(BUILD)/$(MICROPY_DIR)/openamp/metal/,\
	device.o                        \
	dma.o                           \
	init.o                          \
	io.o                            \
	irq.o                           \
	log.o                           \
	shmem.o                         \
	softirq.o                       \
	version.o                       \
	system/micropython/condition.o  \
	system/micropython/device.o     \
	system/micropython/io.o         \
	system/micropython/irq.o        \
	system/micropython/shmem.o      \
	system/micropython/time.o       \
	)

FIRM_OBJ += $(addprefix $(BUILD)/$(MICROPY_DIR)/,\
	mpmetalport.o                                   \
	mpremoteprocport.o                              \
	lib/open-amp/lib/virtio/virtio.o                \
	lib/open-amp/lib/virtio/virtqueue.o             \
	lib/open-amp/lib/virtio_mmio/virtio_mmio_drv.o  \
	lib/open-amp/lib/rpmsg/rpmsg.o                  \
	lib/open-amp/lib/rpmsg/rpmsg_virtio.o           \
	lib/open-amp/lib/remoteproc/elf_loader.o        \
	lib/open-amp/lib/remoteproc/remoteproc.o        \
	lib/open-amp/lib/remoteproc/remoteproc_virtio.o \
	lib/open-amp/lib/remoteproc/rsc_table_parser.o  \
	)
endif

#------------- Libraries ----------------#
ifeq ($(MICROPY_PY_AUDIO), 1)
LIBS += $(TOP_DIR)/$(LIBPDM_DIR)/libPDMFilter_CM7_GCC_wc32.a
endif

#------------- ML libraries ----------------#
ifeq ($(MICROPY_PY_ML_TFLM), 1)
OMV_CFLAGS += -I$(BUILD)/$(TENSORFLOW_DIR)/
FIRM_OBJ += $(addprefix $(BUILD)/$(TENSORFLOW_DIR)/, \
	tflm_backend.o \
	)
LIBS += $(TOP_DIR)/$(TENSORFLOW_DIR)/libtflm/lib/libtflm-$(CPU)+fp-release.a
endif

###################################################
all: $(FIRMWARE) .WAIT $(ROMFS_IMAGE)
	$(SIZE) $(FW_DIR)/$(FIRMWARE).elf

$(BUILD):
	$(MKDIR) -p $@

$(FW_DIR):
	$(MKDIR) -p $@

FIRMWARE_OBJS: | $(BUILD) $(FW_DIR)
	$(MAKE)  -C $(CMSIS_DIR)                 BUILD=$(BUILD)/$(CMSIS_DIR)        CFLAGS="$(CFLAGS) -MMD"
	$(MAKE)  -C $(HAL_DIR)                   BUILD=$(BUILD)/$(HAL_DIR)          CFLAGS="$(CFLAGS) -MMD"
	$(MAKE)  -C $(MICROPY_DIR)/ports/$(PORT) BUILD=$(BUILD)/$(MICROPY_DIR)      $(MPY_MKARGS)
	$(MAKE)  -C $(GENX320_DIR)               BUILD=$(BUILD)/$(GENX320_DIR)      CFLAGS="$(CFLAGS) -MMD"
	$(MAKE)  -C $(BOSON_DIR)                 BUILD=$(BUILD)/$(BOSON_DIR)        CFLAGS="$(CFLAGS) -MMD"
	$(MAKE)  -C $(LEPTON_DIR)                BUILD=$(BUILD)/$(LEPTON_DIR)       CFLAGS="$(CFLAGS) -MMD"
ifeq ($(MICROPY_PY_IMU), 1)
	$(MAKE)  -C $(LSM6DS3_DIR)               BUILD=$(BUILD)/$(LSM6DS3_DIR)      CFLAGS="$(CFLAGS) -MMD"
	$(MAKE)  -C $(LSM6DSOX_DIR)              BUILD=$(BUILD)/$(LSM6DSOX_DIR)     CFLAGS="$(CFLAGS) -MMD"
endif
	$(MAKE)  -C $(MLX90621_DIR)              BUILD=$(BUILD)/$(MLX90621_DIR)     CFLAGS="$(CFLAGS) -MMD"
	$(MAKE)  -C $(MLX90640_DIR)              BUILD=$(BUILD)/$(MLX90640_DIR)     CFLAGS="$(CFLAGS) -MMD"
	$(MAKE)  -C $(MLX90641_DIR)              BUILD=$(BUILD)/$(MLX90641_DIR)     CFLAGS="$(CFLAGS) -MMD"
	$(MAKE)  -C $(VL53L5CX_DIR)              BUILD=$(BUILD)/$(VL53L5CX_DIR)     CFLAGS="$(CFLAGS) -MMD"
	$(MAKE)  -C $(PIXART_DIR)                BUILD=$(BUILD)/$(PIXART_DIR)       CFLAGS="$(CFLAGS) -MMD"
ifeq ($(MICROPY_PY_ML_TFLM), 1)
	$(MAKE)  -C $(TENSORFLOW_DIR)            BUILD=$(BUILD)/$(TENSORFLOW_DIR)   CFLAGS="$(CFLAGS) -MMD"
endif
	$(MAKE)  -C $(OMV_DIR)                   BUILD=$(BUILD)/$(OMV_DIR)          CFLAGS="$(CFLAGS) -MMD"

$(ROMFS_IMAGE): $(ROMFS_CONFIG)
	$(ECHO) "GEN romfs image"
	$(PYTHON) $(TOOLS)/$(MKROMFS) --top-dir $(TOP_DIR) --out-dir $(FW_DIR) \
                                  --build-dir $(BUILD) --config $(ROMFS_CONFIG)
	touch $@

# This target generates the main/app firmware image located at 0x08010000
$(FIRMWARE): FIRMWARE_OBJS
	$(CPP) -P -E -DLINKER_SCRIPT -I$(OMV_COMMON_DIR) -I$(OMV_BOARD_CONFIG_DIR) \
        $(OMV_DIR)/ports/$(PORT)/$(LDSCRIPT).ld.S > $(BUILD)/$(LDSCRIPT).lds
	$(CC) $(LDFLAGS) $(FIRM_OBJ) -o $(FW_DIR)/$(FIRMWARE).elf $(LIBS) -lm
	$(OBJCOPY) -Obinary -R .big_const* $(FW_DIR)/$(FIRMWARE).elf $(FW_DIR)/$(FIRMWARE).bin

size:
	$(SIZE) --format=SysV $(FW_DIR)/$(FIRMWARE).elf

# Flash the main firmware image
flash_image::
	$(PYDFU) -u $(FW_DIR)/$(FIRMWARE).dfu

# Flash the main firmware image using dfu_util
flash_image_dfu_util::
	dfu-util -a 0 -d $(DFU_DEVICE) -D $(FW_DIR)/$(FIRMWARE).dfu
