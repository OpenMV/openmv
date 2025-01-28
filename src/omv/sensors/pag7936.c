/*
 * Copyright (C) 2023-2024 OpenMV, LLC.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 * 3. Any redistribution, use, or modification in source or binary form
 *    is done solely for personal benefit and not for any commercial
 *    purpose or for monetary gain. For commercial licensing options,
 *    please contact openmv@openmv.io
 *
 * THIS SOFTWARE IS PROVIDED BY THE LICENSOR AND COPYRIGHT OWNER "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE LICENSOR OR COPYRIGHT
 * OWNER BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * PixArt PAG7936 driver.
 */
#include "omv_boardconfig.h"
#if (OMV_PAG7936_ENABLE == 1)

#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "omv_i2c.h"
#include "omv_csi.h"
#include "pag7936.h"
#include "py/mphal.h"

#define FRAME_TIME_20_16            (0x004E)
#define FRAME_TIME_15_8             (0x004D)
#define FRAME_TIME_7_0              (0x004C)
#define AE_START_DIV_4_X_7_0        (0x0482)
#define AE_START_DIV_4_X_8_7        (0x0483)
#define AE_START_DIV_4_Y            (0x0484)
#define AE_SIZE_DIV_4_X_7_0         (0x0485)
#define AE_SIZE_DIV_4_X_8_7         (0x0486)
#define AE_SIZE_DIV_4_Y             (0x0487)
#define AE_EXPO_MANUAL              (0x1400)
#define AE_EXPO_MANUAL_AE_MANUAL_EN (0x10)
#define AE_EXPO_MANUAL_AE_ENH       (0x1)
#define AE_FREQ_60                  (0x1404)
#define AE_YTAR8BIT                 (0x1406)
#define AE_LOCKRANGE_IN             (0x1407)
#define AE_LOCKRANGE_OUT_LB         (0x1408)
#define AE_LOCKRANGE_OUT_UB         (0x1409)
#define AE_STABLE_7_0               (0x145B)
#define AE_MINGAIN_7_0              (0x140A)
#define AE_MINGAIN_10_8             (0x140B)
#define AE_MAXGAIN_7_0              (0x140C)
#define AE_MAXGAIN_10_8             (0x140D)
#define AE_MINEXPO_7_0              (0x140E)
#define AE_MINEXPO_15_8             (0x140F)
#define AE_MINEXPO_17_16            (0x1410)
#define AE_MAXEXPO_7_0              (0x1412)
#define AE_MAXEXPO_15_8             (0x1413)
#define AE_MAXEXPO_17_16            (0x1414)
#define AE_GAIN_MANUAL_7_0          (0x1423)
#define AE_GAIN_MANUAL_10_8         (0x1424)
#define AE_EXP_LINE_NUM_7_0         (0x1450)
#define AE_EXP_LINE_NUM_15_8        (0x1451)
#define AE_EXP_LINE_NUM_17_16       (0x1452)
#define TOTAL_GAIN_7_0              (0x1453)
#define TOTAL_GAIN_10_8             (0x1454)
#define AE_EXPO_MANUAL_17_16        (0x1427)
#define AE_EXPO_MANUAL_15_8         (0x1426)
#define AE_EXPO_MANUAL_7_0          (0x1425)
#define SENSOR_UPDATE               (0x00EB)
#define SENSOR_UPDATE_FLAG          (0x80)
#define INTERFACE_POLARITY          (0x0EAF)
#define SENSOR_OPMODE               (0x0008)
#define SENSOR_OPMODE_RUN           (0x83)
#define SENSOR_OPMODE_SUSPEND       (0x85)
#define SENSOR_TRIGGER_FRAMENUM     (0x002E)
#define SENSOR_TRIGGER_EN           (0x002F)
#define SENSOR_TRIGGER_EN_FLAG      (0x01)
#define SENSOR_TRIGGER_MODE_SOFT    (0x00)
#define SENSOR_TRIGGER_MODE_GPIO0   (0x10)
#define SENSOR_TRIGGER_MODE_GPIO1   (0x20)
#define SENSOR_TRIGGER_MODE_GPIO2   (0x30)
#define SENSOR_TG_EN                (0x0030)
#define SENSOR_TG_EN_FLAG           (0x01)
#define SENSOR_TRIGGER_MODE         (0x0031)
#define SENSOR_TRIGGER_MODE_EN      (0x01)
#define SENSOR_TRIGGER_MODE_0       (0x00)
#define SENSOR_TRIGGER_MODE_1       (0x02)
#define SENSOR_SOFTWARE_TRIGGER     (0x00EA)
#define ISP_EN_H                    (0x0800)
#define ISP_EN_H_EN                 (0x01)
#define ISP_TEST_MODE               (0x0801)
#define ISP_TEST_MODE_RAMP          (0x04)
#define ISP_WOI_EN                  (0x0E10)
#define ISP_WOI_HSIZE_L             (0x0E11)
#define ISP_WOI_HSIZE_H             (0x0E12)
#define ISP_WOI_VSIZE_L             (0x0E13)
#define ISP_WOI_VSIZE_H             (0x0E14)
#define ISP_WOI_HOFFSET_L           (0x0E15)
#define ISP_WOI_HOFFSET_H           (0x0E16)
#define ISP_WOI_VOFFSET_L           (0x0E17)
#define ISP_WOI_VOFFSET_H           (0x0E18)
#define TG_MONO_SENSOR              (0x01BF)
#define TG_FLIP                     (0x01CE)
#define TG_FLIP_SET_VFLIP(r, x)     ((r & 0xFB) | ((x & 1) << 2))
#define TG_FLIP_SET_HFLIP(r, x)     ((r & 0xF7) | ((x & 1) << 3))
#define AVERAGE_MODE                (0x01C0)
#define ROW_AVERAGE_MODE            (0x0166)
#define COL_AVERAGE_MODE            (0x016F)
#define HSIZE_L                     (0x01C6)
#define HSIZE_H                     (0x01C7)
#define VSIZE_L                     (0x01C8)
#define VSIZE_H                     (0x01C9)
#define WOI_HSIZE_L                 (0x0221)
#define WOI_HSIZE_H                 (0x0222)
#define WOI_VSIZE_L                 (0x0223)
#define WOI_VSIZE_H                 (0x0224)
#define WOI_HSTART_L                (0x0225)
#define WOI_HSTART_H                (0x0226)
#define WOI_VSTART_L                (0x0227)
#define WOI_VSTART_H                (0x0228)

