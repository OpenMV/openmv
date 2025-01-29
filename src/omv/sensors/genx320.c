/*
 * SPDX-License-Identifier: MIT
 *
 * Copyright (C) 2013-2024 OpenMV, LLC.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 *
 * GENX320 driver.
 */
#include "omv_boardconfig.h"
#if (OMV_GENX320_ENABLE == 1)

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "py/mphal.h"
#include "framebuffer.h"
#include "omv_i2c.h"
#include "omv_csi.h"

#include "genx320.h"
#include "evt_2_0.h"
#include "psee_genx320.h"

#define BLANK_LINES                     4
#define BLANK_COLUMNS                   4

#define SENSOR_WIDTH                    324
#define SENSOR_HEIGHT                   324

#define ACTIVE_SENSOR_WIDTH             (SENSOR_WIDTH - BLANK_COLUMNS)
#define ACTIVE_SENSOR_HEIGHT            (SENSOR_HEIGHT - BLANK_LINES)
#define ACTIVE_SENSOR_SIZE              (ACTIVE_SENSOR_WIDTH * ACTIVE_SENSOR_HEIGHT)

#define HSYNC_CLOCK_CYCLES              32
#define VSYNC_CLOCK_CYCLES              32

#define CONTRAST_DEFAULT                16
#define BRIGHTNESS_DEFAULT              128

#define I2C_TIMEOUT                     1000

#define INTEGRATION_DEF_PREIOD          20000 // 20ms (50 FPS)
#define INTEGRATION_MIN_PREIOD          0x10 // 16us
#define INTEGRATION_MAX_PREIOD          0x1FFF0 // 131056us

#define FPS_TO_US(fps)                  (1000000 / (fps))

#define EVENT_THRESHOLD_TO_CALIBRATE    100000
#define EVENT_THRESHOLD_SIGMA           10

#define EVT_CLK_FREQ                    ((omv_csi_get_xclk_frequency() + 500000) / 1000000)

#define AFK_50_HZ                       (50)
#define AFK_60_HZ                       (60)
#define AFK_LOW_FREQ                    IM_MIN(AFK_50_HZ, AFK_60_HZ)
#define AFK_HIGH_FREQ                   IM_MAX(AFK_50_HZ, AFK_60_HZ)
#define AFK_LOW_BAND                    ((AFK_LOW_FREQ * 2) - 10)
#define AFK_HIGH_BAND                   ((AFK_HIGH_FREQ * 2) + 10)

#define EHC_DIFF3D_N_BITS_SIZE          (7) // signed 8-bit value

#if (OMV_GENX320_CAL_ENABLE == 1)
static bool hot_pixels_disabled = false;
#endif // (OMV_GENX320_CAL_ENABLE == 1)
static int32_t contrast = CONTRAST_DEFAULT;
static int32_t brightness = BRIGHTNESS_DEFAULT;

