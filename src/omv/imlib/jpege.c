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
 * Minimalistic JPEG baseline encoder.
 * Ported from public domain JPEG writer by Jon Olick - http://jonolick.com
 * DCT implementation is based on Arai, Agui, and Nakajima's algorithm for scaled DCT.
 */
#include "imlib.h"
#include "file_utils.h"

// Expand 4 bits to 32 for binary to grayscale - process 4 pixels at a time
#if (OMV_JPEG_CODEC_ENABLE == 1)
#define JPEG_BINARY_0              0x00
#define JPEG_BINARY_1              0xFF
static const uint32_t jpeg_expand[16] = {
    0x00000000, 0x000000ff, 0x0000ff00, 0x0000ffff,
    0x00ff0000, 0x00ff00ff, 0x00ffff00, 0x00ffffff,
    0xff000000, 0xff0000ff, 0xff00ff00, 0xff00ffff,
    0xffff0000, 0xffff00ff, 0xffffff00, 0xffffffff
};
#else
#define JPEG_BINARY_0              0x80
#define JPEG_BINARY_1              0x7F
static const uint32_t jpeg_expand[16] = {
    0x80808080, 0x8080807f, 0x80807f80, 0x80807f7f,
    0x807f8080, 0x807f807f, 0x807f7f80, 0x807f7f7f,
    0x7f808080, 0x7f80807f, 0x7f807f80, 0x7f807f7f,
    0x7f7f8080, 0x7f7f807f, 0x7f7f7f80, 0x7f7f7f7f
};
#endif

void jpeg_get_mcu(image_t *src, int x_offset, int y_offset, int dx, int dy,
                  int8_t *Y0, int8_t *CB, int8_t *CR) {
    switch (src->pixfmt) {
        case PIXFORMAT_BINARY: {
            if ((dx != JPEG_MCU_W) || (dy != JPEG_MCU_H)) {
                // partial MCU, fill with 0's to start
                memset(Y0, 0, JPEG_444_GS_MCU_SIZE);
            }

            for (int y = y_offset, yy = y + dy; y < yy; y++) {
                uint32_t *rp = IMAGE_COMPUTE_BINARY_PIXEL_ROW_PTR(src, y);
                uint8_t pixels = rp[x_offset >> UINT32_T_SHIFT] >> (x_offset & UINT32_T_MASK);

                if (dx == JPEG_MCU_W) {
                    *((uint32_t *) Y0) = jpeg_expand[pixels & 0xf];
                    *(((uint32_t *) Y0) + 1) = jpeg_expand[pixels >> 4];
                } else if (dx >= 4) {
                    *((uint32_t *) Y0) = jpeg_expand[pixels & 0xf];

                    if (dx >= 6) {
                        *(((uint16_t *) Y0) + 2) = jpeg_expand[pixels >> 4];

                        if (dx & 1) {
                            Y0[6] = (pixels & 0x40) ? JPEG_BINARY_1 : JPEG_BINARY_0;
                        }
                    } else if (dx & 1) {
                        Y0[4] = (pixels & 0x10) ? JPEG_BINARY_1 : JPEG_BINARY_0;
                    }
                } else if (dx >= 2) {
                    *((uint16_t *) Y0) = jpeg_expand[pixels & 0x3];

                    if (dx & 1) {
                        Y0[2] = (pixels & 0x4) ? JPEG_BINARY_1 : JPEG_BINARY_0;
                    }
                } else {
                    *Y0 = (pixels & 0x1) ? JPEG_BINARY_1 : JPEG_BINARY_0;
                }

                Y0 += JPEG_MCU_W;
            }
            break;
        }
        case PIXFORMAT_GRAYSCALE: {
            if ((dx != JPEG_MCU_W) || (dy != JPEG_MCU_H)) {
                // partial MCU, fill with 0's to start
                memset(Y0, 0, JPEG_444_GS_MCU_SIZE);
            }

            for (int y = y_offset, yy = y + dy; y < yy; y++) {
                uint8_t *rp = IMAGE_COMPUTE_GRAYSCALE_PIXEL_ROW_PTR(src, y) + x_offset;

                #if (OMV_JPEG_CODEC_ENABLE == 0)
                if (dx == JPEG_MCU_W) {
                    *((uint32_t *) Y0) = *((uint32_t *) rp) ^ 0x80808080;
                    *(((uint32_t *) Y0) + 1) = *(((uint32_t *) rp) + 1) ^ 0x80808080;
                } else if (dx >= 4) {
                    *((uint32_t *) Y0) = *((uint32_t *) rp) ^ 0x80808080;

                    if (dx >= 6) {
                        *(((uint16_t *) Y0) + 2) = *(((uint16_t *) rp) + 2) ^ 0x8080;

                        if (dx & 1) {
                            Y0[6] = rp[6] ^ 0x80;
                        }
                    } else if (dx & 1) {
                        Y0[4] = rp[4] ^ 0x80;
                    }
                } else if (dx >= 2) {
                    *((uint16_t *) Y0) = *((uint16_t *) rp) ^ 0x8080;

                    if (dx & 1) {
                        Y0[2] = rp[2] ^ 0x80;
                    }
                } else{
                    *Y0 = *rp ^ 0x80;
                }
                #else
                if (dx == JPEG_MCU_W) {
                    *((uint32_t *) Y0) = *((uint32_t *) rp);
                    *(((uint32_t *) Y0) + 1) = *(((uint32_t *) rp) + 1);
                } else if (dx >= 4) {
                    *((uint32_t *) Y0) = *((uint32_t *) rp);

                    if (dx >= 6) {
                        *(((uint16_t *) Y0) + 2) = *(((uint16_t *) rp) + 2);

                        if (dx & 1) {
                            Y0[6] = rp[6];
                        }
                    } else if (dx & 1) {
                        Y0[4] = rp[4];
                    }
                } else if (dx >= 2) {
                    *((uint16_t *) Y0) = *((uint16_t *) rp);

                    if (dx & 1) {
                        Y0[2] = rp[2];
                    }
                } else{
                    *Y0 = *rp;
                }
                #endif

                Y0 += JPEG_MCU_W;
            }
            break;
        }
        case PIXFORMAT_RGB565: {
            if ((dx != JPEG_MCU_W) || (dy != JPEG_MCU_H)) {
                // partial MCU, fill with 0's to start
                memset(Y0, 0, JPEG_444_GS_MCU_SIZE);
                memset(CB, 0, JPEG_444_GS_MCU_SIZE);
                memset(CR, 0, JPEG_444_GS_MCU_SIZE);
            }

            for (int y = y_offset, yy = y + dy, index = 0; y < yy; y++) {
                uint32_t *rp = (uint32_t *) (IMAGE_COMPUTE_RGB565_PIXEL_ROW_PTR(src, y) + x_offset);

                for (int x = 0, xx = dx - 1; x < xx; x += 2, index += 2) {
                    int pixels = *rp++;
                    int r_pixels = ((pixels >> 8) & 0xf800f8) | ((pixels >> 13) & 0x70007);
                    int g_pixels = ((pixels >> 3) & 0xfc00fc) | ((pixels >> 9) & 0x30003);
                    int b_pixels = ((pixels << 3) & 0xf800f8) | ((pixels >> 2) & 0x70007);

                    int y = ((r_pixels * 38) + (g_pixels * 75) + (b_pixels * 15)) >> 7;

                    #if (OMV_JPEG_CODEC_ENABLE == 0)
                    y ^= 0x800080;
                    #endif

                    Y0[index] = y, Y0[index + 1] = y >> 16;

                    int u = __SSUB16(b_pixels * 64, (r_pixels * 21) + (g_pixels * 43)) >> 7;

                    #if (OMV_JPEG_CODEC_ENABLE == 1)
                    u ^= 0x800080;
                    #endif

                    CB[index] = u, CB[index + 1] = u >> 16;

                    int v = __SSUB16(r_pixels * 64, (g_pixels * 54) + (b_pixels * 10)) >> 7;

                    #if (OMV_JPEG_CODEC_ENABLE == 1)
                    v ^= 0x800080;
                    #endif

                    CR[index] = v, CR[index + 1] = v >> 16;
                }

                if (dx & 1) {
                    int pixel = *((uint16_t *) rp);
                    int r = COLOR_RGB565_TO_R8(pixel);
                    int g = COLOR_RGB565_TO_G8(pixel);
                    int b = COLOR_RGB565_TO_B8(pixel);

                    int y0 = COLOR_RGB888_TO_Y(r, g, b);

                    #if (OMV_JPEG_CODEC_ENABLE == 0)
                    y0 ^= 0x80;
                    #endif

                    Y0[index] = y0;

                    int cb = COLOR_RGB888_TO_U(r, g, b);

                    #if (OMV_JPEG_CODEC_ENABLE == 1)
                    cb ^= 0x80;
                    #endif

                    CB[index] = cb;

                    int cr = COLOR_RGB888_TO_V(r, g, b);

                    #if (OMV_JPEG_CODEC_ENABLE == 1)
                    cr ^= 0x80;
                    #endif

                    CR[index++] = cr;
                }

                index += JPEG_MCU_W - dx;
            }
            break;
        }
        case PIXFORMAT_YUV_ANY: {
            if ((dx != JPEG_MCU_W) || (dy != JPEG_MCU_H)) {
                // partial MCU, fill with 0's to start
                memset(Y0, 0, JPEG_444_GS_MCU_SIZE);
                memset(CB, 0, JPEG_444_GS_MCU_SIZE);
                memset(CR, 0, JPEG_444_GS_MCU_SIZE);
            }

            int shift = (src->pixfmt == PIXFORMAT_YUV422) ? 24 : 8;

            for (int y = y_offset, yy = y + dy, index = 0; y < yy; y++) {
                uint32_t *rp = (uint32_t *) (IMAGE_COMPUTE_YUV_PIXEL_ROW_PTR(src, y) + x_offset);

                for (int x = 0, xx = dx - 1; x < xx; x += 2, index += 2) {
                    int pixels = *rp++;

                    #if (OMV_JPEG_CODEC_ENABLE == 0)
                    pixels ^= 0x80808080;
                    #endif

                    Y0[index] = pixels, Y0[index + 1] = pixels >> 16;

                    int cb = pixels >> shift;
                    CB[index] = cb, CB[index + 1] = cb;

                    int cr = pixels >> (32 - shift);
                    CR[index] = cr, CR[index + 1] = cr;
                }

                if (dx & 1) {
                    int pixel = *((uint16_t *) rp);

                    #if (OMV_JPEG_CODEC_ENABLE == 0)
                    pixel ^= 0x8080;
                    #endif

                    Y0[index] = pixel;

                    if (index % JPEG_MCU_W) {
                        if (shift == 8) {
                            CR[index] = CR[index - 1];
                            CB[index++] = pixel >> 8;
                        } else {
                            CB[index] = CB[index - 1];
                            CR[index++] = pixel >> 8;
                        }
                    } else {
                        if (shift == 8) {
                            CB[index] = pixel >> 8;
                            #if (OMV_JPEG_CODEC_ENABLE == 0)
                            CR[index++] = 0;
                            #else
                            CR[index++] = 0x80;
                            #endif
                        } else {
                            #if (OMV_JPEG_CODEC_ENABLE == 0)
                            CB[index] = 0;
                            #else
                            CB[index] = 0x80;
                            #endif
                            CR[index++] = pixel >> 8;
                        }
                    }
                }

                index += JPEG_MCU_W - dx;
            }
            break;
        }
        case PIXFORMAT_BAYER_ANY: {
            if ((dx != JPEG_MCU_W) || (dy != JPEG_MCU_H)) {
                // partial MCU, fill with 0's to start
                memset(Y0, 0, JPEG_444_GS_MCU_SIZE);
                memset(CB, 0, JPEG_444_GS_MCU_SIZE);
                memset(CR, 0, JPEG_444_GS_MCU_SIZE);
            }

            rectangle_t roi = {
                .x = x_offset,
                .y = y_offset,
                .w = dx,
                .h = dy
            };

            imlib_debayer_ycbcr(src, &roi, Y0, CB, CR);
            break;
        }
    }
}