#define RGB_STAT_B_VS               (0x04C9)
#define RGB_STAT_GB_VS              (0x04CD)
#define RGB_STAT_R_VS               (0x04D1)
#define RGB_STAT_GR_VS              (0x04D5)

#define FT_CLK                      (1000000)
#define FPS_MAX                     (120)

#define FMAX_720P_120_LINEAR        (1041)
#define LT_T_RATIO                  (1000000 / FPS_MAX / FMAX_720P_120_LINEAR / 8)    // around ~1.0 with current setting
#define ConvertL2T(line)            (line * LT_T_RATIO)   // 1-Line time = 8usec = 1T, return T
#define ConvertT2L(t)               ((t + (LT_T_RATIO / 2)) / LT_T_RATIO) // 1-Line time = 8usec = 1T, return Lines

#define PAG7936_MIN_INT             (ConvertT2L(10)) // ExpLine min vlaue
#define PAG7936_MAX_INT             ((FMAX_720P_120_LINEAR - 11)) // ExpLine max vlaue
#define PAG7936_MIN_AGAIN           (1472)  // AGain min value, unit is 1/1024x
#define PAG7936_MAX_AGAIN           (16384) // AGain max value, unit is 1/1024x
#define PAG7936_MIN_AGAIN_REG       ((PAG7936_MIN_AGAIN) / 64)
#define PAG7936_MAX_AGAIN_REG       ((PAG7936_MAX_AGAIN) / 64)

#define PAG7936_GAIN_SCALE          (16)
#define PAG7936_GAIN_SCALE_F        ((PAG7936_GAIN_SCALE) * 1.0f)

#define PAG7936_EXP_OFFSET          (80)
#define PAG7936_EXP_MIN             (80)
#define PAG7936_EXP_DIV             (8)

#define PAG7936_FRAME_TIME(h, m, l) ((((h) & 0x1f) << 16) | ((m) << 8) | (l))
#define PAG7936_FRAME_TIME_H(r, ft) (((r) & 0xe0) | (((ft) >> 16) & 0x1f))
#define PAG7936_FRAME_TIME_M(ft)    (((ft) >> 8) & 0xff)
#define PAG7936_FRAME_TIME_L(ft)    ((ft) & 0xff)

#define PAG7936_GAIN(h, l)          ((((h) & 0x7) << 8) | (l))
#define PAG7936_GAIN_H(r, gain)     (((r) & 0xf8) | (((gain) >> 8) & 0x7))
#define PAG7936_GAIN_L(gain)        ((gain) & 0xff)

#define PAG7936_EXPOSURE(h, m, l)   (((((h) & 0x3) << 16) | ((m) << 8) | (l)) * 8)
#define PAG7936_EXPOSURE_H(r, exp)  (((r) & 0xfc) | (((exp) >> 16) & 0x3))
#define PAG7936_EXPOSURE_M(exp)     (((exp) >> 8) & 0xff)
#define PAG7936_EXPOSURE_L(exp)     ((exp) & 0xff)

#if OMV_PAG7936_MIPI_CSI2
#define PAG7936_WIDTH_ALIGN         (8)
#define PAG7936_QVGA_FPS_MAX        (480)
#define PAG7936_VGA_FPS_MAX         (240)
#define PAG7936_HD_FPS_MAX          (120)
#else
#define PAG7936_WIDTH_ALIGN         (4)
#define PAG7936_QVGA_FPS_MAX        (240)
#define PAG7936_VGA_FPS_MAX         (120)
#define PAG7936_HD_FPS_MAX          (60)
#endif