static int reset(omv_csi_t *csi) {
    csi->color_palette = NULL;

    #if (OMV_GENX320_CAL_ENABLE == 1)
    hot_pixels_disabled = false;
    #endif // (OMV_GENX320_CAL_ENABLE == 1)
    contrast = CONTRAST_DEFAULT;
    brightness = BRIGHTNESS_DEFAULT;

    BIAS_Params_t biases = (csi->chip_id == SAPHIR_ES_ID) ? genx320es_default_biases : genx320mp_default_biases;

    // Force CPI with chicken bits
    psee_sensor_write(TOP_CHICKEN, TOP_CHICKEN_OVERRIDE_MIPI_MODE_EN |
                      TOP_CHICKEN_OVERRIDE_HISTO_MODE_EN |
                      #if (OMV_GENX320_EHC_ENABLE == 1)
                      1 << TOP_CHICKEN_OVERRIDE_HISTO_MODE_Pos |
                      #else
                      0 << TOP_CHICKEN_OVERRIDE_HISTO_MODE_Pos |
                      #endif // (OMV_GENX320_EHC_ENABLE == 1)
                      I2C_TIMEOUT << TOP_CHICKEN_I2C_TIMEOUT_Pos);

    // Start the Init sequence
    #if (OMV_GENX320_EHC_ENABLE == 1)
    psee_sensor_init(&dcmi_histo);
    #else
    psee_sensor_init(&dcmi_evt);

    // Set EVT20 mode
    psee_sensor_write(EDF_CONTROL, 0);

    // Configure Packet and Frame sizes
    psee_sensor_write(CPI_PACKET_SIZE_CONTROL, ACTIVE_SENSOR_WIDTH);
    psee_sensor_write(CPI_PACKET_TIME_CONTROL, ACTIVE_SENSOR_WIDTH << CPI_PACKET_TIME_CONTROL_PERIOD_Pos |
                      HSYNC_CLOCK_CYCLES << CPI_PACKET_TIME_CONTROL_BLANKING_Pos);
    psee_sensor_write(CPI_FRAME_SIZE_CONTROL, ACTIVE_SENSOR_HEIGHT);
    psee_sensor_write(CPI_FRAME_TIME_CONTROL, VSYNC_CLOCK_CYCLES);
    #endif // (OMV_GENX320_EHC_ENABLE == 1)

    // Enable dropping
    psee_sensor_write(RO_READOUT_CTRL, RO_READOUT_CTRL_DIGITAL_PIPE_EN |
                      RO_READOUT_CTRL_AVOID_BPRESS_TD |
                      RO_READOUT_CTRL_DROP_EN |
                      RO_READOUT_CTRL_DROP_ON_FULL_EN);

    // Enable the Anti-FlicKering filter
    AFK_HandleTypeDef psee_afk;

    if (psee_afk_init(&psee_afk) != AFK_OK) {
        return -1;
    }

    if (psee_afk_activate(&psee_afk, AFK_LOW_BAND, AFK_HIGH_BAND, EVT_CLK_FREQ) != AFK_OK) {
        return -1;
    }

    // Operation Mode Configuration
    #if (OMV_GENX320_EHC_ENABLE == 1)
    psee_PM3C_Histo_config();
    #else
    psee_PM3C_config();
    #endif // (OMV_GENX320_EHC_ENABLE == 1)

    // Set the default border for the Activity map
    psee_set_default_XY_borders(&genx320mp_default_am_borders);

    // Configure the activity map
    psee_configure_activity_map();

    // Set Standard biases
    psee_sensor_set_biases(&biases);

    // Start the csi
    #if (OMV_GENX320_EHC_ENABLE == 1)
    psee_sensor_start(&dcmi_histo);

    EHC_HandleTypeDef psee_ehc;

    if (psee_ehc_init(&psee_ehc) != EHC_OK) {
        return -1;
    }

    if (psee_ehc_activate(&psee_ehc, EHC_ALGO_DIFF3D, 0, EHC_DIFF3D_N_BITS_SIZE,
                          INTEGRATION_DEF_PREIOD, EHC_WITHOUT_PADDING) != EHC_OK) {
        return -1;
    }
    #else
    psee_sensor_start(&dcmi_evt);
    #endif //  (OMV_GENX320_EHC_ENABLE == 1)

    return 0;
}

static int sleep(omv_csi_t *csi, int enable) {
    if (enable) {
        #if (OMV_GENX320_EHC_ENABLE == 1)
        psee_PM2_Histo_config();
        #else
        psee_PM2_config();
        #endif // (OMV_GENX320_EHC_ENABLE == 1)
    } else {
        #if (OMV_GENX320_EHC_ENABLE == 1)
        psee_PM3C_Histo_config();
        #else
        psee_PM3C_config();
        #endif // (OMV_GENX320_EHC_ENABLE == 1)
    }
    return 0;
}

static int read_reg(omv_csi_t *csi, uint16_t reg_addr) {
    uint32_t reg_data;
    uint8_t addr[] = {(reg_addr >> 8), reg_addr};
    if (omv_i2c_write_bytes(&csi->i2c_bus, csi->slv_addr, addr, 2, OMV_I2C_XFER_NO_STOP) != 0) {
        return -1;
    }
    if (omv_i2c_read_bytes(&csi->i2c_bus, csi->slv_addr, (uint8_t *) &reg_data, 4, OMV_I2C_XFER_NO_FLAGS) != 0) {
        return -1;
    }
    reg_data = __REV(reg_data);
    return reg_data;
}

static int write_reg(omv_csi_t *csi, uint16_t reg_addr, uint16_t reg_data) {
    uint8_t buf[] = {(reg_addr >> 8), reg_addr, (reg_data >> 24), (reg_data >> 16), (reg_data >> 8), reg_data};
    return omv_i2c_write_bytes(&csi->i2c_bus, csi->slv_addr, buf, 6, OMV_I2C_XFER_NO_FLAGS);
}