#if (OMV_JPEG_CODEC_ENABLE == 0)

// Software JPEG implementation.
#define FIX_0_382683433    ((int32_t) 98)
#define FIX_0_541196100    ((int32_t) 139)
#define FIX_0_707106781    ((int32_t) 181)
#define FIX_1_306562965    ((int32_t) 334)

#define DESCALE(x, y)      (x >> y)
#define MULTIPLY(x, y)     DESCALE((x) * (y), 8)

typedef struct {
    int idx;
    int length;
    uint8_t *buf;
    uint32_t bitb;
    uint32_t bitc;
    bool realloc;
    bool overflow;
} jpeg_buf_t;

// Quantization tables
static float fdtbl_Y[64], fdtbl_UV[64];
static uint8_t YTable[64], UVTable[64];

static const uint8_t s_jpeg_ZigZag[] = {
    0,  1,   5,  6, 14, 15, 27, 28,
    2,  4,   7, 13, 16, 26, 29, 42,
    3,  8,  12, 17, 25, 30, 41, 43,
    9,  11, 18, 24, 31, 40, 44, 53,
    10, 19, 23, 32, 39, 45, 52, 54,
    20, 22, 33, 38, 46, 51, 55, 60,
    21, 34, 37, 47, 50, 56, 59, 61,
    35, 36, 48, 49, 57, 58, 62, 63
};

static const uint8_t YQT[] = {
    16, 11, 10, 16, 24,  40,  51,  61,
    12, 12, 14, 19, 26,  58,  60,  55,
    14, 13, 16, 24, 40,  57,  69,  56,
    14, 17, 22, 29, 51,  87,  80,  62,
    18, 22, 37, 56, 68,  109, 103, 77,
    24, 35, 55, 64, 81,  104, 113, 92,
    49, 64, 78, 87, 103, 121, 120, 101,
    72, 92, 95, 98, 112, 100, 103, 99
};

static const uint8_t UVQT[] = {
    17, 18, 24, 47, 99, 99, 99, 99,
    18, 21, 26, 66, 99, 99, 99, 99,
    24, 26, 56, 99, 99, 99, 99, 99,
    47, 66, 99, 99, 99, 99, 99, 99,
    99, 99, 99, 99, 99, 99, 99, 99,
    99, 99, 99, 99, 99, 99, 99, 99,
    99, 99, 99, 99, 99, 99, 99, 99,
    99, 99, 99, 99, 99, 99, 99, 99
};

static const float aasf[] = {
    1.0f, 1.387039845f, 1.306562965f, 1.175875602f,
    1.0f, 0.785694958f, 0.541196100f, 0.275899379f
};


static const uint8_t std_dc_luminance_nrcodes[] = {0, 0, 1, 5, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0};
static const uint8_t std_dc_luminance_values[] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11};
static const uint8_t std_ac_luminance_nrcodes[] = {0, 0, 2, 1, 3, 3, 2, 4, 3, 5, 5, 4, 4, 0, 0, 1, 0x7d};
static const uint8_t std_ac_luminance_values[] = {
    0x01, 0x02, 0x03, 0x00, 0x04, 0x11, 0x05, 0x12, 0x21,
    0x31, 0x41, 0x06, 0x13, 0x51, 0x61, 0x07, 0x22, 0x71,
    0x14, 0x32, 0x81, 0x91, 0xa1, 0x08, 0x23, 0x42, 0xb1,
    0xc1, 0x15, 0x52, 0xd1, 0xf0, 0x24, 0x33, 0x62, 0x72,
    0x82, 0x09, 0x0a, 0x16, 0x17, 0x18, 0x19, 0x1a, 0x25,
    0x26, 0x27, 0x28, 0x29, 0x2a, 0x34, 0x35, 0x36, 0x37,
    0x38, 0x39, 0x3a, 0x43, 0x44, 0x45, 0x46, 0x47, 0x48,
    0x49, 0x4a, 0x53, 0x54, 0x55, 0x56, 0x57, 0x58, 0x59,
    0x5a, 0x63, 0x64, 0x65, 0x66, 0x67, 0x68, 0x69, 0x6a,
    0x73, 0x74, 0x75, 0x76, 0x77, 0x78, 0x79, 0x7a, 0x83,
    0x84, 0x85, 0x86, 0x87, 0x88, 0x89, 0x8a, 0x92, 0x93,
    0x94, 0x95, 0x96, 0x97, 0x98, 0x99, 0x9a, 0xa2, 0xa3,
    0xa4, 0xa5, 0xa6, 0xa7, 0xa8, 0xa9, 0xaa, 0xb2, 0xb3,
    0xb4, 0xb5, 0xb6, 0xb7, 0xb8, 0xb9, 0xba, 0xc2, 0xc3,
    0xc4, 0xc5, 0xc6, 0xc7, 0xc8, 0xc9, 0xca, 0xd2, 0xd3,
    0xd4, 0xd5, 0xd6, 0xd7, 0xd8, 0xd9, 0xda, 0xe1, 0xe2,
    0xe3, 0xe4, 0xe5, 0xe6, 0xe7, 0xe8, 0xe9, 0xea, 0xf1,
    0xf2, 0xf3, 0xf4, 0xf5, 0xf6, 0xf7, 0xf8, 0xf9, 0xfa
};

static const uint8_t std_dc_chrominance_nrcodes[] = {0, 0, 3, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0};
static const uint8_t std_dc_chrominance_values[] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11};
static const uint8_t std_ac_chrominance_nrcodes[] = {0, 0, 2, 1, 2, 4, 4, 3, 4, 7, 5, 4, 4, 0, 1, 2, 0x77};
static const uint8_t std_ac_chrominance_values[] = {
    0x00, 0x01, 0x02, 0x03, 0x11, 0x04, 0x05, 0x21, 0x31,
    0x06, 0x12, 0x41, 0x51, 0x07, 0x61, 0x71, 0x13, 0x22,
    0x32, 0x81, 0x08, 0x14, 0x42, 0x91, 0xa1, 0xb1, 0xc1,
    0x09, 0x23, 0x33, 0x52, 0xf0, 0x15, 0x62, 0x72, 0xd1,
    0x0a, 0x16, 0x24, 0x34, 0xe1, 0x25, 0xf1, 0x17, 0x18,
    0x19, 0x1a, 0x26, 0x27, 0x28, 0x29, 0x2a, 0x35, 0x36,
    0x37, 0x38, 0x39, 0x3a, 0x43, 0x44, 0x45, 0x46, 0x47,
    0x48, 0x49, 0x4a, 0x53, 0x54, 0x55, 0x56, 0x57, 0x58,
    0x59, 0x5a, 0x63, 0x64, 0x65, 0x66, 0x67, 0x68, 0x69,
    0x6a, 0x73, 0x74, 0x75, 0x76, 0x77, 0x78, 0x79, 0x7a,
    0x82, 0x83, 0x84, 0x85, 0x86, 0x87, 0x88, 0x89, 0x8a,
    0x92, 0x93, 0x94, 0x95, 0x96, 0x97, 0x98, 0x99, 0x9a,
    0xa2, 0xa3, 0xa4, 0xa5, 0xa6, 0xa7, 0xa8, 0xa9, 0xaa,
    0xb2, 0xb3, 0xb4, 0xb5, 0xb6, 0xb7, 0xb8, 0xb9, 0xba,
    0xc2, 0xc3, 0xc4, 0xc5, 0xc6, 0xc7, 0xc8, 0xc9, 0xca,
    0xd2, 0xd3, 0xd4, 0xd5, 0xd6, 0xd7, 0xd8, 0xd9, 0xda,
    0xe2, 0xe3, 0xe4, 0xe5, 0xe6, 0xe7, 0xe8, 0xe9, 0xea,
    0xf2, 0xf3, 0xf4, 0xf5, 0xf6, 0xf7, 0xf8, 0xf9, 0xfa
};

// Huffman tables
static const uint16_t YDC_HT[12][2] = {
    {0, 2}, {2, 3}, {3, 3}, {4, 3},
    {5, 3}, {6, 3}, {14, 4}, {30, 5},
    {62, 6}, {126, 7}, {254, 8}, {510, 9},
};

static const uint16_t UVDC_HT[12][2] = {
    {0, 2}, {1, 2}, {2, 2}, {6, 3},
    {14, 4}, {30, 5}, {62, 6}, {126, 7},
    {254, 8}, {510, 9}, {1022, 10}, {2046, 11},
};