static const uint16_t default_regs[][2] = {
    #if OMV_PAG7936_MIPI_CSI2
    { 0x004C,   0x8D },
    { 0x004D,   0x20 },
    { 0x004E,   0x00 },
    { 0x004F,   0x00 },
    { 0x110A,   0x00 },
    { 0x0905,   0x60 },
    { 0x0978,   0x01 },
    { 0x0979,   0x66 },
    { 0x097C,   0x30 },
    { 0x097D,   0x02 },
    { 0x0985,   0x06 },
    { 0x0989,   0x10 },
    { 0x098B,   0x91 },
    { 0x098E,   0x06 },
    { 0x0850,   0x00 },
    { 0x094A,   0x43 },
    { 0x09A0,   0x08 },
    { 0x0032,   0x00 },
    { 0x0033,   0x00 },
    { 0x0764,   0x00 },
    { 0x0304,   0x30 },
    { 0x0305,   0x03 },
    { 0x0307,   0x10 },
    { 0x0308,   0x05 },
    { 0x0306,   0x04 },
    { 0x0311,   0x00 },
    { 0x030F,   0x01 },
    { 0x0168,   0x6D },
    { 0x0730,   0x78 },
    { 0x0724,   0x20 },
    { 0x0188,   0x14 },
    { 0x02A5,   0xEF },
    { 0x0186,   0x40 },
    { 0x0A1A,   0x20 },
    { 0x0A32,   0x2A },
    { 0x0A33,   0x2A },
    { 0x0A34,   0x0D },
    { 0x0A35,   0x0D },
    { 0x000B,   0x01 },
    #else
    { 0x010C,   0x23 },
    { 0x010D,   0x05 },
    { 0x007B,   0x8C },
    { 0x007C,   0x8C },
    { 0x007D,   0x8C },
    { 0x007E,   0x8C },
    { 0x007F,   0x8C },
    { 0x0EAF,   0x31 }, // Interface polarity (Not default)
    { 0x004C,   0x1A }, // frame time
    { 0x004D,   0x41 }, // ftime
    { 0x004E,   0x00 }, // ftime
    { 0x004F,   0x00 },
    { 0x110A,   0x00 },
    { 0x0905,   0x60 },
    { 0x0978,   0x01 },
    { 0x0979,   0x64 },
    { 0x097C,   0xE6 },
    { 0x097D,   0x00 },
    { 0x097E,   0x08 },
    { 0x097F,   0x02 },
    { 0x0985,   0x10 },
    { 0x098B,   0x90 },
    { 0x098E,   0x06 },
    { 0x0850,   0x00 }, // TODO BPC
    { 0x094A,   0x43 },
    { 0x09A0,   0x08 },
    { 0x0724,   0x10 },
    { 0x0110,   0x1B },
    { 0x0111,   0x03 },
    { 0x0114,   0xF4 },
    { 0x0186,   0x40 },
    { 0x0188,   0x14 },
    { 0x02A5,   0xEF },
    { 0x016B,   0x5D },
    { 0x0634,   0xE0 },
    { 0x0635,   0x01 },
    { 0x0646,   0x12 },
    { 0x0647,   0x02 },
    { 0x064E,   0x3A },
    { 0x064F,   0x02 },
    { 0x0654,   0x30 },
    { 0x0655,   0x02 },
    { 0x065A,   0x3F },
    { 0x065B,   0x02 },
    { 0x066A,   0x7B },
    { 0x066B,   0x02 },
    { 0x066E,   0xD2 },
    { 0x0683,   0x9E },
    { 0x0684,   0x02 },
    { 0x0687,   0xF5 },
    { 0x06B5,   0xA8 },
    { 0x06B6,   0x02 },
    { 0x06B9,   0xFF },
    { 0x06C3,   0xA3 },
    { 0x06C4,   0x02 },
    { 0x06C7,   0xFA },
    { 0x06D1,   0x12 },
    { 0x06D2,   0x02 },
    { 0x06D7,   0x76 },
    { 0x06D8,   0x02 },
    { 0x06DB,   0xCD },
    { 0x0A1A,   0x20 },
    { 0x0A32,   0x2A },
    { 0x0A33,   0x2A },
    { 0x0A34,   0x0D },
    { 0x0A35,   0x0D },
    { 0x000B,   0x02 },
    #endif
    { 0x0810,   0x01 },
    { 0x0814,   0xB3 }, //R_center_rx[10:0]=691
    { 0x0815,   0x02 }, //R_center_rx[10:0]=691
    { 0x0816,   0xBB }, //R_center_ry[9:0]=443
    { 0x0817,   0x01 }, //R_center_ry[9:0]=443
    { 0x0818,   0xA9 }, //R_center_gx[10:0]=681
    { 0x0819,   0x02 }, //R_center_gx[10:0]=681
    { 0x081A,   0xBB }, //R_center_gy[9:0]=443
    { 0x081B,   0x01 }, //R_center_gy[9:0]=443
    { 0x081C,   0xB0 }, //R_center_bx[10:0]=688
    { 0x081D,   0x02 }, //R_center_bx[10:0]=688
    { 0x081E,   0xBD }, //R_center_by[9:0]=445
    { 0x081F,   0x01 }, //R_center_by[9:0]=445
    { 0x0820,   0x61 }, //R_LSC_RS[7:0]=97
    { 0x0821,   0x5B }, //R_LSC_GS[7:0]=91
    { 0x0822,   0x5F }, //R_LSC_BS[7:0]=95
    { 0x0823,   0x41 }, //R_LSC_RQ[7:0]=65
    { 0x0824,   0x50 }, //R_LSC_GQ[7:0]=80
    { 0x0825,   0x7F }, //R_LSC_BQ[7:0]=127
    { 0x0826,   0x07 }, //R_LSC_SftRS[3:0]=7
    { 0x0827,   0x07 }, //R_LSC_SftGS[3:0]=7
    { 0x0828,   0x07 }, //R_LSC_SftBS[3:0]=7
    { 0x0829,   0x09 }, //R_LSC_SftRQ[3:0]=9
    { 0x082A,   0x09 }, //R_LSC_SftGQ[3:0]=9
    { 0x082B,   0x09 }, //R_LSC_SftBQ[3:0]=9
    { 0x082E,   0xD8 }, //R_LSC_LMaxR2R[15:0]=39640
    { 0x082F,   0x9A }, //R_LSC_LMaxR2R[15:0]=39640
    { 0x0830,   0xD8 }, //R_LSC_LMaxR2G[15:0]=39640
    { 0x0831,   0x9A }, //R_LSC_LMaxR2G[15:0]=39640
    { 0x0832,   0xD8 }, //R_LSC_LMaxR2B[15:0]=39640
    { 0x0833,   0x9A }, //R_LSC_LMaxR2B[15:0]=39640

    // WOI
    { 0x0E10,   0x01 },
    { 0x0E11,   0x00 },
    { 0x0E12,   0x05 },
    { 0x0E13,   0x20 },
    { 0x0E14,   0x03 },
    { 0x0E15,   0x10 },
    { 0x0E16,   0x00 },
    { 0x0E17,   0x10 },
    { 0x0E18,   0x00 },

    { 0x00EB,   0x80 },
    { 0x0030,   0x01 },
    { 0x0008,   0x83 },
    { 0x0000,   0x00 },
};