static int set_pixformat(omv_csi_t *csi, pixformat_t pixformat) {
    return (pixformat == PIXFORMAT_GRAYSCALE) ? 0 : -1;
}

static int set_framesize(omv_csi_t *csi, omv_csi_framesize_t framesize) {
    return (framesize == OMV_CSI_FRAMESIZE_320X320) ? 0 : -1;
}

static int set_framerate(omv_csi_t *csi, int framerate) {
    #if (OMV_GENX320_EHC_ENABLE == 1)
    int us = FPS_TO_US(framerate);

    if (us < INTEGRATION_MIN_PREIOD) {
        return -1;
    }

    if (us > INTEGRATION_MAX_PREIOD) {
        return -1;
    }

    psee_sensor_write(EHC_INTEGRATION_PERIOD, us);
    #endif // (OMV_GENX320_EHC_ENABLE == 1)
    return 0;
}

static int set_contrast(omv_csi_t *csi, int level) {
    contrast = __USAT(level, UINT8_T_BITS);
    return 0;
}

static int set_brightness(omv_csi_t *csi, int level) {
    brightness = __USAT(level, UINT8_T_BITS);
    return 0;
}

static int set_colorbar(omv_csi_t *csi, int enable) {
    uint32_t reg;
    psee_sensor_read(RO_READOUT_CTRL, &reg);
    reg = (reg & ~RO_READOUT_CTRL_ERC_SELF_TEST_EN) | (enable ? RO_READOUT_CTRL_ERC_SELF_TEST_EN : 0);
    psee_sensor_write(RO_READOUT_CTRL, reg);
    return 0;
}

static int set_hmirror(omv_csi_t *csi, int enable) {
    psee_sensor_set_flip(enable, csi->vflip);
    return 0;
}

static int set_vflip(omv_csi_t *csi, int enable) {
    psee_sensor_set_flip(csi->hmirror, enable);
    return 0;
}

#if (OMV_GENX320_CAL_ENABLE == 1)
static void disable_hot_pixels(uint8_t *histogram) {
    // Compute average
    int32_t avg = 0;

    for (uint32_t i = 0; i < ACTIVE_SENSOR_SIZE; i++) {
        avg += histogram[i];
    }

    avg /= ACTIVE_SENSOR_SIZE;

    // Compute std
    int32_t std = 0;

    for (uint32_t i = 0; i < ACTIVE_SENSOR_SIZE; i++) {
        int32_t diff = histogram[i] - avg;
        std += diff * diff;
    }

    std = fast_sqrtf(std / ACTIVE_SENSOR_SIZE);

    int32_t threshold = avg + (std * EVENT_THRESHOLD_SIGMA);

    for (uint32_t y = 0; y < ACTIVE_SENSOR_HEIGHT; y++) {
        // Reset all blocks
        for (uint32_t i = 0; i < (ACTIVE_SENSOR_WIDTH / UINT32_T_BITS); i++) {
            psee_write_ROI_X(i * sizeof(uint32_t), 0);
        }

        // Select line
        uint32_t offset = y / UINT32_T_BITS;
        psee_write_ROI_Y(offset * sizeof(uint32_t), 1 << (y % UINT32_T_BITS));

        // Trigger shadow
        psee_write_ROI_CTRL(ROI_CTRL_PX_SW_RSTN | ROI_CTRL_TD_SHADOW_TRIGGER);

        uint32_t tmp[ACTIVE_SENSOR_WIDTH / UINT32_T_BITS] = {};
        for (uint32_t x = 0; x < ACTIVE_SENSOR_WIDTH; x++) {
            if (histogram[(y * ACTIVE_SENSOR_WIDTH) + x] > threshold) {
                tmp[x / UINT32_T_BITS] |= 1 << (x % UINT32_T_BITS);
            }
        }

        // Write x values to disable
        for (uint32_t i = 0; i < (ACTIVE_SENSOR_WIDTH / UINT32_T_BITS); i++) {
            psee_write_ROI_X(i * sizeof(uint32_t), tmp[i]);
        }

        // Activate block
        psee_write_ROI_CTRL(ROI_CTRL_PX_SW_RSTN | ROI_CTRL_TD_SHADOW_TRIGGER | ROI_CTRL_TD_EN);

        // Disable roi block
        psee_write_ROI_CTRL(ROI_CTRL_PX_SW_RSTN);
        psee_write_ROI_Y(offset * sizeof(uint32_t), 0);
    }
}
#endif // (OMV_GENX320_CAL_ENABLE == 1)