static const uint16_t YAC_HT[256][2] = {
    {0x000A, 0x0004}, {0x0000, 0x0002}, {0x0001, 0x0002}, {0x0004, 0x0003},
    {0x000B, 0x0004}, {0x001A, 0x0005}, {0x0078, 0x0007}, {0x00F8, 0x0008},
    {0x03F6, 0x000A}, {0xFF82, 0x0010}, {0xFF83, 0x0010}, {0x0000, 0x0000},
    {0x0000, 0x0000}, {0x0000, 0x0000}, {0x0000, 0x0000}, {0x0000, 0x0000},
    {0x0000, 0x0000}, {0x000C, 0x0004}, {0x001B, 0x0005}, {0x0079, 0x0007},
    {0x01F6, 0x0009}, {0x07F6, 0x000B}, {0xFF84, 0x0010}, {0xFF85, 0x0010},
    {0xFF86, 0x0010}, {0xFF87, 0x0010}, {0xFF88, 0x0010}, {0x0000, 0x0000},
    {0x0000, 0x0000}, {0x0000, 0x0000}, {0x0000, 0x0000}, {0x0000, 0x0000},
    {0x0000, 0x0000}, {0x001C, 0x0005}, {0x00F9, 0x0008}, {0x03F7, 0x000A},
    {0x0FF4, 0x000C}, {0xFF89, 0x0010}, {0xFF8A, 0x0010}, {0xFF8B, 0x0010},
    {0xFF8C, 0x0010}, {0xFF8D, 0x0010}, {0xFF8E, 0x0010}, {0x0000, 0x0000},
    {0x0000, 0x0000}, {0x0000, 0x0000}, {0x0000, 0x0000}, {0x0000, 0x0000},
    {0x0000, 0x0000}, {0x003A, 0x0006}, {0x01F7, 0x0009}, {0x0FF5, 0x000C},
    {0xFF8F, 0x0010}, {0xFF90, 0x0010}, {0xFF91, 0x0010}, {0xFF92, 0x0010},
    {0xFF93, 0x0010}, {0xFF94, 0x0010}, {0xFF95, 0x0010}, {0x0000, 0x0000},
    {0x0000, 0x0000}, {0x0000, 0x0000}, {0x0000, 0x0000}, {0x0000, 0x0000},
    {0x0000, 0x0000}, {0x003B, 0x0006}, {0x03F8, 0x000A}, {0xFF96, 0x0010},
    {0xFF97, 0x0010}, {0xFF98, 0x0010}, {0xFF99, 0x0010}, {0xFF9A, 0x0010},
    {0xFF9B, 0x0010}, {0xFF9C, 0x0010}, {0xFF9D, 0x0010}, {0x0000, 0x0000},
    {0x0000, 0x0000}, {0x0000, 0x0000}, {0x0000, 0x0000}, {0x0000, 0x0000},
    {0x0000, 0x0000}, {0x007A, 0x0007}, {0x07F7, 0x000B}, {0xFF9E, 0x0010},
    {0xFF9F, 0x0010}, {0xFFA0, 0x0010}, {0xFFA1, 0x0010}, {0xFFA2, 0x0010},
    {0xFFA3, 0x0010}, {0xFFA4, 0x0010}, {0xFFA5, 0x0010}, {0x0000, 0x0000},
    {0x0000, 0x0000}, {0x0000, 0x0000}, {0x0000, 0x0000}, {0x0000, 0x0000},
    {0x0000, 0x0000}, {0x007B, 0x0007}, {0x0FF6, 0x000C}, {0xFFA6, 0x0010},
    {0xFFA7, 0x0010}, {0xFFA8, 0x0010}, {0xFFA9, 0x0010}, {0xFFAA, 0x0010},
    {0xFFAB, 0x0010}, {0xFFAC, 0x0010}, {0xFFAD, 0x0010}, {0x0000, 0x0000},
    {0x0000, 0x0000}, {0x0000, 0x0000}, {0x0000, 0x0000}, {0x0000, 0x0000},
    {0x0000, 0x0000}, {0x00FA, 0x0008}, {0x0FF7, 0x000C}, {0xFFAE, 0x0010},
    {0xFFAF, 0x0010}, {0xFFB0, 0x0010}, {0xFFB1, 0x0010}, {0xFFB2, 0x0010},
    {0xFFB3, 0x0010}, {0xFFB4, 0x0010}, {0xFFB5, 0x0010}, {0x0000, 0x0000},
    {0x0000, 0x0000}, {0x0000, 0x0000}, {0x0000, 0x0000}, {0x0000, 0x0000},
    {0x0000, 0x0000}, {0x01F8, 0x0009}, {0x7FC0, 0x000F}, {0xFFB6, 0x0010},
    {0xFFB7, 0x0010}, {0xFFB8, 0x0010}, {0xFFB9, 0x0010}, {0xFFBA, 0x0010},
    {0xFFBB, 0x0010}, {0xFFBC, 0x0010}, {0xFFBD, 0x0010}, {0x0000, 0x0000},
    {0x0000, 0x0000}, {0x0000, 0x0000}, {0x0000, 0x0000}, {0x0000, 0x0000},
    {0x0000, 0x0000}, {0x01F9, 0x0009}, {0xFFBE, 0x0010}, {0xFFBF, 0x0010},
    {0xFFC0, 0x0010}, {0xFFC1, 0x0010}, {0xFFC2, 0x0010}, {0xFFC3, 0x0010},
    {0xFFC4, 0x0010}, {0xFFC5, 0x0010}, {0xFFC6, 0x0010}, {0x0000, 0x0000},
    {0x0000, 0x0000}, {0x0000, 0x0000}, {0x0000, 0x0000}, {0x0000, 0x0000},
    {0x0000, 0x0000}, {0x01FA, 0x0009}, {0xFFC7, 0x0010}, {0xFFC8, 0x0010},
    {0xFFC9, 0x0010}, {0xFFCA, 0x0010}, {0xFFCB, 0x0010}, {0xFFCC, 0x0010},
    {0xFFCD, 0x0010}, {0xFFCE, 0x0010}, {0xFFCF, 0x0010}, {0x0000, 0x0000},
    {0x0000, 0x0000}, {0x0000, 0x0000}, {0x0000, 0x0000}, {0x0000, 0x0000},
    {0x0000, 0x0000}, {0x03F9, 0x000A}, {0xFFD0, 0x0010}, {0xFFD1, 0x0010},
    {0xFFD2, 0x0010}, {0xFFD3, 0x0010}, {0xFFD4, 0x0010}, {0xFFD5, 0x0010},
    {0xFFD6, 0x0010}, {0xFFD7, 0x0010}, {0xFFD8, 0x0010}, {0x0000, 0x0000},
    {0x0000, 0x0000}, {0x0000, 0x0000}, {0x0000, 0x0000}, {0x0000, 0x0000},
    {0x0000, 0x0000}, {0x03FA, 0x000A}, {0xFFD9, 0x0010}, {0xFFDA, 0x0010},
    {0xFFDB, 0x0010}, {0xFFDC, 0x0010}, {0xFFDD, 0x0010}, {0xFFDE, 0x0010},
    {0xFFDF, 0x0010}, {0xFFE0, 0x0010}, {0xFFE1, 0x0010}, {0x0000, 0x0000},
    {0x0000, 0x0000}, {0x0000, 0x0000}, {0x0000, 0x0000}, {0x0000, 0x0000},
    {0x0000, 0x0000}, {0x07F8, 0x000B}, {0xFFE2, 0x0010}, {0xFFE3, 0x0010},
    {0xFFE4, 0x0010}, {0xFFE5, 0x0010}, {0xFFE6, 0x0010}, {0xFFE7, 0x0010},
    {0xFFE8, 0x0010}, {0xFFE9, 0x0010}, {0xFFEA, 0x0010}, {0x0000, 0x0000},
    {0x0000, 0x0000}, {0x0000, 0x0000}, {0x0000, 0x0000}, {0x0000, 0x0000},
    {0x0000, 0x0000}, {0xFFEB, 0x0010}, {0xFFEC, 0x0010}, {0xFFED, 0x0010},
    {0xFFEE, 0x0010}, {0xFFEF, 0x0010}, {0xFFF0, 0x0010}, {0xFFF1, 0x0010},
    {0xFFF2, 0x0010}, {0xFFF3, 0x0010}, {0xFFF4, 0x0010}, {0x0000, 0x0000},
    {0x0000, 0x0000}, {0x0000, 0x0000}, {0x0000, 0x0000}, {0x0000, 0x0000},
    {0x07F9, 0x000B}, {0xFFF5, 0x0010}, {0xFFF6, 0x0010}, {0xFFF7, 0x0010},
    {0xFFF8, 0x0010}, {0xFFF9, 0x0010}, {0xFFFA, 0x0010}, {0xFFFB, 0x0010},
    {0xFFFC, 0x0010}, {0xFFFD, 0x0010}, {0xFFFE, 0x0010}, {0x0000, 0x0000},
    {0x0000, 0x0000}, {0x0000, 0x0000}, {0x0000, 0x0000}, {0x0000, 0x0000},
};