static const uint16_t qvga_regs[][2] = {
    { TG_MONO_SENSOR,       0x00 },
    { AVERAGE_MODE,         0x02 },
    { ROW_AVERAGE_MODE,     0x00 },
    { COL_AVERAGE_MODE,     0x00 },
    { ISP_WOI_EN,           0x00 },
    { HSIZE_L,              0x44 },
    { HSIZE_H,              0x01 },
    { VSIZE_L,              0xCC },
    { VSIZE_H,              0x00 },
    { WOI_HSIZE_L,          0x40 },
    { WOI_HSIZE_H,          0x01 },
    { WOI_VSIZE_L,          0xC8 },
    { WOI_VSIZE_H,          0x00 },
    { WOI_HSTART_L,         0x02 },
    { WOI_HSTART_H,         0x00 },
    { WOI_VSTART_L,         0x02 },
    { WOI_VSTART_H,         0x00 },
    { SENSOR_UPDATE,        0x80 },
    { 0x0000,               0x00 },
};

static const uint16_t vga_regs[][2] = {
    { TG_MONO_SENSOR,       0x00 },
    { AVERAGE_MODE,         0x01 },
    { ROW_AVERAGE_MODE,     0x00 },
    { COL_AVERAGE_MODE,     0x00 },
    { ISP_WOI_EN,           0x00 },
    { HSIZE_L,              0x88 },
    { HSIZE_H,              0x02 },
    { VSIZE_L,              0x98 },
    { VSIZE_H,              0x01 },
    { WOI_HSIZE_L,          0x80 },
    { WOI_HSIZE_H,          0x02 },
    { WOI_VSIZE_L,          0x90 },
    { WOI_VSIZE_H,          0x01 },
    { WOI_HSTART_L,         0x04 },
    { WOI_HSTART_H,         0x00 },
    { WOI_VSTART_L,         0x04 },
    { WOI_VSTART_H,         0x00 },
    { SENSOR_UPDATE,        0x80 },
    { 0x0000,               0x00 },
};

static const uint16_t hd_regs[][2] = {
    { TG_MONO_SENSOR,       0x00 },
    { AVERAGE_MODE,         0x00 },
    { ROW_AVERAGE_MODE,     0x00 },
    { COL_AVERAGE_MODE,     0x00 },
    { ISP_WOI_EN,           0x00 },
    { HSIZE_L,              0x10 },
    { HSIZE_H,              0x05 },
    { VSIZE_L,              0x30 },
    { VSIZE_H,              0x03 },
    { WOI_HSIZE_L,          0x00 },
    { WOI_HSIZE_H,          0x05 },
    { WOI_VSIZE_L,          0x20 },
    { WOI_VSIZE_H,          0x03 },
    { WOI_HSTART_L,         0x08 },
    { WOI_HSTART_H,         0x00 },
    { WOI_VSTART_L,         0x08 },
    { WOI_VSTART_H,         0x00 },
    { SENSOR_UPDATE,        0x80 },
    { 0x0000,               0x00 },
};

static int reset(omv_csi_t *csi) {
    int ret = 0;
    // Write default registers
    for (int i = 0; default_regs[i][0] && ret == 0; i++) {
        ret |= omv_i2c_writeb2(&csi->i2c_bus, csi->slv_addr, default_regs[i][0], default_regs[i][1]);
    }
    return ret;
}

static int sleep(omv_csi_t *csi, int enable) {
    int ret = omv_i2c_writeb2(&csi->i2c_bus, csi->slv_addr, SENSOR_TG_EN, SENSOR_TG_EN_FLAG);
    ret |= omv_i2c_writeb2(&csi->i2c_bus, csi->slv_addr, SENSOR_OPMODE,
                           enable ? SENSOR_OPMODE_SUSPEND : SENSOR_OPMODE_RUN);
    return 0;
}

static int read_reg(omv_csi_t *csi, uint16_t reg) {
    uint8_t reg_data;
    if (omv_i2c_readb2(&csi->i2c_bus, csi->slv_addr, reg, &reg_data) != 0) {
        return -1;
    }
    return reg_data;
}

static int write_reg(omv_csi_t *csi, uint16_t reg, uint16_t reg_data) {
    return omv_i2c_writeb2(&csi->i2c_bus, csi->slv_addr, reg, reg_data);
}

static int read_reg_seq(omv_csi_t *csi, uint16_t addr, size_t size, uint8_t *buf) {
    int ret = 0;
    for (size_t i = 0; i < size; i++) {
        ret |= omv_i2c_readb2(&csi->i2c_bus, csi->slv_addr, addr + i, &buf[i]);
    }
    return ret;
}

static int set_pixformat(omv_csi_t *csi, pixformat_t pixformat) {
    switch (pixformat) {
        case PIXFORMAT_RGB565:
        case PIXFORMAT_BAYER:
        case PIXFORMAT_GRAYSCALE:
            return 0;
        default:
            return -1;
    }
}

static int set_framesize(omv_csi_t *csi, omv_csi_framesize_t framesize) {
    int ret = 0;
    const uint16_t(*regs)[2];

    switch (framesize) {
        case OMV_CSI_FRAMESIZE_HD:
            regs = hd_regs;
            break;
        case OMV_CSI_FRAMESIZE_VGA:
            regs = vga_regs;
            break;
        case OMV_CSI_FRAMESIZE_QVGA:
            regs = qvga_regs;
            break;
        default:
            return -1;
    }

    for (int i = 0; regs[i][0] && ret == 0; i++) {
        ret |= omv_i2c_writeb2(&csi->i2c_bus, csi->slv_addr, regs[i][0], regs[i][1]);
    }

    return ret;
}