static void snapshot_post_process(image_t *image) {
    #if (OMV_GENX320_EHC_ENABLE == 1)
    for (uint32_t i = 0; i < ACTIVE_SENSOR_SIZE; i++) {
        image->data[i] = __USAT((((int8_t *) image->data)[i] * contrast) + brightness, UINT8_T_BITS);
    }
    #else
    uint8_t *out = fb_alloc(ACTIVE_SENSOR_SIZE, FB_ALLOC_NO_HINT);
    memset(out, brightness, ACTIVE_SENSOR_SIZE);

    for (uint32_t i = 0; i < (ACTIVE_SENSOR_SIZE / sizeof(uint32_t)); i++) {
        uint32_t val = ((uint32_t *) image->data)[i];
        uint32_t x = __EVT20_X(val);
        uint32_t y = __EVT20_Y(val);
        switch (__EVT20_TYPE(val)) {
            case TD_LOW: {
                if ((x < ACTIVE_SENSOR_WIDTH) && (y < ACTIVE_SENSOR_HEIGHT)) {
                    uint32_t index = (y * ACTIVE_SENSOR_WIDTH) + x;
                    out[index] = __USAT(((int32_t) out[index]) - contrast, UINT8_T_BITS);
                }
                break;
            }
            case TD_HIGH: {
                if ((x < ACTIVE_SENSOR_WIDTH) && (y < ACTIVE_SENSOR_HEIGHT)) {
                    uint32_t index = (y * ACTIVE_SENSOR_WIDTH) + x;
                    out[index] = __USAT(((int32_t) out[index]) + contrast, UINT8_T_BITS);
                }
                break;
            }
            default: {
                break;
            }
        }
    }

    memcpy(image->data, out, ACTIVE_SENSOR_SIZE);
    fb_free();
    #endif // (OMV_GENX320_EHC_ENABLE == 1)

    if (csi.color_palette && (framebuffer_get_buffer_size() >= (ACTIVE_SENSOR_SIZE * sizeof(uint16_t)))) {
        for (int32_t i = ACTIVE_SENSOR_SIZE - 1; i >= 0; i--) {
            ((uint16_t *) image->data)[i] = csi.color_palette[image->data[i]];
        }
        image->pixfmt = PIXFORMAT_RGB565;
        MAIN_FB()->pixfmt = PIXFORMAT_RGB565;
    }
}

static int snapshot(omv_csi_t *csi, image_t *image, uint32_t flags) {
    #if (OMV_GENX320_CAL_ENABLE == 1)
    if (!hot_pixels_disabled) {
        uint8_t *histogram = fb_alloc0(ACTIVE_SENSOR_SIZE, FB_ALLOC_NO_HINT);

        // Collect events to calibrate hot pixels.
        for (uint32_t i = 0; i < EVENT_THRESHOLD_TO_CALIBRATE; ) {
            int ret = omv_csi_snapshot(csi, image, flags);

            if (ret < 0) {
                return ret;
            }

            // Build histogram of events.
            #if (OMV_GENX320_EHC_ENABLE == 1)
            for (uint32_t j = 0; j < ACTIVE_SENSOR_SIZE; j++) {
                int32_t val = abs(((int8_t *) image->data)[j]);
                histogram[j] = val;
                i += val;
            }
            #else
            for (uint32_t j = 0; j < (ACTIVE_SENSOR_SIZE / sizeof(uint32_t)); j++) {
                uint32_t val = ((uint32_t *) image->data)[j];
                switch (__EVT20_TYPE(val)) {
                    case TD_LOW:
                    case TD_HIGH: {
                        uint32_t x = __EVT20_X(val);
                        uint32_t y = __EVT20_Y(val);
                        if ((x < ACTIVE_SENSOR_WIDTH) && (y < ACTIVE_SENSOR_HEIGHT)) {
                            uint32_t index = (y * ACTIVE_SENSOR_WIDTH) + x;
                            histogram[index] = __USAT(histogram[index] + 1, UINT8_T_BITS);
                            i++;
                        }
                        break;
                    }
                    default: {
                        break;
                    }
                }
            }
            #endif // (OMV_GENX320_EHC_ENABLE == 1)

            snapshot_post_process(image);
        }

        disable_hot_pixels(histogram);

        fb_free();
        hot_pixels_disabled = true;
    }
    #endif // (OMV_GENX320_CAL_ENABLE == 1)

    int ret = omv_csi_snapshot(csi, image, flags);

    if (ret < 0) {
        return ret;
    }

    snapshot_post_process(image);
    return ret;
}