static const uint16_t UVAC_HT[256][2] = {
    {0x0000, 0x0002}, {0x0001, 0x0002}, {0x0004, 0x0003}, {0x000A, 0x0004},
    {0x0018, 0x0005}, {0x0019, 0x0005}, {0x0038, 0x0006}, {0x0078, 0x0007},
    {0x01F4, 0x0009}, {0x03F6, 0x000A}, {0x0FF4, 0x000C}, {0x0000, 0x0000},
    {0x0000, 0x0000}, {0x0000, 0x0000}, {0x0000, 0x0000}, {0x0000, 0x0000},
    {0x0000, 0x0000}, {0x000B, 0x0004}, {0x0039, 0x0006}, {0x00F6, 0x0008},
    {0x01F5, 0x0009}, {0x07F6, 0x000B}, {0x0FF5, 0x000C}, {0xFF88, 0x0010},
    {0xFF89, 0x0010}, {0xFF8A, 0x0010}, {0xFF8B, 0x0010}, {0x0000, 0x0000},
    {0x0000, 0x0000}, {0x0000, 0x0000}, {0x0000, 0x0000}, {0x0000, 0x0000},
    {0x0000, 0x0000}, {0x001A, 0x0005}, {0x00F7, 0x0008}, {0x03F7, 0x000A},
    {0x0FF6, 0x000C}, {0x7FC2, 0x000F}, {0xFF8C, 0x0010}, {0xFF8D, 0x0010},
    {0xFF8E, 0x0010}, {0xFF8F, 0x0010}, {0xFF90, 0x0010}, {0x0000, 0x0000},
    {0x0000, 0x0000}, {0x0000, 0x0000}, {0x0000, 0x0000}, {0x0000, 0x0000},
    {0x0000, 0x0000}, {0x001B, 0x0005}, {0x00F8, 0x0008}, {0x03F8, 0x000A},
    {0x0FF7, 0x000C}, {0xFF91, 0x0010}, {0xFF92, 0x0010}, {0xFF93, 0x0010},
    {0xFF94, 0x0010}, {0xFF95, 0x0010}, {0xFF96, 0x0010}, {0x0000, 0x0000},
    {0x0000, 0x0000}, {0x0000, 0x0000}, {0x0000, 0x0000}, {0x0000, 0x0000},
    {0x0000, 0x0000}, {0x003A, 0x0006}, {0x01F6, 0x0009}, {0xFF97, 0x0010},
    {0xFF98, 0x0010}, {0xFF99, 0x0010}, {0xFF9A, 0x0010}, {0xFF9B, 0x0010},
    {0xFF9C, 0x0010}, {0xFF9D, 0x0010}, {0xFF9E, 0x0010}, {0x0000, 0x0000},
    {0x0000, 0x0000}, {0x0000, 0x0000}, {0x0000, 0x0000}, {0x0000, 0x0000},
    {0x0000, 0x0000}, {0x003B, 0x0006}, {0x03F9, 0x000A}, {0xFF9F, 0x0010},
    {0xFFA0, 0x0010}, {0xFFA1, 0x0010}, {0xFFA2, 0x0010}, {0xFFA3, 0x0010},
    {0xFFA4, 0x0010}, {0xFFA5, 0x0010}, {0xFFA6, 0x0010}, {0x0000, 0x0000},
    {0x0000, 0x0000}, {0x0000, 0x0000}, {0x0000, 0x0000}, {0x0000, 0x0000},
    {0x0000, 0x0000}, {0x0079, 0x0007}, {0x07F7, 0x000B}, {0xFFA7, 0x0010},
    {0xFFA8, 0x0010}, {0xFFA9, 0x0010}, {0xFFAA, 0x0010}, {0xFFAB, 0x0010},
    {0xFFAC, 0x0010}, {0xFFAD, 0x0010}, {0xFFAE, 0x0010}, {0x0000, 0x0000},
    {0x0000, 0x0000}, {0x0000, 0x0000}, {0x0000, 0x0000}, {0x0000, 0x0000},
    {0x0000, 0x0000}, {0x007A, 0x0007}, {0x07F8, 0x000B}, {0xFFAF, 0x0010},
    {0xFFB0, 0x0010}, {0xFFB1, 0x0010}, {0xFFB2, 0x0010}, {0xFFB3, 0x0010},
    {0xFFB4, 0x0010}, {0xFFB5, 0x0010}, {0xFFB6, 0x0010}, {0x0000, 0x0000},
    {0x0000, 0x0000}, {0x0000, 0x0000}, {0x0000, 0x0000}, {0x0000, 0x0000},
    {0x0000, 0x0000}, {0x00F9, 0x0008}, {0xFFB7, 0x0010}, {0xFFB8, 0x0010},
    {0xFFB9, 0x0010}, {0xFFBA, 0x0010}, {0xFFBB, 0x0010}, {0xFFBC, 0x0010},
    {0xFFBD, 0x0010}, {0xFFBE, 0x0010}, {0xFFBF, 0x0010}, {0x0000, 0x0000},
    {0x0000, 0x0000}, {0x0000, 0x0000}, {0x0000, 0x0000}, {0x0000, 0x0000},
    {0x0000, 0x0000}, {0x01F7, 0x0009}, {0xFFC0, 0x0010}, {0xFFC1, 0x0010},
    {0xFFC2, 0x0010}, {0xFFC3, 0x0010}, {0xFFC4, 0x0010}, {0xFFC5, 0x0010},
    {0xFFC6, 0x0010}, {0xFFC7, 0x0010}, {0xFFC8, 0x0010}, {0x0000, 0x0000},
    {0x0000, 0x0000}, {0x0000, 0x0000}, {0x0000, 0x0000}, {0x0000, 0x0000},
    {0x0000, 0x0000}, {0x01F8, 0x0009}, {0xFFC9, 0x0010}, {0xFFCA, 0x0010},
    {0xFFCB, 0x0010}, {0xFFCC, 0x0010}, {0xFFCD, 0x0010}, {0xFFCE, 0x0010},
    {0xFFCF, 0x0010}, {0xFFD0, 0x0010}, {0xFFD1, 0x0010}, {0x0000, 0x0000},
    {0x0000, 0x0000}, {0x0000, 0x0000}, {0x0000, 0x0000}, {0x0000, 0x0000},
    {0x0000, 0x0000}, {0x01F9, 0x0009}, {0xFFD2, 0x0010}, {0xFFD3, 0x0010},
    {0xFFD4, 0x0010}, {0xFFD5, 0x0010}, {0xFFD6, 0x0010}, {0xFFD7, 0x0010},
    {0xFFD8, 0x0010}, {0xFFD9, 0x0010}, {0xFFDA, 0x0010}, {0x0000, 0x0000},
    {0x0000, 0x0000}, {0x0000, 0x0000}, {0x0000, 0x0000}, {0x0000, 0x0000},
    {0x0000, 0x0000}, {0x01FA, 0x0009}, {0xFFDB, 0x0010}, {0xFFDC, 0x0010},
    {0xFFDD, 0x0010}, {0xFFDE, 0x0010}, {0xFFDF, 0x0010}, {0xFFE0, 0x0010},
    {0xFFE1, 0x0010}, {0xFFE2, 0x0010}, {0xFFE3, 0x0010}, {0x0000, 0x0000},
    {0x0000, 0x0000}, {0x0000, 0x0000}, {0x0000, 0x0000}, {0x0000, 0x0000},
    {0x0000, 0x0000}, {0x07F9, 0x000B}, {0xFFE4, 0x0010}, {0xFFE5, 0x0010},
    {0xFFE6, 0x0010}, {0xFFE7, 0x0010}, {0xFFE8, 0x0010}, {0xFFE9, 0x0010},
    {0xFFEA, 0x0010}, {0xFFEB, 0x0010}, {0xFFEC, 0x0010}, {0x0000, 0x0000},
    {0x0000, 0x0000}, {0x0000, 0x0000}, {0x0000, 0x0000}, {0x0000, 0x0000},
    {0x0000, 0x0000}, {0x3FE0, 0x000E}, {0xFFED, 0x0010}, {0xFFEE, 0x0010},
    {0xFFEF, 0x0010}, {0xFFF0, 0x0010}, {0xFFF1, 0x0010}, {0xFFF2, 0x0010},
    {0xFFF3, 0x0010}, {0xFFF4, 0x0010}, {0xFFF5, 0x0010}, {0x0000, 0x0000},
    {0x0000, 0x0000}, {0x0000, 0x0000}, {0x0000, 0x0000}, {0x0000, 0x0000},
    {0x03FA, 0x000A}, {0x7FC3, 0x000F}, {0xFFF6, 0x0010}, {0xFFF7, 0x0010},
    {0xFFF8, 0x0010}, {0xFFF9, 0x0010}, {0xFFFA, 0x0010}, {0xFFFB, 0x0010},
    {0xFFFC, 0x0010}, {0xFFFD, 0x0010}, {0xFFFE, 0x0010}, {0x0000, 0x0000},
    {0x0000, 0x0000}, {0x0000, 0x0000}, {0x0000, 0x0000}, {0x0000, 0x0000},
};

// Check if the output buffer is nearly full and allocate more space
// if needed. If realloc is disabled, return true to halt the encoding.
static int jpeg_check_highwater(jpeg_buf_t *jpeg_buf) {
    if ((jpeg_buf->idx + 1) >= jpeg_buf->length - 256) {
        if (jpeg_buf->realloc == false) {
            // Can't realloc buffer
            jpeg_buf->overflow = true;
            return 1;
        }
        jpeg_buf->length += 1024;
        jpeg_buf->buf = xrealloc(jpeg_buf->buf, jpeg_buf->length);
    }
    return 0;
}

static void jpeg_put_char(jpeg_buf_t *jpeg_buf, char c) {
    if ((jpeg_buf->idx + 1) >= jpeg_buf->length) {
        if (jpeg_buf->realloc == false) {
            // Can't realloc buffer
            jpeg_buf->overflow = true;
            return;
        }
        jpeg_buf->length += 1024;
        jpeg_buf->buf = xrealloc(jpeg_buf->buf, jpeg_buf->length);
    }

    jpeg_buf->buf[jpeg_buf->idx++] = c;
}

static void jpeg_put_bytes(jpeg_buf_t *jpeg_buf, const void *data, int size) {
    if ((jpeg_buf->idx + size) >= jpeg_buf->length) {
        if (jpeg_buf->realloc == false) {
            // Can't realloc buffer
            jpeg_buf->overflow = true;
            return;
        }
        jpeg_buf->length += 1024;
        jpeg_buf->buf = xrealloc(jpeg_buf->buf, jpeg_buf->length);
    }

    memcpy(jpeg_buf->buf + jpeg_buf->idx, data, size);
    jpeg_buf->idx += size;
}

static inline void jpeg_write_bits(jpeg_buf_t *jpeg_buf, const uint16_t *bs) {
    jpeg_buf->bitc += bs[1];
    jpeg_buf->bitb |= bs[0] << (24 - jpeg_buf->bitc);

    while (jpeg_buf->bitc > 7) {
        uint8_t c = (jpeg_buf->bitb >> 16) & 255;
        jpeg_put_char(jpeg_buf, c);
        if (c == 255) {
            jpeg_put_char(jpeg_buf, 0);
        }
        jpeg_buf->bitb <<= 8;
        jpeg_buf->bitc -= 8;
    }
}

//Huffman-encoded magnitude value
static inline void jpeg_calc_bits(int val, uint16_t bits[2]) {
    int t1 = val;
    if (val < 0) {
        t1 = -val;
        val = val - 1;
    }
    bits[1] = 32 - __CLZ(t1);
    bits[0] = val & ((1 << bits[1]) - 1);
}