static int set_framerate(omv_csi_t *csi, int framerate) {
    uint8_t reg, exposure_us_17_16, exposure_us_15_8, exposure_us_7_0;
    int ret = 0;

    switch (csi->framesize) {
        case OMV_CSI_FRAMESIZE_HD:
            framerate = IM_MIN(framerate, PAG7936_HD_FPS_MAX);
            break;
        case OMV_CSI_FRAMESIZE_VGA:
            framerate = IM_MIN(framerate, PAG7936_VGA_FPS_MAX);
            break;
        case OMV_CSI_FRAMESIZE_QVGA:
            framerate = IM_MIN(framerate, PAG7936_QVGA_FPS_MAX);
            break;
        default:
            return -1;
    }

    int32_t frame_time = FT_CLK / framerate;

    ret |= omv_i2c_readb2(&csi->i2c_bus, csi->slv_addr, FRAME_TIME_20_16, &reg);
    ret |= omv_i2c_writeb2(&csi->i2c_bus, csi->slv_addr, FRAME_TIME_20_16, PAG7936_FRAME_TIME_H(reg, frame_time));
    ret |= omv_i2c_writeb2(&csi->i2c_bus, csi->slv_addr, FRAME_TIME_15_8, PAG7936_FRAME_TIME_M(frame_time));
    ret |= omv_i2c_writeb2(&csi->i2c_bus, csi->slv_addr, FRAME_TIME_7_0, PAG7936_FRAME_TIME_L(frame_time));

    ret |= omv_i2c_readb2(&csi->i2c_bus, csi->slv_addr, AE_EXPO_MANUAL, &reg);

    if (reg & AE_EXPO_MANUAL_AE_MANUAL_EN) {
        ret |= omv_i2c_readb2(&csi->i2c_bus, csi->slv_addr, AE_EXPO_MANUAL_17_16, &exposure_us_17_16);
        ret |= omv_i2c_readb2(&csi->i2c_bus, csi->slv_addr, AE_EXPO_MANUAL_15_8, &exposure_us_15_8);
        ret |= omv_i2c_readb2(&csi->i2c_bus, csi->slv_addr, AE_EXPO_MANUAL_7_0, &exposure_us_7_0);
    } else {
        ret |= omv_i2c_readb2(&csi->i2c_bus, csi->slv_addr, AE_MAXEXPO_17_16, &exposure_us_17_16);
        ret |= omv_i2c_readb2(&csi->i2c_bus, csi->slv_addr, AE_MAXEXPO_15_8, &exposure_us_15_8);
        ret |= omv_i2c_readb2(&csi->i2c_bus, csi->slv_addr, AE_MAXEXPO_7_0, &exposure_us_7_0);
    }

    int32_t exposure_us = PAG7936_EXPOSURE(exposure_us_17_16, exposure_us_15_8, exposure_us_7_0);
    exposure_us = IM_CLAMP(exposure_us, PAG7936_EXP_MIN, (frame_time - PAG7936_EXP_OFFSET)) / PAG7936_EXP_DIV;

    if (reg & AE_EXPO_MANUAL_AE_MANUAL_EN) {
        ret |= omv_i2c_readb2(&csi->i2c_bus, csi->slv_addr, AE_EXPO_MANUAL_17_16, &reg);
        ret |= omv_i2c_writeb2(&csi->i2c_bus, csi->slv_addr, AE_EXPO_MANUAL_17_16, PAG7936_EXPOSURE_H(reg, exposure_us));
        ret |= omv_i2c_writeb2(&csi->i2c_bus, csi->slv_addr, AE_EXPO_MANUAL_15_8, PAG7936_EXPOSURE_M(exposure_us));
        ret |= omv_i2c_writeb2(&csi->i2c_bus, csi->slv_addr, AE_EXPO_MANUAL_7_0, PAG7936_EXPOSURE_L(exposure_us));
    } else {
        ret |= omv_i2c_readb2(&csi->i2c_bus, csi->slv_addr, AE_MAXEXPO_17_16, &reg);
        ret |= omv_i2c_writeb2(&csi->i2c_bus, csi->slv_addr, AE_MAXEXPO_17_16, PAG7936_EXPOSURE_H(reg, exposure_us));
        ret |= omv_i2c_writeb2(&csi->i2c_bus, csi->slv_addr, AE_MAXEXPO_15_8, PAG7936_EXPOSURE_M(exposure_us));
        ret |= omv_i2c_writeb2(&csi->i2c_bus, csi->slv_addr, AE_MAXEXPO_7_0, PAG7936_EXPOSURE_L(exposure_us));
    }

    ret |= omv_i2c_writeb2(&csi->i2c_bus, csi->slv_addr, SENSOR_UPDATE, SENSOR_UPDATE_FLAG);
    return ret;
}