static int ioctl(omv_csi_t *csi, int request, va_list ap) {
    int ret = 0;

    switch (request) {
        // Setting a preset of biases tuned for a particular application/condition
        case OMV_CSI_IOCTL_GENX320_SET_BIASES: {
            int mode = va_arg(ap, int);
            switch (mode) {
                case OMV_CSI_GENX320_BIASES_DEFAULT: {
                    // Set default biases V2.0.0
                    psee_sensor_set_bias(DIFF, 51);
                    psee_sensor_set_bias(DIFF_OFF, 28);
                    psee_sensor_set_bias(DIFF_ON, 25);
                    psee_sensor_set_bias(FO, 34);
                    psee_sensor_set_bias(HPF, 40);
                    psee_sensor_set_bias(REFR, 10);
                    break;
                }
                case OMV_CSI_GENX320_BIASES_LOW_LIGHT: {
                    // Set biases tuned for low light
                    psee_sensor_set_bias(DIFF, 51);
                    psee_sensor_set_bias(DIFF_OFF, 19);
                    psee_sensor_set_bias(DIFF_ON, 24);
                    psee_sensor_set_bias(FO, 19);
                    psee_sensor_set_bias(HPF, 0);
                    psee_sensor_set_bias(REFR, 10);
                    break;
                }
                case OMV_CSI_GENX320_BIASES_ACTIVE_MARKER: {
                    // Set biases tuned for active marker
                    psee_sensor_set_bias(DIFF, 51);
                    psee_sensor_set_bias(DIFF_OFF, 45); //127
                    psee_sensor_set_bias(DIFF_ON, 55); //78
                    psee_sensor_set_bias(FO, 50);
                    psee_sensor_set_bias(HPF, 127);
                    psee_sensor_set_bias(REFR, 0);
                    break;
                }
                case OMV_CSI_GENX320_BIASES_LOW_NOISE: {
                    // Set low sensitivity low noise biases
                    psee_sensor_set_bias(DIFF, 51);
                    psee_sensor_set_bias(DIFF_OFF, 38);
                    psee_sensor_set_bias(DIFF_ON, 35);
                    psee_sensor_set_bias(FO, 24);
                    psee_sensor_set_bias(HPF, 40);
                    psee_sensor_set_bias(REFR, 10);
                    break;
                }
                case OMV_CSI_GENX320_BIASES_HIGH_SPEED: {
                    // Set biases tuned for high speed motion
                    psee_sensor_set_bias(DIFF, 51);
                    psee_sensor_set_bias(DIFF_OFF, 26);
                    psee_sensor_set_bias(DIFF_ON, 37);
                    psee_sensor_set_bias(FO, 38);
                    psee_sensor_set_bias(HPF, 74);
                    psee_sensor_set_bias(REFR, 25);
                    break;
                }
                default: {
                    ret = -1;
                    break;
                }
            }
            break;
        }
        default: {
            ret = -1;
            break;
        }
    }

    return ret;
}

int genx320_init(omv_csi_t *csi) {
    // Initialize csi structure
    csi->reset = reset;
    csi->sleep = sleep;
    csi->read_reg = read_reg;
    csi->write_reg = write_reg;
    csi->set_pixformat = set_pixformat;
    csi->set_framesize = set_framesize;
    csi->set_framerate = set_framerate;
    csi->set_contrast = set_contrast;
    csi->set_brightness = set_brightness;
    csi->set_colorbar = set_colorbar;
    csi->set_hmirror = set_hmirror;
    csi->set_vflip = set_vflip;
    csi->snapshot = snapshot;
    csi->ioctl = ioctl;

    // Set csi flags
    csi->mono_bpp = sizeof(uint8_t);

    return 0;
}
#endif // (OMV_GENX320_ENABLE == 1)