static int jpeg_processDU(jpeg_buf_t *jpeg_buf, int8_t *CDU, float *fdtbl, int DC, const uint16_t (*HTDC)[2],
                          const uint16_t (*HTAC)[2]) {
    int DUQ[64];
    #if defined(ARM_MATH_DSP)
    int16_t DU[64];
    #else
    int DU[64];
    int z1, z2, z3, z4, z5, z11, z13;
    int t0, t1, t2, t3, t4, t5, t6, t7, t10, t11, t12, t13;
    #endif
    const uint16_t EOB[2] = { HTAC[0x00][0], HTAC[0x00][1] };
    const uint16_t M16zeroes[2] = { HTAC[0xF0][0], HTAC[0xF0][1] };

    // DCT rows
    #if defined(ARM_MATH_DSP)
    int16_t *p = DU;
    int32_t *CDUp = (int32_t *) CDU;
    for (int i = 8; i > 0; i--, p += 8) {
        int32_t CDU_0123 = __REV(*CDUp++);
        int32_t CDU_7654 = *CDUp++;
        int32_t CDU_1_3 = __SXTB16(CDU_0123);
        int32_t CDU_0_2 = __SXTB16_RORn(CDU_0123, 8);
        int32_t CDU_6_4 = __SXTB16(CDU_7654);
        int32_t CDU_7_5 = __SXTB16_RORn(CDU_7654, 8);
        int32_t t0_t2 = __SADD16(CDU_0_2, CDU_7_5);
        int32_t t1_t3 = __SADD16(CDU_1_3, CDU_6_4);
        int32_t t7_t5 = __SSUB16(CDU_0_2, CDU_7_5);
        int32_t t6_t4 = __SSUB16(CDU_1_3, CDU_6_4);
        // Even part
        int32_t t12_t10 = __SSAX(t1_t3, t0_t2);
        int32_t t13_t11 = __SSAX(t0_t2, t1_t3);
        int32_t t25_t21 = __SADD16(t12_t10, t13_t11);
        int32_t z1 = ((int32_t) __SMUAD(t25_t21 >> 16, FIX_0_707106781)) >> 8;
        p[0] = t25_t21; // t10 + t11
        p[4] = __SSUB16(t12_t10, t13_t11); // t10 - t11
        int32_t t13 = t13_t11 >> 16;
        p[2] = t13 + z1;
        p[6] = t13 - z1;
        // Odd part
        t12_t10 = __SADD16(t7_t5, t6_t4);
        int32_t xxx_t11 = __SSAX(t7_t5, t6_t4);
        int32_t z5 = __SMUAD(__SASX(t12_t10, t12_t10), FIX_0_382683433);
        int32_t z2 = ((int32_t) __SMLAD(t12_t10, FIX_0_541196100, z5)) >> 8;
        int32_t z4 = ((int32_t) __SMLAD(t12_t10 >> 16, FIX_1_306562965, z5)) >> 8;
        int32_t z3 = ((int32_t) __SMUAD(xxx_t11, FIX_0_707106781)) >> 8;
        int32_t t7 = t7_t5 >> 16;
        int32_t z11 = t7 + z3;
        int32_t z13 = t7 - z3;
        p[5] = z13 + z2;
        p[3] = z13 - z2;
        p[1] = z11 + z4;
        p[7] = z11 - z4;
    }
    #else
    for (int i = 8, *p = DU; i > 0; i--, p += 8, CDU += 8) {
        t0 = CDU[0] + CDU[7];
        t1 = CDU[1] + CDU[6];
        t2 = CDU[2] + CDU[5];
        t3 = CDU[3] + CDU[4];

        t7 = CDU[0] - CDU[7];
        t6 = CDU[1] - CDU[6];
        t5 = CDU[2] - CDU[5];
        t4 = CDU[3] - CDU[4];

        // Even part
        t10 = t0 + t3;
        t13 = t0 - t3;
        t11 = t1 + t2;
        t12 = t1 - t2;
        z1 = MULTIPLY(t12 + t13, FIX_0_707106781); // c4

        p[0] = t10 + t11;
        p[4] = t10 - t11;
        p[2] = t13 + z1;
        p[6] = t13 - z1;

        // Odd part
        t10 = t4 + t5;// phase 2
        t11 = t5 + t6;
        t12 = t6 + t7;

        // The rotator is modified from fig 4-8 to avoid extra negations.
        z5 = MULTIPLY(t10 - t12, FIX_0_382683433); // c6
        z2 = MULTIPLY(t10, FIX_0_541196100) + z5; // 1.306562965f-c6
        z4 = MULTIPLY(t12, FIX_1_306562965) + z5; // 1.306562965f+c6
        z3 = MULTIPLY(t11, FIX_0_707106781); // c4
        z11 = t7 + z3;    // phase 5
        z13 = t7 - z3;

        p[5] = z13 + z2;// phase 6
        p[3] = z13 - z2;
        p[1] = z11 + z4;
        p[7] = z11 - z4;
    }
    #endif

    // DCT columns
    #if defined(ARM_MATH_DSP)
    int32_t *DUp = (int32_t *) DU;
    for (int i = 4; i > 0; i--, DUp++) {
        int32_t p1_p0 = DUp[0];
        int32_t p57_p56 = DUp[28];
        int32_t t0 = __SADD16(p1_p0, p57_p56);
        int32_t p9_p8 = DUp[4];
        int32_t t7 = __SSUB16(p1_p0, p57_p56);
        int32_t p49_p48 = DUp[24];
        int32_t t1 = __SADD16(p9_p8, p49_p48);
        int32_t p17_p16 = DUp[8];
        int32_t t6 = __SSUB16(p9_p8, p49_p48);
        int32_t p41_p40 = DUp[20];
        int32_t t2 = __SADD16(p17_p16, p41_p40);
        int32_t p25_p24 = DUp[12];
        int32_t t5 = __SSUB16(p17_p16, p41_p40);
        int32_t p33_p32 = DUp[16];
        int32_t t3 = __SADD16(p25_p24, p33_p32);
        int32_t t4 = __SSUB16(p25_p24, p33_p32);
        // Even part
        int32_t t10 = __SADD16(t0, t3);
        int32_t t13 = __SSUB16(t0, t3);
        int32_t t11 = __SADD16(t1, t2);
        int32_t t12 = __SSUB16(t1, t2);
        int32_t t25 = __SADD16(t12, t13);
        int32_t z1_c0 = ((int32_t) __SMUAD(t25, FIX_0_707106781)) >> 8;
        int32_t z1_c1 = ((int32_t) __SMUAD(t25 >> 16, FIX_0_707106781)) >> 8;
        int32_t z1 = __PKHBT(z1_c0, z1_c1, 16);
        DUp[0] = __SADD16(t10, t11);
        DUp[16] = __SSUB16(t10, t11);
        DUp[8] = __SADD16(t13, z1);
        DUp[24] = __SSUB16(t13, z1);
        // Odd part
        t10 = __SADD16(t4, t5);
        t11 = __SADD16(t5, t6);
        t12 = __SADD16(t6, t7);
        int32_t t22 = __SSUB16(t10, t12);
        int32_t z5_c0 = __SMUAD(t22, FIX_0_382683433);
        int32_t z5_c1 = __SMUAD(t22 >> 16, FIX_0_382683433);
        int32_t z2_c0 = ((int32_t) __SMLAD(t10, FIX_0_541196100, z5_c0)) >> 8;
        int32_t z2_c1 = ((int32_t) __SMLAD(t10 >> 16, FIX_0_541196100, z5_c1)) >> 8;
        int32_t z2 = __PKHBT(z2_c0, z2_c1, 16);
        int32_t z4_c0 = ((int32_t) __SMLAD(t12, FIX_1_306562965, z5_c0)) >> 8;
        int32_t z4_c1 = ((int32_t) __SMLAD(t12 >> 16, FIX_1_306562965, z5_c1)) >> 8;
        int32_t z4 = __PKHBT(z4_c0, z4_c1, 16);
        int32_t z3_c0 = ((int32_t) __SMUAD(t11, FIX_0_707106781)) >> 8;
        int32_t z3_c1 = ((int32_t) __SMUAD(t11 >> 16, FIX_0_707106781)) >> 8;
        int32_t z3 = __PKHBT(z3_c0, z3_c1, 16);
        int32_t z11 = __SADD16(t7, z3);
        int32_t z13 = __SSUB16(t7, z3);
        DUp[20] = __SADD16(z13, z2);
        DUp[12] = __SSUB16(z13, z2);
        DUp[4] = __SADD16(z11, z4);
        DUp[28] = __SSUB16(z11, z4);
    }
    #else
    for (int i = 8, *p = DU; i > 0; i--, p++) {
        t0 = p[0] + p[56];
        t1 = p[8] + p[48];
        t2 = p[16] + p[40];
        t3 = p[24] + p[32];

        t7 = p[0] - p[56];
        t6 = p[8] - p[48];
        t5 = p[16] - p[40];
        t4 = p[24] - p[32];

        // Even part
        t10 = t0 + t3;  // phase 2
        t13 = t0 - t3;
        t11 = t1 + t2;
        t12 = t1 - t2;
        z1 = MULTIPLY(t12 + t13, FIX_0_707106781); // c4

        p[0] = t10 + t11;               // phase 3
        p[32] = t10 - t11;
        p[16] = t13 + z1;               // phase 5
        p[48] = t13 - z1;

        // Odd part
        t10 = t4 + t5;          // phase 2
        t11 = t5 + t6;
        t12 = t6 + t7;

        // The rotator is modified from fig 4-8 to avoid extra negations.
        z5 = MULTIPLY(t10 - t12, FIX_0_382683433); // c6
        z2 = MULTIPLY(t10, FIX_0_541196100) + z5; // 1.306562965f-c6
        z4 = MULTIPLY(t12, FIX_1_306562965) + z5; // 1.306562965f+c6
        z3 = MULTIPLY(t11, FIX_0_707106781); // c4
        z11 = t7 + z3;          // phase 5
        z13 = t7 - z3;

        p[40] = z13 + z2;// phase 6
        p[24] = z13 - z2;
        p[8] = z11 + z4;
        p[56] = z11 - z4;
    }
    #endif

    // first non-zero element in reverse order
    int end0pos = 0;
    // Quantize/descale/zigzag the coefficients
    for (int i = 0; i < 64; ++i) {
        DUQ[s_jpeg_ZigZag[i]] = fast_roundf(DU[i] * fdtbl[i]);
        if (s_jpeg_ZigZag[i] > end0pos && DUQ[s_jpeg_ZigZag[i]]) {
            end0pos = s_jpeg_ZigZag[i];
        }
    }

    if (jpeg_check_highwater(jpeg_buf)) {
        // check if we're getting close to the end of the buffer
        return 0; // stop encoding, we've run out of space
    }

    // Encode DC
    int diff = DUQ[0] - DC;
    if (diff == 0) {
        jpeg_write_bits(jpeg_buf, HTDC[0]);
    } else {
        uint16_t bits[2];
        jpeg_calc_bits(diff, bits);
        jpeg_write_bits(jpeg_buf, HTDC[bits[1]]);
        jpeg_write_bits(jpeg_buf, bits);
    }

    // Encode ACs
    if (end0pos == 0) {
        jpeg_write_bits(jpeg_buf, EOB);
        return DUQ[0];
    }

    for (int i = 1; i <= end0pos; ++i) {
        int startpos = i;
        for (; DUQ[i] == 0 && i <= end0pos ; ++i) {
        }
        int nrzeroes = i - startpos;
        if (nrzeroes >= 16) {
            int lng = nrzeroes >> 4;
            for (int nrmarker = 1; nrmarker <= lng; ++nrmarker) {
                jpeg_write_bits(jpeg_buf, M16zeroes);
            }
            nrzeroes &= 15;
        }
        uint16_t bits[2];
        jpeg_calc_bits(DUQ[i], bits);
        jpeg_write_bits(jpeg_buf, HTAC[(nrzeroes << 4) + bits[1]]);
        jpeg_write_bits(jpeg_buf, bits);
    }
    if (end0pos != 63) {
        jpeg_write_bits(jpeg_buf, EOB);
    }
    return DUQ[0];
}