static int set_gainceiling(omv_csi_t *csi, omv_csi_gainceiling_t gainceiling) {
    uint8_t aec, reg;
    int ret = omv_i2c_readb2(&csi->i2c_bus, csi->slv_addr, AE_EXPO_MANUAL, &aec);
    ret |= omv_i2c_writeb2(&csi->i2c_bus, csi->slv_addr, AE_EXPO_MANUAL, aec | AE_EXPO_MANUAL_AE_MANUAL_EN);

    int new_gainceiling = PAG7936_GAIN_SCALE << (gainceiling + 1);
    if (new_gainceiling > PAG7936_MAX_AGAIN_REG) {
        return -1;
    }

    ret |= omv_i2c_readb2(&csi->i2c_bus, csi->slv_addr, AE_MAXGAIN_10_8, &reg);
    ret |= omv_i2c_writeb2(&csi->i2c_bus, csi->slv_addr, AE_MAXGAIN_10_8, PAG7936_GAIN_H(reg, new_gainceiling));
    ret |= omv_i2c_writeb2(&csi->i2c_bus, csi->slv_addr, AE_MAXGAIN_7_0, PAG7936_GAIN_L(new_gainceiling));

    // Force AEC/AGC to reload the new values.
    ret |= omv_i2c_writeb2(&csi->i2c_bus, csi->slv_addr, SENSOR_UPDATE, SENSOR_UPDATE_FLAG);
    ret |= omv_i2c_writeb2(&csi->i2c_bus, csi->slv_addr, AE_EXPO_MANUAL, aec);
    ret |= omv_i2c_writeb2(&csi->i2c_bus, csi->slv_addr, SENSOR_UPDATE, SENSOR_UPDATE_FLAG);
    return ret;
}

static int set_colorbar(omv_csi_t *csi, int enable) {
    uint8_t reg;
    int ret = omv_i2c_readb2(&csi->i2c_bus, csi->slv_addr, ISP_EN_H, &reg);
    ret |= omv_i2c_writeb2(&csi->i2c_bus, csi->slv_addr, ISP_EN_H,
                           (reg & ~ISP_EN_H_EN) | (enable ? ISP_EN_H_EN : 0));
    ret |= omv_i2c_readb2(&csi->i2c_bus, csi->slv_addr, ISP_TEST_MODE, &reg);
    ret |= omv_i2c_writeb2(&csi->i2c_bus, csi->slv_addr, ISP_TEST_MODE,
                           (reg & ~ISP_TEST_MODE_RAMP) | (enable ? ISP_TEST_MODE_RAMP : 0));
    ret |= omv_i2c_writeb2(&csi->i2c_bus, csi->slv_addr, SENSOR_UPDATE, SENSOR_UPDATE_FLAG);
    return ret;
}

static int set_auto_gain(omv_csi_t *csi, int enable, float gain_db, float gain_db_ceiling) {
    uint8_t reg;
    int ret = omv_i2c_readb2(&csi->i2c_bus, csi->slv_addr, AE_EXPO_MANUAL, &reg);
    ret |= omv_i2c_writeb2(&csi->i2c_bus, csi->slv_addr, AE_EXPO_MANUAL, reg | AE_EXPO_MANUAL_AE_MANUAL_EN);

    if ((enable == 0) && (!isnanf(gain_db)) && (!isinff(gain_db))) {
        int gain = fast_roundf(expf((gain_db / 20.0f) * M_LN10) * PAG7936_GAIN_SCALE_F);
        gain = IM_CLAMP(gain, PAG7936_MIN_AGAIN_REG, PAG7936_MAX_AGAIN_REG);

        ret |= omv_i2c_readb2(&csi->i2c_bus, csi->slv_addr, AE_GAIN_MANUAL_10_8, &reg);
        ret |= omv_i2c_writeb2(&csi->i2c_bus, csi->slv_addr, AE_GAIN_MANUAL_10_8, PAG7936_GAIN_H(reg, gain));
        ret |= omv_i2c_writeb2(&csi->i2c_bus, csi->slv_addr, AE_GAIN_MANUAL_7_0, PAG7936_GAIN_L(gain));
    } else if ((enable != 0) && (!isnanf(gain_db_ceiling)) && (!isinff(gain_db_ceiling))) {
        int gain_ceiling = fast_roundf(expf((gain_db_ceiling / 20.0f) * M_LN10) * PAG7936_GAIN_SCALE_F);
        gain_ceiling = IM_CLAMP(gain_ceiling, PAG7936_MIN_AGAIN_REG, PAG7936_MAX_AGAIN_REG);

        ret |= omv_i2c_readb2(&csi->i2c_bus, csi->slv_addr, AE_MAXGAIN_10_8, &reg);
        ret |= omv_i2c_writeb2(&csi->i2c_bus, csi->slv_addr, AE_MAXGAIN_10_8, PAG7936_GAIN_H(reg, gain_ceiling));
        ret |= omv_i2c_writeb2(&csi->i2c_bus, csi->slv_addr, AE_MAXGAIN_7_0, PAG7936_GAIN_L(gain_ceiling));
    }

    // Force AEC/AGC to reload the new values.
    ret |= omv_i2c_writeb2(&csi->i2c_bus, csi->slv_addr, SENSOR_UPDATE, SENSOR_UPDATE_FLAG);
    ret |= omv_i2c_readb2(&csi->i2c_bus, csi->slv_addr, AE_EXPO_MANUAL, &reg);
    ret |= omv_i2c_writeb2(&csi->i2c_bus, csi->slv_addr, AE_EXPO_MANUAL,
                           (reg & ~AE_EXPO_MANUAL_AE_MANUAL_EN) | (enable ? 0 : AE_EXPO_MANUAL_AE_MANUAL_EN));
    ret |= omv_i2c_writeb2(&csi->i2c_bus, csi->slv_addr, SENSOR_UPDATE, SENSOR_UPDATE_FLAG);
    return ret;
}

static int get_gain_db(omv_csi_t *csi, float *gain_db) {
    uint8_t gainh, gainl;
    int ret = 0;

    ret |= omv_i2c_readb2(&csi->i2c_bus, csi->slv_addr, TOTAL_GAIN_10_8, &gainh);
    ret |= omv_i2c_readb2(&csi->i2c_bus, csi->slv_addr, TOTAL_GAIN_7_0, &gainl);

    *gain_db = 20.0f * log10f(PAG7936_GAIN(gainh, gainl) / PAG7936_GAIN_SCALE_F);

    return ret;
}