static void jpeg_init(int quality) {
    static int q = 0;

    quality = quality < 50 ? 5000 / quality : 200 - quality * 2;

    // If quality changed, update quantization matrix
    if (q != quality) {
        q = quality;
        for (int i = 0; i < 64; ++i) {
            int yti = (YQT[i] * quality + 50) / 100;
            YTable[s_jpeg_ZigZag[i]] = yti < 1 ? 1 : yti > 255 ? 255 : yti;
            int uvti = (UVQT[i] * quality + 50) / 100;
            UVTable[s_jpeg_ZigZag[i]] = uvti < 1 ? 1 : uvti > 255 ? 255 : uvti;
        }

        for (int r = 0, k = 0; r < 8; ++r) {
            for (int c = 0; c < 8; ++c, ++k) {
                fdtbl_Y[k] = 1.0f / (aasf[r] * aasf[c] * YTable [s_jpeg_ZigZag[k]] * 8.0f);
                fdtbl_UV[k] = 1.0f / (aasf[r] * aasf[c] * UVTable[s_jpeg_ZigZag[k]] * 8.0f);
            }
        }
    }
}

static void jpeg_write_headers(jpeg_buf_t *jpeg_buf, int w, int h, int bpp, jpeg_subsampling_t subsampling) {
    // Number of components (1 or 3)
    uint8_t nr_comp = (bpp == 1)? 1 : 3;

    // JPEG headers
    uint8_t m_soi[] = {
        0xFF, 0xD8          // SOI
    };

    uint8_t m_app0[] = {
        0xFF, 0xE0,         // APP0
        0x00, 0x10,  'J',  'F',  'I',  'F', 0x00, 0x01,
        0x01, 0x00, 0x00, 0x01, 0x00, 0x01, 0x00, 0x00
    };

    uint8_t m_dqt[] = {
        0xFF, 0xDB,         // DQT
        (bpp * 65 + 2) >> 8,      // Header length MSB
        (bpp * 65 + 2) & 0xFF,    // Header length LSB
    };

    uint8_t m_sof0[] = {
        0xFF, 0xC0,         // SOF0
        (nr_comp * 3 + 8) >> 8,   // Header length MSB
        (nr_comp * 3 + 8) & 0xFF, // Header length LSB
        0x08,               // Bits per sample
        h >> 8, h & 0xFF,       // Height
        w >> 8, w & 0xFF,       // Width
        nr_comp,            // Number of components
    };

    uint8_t m_dht[] = {
        0xFF, 0xC4,         // DHT
        (bpp * 208 + 2) >> 8,     // Header length MSB
        (bpp * 208 + 2) & 0xFF,   // Header length LSB
    };

    uint8_t m_sos[] = {
        0xFF, 0xDA,         // SOS
        (nr_comp * 2 + 6) >> 8,   // Header length MSB
        (nr_comp * 2 + 6) & 0xFF, // Header length LSB
        nr_comp,            // Number of components
    };

    // Write SOI marker
    jpeg_put_bytes(jpeg_buf, m_soi, sizeof(m_soi));
    // Write APP0 marker
    jpeg_put_bytes(jpeg_buf, m_app0, sizeof(m_app0));

    // Write DQT marker
    jpeg_put_bytes(jpeg_buf, m_dqt, sizeof(m_dqt));
    // Write Y quantization table (index, table)
    jpeg_put_char(jpeg_buf, 0);
    jpeg_put_bytes(jpeg_buf, YTable, sizeof(YTable));

    if (bpp > 1) {
        // Write UV quantization table (index, table)
        jpeg_put_char(jpeg_buf, 1);
        jpeg_put_bytes(jpeg_buf, UVTable, sizeof(UVTable));
    }

    // Write SOF0 marker
    jpeg_put_bytes(jpeg_buf, m_sof0, sizeof(m_sof0));
    for (int i = 0; i < nr_comp; i++) {
        // Component ID, HV sampling, q table idx
        jpeg_put_bytes(jpeg_buf, (uint8_t [3]) {i + 1, (i == 0 && bpp == 2)? subsampling:0x11, (i > 0)}, 3);

    }

    // Write DHT marker
    jpeg_put_bytes(jpeg_buf, m_dht, sizeof(m_dht));

    // Write DHT-YDC
    jpeg_put_char(jpeg_buf, 0x00);
    jpeg_put_bytes(jpeg_buf, std_dc_luminance_nrcodes + 1, sizeof(std_dc_luminance_nrcodes) - 1);
    jpeg_put_bytes(jpeg_buf, std_dc_luminance_values, sizeof(std_dc_luminance_values));

    // Write DHT-YAC
    jpeg_put_char(jpeg_buf, 0x10);
    jpeg_put_bytes(jpeg_buf, std_ac_luminance_nrcodes + 1, sizeof(std_ac_luminance_nrcodes) - 1);
    jpeg_put_bytes(jpeg_buf, std_ac_luminance_values, sizeof(std_ac_luminance_values));

    if (bpp > 1) {
        // Write DHT-UDC
        jpeg_put_char(jpeg_buf, 0x01);
        jpeg_put_bytes(jpeg_buf, std_dc_chrominance_nrcodes + 1, sizeof(std_dc_chrominance_nrcodes) - 1);
        jpeg_put_bytes(jpeg_buf, std_dc_chrominance_values, sizeof(std_dc_chrominance_values));

        // Write DHT-UAC
        jpeg_put_char(jpeg_buf, 0x11);
        jpeg_put_bytes(jpeg_buf, std_ac_chrominance_nrcodes + 1, sizeof(std_ac_chrominance_nrcodes) - 1);
        jpeg_put_bytes(jpeg_buf, std_ac_chrominance_values, sizeof(std_ac_chrominance_values));
    }

    // Write SOS marker
    jpeg_put_bytes(jpeg_buf, m_sos, sizeof(m_sos));
    for (int i = 0; i < nr_comp; i++) {
        jpeg_put_bytes(jpeg_buf, (uint8_t [2]) {i + 1, (i == 0)? 0x00:0x11}, 2);
    }

    // Spectral selection
    jpeg_put_bytes(jpeg_buf, (uint8_t [3]) {0x00, 0x3F, 0x0}, 3);
}

bool jpeg_compress(image_t *src, image_t *dst, int quality, bool realloc, jpeg_subsampling_t subsampling) {
    OMV_PROFILE_START();

    if (!dst->data) {
        uint32_t size = 0;
        dst->data = fb_alloc_all(&size, FB_ALLOC_PREFER_SIZE | FB_ALLOC_CACHE_ALIGN);
        dst->size = IMLIB_IMAGE_MAX_SIZE(size);
    }

    if (src->is_compressed) {
        return true;
    }

    // JPEG buffer
    jpeg_buf_t jpeg_buf = {
        .idx = 0,
        .buf = dst->pixels,
        .length = dst->size,
        .bitc = 0,
        .bitb = 0,
        .realloc = realloc,
        .overflow = false,
    };

    // Initialize quantization tables
    jpeg_init(quality);

    if (src->is_color) {
        if (subsampling == JPEG_SUBSAMPLING_AUTO) {
            if (quality <= 35) {
                subsampling = JPEG_SUBSAMPLING_420;
            } else if (quality < 60) {
                subsampling = JPEG_SUBSAMPLING_422;
            } else {
                subsampling = JPEG_SUBSAMPLING_444;
            }
        }
    } else {
        subsampling = JPEG_SUBSAMPLING_444;
    }

    jpeg_write_headers(&jpeg_buf, src->w, src->h, src->is_color ? 2 : 1, subsampling);

    int DCY = 0, DCU = 0, DCV = 0;

    switch (subsampling) {
        // Quiet GCC compiler warning (this is never reached)
        case JPEG_SUBSAMPLING_AUTO: {
            break;
        }
        case JPEG_SUBSAMPLING_444: {
            int8_t YDU[JPEG_444_GS_MCU_SIZE];
            int8_t UDU[JPEG_444_GS_MCU_SIZE];
            int8_t VDU[JPEG_444_GS_MCU_SIZE];

            for (int y_offset = 0; y_offset < src->h; y_offset += JPEG_MCU_H) {
                int dy = IM_MIN(JPEG_MCU_H, src->h - y_offset);

                for (int x_offset = 0; x_offset < src->w; x_offset += JPEG_MCU_W) {
                    int dx = IM_MIN(JPEG_MCU_W, src->w - x_offset);

                    jpeg_get_mcu(src, x_offset, y_offset, dx, dy, YDU, UDU, VDU);
                    DCY = jpeg_processDU(&jpeg_buf, YDU, fdtbl_Y, DCY, YDC_HT, YAC_HT);

                    if (src->is_color) {
                        DCU = jpeg_processDU(&jpeg_buf, UDU, fdtbl_UV, DCU, UVDC_HT, UVAC_HT);
                        DCV = jpeg_processDU(&jpeg_buf, VDU, fdtbl_UV, DCV, UVDC_HT, UVAC_HT);
                    }
                }

                if (jpeg_buf.overflow) {
                    return true;
                }
            }
            break;
        }
        case JPEG_SUBSAMPLING_422: {
            // color only
            int8_t YDU[JPEG_444_GS_MCU_SIZE * 2];
            int8_t UDU[JPEG_444_GS_MCU_SIZE * 2];
            int8_t VDU[JPEG_444_GS_MCU_SIZE * 2];
            int8_t UDU_avg[JPEG_444_GS_MCU_SIZE];
            int8_t VDU_avg[JPEG_444_GS_MCU_SIZE];

            for (int y_offset = 0; y_offset < src->h; y_offset += JPEG_MCU_H) {
                int dy = IM_MIN(JPEG_MCU_H, src->h - y_offset);

                for (int x_offset = 0; x_offset < src->w; ) {
                    for (int i = 0; i < (JPEG_444_GS_MCU_SIZE * 2);
                         i += JPEG_444_GS_MCU_SIZE, x_offset += JPEG_MCU_W) {
                        int dx = IM_MIN(JPEG_MCU_W, src->w - x_offset);

                        if (dx > 0) {
                            jpeg_get_mcu(src, x_offset, y_offset, dx, dy, YDU + i, UDU + i, VDU + i);
                        } else {
                            memset(YDU + i, 0, JPEG_444_GS_MCU_SIZE);
                            memset(UDU + i, 0, JPEG_444_GS_MCU_SIZE);
                            memset(VDU + i, 0, JPEG_444_GS_MCU_SIZE);
                        }

                        DCY = jpeg_processDU(&jpeg_buf, YDU + i, fdtbl_Y, DCY, YDC_HT, YAC_HT);
                    }

                    // horizontal subsampling of U & V
                    #if defined(ARM_MATH_DSP)
                    uint32_t *UDUp0 = (uint32_t *) UDU;
                    uint32_t *VDUp0 = (uint32_t *) VDU;
                    uint32_t *UDUp1 = (uint32_t *) (UDU + JPEG_444_GS_MCU_SIZE);
                    uint32_t *VDUp1 = (uint32_t *) (VDU + JPEG_444_GS_MCU_SIZE);
                    #else
                    int8_t *UDUp0 = UDU;
                    int8_t *VDUp0 = VDU;
                    int8_t *UDUp1 = UDUp0 + JPEG_444_GS_MCU_SIZE;
                    int8_t *VDUp1 = VDUp0 + JPEG_444_GS_MCU_SIZE;
                    #endif
                    for (int j = 0; j < JPEG_444_GS_MCU_SIZE; j += JPEG_MCU_W) {
                        #if defined(ARM_MATH_DSP)
                        uint32_t UDUp0_3210 = *UDUp0++;
                        uint32_t UDUp0_avg_32_10 = __SHADD8(UDUp0_3210, __UXTB16_RORn(UDUp0_3210, 8));
                        UDU_avg[j] = UDUp0_avg_32_10;
                        UDU_avg[j + 1] = UDUp0_avg_32_10 >> 16;

                        uint32_t UDUp0_7654 = *UDUp0++;
                        uint32_t UDUp0_avg_76_54 = __SHADD8(UDUp0_7654, __UXTB16_RORn(UDUp0_7654, 8));
                        UDU_avg[j + 2] = UDUp0_avg_76_54;
                        UDU_avg[j + 3] = UDUp0_avg_76_54 >> 16;

                        uint32_t UDUp1_3210 = *UDUp1++;
                        uint32_t UDUp1_avg_32_10 = __SHADD8(UDUp1_3210, __UXTB16_RORn(UDUp1_3210, 8));
                        UDU_avg[j + 4] = UDUp1_avg_32_10;
                        UDU_avg[j + 5] = UDUp1_avg_32_10 >> 16;

                        uint32_t UDUp1_7654 = *UDUp1++;
                        uint32_t UDUp1_avg_76_54 = __SHADD8(UDUp1_7654, __UXTB16_RORn(UDUp1_7654, 8));
                        UDU_avg[j + 6] = UDUp1_avg_76_54;
                        UDU_avg[j + 7] = UDUp1_avg_76_54 >> 16;

                        uint32_t VDUp0_3210 = *VDUp0++;
                        uint32_t VDUp0_avg_32_10 = __SHADD8(VDUp0_3210, __UXTB16_RORn(VDUp0_3210, 8));
                        VDU_avg[j] = VDUp0_avg_32_10;
                        VDU_avg[j + 1] = VDUp0_avg_32_10 >> 16;

                        uint32_t VDUp0_7654 = *VDUp0++;
                        uint32_t VDUp0_avg_76_54 = __SHADD8(VDUp0_7654, __UXTB16_RORn(VDUp0_7654, 8));
                        VDU_avg[j + 2] = VDUp0_avg_76_54;
                        VDU_avg[j + 3] = VDUp0_avg_76_54 >> 16;

                        uint32_t VDUp1_3210 = *VDUp1++;
                        uint32_t VDUp1_avg_32_10 = __SHADD8(VDUp1_3210, __UXTB16_RORn(VDUp1_3210, 8));
                        VDU_avg[j + 4] = VDUp1_avg_32_10;
                        VDU_avg[j + 5] = VDUp1_avg_32_10 >> 16;

                        uint32_t VDUp1_7654 = *VDUp1++;
                        uint32_t VDUp1_avg_76_54 = __SHADD8(VDUp1_7654, __UXTB16_RORn(VDUp1_7654, 8));
                        VDU_avg[j + 6] = VDUp1_avg_76_54;
                        VDU_avg[j + 7] = VDUp1_avg_76_54 >> 16;
                        #else
                        for (int i = 0; i < JPEG_MCU_W; i += 2) {
                            UDU_avg[j + (i / 2)] = (UDUp0[i] + UDUp0[i + 1]) / 2;
                            VDU_avg[j + (i / 2)] = (VDUp0[i] + VDUp0[i + 1]) / 2;
                            UDU_avg[j + (i / 2) + (JPEG_MCU_W / 2)] = (UDUp1[i] + UDUp1[i + 1]) / 2;
                            VDU_avg[j + (i / 2) + (JPEG_MCU_W / 2)] = (VDUp1[i] + VDUp1[i + 1]) / 2;
                        }
                        UDUp0 += JPEG_MCU_W;
                        VDUp0 += JPEG_MCU_W;
                        UDUp1 += JPEG_MCU_W;
                        VDUp1 += JPEG_MCU_W;
                        #endif
                    }

                    DCU = jpeg_processDU(&jpeg_buf, UDU_avg, fdtbl_UV, DCU, UVDC_HT, UVAC_HT);
                    DCV = jpeg_processDU(&jpeg_buf, VDU_avg, fdtbl_UV, DCV, UVDC_HT, UVAC_HT);
                }

                if (jpeg_buf.overflow) {
                    return true;
                }
            }
            break;
        }
        case JPEG_SUBSAMPLING_420: {
            // color only
            int8_t YDU[JPEG_444_GS_MCU_SIZE * 4];
            int8_t UDU[JPEG_444_GS_MCU_SIZE * 4];
            int8_t VDU[JPEG_444_GS_MCU_SIZE * 4];
            int8_t UDU_avg[JPEG_444_GS_MCU_SIZE];
            int8_t VDU_avg[JPEG_444_GS_MCU_SIZE];

            for (int y_offset = 0; y_offset < src->h; ) {
                for (int x_offset = 0; x_offset < src->w; ) {
                    for (int j = 0; j < (JPEG_444_GS_MCU_SIZE * 4);
                         j += (JPEG_444_GS_MCU_SIZE * 2), y_offset += JPEG_MCU_H) {
                        int dy = IM_MIN(JPEG_MCU_H, src->h - y_offset);

                        for (int i = 0; i < (JPEG_444_GS_MCU_SIZE * 2);
                             i += JPEG_444_GS_MCU_SIZE, x_offset += JPEG_MCU_W) {
                            int dx = IM_MIN(JPEG_MCU_W, src->w - x_offset);

                            if ((dx > 0) && (dy > 0)) {
                                jpeg_get_mcu(src, x_offset, y_offset, dx, dy, YDU + i + j, UDU + i + j, VDU + i + j);
                            } else {
                                memset(YDU + i + j, 0, JPEG_444_GS_MCU_SIZE);
                                memset(UDU + i + j, 0, JPEG_444_GS_MCU_SIZE);
                                memset(VDU + i + j, 0, JPEG_444_GS_MCU_SIZE);
                            }

                            DCY = jpeg_processDU(&jpeg_buf, YDU + i + j, fdtbl_Y, DCY, YDC_HT, YAC_HT);
                        }

                        // Reset back two columns.
                        x_offset -= (JPEG_MCU_W * 2);
                    }

                    // Advance to the next columns.
                    x_offset += (JPEG_MCU_W * 2);

                    // Reset back two rows.
                    y_offset -= (JPEG_MCU_H * 2);

                    // horizontal and vertical subsampling of U & V
                    #if defined(ARM_MATH_DSP)
                    uint32_t *UDUp = (uint32_t *) UDU;
                    uint32_t *VDUp = (uint32_t *) VDU;
                    #else
                    int8_t *UDUp0 = UDU;
                    int8_t *VDUp0 = VDU;
                    int8_t *UDUp1 = UDUp0 + JPEG_444_GS_MCU_SIZE;
                    int8_t *VDUp1 = VDUp0 + JPEG_444_GS_MCU_SIZE;
                    int8_t *UDUp2 = UDUp1 + JPEG_444_GS_MCU_SIZE;
                    int8_t *VDUp2 = VDUp1 + JPEG_444_GS_MCU_SIZE;
                    int8_t *UDUp3 = UDUp2 + JPEG_444_GS_MCU_SIZE;
                    int8_t *VDUp3 = VDUp2 + JPEG_444_GS_MCU_SIZE;
                    #endif
                    for (int j = 0, k = JPEG_444_GS_MCU_SIZE / 2; k < JPEG_444_GS_MCU_SIZE;
                         j += JPEG_MCU_W, k += JPEG_MCU_W) {
                        #if defined(ARM_MATH_DSP)
                        for (int i = 0; i < 4; i++) {
                            int index = ((i & 2) ? k : j) + ((i & 1) * 4);

                            uint32_t UDU_r0_3210 = UDUp[i * 16];
                            uint32_t UDU_r0_avg_32_10 = __SHADD8(UDU_r0_3210, __UXTB16_RORn(UDU_r0_3210, 8));
                            uint32_t UDU_r0_7654 = UDUp[(i * 16) + 1];
                            uint32_t UDU_r0_avg_76_54 = __SHADD8(UDU_r0_7654, __UXTB16_RORn(UDU_r0_7654, 8));

                            uint32_t UDU_r1_3210 = UDUp[(i * 16) + 2];
                            uint32_t UDU_r1_avg_32_10 = __SHADD8(UDU_r1_3210, __UXTB16_RORn(UDU_r1_3210, 8));
                            uint32_t UDU_r1_7654 = UDUp[(i * 16) + 3];
                            uint32_t UDU_r1_avg_76_54 = __SHADD8(UDU_r1_7654, __UXTB16_RORn(UDU_r1_7654, 8));

                            uint32_t UDU_r0_r1_avg_32_10 = __SHADD8(UDU_r0_avg_32_10, UDU_r1_avg_32_10);
                            UDU_avg[index] = UDU_r0_r1_avg_32_10;
                            UDU_avg[index + 1] = UDU_r0_r1_avg_32_10 >> 16;

                            uint32_t UDU_r0_r1_avg_76_54 = __SHADD8(UDU_r0_avg_76_54, UDU_r1_avg_76_54);
                            UDU_avg[index + 2] = UDU_r0_r1_avg_76_54;
                            UDU_avg[index + 3] = UDU_r0_r1_avg_76_54 >> 16;

                            uint32_t VDU_r0_3210 = VDUp[i * 16];
                            uint32_t VDU_r0_avg_32_10 = __SHADD8(VDU_r0_3210, __UXTB16_RORn(VDU_r0_3210, 8));
                            uint32_t VDU_r0_7654 = VDUp[(i * 16) + 1];
                            uint32_t VDU_r0_avg_76_54 = __SHADD8(VDU_r0_7654, __UXTB16_RORn(VDU_r0_7654, 8));

                            uint32_t VDU_r1_3210 = VDUp[(i * 16) + 2];
                            uint32_t VDU_r1_avg_32_10 = __SHADD8(VDU_r1_3210, __UXTB16_RORn(VDU_r1_3210, 8));
                            uint32_t VDU_r1_7654 = VDUp[(i * 16) + 3];
                            uint32_t VDU_r1_avg_76_54 = __SHADD8(VDU_r1_7654, __UXTB16_RORn(VDU_r1_7654, 8));

                            uint32_t VDU_r0_r1_avg_32_10 = __SHADD8(VDU_r0_avg_32_10, VDU_r1_avg_32_10);
                            VDU_avg[index] = VDU_r0_r1_avg_32_10;
                            VDU_avg[index + 1] = VDU_r0_r1_avg_32_10 >> 16;

                            uint32_t VDU_r0_r1_avg_76_54 = __SHADD8(VDU_r0_avg_76_54, VDU_r1_avg_76_54);
                            VDU_avg[index + 2] = VDU_r0_r1_avg_76_54;
                            VDU_avg[index + 3] = VDU_r0_r1_avg_76_54 >> 16;
                        }
                        UDUp += 4;
                        VDUp += 4;
                        #else
                        for (int i = 0; i < JPEG_MCU_W; i += 2) {
                            UDU_avg[j + (i / 2)] =
                                (UDUp0[i] + UDUp0[i + 1] + UDUp0[i + JPEG_MCU_W] + UDUp0[i + 1 + JPEG_MCU_W]) / 4;
                            VDU_avg[j + (i / 2)] =
                                (VDUp0[i] + VDUp0[i + 1] + VDUp0[i + JPEG_MCU_W] + VDUp0[i + 1 + JPEG_MCU_W]) / 4;
                            UDU_avg[j + (i / 2) + (JPEG_MCU_W / 2)] =
                                (UDUp1[i] + UDUp1[i + 1] + UDUp1[i + JPEG_MCU_W] + UDUp1[i + 1 + JPEG_MCU_W]) / 4;
                            VDU_avg[j + (i / 2) + (JPEG_MCU_W / 2)] =
                                (VDUp1[i] + VDUp1[i + 1] + VDUp1[i + JPEG_MCU_W] + VDUp1[i + 1 + JPEG_MCU_W]) / 4;
                            UDU_avg[k + (i / 2)] =
                                (UDUp2[i] + UDUp2[i + 1] + UDUp2[i + JPEG_MCU_W] + UDUp2[i + 1 + JPEG_MCU_W]) / 4;
                            VDU_avg[k + (i / 2)] =
                                (VDUp2[i] + VDUp2[i + 1] + VDUp2[i + JPEG_MCU_W] + VDUp2[i + 1 + JPEG_MCU_W]) / 4;
                            UDU_avg[k + (i / 2) + (JPEG_MCU_W / 2)] =
                                (UDUp3[i] + UDUp3[i + 1] + UDUp3[i + JPEG_MCU_W] + UDUp3[i + 1 + JPEG_MCU_W]) / 4;
                            VDU_avg[k + (i / 2) + (JPEG_MCU_W / 2)] =
                                (VDUp3[i] + VDUp3[i + 1] + VDUp3[i + JPEG_MCU_W] + VDUp3[i + 1 + JPEG_MCU_W]) / 4;
                        }
                        UDUp0 += JPEG_MCU_W * 2;
                        VDUp0 += JPEG_MCU_W * 2;
                        UDUp1 += JPEG_MCU_W * 2;
                        VDUp1 += JPEG_MCU_W * 2;
                        UDUp2 += JPEG_MCU_W * 2;
                        VDUp2 += JPEG_MCU_W * 2;
                        UDUp3 += JPEG_MCU_W * 2;
                        VDUp3 += JPEG_MCU_W * 2;
                        #endif
                    }

                    DCU = jpeg_processDU(&jpeg_buf, UDU_avg, fdtbl_UV, DCU, UVDC_HT, UVAC_HT);
                    DCV = jpeg_processDU(&jpeg_buf, VDU_avg, fdtbl_UV, DCV, UVDC_HT, UVAC_HT);
                }

                if (jpeg_buf.overflow) {
                    return true;
                }

                // Advance to the next rows.
                y_offset += (JPEG_MCU_H * 2);
            }
            break;
        }
    }

    // Do the bit alignment of the EOI marker
    jpeg_write_bits(&jpeg_buf, (const uint16_t []) {0x7F, 7});

    // EOI
    jpeg_put_char(&jpeg_buf, 0xFF);
    jpeg_put_char(&jpeg_buf, 0xD9);

    dst->size = jpeg_buf.idx;
    dst->data = jpeg_buf.buf;

    OMV_PROFILE_PRINT();
    return false;
}