static int set_auto_exposure(omv_csi_t *csi, int enable, int exposure_us) {
    uint8_t reg, frame_time_20_16, frame_time_15_8, frame_time_7_0;
    int ret = omv_i2c_readb2(&csi->i2c_bus, csi->slv_addr, AE_EXPO_MANUAL, &reg);
    ret |= omv_i2c_writeb2(&csi->i2c_bus, csi->slv_addr, AE_EXPO_MANUAL, reg | AE_EXPO_MANUAL_AE_MANUAL_EN);

    ret |= omv_i2c_readb2(&csi->i2c_bus, csi->slv_addr, FRAME_TIME_20_16, &frame_time_20_16);
    ret |= omv_i2c_readb2(&csi->i2c_bus, csi->slv_addr, FRAME_TIME_15_8, &frame_time_15_8);
    ret |= omv_i2c_readb2(&csi->i2c_bus, csi->slv_addr, FRAME_TIME_7_0, &frame_time_7_0);

    int32_t frame_time_us = PAG7936_FRAME_TIME(frame_time_20_16, frame_time_15_8, frame_time_7_0);
    exposure_us = IM_CLAMP(exposure_us, PAG7936_EXP_MIN, (frame_time_us - PAG7936_EXP_OFFSET)) / PAG7936_EXP_DIV;

    if ((enable == 0) && (exposure_us >= 0)) {
        ret |= omv_i2c_readb2(&csi->i2c_bus, csi->slv_addr, AE_EXPO_MANUAL_17_16, &reg);
        ret |= omv_i2c_writeb2(&csi->i2c_bus, csi->slv_addr, AE_EXPO_MANUAL_17_16, PAG7936_EXPOSURE_H(reg, exposure_us));
        ret |= omv_i2c_writeb2(&csi->i2c_bus, csi->slv_addr, AE_EXPO_MANUAL_15_8, PAG7936_EXPOSURE_M(exposure_us));
        ret |= omv_i2c_writeb2(&csi->i2c_bus, csi->slv_addr, AE_EXPO_MANUAL_7_0, PAG7936_EXPOSURE_L(exposure_us));
    } else if ((enable != 0) && (exposure_us >= 0)) {
        ret |= omv_i2c_readb2(&csi->i2c_bus, csi->slv_addr, AE_MAXEXPO_17_16, &reg);
        ret |= omv_i2c_writeb2(&csi->i2c_bus, csi->slv_addr, AE_MAXEXPO_17_16, PAG7936_EXPOSURE_H(reg, exposure_us));
        ret |= omv_i2c_writeb2(&csi->i2c_bus, csi->slv_addr, AE_MAXEXPO_15_8, PAG7936_EXPOSURE_M(exposure_us));
        ret |= omv_i2c_writeb2(&csi->i2c_bus, csi->slv_addr, AE_MAXEXPO_7_0, PAG7936_EXPOSURE_L(exposure_us));
    }

    // Force AEC/AGC to reload the new values.
    ret |= omv_i2c_writeb2(&csi->i2c_bus, csi->slv_addr, SENSOR_UPDATE, SENSOR_UPDATE_FLAG);
    ret |= omv_i2c_readb2(&csi->i2c_bus, csi->slv_addr, AE_EXPO_MANUAL, &reg);
    ret |= omv_i2c_writeb2(&csi->i2c_bus, csi->slv_addr, AE_EXPO_MANUAL,
                           (reg & ~AE_EXPO_MANUAL_AE_MANUAL_EN) | (enable ? 0 : AE_EXPO_MANUAL_AE_MANUAL_EN));
    ret |= omv_i2c_writeb2(&csi->i2c_bus, csi->slv_addr, SENSOR_UPDATE, SENSOR_UPDATE_FLAG);
    return ret;
}

static int get_exposure_us(omv_csi_t *csi, int *exposure_us) {
    uint8_t exposure_us_17_16, exposure_us_15_8, exposure_us_7_0;
    int ret = 0;

    ret |= omv_i2c_readb2(&csi->i2c_bus, csi->slv_addr, AE_EXP_LINE_NUM_17_16, &exposure_us_17_16);
    ret |= omv_i2c_readb2(&csi->i2c_bus, csi->slv_addr, AE_EXP_LINE_NUM_15_8, &exposure_us_15_8);
    ret |= omv_i2c_readb2(&csi->i2c_bus, csi->slv_addr, AE_EXP_LINE_NUM_7_0, &exposure_us_7_0);

    *exposure_us = PAG7936_EXPOSURE(exposure_us_17_16, exposure_us_15_8, exposure_us_7_0);

    return ret;
}

static int set_hmirror(omv_csi_t *csi, int enable) {
    uint8_t reg;
    int ret = omv_i2c_readb2(&csi->i2c_bus, csi->slv_addr, TG_FLIP, &reg);
    ret |= omv_i2c_writeb2(&csi->i2c_bus, csi->slv_addr, TG_FLIP, TG_FLIP_SET_HFLIP(reg, enable));
    ret |= omv_i2c_writeb2(&csi->i2c_bus, csi->slv_addr, SENSOR_UPDATE, SENSOR_UPDATE_FLAG);
    return ret;
}

static int set_vflip(omv_csi_t *csi, int enable) {
    uint8_t reg;
    int ret = omv_i2c_readb2(&csi->i2c_bus, csi->slv_addr, TG_FLIP, &reg);
    ret |= omv_i2c_writeb2(&csi->i2c_bus, csi->slv_addr, TG_FLIP, TG_FLIP_SET_VFLIP(reg, enable));
    ret |= omv_i2c_writeb2(&csi->i2c_bus, csi->slv_addr, SENSOR_UPDATE, SENSOR_UPDATE_FLAG);
    return ret;
}