#endif // (OMV_JPEG_CODEC_ENABLE == 0)

bool jpeg_is_valid(image_t *img) {
    uint8_t *p = img->data, *p_end = img->data + img->size;
    while (p < p_end) {
        uint16_t header = (p[0] << 8) | p[1];
        p += sizeof(uint16_t);
        if ((0xFFD0 <= header) && (header <= 0xFFD9)) {
            continue;
        } else if (0xFFDA == header) {
            // Start-of-Scan (no more jpeg headers left).
            return true;
        } else if (((0xFFC0 <= header) && (header <= 0xFFCF))
                   || ((0xFFDB <= header) && (header <= 0xFFDF))
                   || ((0xFFE0 <= header) && (header <= 0xFFEF))
                   || ((0xFFF0 <= header) && (header <= 0xFFFE))) {
            uint16_t size = (p[0] << 8) | p[1];
            p += sizeof(uint16_t);
            if (((0xFFC1 <= header) && (header <= 0xFFC3))
                || ((0xFFC5 <= header) && (header <= 0xFFC7))
                || ((0xFFC9 <= header) && (header <= 0xFFCB))
                || ((0xFFCD <= header) && (header <= 0xFFCF))) {
                // Non-baseline jpeg.
                return false;
            } else {
                p += size - sizeof(uint16_t);
            }
        } else {
            // Invalid JPEG
            return false;
        }
    }
    return false;
}

int jpeg_clean_trailing_bytes(int size, uint8_t *data) {
    while ((size > 1) && ((data[size - 2] != 0xFF) || (data[size - 1] != 0xD9))) {
        size -= 1;
    }

    return size;
}

#if defined(IMLIB_ENABLE_IMAGE_FILE_IO)
// This function inits the geometry values of an image.
void jpeg_read_geometry(FIL *fp, image_t *img, const char *path, jpg_read_settings_t *rs) {
    for (;;) {
        uint16_t header;
        file_read(fp, &header, 2);
        header = __REV16(header);
        if ((0xFFD0 <= header) && (header <= 0xFFD9)) {
            continue;
        } else if (((0xFFC0 <= header) && (header <= 0xFFCF))
                   || ((0xFFDA <= header) && (header <= 0xFFDF))
                   || ((0xFFE0 <= header) && (header <= 0xFFEF))
                   || ((0xFFF0 <= header) && (header <= 0xFFFE))) {
            uint16_t size;
            file_read(fp, &size, 2);
            size = __REV16(size);
            if (((0xFFC0 <= header) && (header <= 0xFFC3))
                || ((0xFFC5 <= header) && (header <= 0xFFC7))
                || ((0xFFC9 <= header) && (header <= 0xFFCB))
                || ((0xFFCD <= header) && (header <= 0xFFCF))) {
                file_read(fp, NULL, 1);
                uint16_t height;
                file_read(fp, &height, 2);
                height = __REV16(height);

                uint16_t width;
                file_read(fp, &width, 2);
                width = __REV16(width);

                rs->jpg_w = width;
                rs->jpg_h = height;
                rs->jpg_size = IMLIB_IMAGE_MAX_SIZE(f_size(fp));

                img->w = rs->jpg_w;
                img->h = rs->jpg_h;
                img->size = rs->jpg_size;
                img->pixfmt = PIXFORMAT_JPEG;
                return;
            } else {
                file_seek(fp, f_tell(fp) + size - 2);
            }
        } else {
            file_raise_corrupted(fp);
        }
    }
}

// This function reads the pixel values of an image.
void jpeg_read_pixels(FIL *fp, image_t *img) {
    file_seek(fp, 0);
    file_read(fp, img->pixels, img->size);
}

void jpeg_read(image_t *img, const char *path) {
    FIL fp;
    jpg_read_settings_t rs;

    // Do not use file buffering here.
    file_open(&fp, path, false, FA_READ | FA_OPEN_EXISTING);
    jpeg_read_geometry(&fp, img, path, &rs);

    if (!img->pixels) {
        image_xalloc(img, img->size);
    }

    jpeg_read_pixels(&fp, img);
    file_close(&fp);
}

void jpeg_write(image_t *img, const char *path, int quality) {
    FIL fp;
    file_open(&fp, path, false, FA_WRITE | FA_CREATE_ALWAYS);
    if (IM_IS_JPEG(img)) {
        file_write(&fp, img->pixels, img->size);
    } else {
        // alloc in jpeg compress
        image_t out = { .w = img->w, .h = img->h, .pixfmt = PIXFORMAT_JPEG, .size = 0, .pixels = NULL };
        // When jpeg_compress needs more memory than in currently allocated it
        // will try to realloc. MP will detect that the pointer is outside of
        // the heap and return NULL which will cause an out of memory error.
        jpeg_compress(img, &out, quality, false, JPEG_SUBSAMPLING_AUTO);
        file_write(&fp, out.pixels, out.size);
        fb_free(); // frees alloc in jpeg_compress()
    }
    file_close(&fp);
}
#endif //IMLIB_ENABLE_IMAGE_FILE_IO)