static int ioctl(omv_csi_t *csi, int request, va_list ap) {
    int ret = 0;

    (void) read_reg_seq;

    switch (request) {
        case OMV_CSI_IOCTL_SET_TRIGGERED_MODE: {
            int enable = va_arg(ap, int);
            if (enable) {
                ret |= omv_i2c_writeb2(&csi->i2c_bus, csi->slv_addr, SENSOR_OPMODE, SENSOR_OPMODE_SUSPEND);
                ret |= omv_i2c_writeb2(&csi->i2c_bus, csi->slv_addr, SENSOR_TG_EN, 0);
                ret |= omv_i2c_writeb2(&csi->i2c_bus, csi->slv_addr, SENSOR_TRIGGER_EN, SENSOR_TRIGGER_EN_FLAG);
                ret |= omv_i2c_writeb2(&csi->i2c_bus, csi->slv_addr, SENSOR_TRIGGER_MODE,
                                       SENSOR_TRIGGER_MODE_EN | SENSOR_TRIGGER_MODE_1);
                ret |= omv_i2c_writeb2(&csi->i2c_bus, csi->slv_addr, SENSOR_OPMODE, SENSOR_OPMODE_RUN);
                // Hardware trigger mode (GPIO1), otherwise trigger from software via I2C.
                #if defined(OMV_CSI_FSYNC_PIN)
                ret |= omv_i2c_writeb2(&csi->i2c_bus, csi->slv_addr, SENSOR_TRIGGER_EN,
                                       SENSOR_TRIGGER_EN_FLAG | SENSOR_TRIGGER_MODE_GPIO1);
                #endif
            } else {
                ret |= omv_i2c_writeb2(&csi->i2c_bus, csi->slv_addr, SENSOR_OPMODE, SENSOR_OPMODE_SUSPEND);
                ret |= omv_i2c_writeb2(&csi->i2c_bus, csi->slv_addr, SENSOR_TG_EN, SENSOR_TRIGGER_EN_FLAG);
                ret |= omv_i2c_writeb2(&csi->i2c_bus, csi->slv_addr, SENSOR_TRIGGER_EN, 0);
                ret |= omv_i2c_writeb2(&csi->i2c_bus, csi->slv_addr, SENSOR_TRIGGER_MODE, 0);
                ret |= omv_i2c_writeb2(&csi->i2c_bus, csi->slv_addr, SENSOR_OPMODE, SENSOR_OPMODE_RUN);
            }
            // Skip past the first corrupt frames...
            if (!csi->disable_delays) {
                mp_hal_delay_ms(100);
            }
            break;
        }
        case OMV_CSI_IOCTL_GET_TRIGGERED_MODE: {
            int *enable = va_arg(ap, int *);
            uint8_t reg;
            ret |= omv_i2c_readb2(&csi->i2c_bus, csi->slv_addr, SENSOR_TG_EN, &reg);
            if (ret >= 0) {
                *enable = (reg & SENSOR_TRIGGER_EN_FLAG) ? 0 : 1;
            }
            break;
        }
        case OMV_CSI_IOCTL_GET_RGB_STATS: {
            uint32_t rgb_stats[4];
            uint8_t buf[] = {(RGB_STAT_B_VS >> 8), RGB_STAT_B_VS & 0xFF };
            ret |= omv_i2c_write_bytes(&csi->i2c_bus, csi->slv_addr, buf, 2, OMV_I2C_XFER_NO_STOP);
            ret |= omv_i2c_read_bytes(&csi->i2c_bus, csi->slv_addr, (uint8_t *) rgb_stats, sizeof(rgb_stats), 0);

            *va_arg(ap, uint32_t *) = rgb_stats[2];
            *va_arg(ap, uint32_t *) = rgb_stats[1];
            *va_arg(ap, uint32_t *) = rgb_stats[3];
            *va_arg(ap, uint32_t *) = rgb_stats[0];
            break;
        }
        default: {
            ret = -1;
            break;
        }
    }

    return ret;
}

int pag7936_init(omv_csi_t *csi) {
    // Initialize csi flags.
    csi->vsync_pol = 0;
    csi->hsync_pol = 0;
    csi->pixck_pol = 1;
    csi->frame_sync = 1;
    csi->mono_bpp = 1;
    csi->raw_output = 1;
    csi->cfa_format = SUBFORMAT_ID_BGGR;
    #if OMV_PAG7936_MIPI_CSI2
    csi->mipi_if = 1;
    #endif

    // Initialize csi ops.
    csi->reset = reset;
    csi->sleep = sleep;
    csi->ioctl = ioctl;
    csi->read_reg = read_reg;
    csi->write_reg = write_reg;
    csi->set_pixformat = set_pixformat;
    csi->set_framesize = set_framesize;
    csi->set_framerate = set_framerate;
    csi->set_gainceiling = set_gainceiling;
    csi->set_colorbar = set_colorbar;
    csi->set_auto_gain = set_auto_gain;
    csi->get_gain_db = get_gain_db;
    csi->set_auto_exposure = set_auto_exposure;
    csi->get_exposure_us = get_exposure_us;
    csi->set_hmirror = set_hmirror;
    csi->set_vflip = set_vflip;

    // Override standard resolutions
    resolution[OMV_CSI_FRAMESIZE_HD][0] = 1280;
    resolution[OMV_CSI_FRAMESIZE_HD][1] = 800;

    resolution[OMV_CSI_FRAMESIZE_VGA][0] = 640;
    resolution[OMV_CSI_FRAMESIZE_VGA][1] = 400;

    resolution[OMV_CSI_FRAMESIZE_QVGA][0] = 320;
    resolution[OMV_CSI_FRAMESIZE_QVGA][1] = 200;
    return 0;
}
#endif // (OMV_PAG7936_ENABLE == 1)
