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
 * Debayering Functions
 */
#include "imlib.h"

#define VBAYER_Y_STRIDE     (2)
#define VBAYER_X_STRIDE     ((UINT8_VECTOR_SIZE) / 2)
#define VBAYER_X_STRIDE_2X2 ((VBAYER_X_STRIDE) * 4)

#define VBAYER_BUF_KSIZE    ((VBAYER_Y_STRIDE) * 2)
#define VBAYER_BUF_BROWS    ((VBAYER_Y_STRIDE) * 4)

pixformat_t imlib_bayer_shift(pixformat_t pixfmt, int x, int y, bool transpose) {
    bool shift_right = x % 2;
    bool shift_down = y % 2;

    switch (pixfmt) {
        case PIXFORMAT_BAYER_BGGR: {
            if (shift_right && shift_down) {
                return PIXFORMAT_BAYER_RGGB;
            } else if (shift_right) {
                return transpose ? PIXFORMAT_BAYER_GRBG : PIXFORMAT_BAYER_GBRG;
            } else if (shift_down) {
                return transpose ? PIXFORMAT_BAYER_GBRG : PIXFORMAT_BAYER_GRBG;
            } else {
                return pixfmt;
            }
        }
        case PIXFORMAT_BAYER_GBRG: {
            if (shift_right && shift_down) {
                return transpose ? PIXFORMAT_BAYER_GBRG : PIXFORMAT_BAYER_GRBG;
            } else if (shift_right) {
                return PIXFORMAT_BAYER_BGGR;
            } else if (shift_down) {
                return PIXFORMAT_BAYER_RGGB;
            } else {
                return pixfmt;
            }
        }
        case PIXFORMAT_BAYER_GRBG: {
            if (shift_right && shift_down) {
                return transpose ? PIXFORMAT_BAYER_GRBG : PIXFORMAT_BAYER_GBRG;
            } else if (shift_right) {
                return PIXFORMAT_BAYER_RGGB;
            } else if (shift_down) {
                return PIXFORMAT_BAYER_BGGR;
            } else {
                return pixfmt;
            }
        }
        case PIXFORMAT_BAYER_RGGB: {
            if (shift_right && shift_down) {
                return PIXFORMAT_BAYER_BGGR;
            } else if (shift_right) {
                return transpose ? PIXFORMAT_BAYER_GBRG : PIXFORMAT_BAYER_GRBG;
            } else if (shift_down) {
                return transpose ? PIXFORMAT_BAYER_GRBG : PIXFORMAT_BAYER_GBRG;
            } else {
                return pixfmt;
            }
        }
        default: {
            __builtin_unreachable();
        }
    }
}

// Row vectors are loaded into memory and processed in little-endian order.
// row_0 stores MSB [G1, R1, G0, R0] LSB pixels where each pixel is 8-bits.
// row_1 stores MSB [B1, G3, B0, G2] LSB pixels where each pixel is 8-bits.
// row_2 stores MSB [G5, R3, G4, R2] LSB pixels where each pixel is 8-bits.
// In the case of vectors larger than 32-bits the pattern is repeated for every 32-bits.
//
// vdebayer_bggr produces two output pixels per 32-bits:
// pixels.r = MSB [0, R@G3, 0, R@B0] LSB pixels where each pixel is 8-bits.
// pixels.g = MSB [0, G@G3, 0, G@B0] LSB pixels where each pixel is 8-bits.
// pixels.b = MSB [0, B@G3, 0, B@B0] LSB pixels where each pixel is 8-bits.
static inline vrgb_pixels_t vdebayer_bggr(v128_t row_0, v128_t row_1, v128_t row_2) {
    vrgb_pixels_t pixels;
    v128_t row_02 = vhadd_u8(row_0, row_2);
    // row_02 = [(G1+G5)/2, (R1+R3)/2, (G0+G4)/2, (R0+R2)/2]
    v128_t row_11 = vhadd_u8(row_1, vpkhtb(row_1, row_1));
    // row_11 = [(B1+B1)/2, (G3+G3)/2, (B0+B1)/2, (G2+G3)/2]
    // row_11 = [B1, G3, (B0+B1)/2, (G2+G3)/2]
    v128_t t_r_pixels = vhadd_u8(row_02, vpkhtb(row_02, row_02));
    // t_r_pixels = [(G1+G5+G1+G5)/4, (R1+R3+R1+R3)/4, (G1+G5+G0+G4)/4, (R1+R3+R0+R2)/4]
    // t_r_pixels = [(G1+G5)/2, R@G3, G@G3, R@B0]
    pixels.r = vuxtb16(t_r_pixels);
    // pixels.r = [0, R@G3, 0, R@B0]
    v128_t t_g_pixels = vhadd_u8(row_11, vpkhtb_ror8(row_11, row_02));
    // t_g_pixels = [(B1+B1)/2, (G3+G3)/2, (B0+B1+R1+R3)/4, (G2+G3+G0+G4)/4]
    // t_g_pixels = [B@B1, G@G3, (B0+B1+R1+R3)/4, G@B0]
    pixels.g = vuxtb16(t_g_pixels);
    // pixels.g = [0, G@G3, 0, G@B0]
    v128_t t_b_pixels = vhadd_u8(row_1, vpkhbt(row_1, row_1));
    // t_b_pixels = [(B1+B0)/2, (G3+G2)/2, (B0+B0)/2, (G2+G2)/2]
    // t_b_pixels = [B@G3, (G3+G2)/2, B@B0, G@G2]
    pixels.b = vuxtb16_ror8(t_b_pixels);
    // pixels.b = [0, B@G3, 0, B@B0]
    return pixels;
}

// Row vectors are loaded into memory and processed in little-endian order.
// row_0 stores MSB [R1, G1, R0, G0] LSB pixels where each pixel is 8-bits.
// row_1 stores MSB [G3, B1, G2, B0] LSB pixels where each pixel is 8-bits.
// row_2 stores MSB [R3, G5, R2, G4] LSB pixels where each pixel is 8-bits.
// In the case of vectors larger than 32-bits the pattern is repeated for every 32-bits.
//
// vdebayer_gbrg produces two output pixels per 32-bits:
// pixels.r = MSB [0, R@B1, 0, R@G2] LSB pixels where each pixel is 8-bits.
// pixels.g = MSB [0, G@B1, 0, G@G2] LSB pixels where each pixel is 8-bits.
// pixels.b = MSB [0, B@B1, 0, B@G2] LSB pixels where each pixel is 8-bits.
static inline vrgb_pixels_t vdebayer_gbrg(v128_t row_0, v128_t row_1, v128_t row_2) {
    vrgb_pixels_t pixels;
    v128_t row_02 = vhadd_u8(row_0, row_2);
    // row_02 = [(R1+R3)/2, (G1+G5)/2, (R0+R2)/2, (G0+G4)/2]
    v128_t row_11 = vhadd_u8(row_1, vpkhbt(row_1, row_1));
    // row_11 = [(G3+G2)/2, (B1+B0)/2, (G2+G2)/2, (B0+B0)/2]
    // row_11 = [(G3+G2)/2, B@G2, G@G2, B@B0]
    v128_t t_r_pixels = vhadd_u8(row_02, vpkhbt(row_02, row_02));
    // t_r_pixels = [(R1+R3+R0+R2)/4, (G1+G5+G0+G4)/4, (R0+R2+R0+R2)/4, (G0+G4+G0+G4)/4]
    // t_r_pixels = [R@B1, (G1+G5+G0+G4)/4, R@G2, (G0+G4)/2]
    pixels.r = vuxtb16_ror8(t_r_pixels);
    // pixels.r = [0, R@B1, 0, R@G2]
    v128_t t_g_pixels = vhadd_u8(row_11, vpkhbt_ror8(row_11, row_02));
    // t_g_pixels = [(G3+G2+G1+G5)/4, (B1+B0+R0+R2)/4, (G2+G2+G2+G2)/4, (B0+B0+B0+B0)/2]
    // t_g_pixels = [G@B1, (B1+B0+R0+R2)/4, G@G2, B@B0]
    pixels.g = vuxtb16_ror8(t_g_pixels);
    // pixels.g = [0, G@B1, 0, G@G2]
    v128_t t_b_pixels = vhadd_u8(row_1, vpkhtb(row_1, row_1));
    // t_b_pixels = [(G3+G3)/2, (B1+B1)/2, (G2+G3)/2, (B0+B1)/2]
    // t_b_pixels = [(G3+G3)/2, B@B1, (G2+G3)/2, B@G2]
    pixels.b = vuxtb16(t_b_pixels);
    // pixels.b = [0, B@B1, 0, B@G2]
    return pixels;
}

// Row vectors are loaded into memory and processed in little-endian order.
// row_0 stores MSB [B1, G1, B0, G0] LSB pixels where each pixel is 8-bits.
// row_1 stores MSB [G3, R3, G2, R0] LSB pixels where each pixel is 8-bits.
// row_2 stores MSB [B3, G5, B2, G4] LSB pixels where each pixel is 8-bits.
// In the case of vectors larger than 32-bits the pattern is repeated for every 32-bits.
//
// vdebayer_grbg produces two output pixels per 32-bits:
// pixels.r = MSB [0, R@R3, 0, R@G2] LSB pixels where each pixel is 8-bits.
// pixels.g = MSB [0, G@R3, 0, G@G2] LSB pixels where each pixel is 8-bits.
// pixels.b = MSB [0, B@R3, 0, B@G2] LSB pixels where each pixel is 8-bits.
static inline vrgb_pixels_t vdebayer_grbg(v128_t row_0, v128_t row_1, v128_t row_2) {
    vrgb_pixels_t pixels;
    v128_t row_02 = vhadd_u8(row_0, row_2);
    // row_02 = [(B1+B3)/2, (G1+G5)/2, (B0+B2)/2, (G0+G4)/2]
    v128_t row_11 = vhadd_u8(row_1, vpkhbt(row_1, row_1));
    // row_11 = [(G3+G2)/2, (R3+R0)/2, (G2+G2)/2, (R0+R0)/2]
    // row_11 = [(G3+G2)/2, R@G2, G@G2, R@R0]
    v128_t t_r_pixels = vhadd_u8(row_1, vpkhtb(row_1, row_1));
    // t_r_pixels = [(G3+G3)/2, (R3+R3)/2, (G2+G3)/2, (R0+R3)/2]
    // t_r_pixels = [(G3+G3)/2, R@R3, (G2+G3)/2, R@G2]
    pixels.r = vuxtb16(t_r_pixels);
    // pixels.r = [0, R@R3, 0, R@G2]
    v128_t t_g_pixels = vhadd_u8(row_11, vpkhbt_ror8(row_11, row_02));
    // t_g_pixels = [(G3+G2+G1+G5)/4, (R3+R0+B0+B2)/4, (G2+G2+G2+G2)/4, (R0+R0+R0+R0)/2]
    // t_g_pixels = [G@R3, (R3+R0+B0+B2)/4, G@G2, R@R0]
    pixels.g = vuxtb16_ror8(t_g_pixels);
    // pixels.g = [0, G@R3, 0, G@G2]
    v128_t t_b_pixels = vhadd_u8(row_02, vpkhbt(row_02, row_02));
    // t_b_pixels = [(B1+B3+B0+B2)/4, (G1+G5+G0+G4)/4, (B0+B2+B0+B2)/4, (G0+G4+G0+G4)/2]
    // t_b_pixels = [B@R3, (G1+G5+G0+G4)/4, B@G2, (G0+G4)/2]
    pixels.b = vuxtb16_ror8(t_b_pixels);
    // pixels.b = [0, B@R3, 0, B@G2]
    return pixels;
}

// Row vectors are loaded into memory and processed in little-endian order.
// row_0 stores MSB [G1, B1, G0, B0] LSB pixels where each pixel is 8-bits.
// row_1 stores MSB [R1, G3, R0, G2] LSB pixels where each pixel is 8-bits.
// row_2 stores MSB [G5, B3, G4, B2] LSB pixels where each pixel is 8-bits.
// In the case of vectors larger than 32-bits the pattern is repeated for every 32-bits.
//
// vdebayer_rggb produces two output pixels per 32-bits:
// pixels.r = MSB [0, R@G3, 0, R@R0] LSB pixels where each pixel is 8-bits.
// pixels.g = MSB [0, G@G3, 0, G@R0] LSB pixels where each pixel is 8-bits.
// pixels.b = MSB [0, B@G3, 0, B@R0] LSB pixels where each pixel is 8-bits.
static inline vrgb_pixels_t vdebayer_rggb(v128_t row_0, v128_t row_1, v128_t row_2) {
    vrgb_pixels_t pixels;
    v128_t row_02 = vhadd_u8(row_0, row_2);
    // row_02 = [(G1+G5)/2, (B1+B3)/2, (G0+G4)/2, (B0+B2)/2]
    v128_t row_11 = vhadd_u8(row_1, vpkhtb(row_1, row_1));
    // row_11 = [(R1+R1)/2, (G3+G3)/2, (R0+R1)/2, (G2+G3)/2]
    // row_11 = [R@R1, G@G3, R@G3, (G2+G3)/2]
    v128_t t_r_pixels = vhadd_u8(row_1, vpkhbt(row_1, row_1));
    // t_r_pixels = [(R1+R0)/2, (G3+G2)/2, (R0+R0)/2, (G2+G2)/2]
    // t_r_pixels = [R@G3, (G3+G2)/2, R@R0, G@G2]
    pixels.r = vuxtb16_ror8(t_r_pixels);
    // pixels.r = [0, R@G3, 0, R@R0]
    v128_t t_g_pixels = vhadd_u8(row_11, vpkhtb_ror8(row_11, row_02));
    // t_g_pixels = [(R1+R1+R1+R1)/4, (G3+G3+G3+G3)/4, (R0+R1+B1+B3)/4, (G2+G3+G0+G4)/4]
    // t_g_pixels = [R@R1, G@G3, (R0+R1+B1+B3)/4, G@R0]
    pixels.g = vuxtb16(t_g_pixels);
    // pixels.g = [0, G@G3, 0, G@R0]
    v128_t t_b_pixels = vhadd_u8(row_02, vpkhtb(row_02, row_02));
    // t_b_pixels = [(G1+G5+G1+G5)/4, (B1+B3+B1+B3)/4, (G0+G4+G1+G5)/4, (B0+B2+B1+B3)/4]
    // t_b_pixels = [(G1+G5)/2, B@G3, (G0+G4+G1+G5)/4, B@R0]
    pixels.b = vuxtb16(t_b_pixels);
    // pixels.b = [0, B@G3, 0, B@R0]
    return pixels;
}

#if !defined(IMLIB_ENABLE_DEBAYER_OPTIMIZATION)
static inline vrgb_pixels_t vdebayer_all_0(image_t *src, v128_t row_0, v128_t row_1, v128_t row_2) {
    switch (src->pixfmt) {
        case PIXFORMAT_BAYER_BGGR: {
            return vdebayer_bggr(row_0, row_1, row_2);
        }
        case PIXFORMAT_BAYER_GBRG: {
            return vdebayer_gbrg(row_0, row_1, row_2);
        }
        case PIXFORMAT_BAYER_GRBG: {
            return vdebayer_grbg(row_0, row_1, row_2);
        }
        case PIXFORMAT_BAYER_RGGB: {
            return vdebayer_rggb(row_0, row_1, row_2);
        }
        default: {
            __builtin_unreachable();
        }
    }
}

static inline vrgb_pixels_t vdebayer_all_1(image_t *src, v128_t row_0, v128_t row_1, v128_t row_2) {
    switch (src->pixfmt) {
        case PIXFORMAT_BAYER_BGGR: {
            // PIXFORMAT_BAYER_BGGR shifted down by 1 becomes PIXFORMAT_BAYER_GRBG
            return vdebayer_grbg(row_0, row_1, row_2);
        }
        case PIXFORMAT_BAYER_GBRG: {
            // PIXFORMAT_BAYER_GBRG shifted down by 1 becomes PIXFORMAT_BAYER_RGGB
            return vdebayer_rggb(row_0, row_1, row_2);
        }
        case PIXFORMAT_BAYER_GRBG: {
            // PIXFORMAT_BAYER_GRBG shifted down by 1 becomes PIXFORMAT_BAYER_BGGR
            return vdebayer_bggr(row_0, row_1, row_2);
        }
        case PIXFORMAT_BAYER_RGGB: {
            // PIXFORMAT_BAYER_RGGB shifted down by 1 becomes PIXFORMAT_BAYER_GBRG
            return vdebayer_gbrg(row_0, row_1, row_2);
        }
        default: {
            __builtin_unreachable();
        }
    }
}
#endif // !IMLIB_ENABLE_DEBAYER_OPTIMIZATION

// Note that the loaded pointers are shifted to up by 1 to account for the offset
// created by debayering the image.
static inline v4x_row_ptrs_t vdebayer_rowptrs_init(const image_t *src, int32_t y) {
    v4x_row_ptrs_t rowptrs;

    // keep row pointers in bounds
    if (y == 0) {
        rowptrs.p1.u8 = src->data;
        rowptrs.p2.u8 = rowptrs.p1.u8 + ((src->h >= 2) ? src->w : 0);
        rowptrs.p3.u8 = rowptrs.p1.u8 + ((src->h >= 3) ? (src->w * 2) : 0);
        rowptrs.p0.u8 = rowptrs.p2.u8;
    } else if (y == (src->h - 2)) {
        rowptrs.p0.u8 = src->data + ((y - 1) * src->w);
        rowptrs.p1.u8 = rowptrs.p0.u8 + src->w;
        rowptrs.p2.u8 = rowptrs.p1.u8 + src->w;
        rowptrs.p3.u8 = rowptrs.p1.u8;
    } else if (y == (src->h - 1)) {
        rowptrs.p0.u8 = src->data + ((y - 1) * src->w);
        rowptrs.p1.u8 = rowptrs.p0.u8 + src->w;
        rowptrs.p2.u8 = rowptrs.p0.u8;
        rowptrs.p3.u8 = rowptrs.p1.u8;
    } else {
        // get 4 neighboring rows
        rowptrs.p0.u8 = src->data + ((y - 1) * src->w);
        rowptrs.p1.u8 = rowptrs.p0.u8 + src->w;
        rowptrs.p2.u8 = rowptrs.p1.u8 + src->w;
        rowptrs.p3.u8 = rowptrs.p2.u8 + src->w;
    }

    // Shift loaded pointers up by 1 to account for the offset created by debayering the image.
    rowptrs.p0.u8 -= 1;
    rowptrs.p1.u8 -= 1;
    rowptrs.p2.u8 -= 1;
    rowptrs.p3.u8 -= 1;
    return rowptrs;
}

static inline v2x_row_ptrs_t vdebayer_quarter_rowptrs_init(const image_t *src, int32_t y) {
    v2x_row_ptrs_t rowptrs;

    // keep row pointers in bounds
    if (y == 0) {
        rowptrs.p0.u8 = src->data;
        rowptrs.p1.u8 = rowptrs.p0.u8 + ((src->h >= 2) ? src->w : 0);
    } else if (y == (src->h - 1)) {
        rowptrs.p0.u8 = src->data + (y * src->w);
        rowptrs.p1.u8 = rowptrs.p0.u8 - src->w;
    } else {
        // get 2 neighboring rows
        rowptrs.p0.u8 = src->data + (y * src->w);
        rowptrs.p1.u8 = rowptrs.p0.u8 + src->w;
    }

    return rowptrs;
}

static inline v128_predicate_t vdebayer_load_pred(const image_t *src, int32_t x) {
    // Load 1x to 4x 32-bit rows overlapping by 2 pixels.
    return vpredicate_8(src->w - x + (VBAYER_X_STRIDE - 2));
}

static inline v128_predicate_t vdebayer_store_pred(int32_t width, int32_t x) {
    // For 2x to 8x 16-bit lanes.
    return vpredicate_16(width - x);
}

// Loads pixels from the image into the 4 row vectors and handles the boundary conditions.
// Note that the loaded pixels are shifted to the right by 1 to account for the offset
// created by debayering the image.
static v4x_rows_t vdebayer_load_rows_inner(v4x_row_ptrs_t rowptrs, uint32_t x, v128_t offsets, v128_predicate_t pred) {
    bool x_is_0 = (x == 0);

    // Start loading 1 pixel behind the x position and load 1 extra pixel.
    if (!x_is_0) {
        pred = vpredicate_8_add(pred, 1);
    } else {
        // Pointers are shifted back by 1 already so this undoes that shift.
        x += 1;
    }

    v4x_rows_t rows = vldr_u32_gather_pred_x4_unaligned(rowptrs, x, offsets, pred);

    // Vector lane access must use constant offsets so we handle the boundary conditions
    // using a switch statement per boundary condition. In this case the get/set functions
    // compile down into 1 instruction each.
    switch (vpredicate_8_get_n(pred)) {
        case 1:
            // MSB [0, 0, 0, G0] LSB -> MSB [G0, G0, G0, G0] LSB
            rows.r0 = vset_u32(rows.r0, 0, vget_u8(rows.r0, 0) * 0x01010101);
            rows.r1 = vset_u32(rows.r1, 0, vget_u8(rows.r1, 0) * 0x01010101);
            rows.r2 = vset_u32(rows.r2, 0, vget_u8(rows.r2, 0) * 0x01010101);
            rows.r3 = vset_u32(rows.r3, 0, vget_u8(rows.r3, 0) * 0x01010101);
            break;
        case 2:
            // MSB [0, 0, R0, G0] LSB -> MSB [R0, G0, R0, G0] LSB
            rows.r0 = vset_u16(rows.r0, 1, vget_u16(rows.r0, 0));
            rows.r1 = vset_u16(rows.r1, 1, vget_u16(rows.r1, 0));
            rows.r2 = vset_u16(rows.r2, 1, vget_u16(rows.r2, 0));
            rows.r3 = vset_u16(rows.r3, 1, vget_u16(rows.r3, 0));
            break;
        case 3:
            // MSB [0, G1, R0, G0] LSB -> MSB [R0, G1, R0, G0] LSB
            rows.r0 = vset_u8(rows.r0, 3, vget_u8(rows.r0, 2));
            rows.r1 = vset_u8(rows.r1, 3, vget_u8(rows.r1, 2));
            rows.r2 = vset_u8(rows.r2, 3, vget_u8(rows.r2, 2));
            rows.r3 = vset_u8(rows.r3, 3, vget_u8(rows.r3, 2));
            break;
        #if UINT8_VECTOR_SIZE >= 8
        case 5:
            // MSB [0, 0, 0, G0] LSB -> MSB [G0, G0, G0, G0] LSB
            rows.r0 = vset_u32(rows.r0, 1, vget_u8(rows.r0, 4) * 0x01010101);
            rows.r1 = vset_u32(rows.r1, 1, vget_u8(rows.r1, 4) * 0x01010101);
            rows.r2 = vset_u32(rows.r2, 1, vget_u8(rows.r2, 4) * 0x01010101);
            rows.r3 = vset_u32(rows.r3, 1, vget_u8(rows.r3, 4) * 0x01010101);
            break;
        case 6:
            // MSB [0, 0, R0, G0] LSB -> MSB [R0, G0, R0, G0] LSB
            rows.r0 = vset_u16(rows.r0, 3, vget_u16(rows.r0, 2));
            rows.r1 = vset_u16(rows.r1, 3, vget_u16(rows.r1, 2));
            rows.r2 = vset_u16(rows.r2, 3, vget_u16(rows.r2, 2));
            rows.r3 = vset_u16(rows.r3, 3, vget_u16(rows.r3, 2));
            break;
        case 7:
            // MSB [0, G1, R0, G0] LSB -> MSB [R0, G1, R0, G0] LSB
            rows.r0 = vset_u8(rows.r0, 7, vget_u8(rows.r0, 5));
            rows.r1 = vset_u8(rows.r1, 7, vget_u8(rows.r1, 5));
            rows.r2 = vset_u8(rows.r2, 7, vget_u8(rows.r2, 5));
            rows.r3 = vset_u8(rows.r3, 7, vget_u8(rows.r3, 5));
            break;
        #endif
        #if UINT8_VECTOR_SIZE >= 16
        case 9:
            // MSB [0, 0, 0, G0] LSB -> MSB [G0, G0, G0, G0] LSB
            rows.r0 = vset_u32(rows.r0, 2, vget_u8(rows.r0, 8) * 0x01010101);
            rows.r1 = vset_u32(rows.r1, 2, vget_u8(rows.r1, 8) * 0x01010101);
            rows.r2 = vset_u32(rows.r2, 2, vget_u8(rows.r2, 8) * 0x01010101);
            rows.r3 = vset_u32(rows.r3, 2, vget_u8(rows.r3, 8) * 0x01010101);
            break;
        case 10:
            // MSB [0, 0, R0, G0] LSB -> MSB [R0, G0, R0, G0] LSB
            rows.r0 = vset_u16(rows.r0, 5, vget_u16(rows.r0, 4));
            rows.r1 = vset_u16(rows.r1, 5, vget_u16(rows.r1, 4));
            rows.r2 = vset_u16(rows.r2, 5, vget_u16(rows.r2, 4));
            rows.r3 = vset_u16(rows.r3, 5, vget_u16(rows.r3, 4));
            break;
        case 11:
            // MSB [0, G1, R0, G0] LSB -> MSB [R0, G1, R0, G0] LSB
            rows.r0 = vset_u8(rows.r0, 11, vget_u8(rows.r0, 9));
            rows.r1 = vset_u8(rows.r1, 11, vget_u8(rows.r1, 9));
            rows.r2 = vset_u8(rows.r2, 11, vget_u8(rows.r2, 9));
            rows.r3 = vset_u8(rows.r3, 11, vget_u8(rows.r3, 9));
            break;
        case 13:
            // MSB [0, 0, 0, G0] LSB -> MSB [G0, G0, G0, G0] LSB
            rows.r0 = vset_u32(rows.r0, 3, vget_u8(rows.r0, 12) * 0x01010101);
            rows.r1 = vset_u32(rows.r1, 3, vget_u8(rows.r1, 12) * 0x01010101);
            rows.r2 = vset_u32(rows.r2, 3, vget_u8(rows.r2, 12) * 0x01010101);
            rows.r3 = vset_u32(rows.r3, 3, vget_u8(rows.r3, 12) * 0x01010101);
            break;
        case 14:
            // MSB [0, 0, R0, G0] LSB -> MSB [R0, G0, R0, G0] LSB
            rows.r0 = vset_u16(rows.r0, 7, vget_u16(rows.r0, 6));
            rows.r1 = vset_u16(rows.r1, 7, vget_u16(rows.r1, 6));
            rows.r2 = vset_u16(rows.r2, 7, vget_u16(rows.r2, 6));
            rows.r3 = vset_u16(rows.r3, 7, vget_u16(rows.r3, 6));
            break;
        case 15:
            // MSB [0, G1, R0, G0] LSB -> MSB [R0, G1, R0, G0] LSB
            rows.r0 = vset_u8(rows.r0, 15, vget_u8(rows.r0, 13));
            rows.r1 = vset_u8(rows.r1, 15, vget_u8(rows.r1, 13));
            rows.r2 = vset_u8(rows.r2, 15, vget_u8(rows.r2, 13));
            rows.r3 = vset_u8(rows.r3, 15, vget_u8(rows.r3, 13));
            break;
        #endif
        #if UINT8_VECTOR_SIZE >= 32
        #error "Unsupported vector size"
        #endif
        default:
            break;
    }

    if (x_is_0) {
        // Shift loaded pixels left by 1 as if we started loading at x - 1.
        // MSB [R1, G1, R0, G0] LSB -> MSB [G1, R0, G0, R0] LSB
        uint32_t r0 = vget_u8(rows.r0, 1);
        uint32_t r1 = vget_u8(rows.r1, 1);
        uint32_t r2 = vget_u8(rows.r2, 1);
        uint32_t r3 = vget_u8(rows.r3, 1);
        rows.r0 = vshlc(rows.r0, &r0, 8);
        rows.r1 = vshlc(rows.r1, &r1, 8);
        rows.r2 = vshlc(rows.r2, &r2, 8);
        rows.r3 = vshlc(rows.r3, &r3, 8);
    }

    return rows;
}

static inline v4x_rows_t vdebayer_load_rows(const image_t *src, v4x_row_ptrs_t rowptrs, uint32_t x, v128_t offsets) {
    v128_predicate_t pred = vdebayer_load_pred(src, x);

    // For the vast majority of cases we load vector size pixels at a time and exit quickly.
    if ((x != 0) && vpredicate_8_all_lanes_active(pred)) {
        // Start loading 1 pixel behind the x position and load 1 extra pixel.
        v4x_rows_t rows;
        rows.r0 = vldr_u32_gather_unaligned(rowptrs.p0.u8 + x, offsets);
        rows.r1 = vldr_u32_gather_unaligned(rowptrs.p1.u8 + x, offsets);
        rows.r2 = vldr_u32_gather_unaligned(rowptrs.p2.u8 + x, offsets);
        rows.r3 = vldr_u32_gather_unaligned(rowptrs.p3.u8 + x, offsets);
        return rows;
    } else {
        return vdebayer_load_rows_inner(rowptrs, x, offsets, pred);
    }
}

// In the case of vectors larger than 32-bits the pattern is repeated for every 32-bits.
//
// pixels.r = MSB [0, R1, 0, R0] LSB pixels where each pixel is 8-bits.
// pixels.g = MSB [0, G1, 0, G0] LSB pixels where each pixel is 8-bits.
// pixels.b = MSB [0, B1, 0, B0] LSB pixels where each pixel is 8-bits.
//
// Y == ((R * 38) + (G * 75) + (B * 15)) / 128
//
// Returns 2x int8_t Y (MSB [garbage, Y1, garbage, Y0] LSB) pixels for every 32-bits.
static inline v128_t vdebayer_to_y(vrgb_pixels_t pixels) {
    pixels.r = vrgb_pixels888_to_grayscale(pixels);
    #if (OMV_JPEG_CODEC_ENABLE == 0)
    pixels.r = veor_u32(pixels.r, vdup_u32(0x800080));
    #endif
    return pixels.r;
}

// In the case of vectors larger than 32-bits the pattern is repeated for every 32-bits.
//
// pixels.r = MSB [0, R1, 0, R0] LSB pixels where each pixel is 8-bits.
// pixels.g = MSB [0, G1, 0, G0] LSB pixels where each pixel is 8-bits.
// pixels.b = MSB [0, B1, 0, B0] LSB pixels where each pixel is 8-bits.
//
// CB == ((B * 64) - ((R * 21) + (G * 43))) / 128
//
// Returns 2x int8_t CB (MSB [garbage, CB1, garbage, CB0] LSB) pixels for every 32-bits.
static inline v128_t vdebayer_to_cb(vrgb_pixels_t pixels) {
    #if (__ARM_ARCH >= 8)
    pixels.r = vmul_n_s16(pixels.r, -21);
    pixels.r = vmla_n_s16(pixels.g, -43, pixels.r);
    pixels.r = vmla_n_s16(pixels.b, 64, pixels.r);
    #else
    pixels.r = vmul_n_u32(pixels.r, 21);
    pixels.r = vmla_n_u32(pixels.g, 43, pixels.r);
    pixels.b = vmul_n_u32(pixels.b, 64);
    pixels.r = vsub_s16(pixels.b, pixels.r);
    #endif
    pixels.r = vlsr_u32(pixels.r, 7);
    #if (OMV_JPEG_CODEC_ENABLE == 1)
    pixels.r = veor_u32(pixels.r, vdup_u32(0x800080));
    #endif
    return pixels.r;
}

// In the case of vectors larger than 32-bits the pattern is repeated for every 32-bits.
//
// pixels.r = MSB [0, R1, 0, R0] LSB pixels where each pixel is 8-bits.
// pixels.g = MSB [0, G1, 0, G0] LSB pixels where each pixel is 8-bits.
// pixels.b = MSB [0, B1, 0, B0] LSB pixels where each pixel is 8-bits.
//
// CR == ((R * 64) - ((G * 54) + (B * 10))) / 128
//
// Returns 2x int8_t CR (MSB [garbage, CR1, garbage, CR0] LSB) pixels for every 32-bits.
static inline v128_t vdebayer_to_cr(vrgb_pixels_t pixels) {
    #if (__ARM_ARCH >= 8)
    pixels.r = vmul_n_s16(pixels.r, 64);
    pixels.r = vmla_n_s16(pixels.g, -54, pixels.r);
    pixels.r = vmla_n_s16(pixels.b, -10, pixels.r);
    #else
    pixels.r = vmul_n_u32(pixels.r, 64);
    pixels.g = vmul_n_u32(pixels.g, 54);
    pixels.g = vmla_n_u32(pixels.b, 10, pixels.g);
    pixels.r = vsub_s16(pixels.r, pixels.g);
    #endif
    pixels.r = vlsr_u32(pixels.r, 7);
    #if (OMV_JPEG_CODEC_ENABLE == 1)
    pixels.r = veor_u32(pixels.r, vdup_u32(0x800080));
    #endif
    return pixels.r;
}

// In the case of vectors larger than 32-bits the pattern is repeated for every 32-bits.
//
// pixels.r = MSB [0, R1, 0, R0] LSB pixels where each pixel is 8-bits.
// pixels.g = MSB [0, G1, 0, G0] LSB pixels where each pixel is 8-bits.
// pixels.b = MSB [0, B1, 0, B0] LSB pixels where each pixel is 8-bits.
//
// Returns the same.
static inline vrgb_pixels_t vdebayer_apply_rb_gain(vrgb_pixels_t pixels, uint32_t red_gain, uint32_t blue_gain) {
    pixels.r = vusat_s16_narrow_u8_lo(pixels.r, vmul_n_u32(pixels.r, red_gain), 5);
    pixels.b = vusat_s16_narrow_u8_lo(pixels.b, vmul_n_u32(pixels.b, blue_gain), 5);
    return pixels;
}

// In the case of vectors larger than 32-bits the pattern is repeated for every 32-bits.
//
// pixels.r = MSB [R3, R2, R1, R0] LSB pixels where each pixel is 8-bits.
// pixels.g = MSB [G3, G2, G1, G0] LSB pixels where each pixel is 8-bits.
// pixels.b = MSB [B3, B2, B2, B0] LSB pixels where each pixel is 8-bits.
//
// Stores 4x Grayscale pixels for every 32-bits.
static inline void vdebayer_store_packed_grayscale(uint8_t *p, vrgb_pixels_t packed_pixels,
                                                   uint32_t red_gain, uint32_t blue_gain,
                                                   int32_t len) {
    vrgb_pixels_t pixels0 = {
        .r = vuxtb16(packed_pixels.r),
        .g = vuxtb16(packed_pixels.g),
        .b = vuxtb16(packed_pixels.b),
    };

    v128_t v0 = vrgb_pixels888_to_grayscale(vdebayer_apply_rb_gain(pixels0, red_gain, blue_gain));

    vrgb_pixels_t pixels1 = {
        .r = vuxtb16_ror8(packed_pixels.r),
        .g = vuxtb16_ror8(packed_pixels.g),
        .b = vuxtb16_ror8(packed_pixels.b),
    };

    v128_t v1 = vrgb_pixels888_to_grayscale(vdebayer_apply_rb_gain(pixels1, red_gain, blue_gain));

    vstr_u8_pred(p, vmov_u16_narrow_u8_combine(v0, v1, true), vpredicate_8(len));
}

// In the case of vectors larger than 32-bits the pattern is repeated for every 32-bits.
//
// pixels.r = MSB [R3, R2, R1, R0] LSB pixels where each pixel is 8-bits.
// pixels.g = MSB [G3, G2, G1, G0] LSB pixels where each pixel is 8-bits.
// pixels.b = MSB [B3, B2, B2, B0] LSB pixels where each pixel is 8-bits.
//
// Stores 4x RGB565 pixels for every 32-bits.
static inline void vdebayer_store_packed_rgb565(uint16_t *p, vrgb_pixels_t packed_pixels,
                                                uint32_t red_gain, uint32_t blue_gain,
                                                int32_t len) {
    v2x_rows_t out;

    vrgb_pixels_t pixels0 = {
        .r = vuxtb16(packed_pixels.r),
        .g = vuxtb16(packed_pixels.g),
        .b = vuxtb16(packed_pixels.b),
    };

    out.r0 = vrgb_pixels888_to_rgb565(vdebayer_apply_rb_gain(pixels0, red_gain, blue_gain));

    vrgb_pixels_t pixels1 = {
        .r = vuxtb16_ror8(packed_pixels.r),
        .g = vuxtb16_ror8(packed_pixels.g),
        .b = vuxtb16_ror8(packed_pixels.b),
    };

    out.r1 = vrgb_pixels888_to_rgb565(vdebayer_apply_rb_gain(pixels1, red_gain, blue_gain));

    if (len >= (UINT16_VECTOR_SIZE * 2)) {
        vst2_u16(p, out);
    } else {
        vst2_u16_len(p, out, len);
    }
}

static inline void vdebayer_grayscale_buf_copy(int32_t y, image_t *buf, image_t *dst) {
    if (y >= VBAYER_BUF_KSIZE) {
        // Transfer buffer lines...
        int32_t y_offset = y - VBAYER_BUF_KSIZE;
        vmemcpy_8(IMAGE_COMPUTE_GRAYSCALE_PIXEL_ROW_PTR(dst, y_offset),
                  IMAGE_COMPUTE_GRAYSCALE_PIXEL_ROW_PTR(buf, (y_offset % VBAYER_BUF_BROWS)),
                  IMAGE_GRAYSCALE_LINE_LEN_BYTES(dst) * VBAYER_Y_STRIDE);
    }
}

static inline void vdebayer_rgb565_buf_copy(int32_t y, image_t *buf, image_t *dst) {
    if (y >= VBAYER_BUF_KSIZE) {
        // Transfer buffer lines...
        int32_t y_offset = y - VBAYER_BUF_KSIZE;
        vmemcpy_16(IMAGE_COMPUTE_RGB565_PIXEL_ROW_PTR(dst, y_offset),
                   IMAGE_COMPUTE_RGB565_PIXEL_ROW_PTR(buf, (y_offset % VBAYER_BUF_BROWS)),
                   IMAGE_RGB565_LINE_LEN_BYTES(dst) * VBAYER_Y_STRIDE);
    }
}

#if defined(IMLIB_ENABLE_DEBAYER_OPTIMIZATION)
static void vdebayer_bggr_to_binary(image_t *src, rectangle_t *roi, int32_t x_offset, image_t *dst) {
    // Load pixels, but, each set of 4 pixels overlaps the previous by 2 pixels.
    v128_t offsets = vidup_u32_unaligned(0, 2);

    for (int32_t y = 0, y_end = roi->h - 1; y < roi->h; y += VBAYER_Y_STRIDE) {
        v4x_row_ptrs_t rowptrs = vdebayer_rowptrs_init(src, y + roi->y);
        uint32_t *p0 = IMAGE_COMPUTE_BINARY_PIXEL_ROW_PTR(dst, y);
        uint32_t *p1 = IMAGE_COMPUTE_BINARY_PIXEL_ROW_PTR(dst, y + 1);

        for (int32_t x = 0; x < roi->w; x += VBAYER_X_STRIDE) {
            v4x_rows_t rows = vdebayer_load_rows(src, rowptrs, x + roi->x, offsets);
            v128_predicate_t pred = vdebayer_store_pred(roi->w, x);

            vrgb_pixels_store_binary(p0, x + x_offset, vdebayer_bggr(rows.r0, rows.r1, rows.r2), pred);

            if (y == y_end) {
                continue;
            }

            // PIXFORMAT_BAYER_BGGR shifted down by 1 becomes PIXFORMAT_BAYER_GRBG
            vrgb_pixels_store_binary(p1, x + x_offset, vdebayer_grbg(rows.r1, rows.r2, rows.r3), pred);
        }
    }
}

static void vdebayer_gbrg_to_binary(image_t *src, rectangle_t *roi, int32_t x_offset, image_t *dst) {
    // Load pixels, but, each set of 4 pixels overlaps the previous by 2 pixels.
    v128_t offsets = vidup_u32_unaligned(0, 2);

    for (int32_t y = 0, y_end = roi->h - 1; y < roi->h; y += VBAYER_Y_STRIDE) {
        v4x_row_ptrs_t rowptrs = vdebayer_rowptrs_init(src, y + roi->y);
        uint32_t *p0 = IMAGE_COMPUTE_BINARY_PIXEL_ROW_PTR(dst, y);
        uint32_t *p1 = IMAGE_COMPUTE_BINARY_PIXEL_ROW_PTR(dst, y + 1);

        for (int32_t x = 0; x < roi->w; x += VBAYER_X_STRIDE) {
            v4x_rows_t rows = vdebayer_load_rows(src, rowptrs, x + roi->x, offsets);
            v128_predicate_t pred = vdebayer_store_pred(roi->w, x);

            vrgb_pixels_store_binary(p0, x + x_offset, vdebayer_gbrg(rows.r0, rows.r1, rows.r2), pred);

            if (y == y_end) {
                continue;
            }

            // PIXFORMAT_BAYER_GBRG shifted down by 1 becomes PIXFORMAT_BAYER_RGGB
            vrgb_pixels_store_binary(p1, x + x_offset, vdebayer_rggb(rows.r1, rows.r2, rows.r3), pred);
        }
    }
}

static void vdebayer_grbg_to_binary(image_t *src, rectangle_t *roi, int32_t x_offset, image_t *dst) {
    // Load pixels, but, each set of 4 pixels overlaps the previous by 2 pixels.
    v128_t offsets = vidup_u32_unaligned(0, 2);

    for (int32_t y = 0, y_end = roi->h - 1; y < roi->h; y += VBAYER_Y_STRIDE) {
        v4x_row_ptrs_t rowptrs = vdebayer_rowptrs_init(src, y + roi->y);
        uint32_t *p0 = IMAGE_COMPUTE_BINARY_PIXEL_ROW_PTR(dst, y);
        uint32_t *p1 = IMAGE_COMPUTE_BINARY_PIXEL_ROW_PTR(dst, y + 1);

        for (int32_t x = 0; x < roi->w; x += VBAYER_X_STRIDE) {
            v4x_rows_t rows = vdebayer_load_rows(src, rowptrs, x + roi->x, offsets);
            v128_predicate_t pred = vdebayer_store_pred(roi->w, x);

            vrgb_pixels_store_binary(p0, x + x_offset, vdebayer_grbg(rows.r0, rows.r1, rows.r2), pred);

            if (y == y_end) {
                continue;
            }

            // PIXFORMAT_BAYER_GRBG shifted down by 1 becomes PIXFORMAT_BAYER_BGGR
            vrgb_pixels_store_binary(p1, x + x_offset, vdebayer_bggr(rows.r1, rows.r2, rows.r3), pred);
        }
    }
}

static void vdebayer_rggb_to_binary(image_t *src, rectangle_t *roi, int32_t x_offset, image_t *dst) {
    // Load pixels, but, each set of 4 pixels overlaps the previous by 2 pixels.
    v128_t offsets = vidup_u32_unaligned(0, 2);

    for (int32_t y = 0, y_end = roi->h - 1; y < roi->h; y += VBAYER_Y_STRIDE) {
        v4x_row_ptrs_t rowptrs = vdebayer_rowptrs_init(src, y + roi->y);
        uint32_t *p0 = IMAGE_COMPUTE_BINARY_PIXEL_ROW_PTR(dst, y);
        uint32_t *p1 = IMAGE_COMPUTE_BINARY_PIXEL_ROW_PTR(dst, y + 1);

        for (int32_t x = 0; x < roi->w; x += VBAYER_X_STRIDE) {
            v4x_rows_t rows = vdebayer_load_rows(src, rowptrs, x + roi->x, offsets);
            v128_predicate_t pred = vdebayer_store_pred(roi->w, x);

            vrgb_pixels_store_binary(p0, x + x_offset, vdebayer_rggb(rows.r0, rows.r1, rows.r2), pred);

            if (y == y_end) {
                continue;
            }

            // PIXFORMAT_BAYER_RGGB shifted down by 1 becomes PIXFORMAT_BAYER_GBRG
            vrgb_pixels_store_binary(p1, x + x_offset, vdebayer_gbrg(rows.r1, rows.r2, rows.r3), pred);
        }
    }
}

static void vdebayer_bggr_to_grayscale(image_t *src, rectangle_t *roi, int32_t x_offset, image_t *dst) {
    // Load pixels, but, each set of 4 pixels overlaps the previous by 2 pixels.
    v128_t offsets = vidup_u32_unaligned(0, 2);

    for (int32_t y = 0, y_end = roi->h - 1; y < roi->h; y += VBAYER_Y_STRIDE) {
        v4x_row_ptrs_t rowptrs = vdebayer_rowptrs_init(src, y + roi->y);
        uint8_t *p0 = IMAGE_COMPUTE_GRAYSCALE_PIXEL_ROW_PTR(dst, y) + x_offset;
        uint8_t *p1 = p0 + dst->w;

        for (int32_t x = 0; x < roi->w; x += VBAYER_X_STRIDE) {
            v4x_rows_t rows = vdebayer_load_rows(src, rowptrs, x + roi->x, offsets);
            v128_predicate_t pred = vdebayer_store_pred(roi->w, x);

            vrgb_pixels_store_grayscale(p0, x, vdebayer_bggr(rows.r0, rows.r1, rows.r2), pred);

            if (y == y_end) {
                continue;
            }

            // PIXFORMAT_BAYER_BGGR shifted down by 1 becomes PIXFORMAT_BAYER_GRBG
            vrgb_pixels_store_grayscale(p1, x, vdebayer_grbg(rows.r1, rows.r2, rows.r3), pred);
        }
    }
}

static void vdebayer_gbrg_to_grayscale(image_t *src, rectangle_t *roi, int32_t x_offset, image_t *dst) {
    // Load pixels, but, each set of 4 pixels overlaps the previous by 2 pixels.
    v128_t offsets = vidup_u32_unaligned(0, 2);

    for (int32_t y = 0, y_end = roi->h - 1; y < roi->h; y += VBAYER_Y_STRIDE) {
        v4x_row_ptrs_t rowptrs = vdebayer_rowptrs_init(src, y + roi->y);
        uint8_t *p0 = IMAGE_COMPUTE_GRAYSCALE_PIXEL_ROW_PTR(dst, y) + x_offset;
        uint8_t *p1 = p0 + dst->w;

        for (int32_t x = 0; x < roi->w; x += VBAYER_X_STRIDE) {
            v4x_rows_t rows = vdebayer_load_rows(src, rowptrs, x + roi->x, offsets);
            v128_predicate_t pred = vdebayer_store_pred(roi->w, x);

            vrgb_pixels_store_grayscale(p0, x, vdebayer_gbrg(rows.r0, rows.r1, rows.r2), pred);

            if (y == y_end) {
                continue;
            }

            // PIXFORMAT_BAYER_GBRG shifted down by 1 becomes PIXFORMAT_BAYER_RGGB
            vrgb_pixels_store_grayscale(p1, x, vdebayer_rggb(rows.r1, rows.r2, rows.r3), pred);
        }
    }
}

static void vdebayer_grbg_to_grayscale(image_t *src, rectangle_t *roi, int32_t x_offset, image_t *dst) {
    // Load pixels, but, each set of 4 pixels overlaps the previous by 2 pixels.
    v128_t offsets = vidup_u32_unaligned(0, 2);

    for (int32_t y = 0, y_end = roi->h - 1; y < roi->h; y += VBAYER_Y_STRIDE) {
        v4x_row_ptrs_t rowptrs = vdebayer_rowptrs_init(src, y + roi->y);
        uint8_t *p0 = IMAGE_COMPUTE_GRAYSCALE_PIXEL_ROW_PTR(dst, y) + x_offset;
        uint8_t *p1 = p0 + dst->w;

        for (int32_t x = 0; x < roi->w; x += VBAYER_X_STRIDE) {
            v4x_rows_t rows = vdebayer_load_rows(src, rowptrs, x + roi->x, offsets);
            v128_predicate_t pred = vdebayer_store_pred(roi->w, x);

            vrgb_pixels_store_grayscale(p0, x, vdebayer_grbg(rows.r0, rows.r1, rows.r2), pred);

            if (y == y_end) {
                continue;
            }

            // PIXFORMAT_BAYER_GRBG shifted down by 1 becomes PIXFORMAT_BAYER_BGGR
            vrgb_pixels_store_grayscale(p1, x, vdebayer_bggr(rows.r1, rows.r2, rows.r3), pred);
        }
    }
}

static void vdebayer_rggb_to_grayscale(image_t *src, rectangle_t *roi, int32_t x_offset, image_t *dst) {
    // Load pixels, but, each set of 4 pixels overlaps the previous by 2 pixels.
    v128_t offsets = vidup_u32_unaligned(0, 2);

    for (int32_t y = 0, y_end = roi->h - 1; y < roi->h; y += VBAYER_Y_STRIDE) {
        v4x_row_ptrs_t rowptrs = vdebayer_rowptrs_init(src, y + roi->y);
        uint8_t *p0 = IMAGE_COMPUTE_GRAYSCALE_PIXEL_ROW_PTR(dst, y) + x_offset;
        uint8_t *p1 = p0 + dst->w;

        for (int32_t x = 0; x < roi->w; x += VBAYER_X_STRIDE) {
            v4x_rows_t rows = vdebayer_load_rows(src, rowptrs, x + roi->x, offsets);
            v128_predicate_t pred = vdebayer_store_pred(roi->w, x);

            vrgb_pixels_store_grayscale(p0, x, vdebayer_rggb(rows.r0, rows.r1, rows.r2), pred);

            if (y == y_end) {
                continue;
            }

            // PIXFORMAT_BAYER_RGGB shifted down by 1 becomes PIXFORMAT_BAYER_GBRG
            vrgb_pixels_store_grayscale(p1, x, vdebayer_gbrg(rows.r1, rows.r2, rows.r3), pred);
        }
    }
}

static void vdebayer_bggr_to_rgb565(image_t *src, rectangle_t *roi, int32_t x_offset, image_t *dst) {
    // Load pixels, but, each set of 4 pixels overlaps the previous by 2 pixels.
    v128_t offsets = vidup_u32_unaligned(0, 2);

    for (int32_t y = 0, y_end = roi->h - 1; y < roi->h; y += VBAYER_Y_STRIDE) {
        v4x_row_ptrs_t rowptrs = vdebayer_rowptrs_init(src, y + roi->y);
        uint16_t *p0 = IMAGE_COMPUTE_RGB565_PIXEL_ROW_PTR(dst, y) + x_offset;
        uint16_t *p1 = p0 + dst->w;

        for (int32_t x = 0; x < roi->w; x += VBAYER_X_STRIDE) {
            v4x_rows_t rows = vdebayer_load_rows(src, rowptrs, x + roi->x, offsets);
            v128_predicate_t pred = vdebayer_store_pred(roi->w, x);

            vrgb_pixels_store_rgb565(p0, x, vdebayer_bggr(rows.r0, rows.r1, rows.r2), pred);

            if (y == y_end) {
                continue;
            }

            // PIXFORMAT_BAYER_BGGR shifted down by 1 becomes PIXFORMAT_BAYER_GRBG
            vrgb_pixels_store_rgb565(p1, x, vdebayer_grbg(rows.r1, rows.r2, rows.r3), pred);
        }
    }
}

static void vdebayer_gbrg_to_rgb565(image_t *src, rectangle_t *roi, int32_t x_offset, image_t *dst) {
    // Load pixels, but, each set of 4 pixels overlaps the previous by 2 pixels.
    v128_t offsets = vidup_u32_unaligned(0, 2);

    for (int32_t y = 0, y_end = roi->h - 1; y < roi->h; y += VBAYER_Y_STRIDE) {
        v4x_row_ptrs_t rowptrs = vdebayer_rowptrs_init(src, y + roi->y);
        uint16_t *p0 = IMAGE_COMPUTE_RGB565_PIXEL_ROW_PTR(dst, y) + x_offset;
        uint16_t *p1 = p0 + dst->w;

        for (int32_t x = 0; x < roi->w; x += VBAYER_X_STRIDE) {
            v4x_rows_t rows = vdebayer_load_rows(src, rowptrs, x + roi->x, offsets);
            v128_predicate_t pred = vdebayer_store_pred(roi->w, x);

            vrgb_pixels_store_rgb565(p0, x, vdebayer_gbrg(rows.r0, rows.r1, rows.r2), pred);

            if (y == y_end) {
                continue;
            }

            // PIXFORMAT_BAYER_GBRG shifted down by 1 becomes PIXFORMAT_BAYER_RGGB
            vrgb_pixels_store_rgb565(p1, x, vdebayer_rggb(rows.r1, rows.r2, rows.r3), pred);
        }
    }
}

static void vdebayer_grbg_to_rgb565(image_t *src, rectangle_t *roi, int32_t x_offset, image_t *dst) {
    // Load pixels, but, each set of 4 pixels overlaps the previous by 2 pixels.
    v128_t offsets = vidup_u32_unaligned(0, 2);

    for (int32_t y = 0, y_end = roi->h - 1; y < roi->h; y += VBAYER_Y_STRIDE) {
        v4x_row_ptrs_t rowptrs = vdebayer_rowptrs_init(src, y + roi->y);
        uint16_t *p0 = IMAGE_COMPUTE_RGB565_PIXEL_ROW_PTR(dst, y) + x_offset;
        uint16_t *p1 = p0 + dst->w;

        for (int32_t x = 0; x < roi->w; x += VBAYER_X_STRIDE) {
            v4x_rows_t rows = vdebayer_load_rows(src, rowptrs, x + roi->x, offsets);
            v128_predicate_t pred = vdebayer_store_pred(roi->w, x);

            vrgb_pixels_store_rgb565(p0, x, vdebayer_grbg(rows.r0, rows.r1, rows.r2), pred);

            if (y == y_end) {
                continue;
            }

            // PIXFORMAT_BAYER_GRBG shifted down by 1 becomes PIXFORMAT_BAYER_BGGR
            vrgb_pixels_store_rgb565(p1, x, vdebayer_bggr(rows.r1, rows.r2, rows.r3), pred);
        }
    }
}

static void vdebayer_rggb_to_rgb565(image_t *src, rectangle_t *roi, int32_t x_offset, image_t *dst) {
    // Load pixels, but, each set of 4 pixels overlaps the previous by 2 pixels.
    v128_t offsets = vidup_u32_unaligned(0, 2);

    for (int32_t y = 0, y_end = roi->h - 1; y < roi->h; y += VBAYER_Y_STRIDE) {
        v4x_row_ptrs_t rowptrs = vdebayer_rowptrs_init(src, y + roi->y);
        uint16_t *p0 = IMAGE_COMPUTE_RGB565_PIXEL_ROW_PTR(dst, y) + x_offset;
        uint16_t *p1 = p0 + dst->w;

        for (int32_t x = 0; x < roi->w; x += VBAYER_X_STRIDE) {
            v4x_rows_t rows = vdebayer_load_rows(src, rowptrs, x + roi->x, offsets);
            v128_predicate_t pred = vdebayer_store_pred(roi->w, x);

            vrgb_pixels_store_rgb565(p0, x, vdebayer_rggb(rows.r0, rows.r1, rows.r2), pred);

            if (y == y_end) {
                continue;
            }

            // PIXFORMAT_BAYER_RGGB shifted down by 1 becomes PIXFORMAT_BAYER_GBRG
            vrgb_pixels_store_rgb565(p1, x, vdebayer_gbrg(rows.r1, rows.r2, rows.r3), pred);
        }
    }
}
#else
static void vdebayer_to_binary(image_t *src, rectangle_t *roi, int32_t x_offset, image_t *dst) {
    // Load pixels, but, each set of 4 pixels overlaps the previous by 2 pixels.
    v128_t offsets = vidup_u32_unaligned(0, 2);

    for (int32_t y = 0, y_end = roi->h - 1; y < roi->h; y += VBAYER_Y_STRIDE) {
        v4x_row_ptrs_t rowptrs = vdebayer_rowptrs_init(src, y + roi->y);
        uint32_t *p0 = IMAGE_COMPUTE_BINARY_PIXEL_ROW_PTR(dst, y);
        uint32_t *p1 = IMAGE_COMPUTE_BINARY_PIXEL_ROW_PTR(dst, y + 1);

        for (int32_t x = 0; x < roi->w; x += VBAYER_X_STRIDE) {
            v4x_rows_t rows = vdebayer_load_rows(src, rowptrs, x + roi->x, offsets);
            v128_predicate_t pred = vdebayer_store_pred(roi->w, x);

            vrgb_pixels_store_binary(p0, x + x_offset, vdebayer_all_0(src, rows.r0, rows.r1, rows.r2), pred);

            if (y == y_end) {
                continue;
            }

            vrgb_pixels_store_binary(p1, x + x_offset, vdebayer_all_1(src, rows.r1, rows.r2, rows.r3), pred);
        }
    }
}

static void vdebayer_to_grayscale(image_t *src, rectangle_t *roi, int32_t x_offset, image_t *dst) {
    // Load pixels, but, each set of 4 pixels overlaps the previous by 2 pixels.
    v128_t offsets = vidup_u32_unaligned(0, 2);

    for (int32_t y = 0, y_end = roi->h - 1; y < roi->h; y += VBAYER_Y_STRIDE) {
        v4x_row_ptrs_t rowptrs = vdebayer_rowptrs_init(src, y + roi->y);
        uint8_t *p0 = IMAGE_COMPUTE_GRAYSCALE_PIXEL_ROW_PTR(dst, y) + x_offset;
        uint8_t *p1 = p0 + dst->w;

        for (int32_t x = 0; x < roi->w; x += VBAYER_X_STRIDE) {
            v4x_rows_t rows = vdebayer_load_rows(src, rowptrs, x + roi->x, offsets);
            v128_predicate_t pred = vdebayer_store_pred(roi->w, x);

            vrgb_pixels_store_grayscale(p0, x, vdebayer_all_0(src, rows.r0, rows.r1, rows.r2), pred);

            if (y == y_end) {
                continue;
            }

            vrgb_pixels_store_grayscale(p1, x, vdebayer_all_1(src, rows.r1, rows.r2, rows.r3), pred);
        }
    }
}

static void vdebayer_to_rgb565(image_t *src, rectangle_t *roi, int32_t x_offset, image_t *dst) {
    // Load pixels, but, each set of 4 pixels overlaps the previous by 2 pixels.
    v128_t offsets = vidup_u32_unaligned(0, 2);

    for (int32_t y = 0, y_end = roi->h - 1; y < roi->h; y += VBAYER_Y_STRIDE) {
        v4x_row_ptrs_t rowptrs = vdebayer_rowptrs_init(src, y + roi->y);
        uint16_t *p0 = IMAGE_COMPUTE_RGB565_PIXEL_ROW_PTR(dst, y) + x_offset;
        uint16_t *p1 = p0 + dst->w;

        for (int32_t x = 0; x < roi->w; x += VBAYER_X_STRIDE) {
            v4x_rows_t rows = vdebayer_load_rows(src, rowptrs, x + roi->x, offsets);
            v128_predicate_t pred = vdebayer_store_pred(roi->w, x);

            vrgb_pixels_store_rgb565(p0, x, vdebayer_all_0(src, rows.r0, rows.r1, rows.r2), pred);

            if (y == y_end) {
                continue;
            }

            vrgb_pixels_store_rgb565(p1, x, vdebayer_all_1(src, rows.r1, rows.r2, rows.r3), pred);
        }
    }
}
#endif // IMLIB_ENABLE_DEBAYER_OPTIMIZATION

static void vdebayer(image_t *src, rectangle_t *roi, int32_t x_offset, image_t *dst) {
    #if defined(IMLIB_ENABLE_DEBAYER_OPTIMIZATION)
    switch (src->pixfmt) {
        case PIXFORMAT_BAYER_BGGR: {
            switch (dst->pixfmt) {
                case PIXFORMAT_BINARY: {
                    vdebayer_bggr_to_binary(src, roi, x_offset, dst);
                    break;
                }
                case PIXFORMAT_GRAYSCALE: {
                    vdebayer_bggr_to_grayscale(src, roi, x_offset, dst);
                    break;
                }
                case PIXFORMAT_RGB565: {
                    vdebayer_bggr_to_rgb565(src, roi, x_offset, dst);
                    break;
                }
                default: {
                    __builtin_unreachable();
                }
            }
            break;
        }
        case PIXFORMAT_BAYER_GBRG: {
            switch (dst->pixfmt) {
                case PIXFORMAT_BINARY: {
                    vdebayer_gbrg_to_binary(src, roi, x_offset, dst);
                    break;
                }
                case PIXFORMAT_GRAYSCALE: {
                    vdebayer_gbrg_to_grayscale(src, roi, x_offset, dst);
                    break;
                }
                case PIXFORMAT_RGB565: {
                    vdebayer_gbrg_to_rgb565(src, roi, x_offset, dst);
                    break;
                }
                default: {
                    __builtin_unreachable();
                }
            }
            break;
        }
        case PIXFORMAT_BAYER_GRBG: {
            switch (dst->pixfmt) {
                case PIXFORMAT_BINARY: {
                    vdebayer_grbg_to_binary(src, roi, x_offset, dst);
                    break;
                }
                case PIXFORMAT_GRAYSCALE: {
                    vdebayer_grbg_to_grayscale(src, roi, x_offset, dst);
                    break;
                }
                case PIXFORMAT_RGB565: {
                    vdebayer_grbg_to_rgb565(src, roi, x_offset, dst);
                    break;
                }
                default: {
                    __builtin_unreachable();
                }
            }
            break;
        }
        case PIXFORMAT_BAYER_RGGB: {
            switch (dst->pixfmt) {
                case PIXFORMAT_BINARY: {
                    vdebayer_rggb_to_binary(src, roi, x_offset, dst);
                    break;
                }
                case PIXFORMAT_GRAYSCALE: {
                    vdebayer_rggb_to_grayscale(src, roi, x_offset, dst);
                    break;
                }
                case PIXFORMAT_RGB565: {
                    vdebayer_rggb_to_rgb565(src, roi, x_offset, dst);
                    break;
                }
                default: {
                    __builtin_unreachable();
                }
            }
            break;
        }
        default: {
            __builtin_unreachable();
        }
    }
    #else
    switch (dst->pixfmt) {
        case PIXFORMAT_BINARY: {
            vdebayer_to_binary(src, roi, x_offset, dst);
            break;
        }
        case PIXFORMAT_GRAYSCALE: {
            vdebayer_to_grayscale(src, roi, x_offset, dst);
            break;
        }
        case PIXFORMAT_RGB565: {
            vdebayer_to_rgb565(src, roi, x_offset, dst);
            break;
        }
        default: {
            __builtin_unreachable();
        }
    }
    #endif // IMLIB_ENABLE_DEBAYER_OPTIMIZATION
}

// assumes dst->w == src->w
// assumes dst->h == 1
void imlib_debayer_line(int x_start, int x_end, int y_row, void *dst_row_ptr, pixformat_t pixfmt, image_t *src) {
    rectangle_t roi = {
        .x = x_start,
        .y = y_row,
        .w = x_end - x_start,
        .h = 1,
    };
    image_t dst = {
        .w = src->w,
        .h = 1,
        .pixfmt = pixfmt,
        .data = dst_row_ptr,
    };
    vdebayer(src, &roi, x_start, &dst);
}

// assumes dst->w == src->w
// assumes dst->h == src->h
// src and dst may not overlap, but, faster than imlib_debayer_image_awb
void imlib_debayer_image(image_t *dst, image_t *src) {
    OMV_PROFILE_START();
    rectangle_t roi = {
        .x = 0,
        .y = 0,
        .w = src->w,
        .h = src->h,
    };
    vdebayer(src, &roi, 0, dst);
    OMV_PROFILE_PRINT();
}

#if defined(IMLIB_ENABLE_DEBAYER_OPTIMIZATION)
static void vdebayer_bggr_to_ycbcr(image_t *src, rectangle_t *roi, int8_t *Y0, int8_t *CB, int8_t *CR) {
    // Load pixels, but, each set of 4 pixels overlaps the previous by 2 pixels.
    v128_t offsets = vidup_u32_unaligned(0, 2);

    for (int32_t y = 0, y_end = roi->h - 1; y < roi->h; y += VBAYER_Y_STRIDE) {
        v4x_row_ptrs_t rowptrs = vdebayer_rowptrs_init(src, y + roi->y);
        uint8_t *Y0p0 = ((uint8_t *) Y0) + (y * JPEG_MCU_W);
        uint8_t *CBp0 = ((uint8_t *) CB) + (y * JPEG_MCU_W);
        uint8_t *CRp0 = ((uint8_t *) CR) + (y * JPEG_MCU_W);
        uint8_t *Y0p1 = Y0p0 + JPEG_MCU_W;
        uint8_t *CBp1 = CBp0 + JPEG_MCU_W;
        uint8_t *CRp1 = CRp0 + JPEG_MCU_W;

        for (int32_t x = 0; x < roi->w; x += VBAYER_X_STRIDE) {
            v4x_rows_t rows = vdebayer_load_rows(src, rowptrs, x + roi->x, offsets);
            v128_predicate_t pred = vdebayer_store_pred(roi->w, x);

            vrgb_pixels_t pixels0 = vdebayer_bggr(rows.r0, rows.r1, rows.r2);
            vstr_u16_narrow_u8_pred(Y0p0 + x, vdebayer_to_y(pixels0), pred);
            vstr_u16_narrow_u8_pred(CBp0 + x, vdebayer_to_cb(pixels0), pred);
            vstr_u16_narrow_u8_pred(CRp0 + x, vdebayer_to_cr(pixels0), pred);

            if (y == y_end) {
                continue;
            }

            // PIXFORMAT_BAYER_BGGR shifted down by 1 becomes PIXFORMAT_BAYER_GRBG
            vrgb_pixels_t pixels1 = vdebayer_grbg(rows.r1, rows.r2, rows.r3);
            vstr_u16_narrow_u8_pred(Y0p1 + x, vdebayer_to_y(pixels1), pred);
            vstr_u16_narrow_u8_pred(CBp1 + x, vdebayer_to_cb(pixels1), pred);
            vstr_u16_narrow_u8_pred(CRp1 + x, vdebayer_to_cr(pixels1), pred);
        }
    }
}

static void vdebayer_gbrg_to_ycbcr(image_t *src, rectangle_t *roi, int8_t *Y0, int8_t *CB, int8_t *CR) {
    // Load pixels, but, each set of 4 pixels overlaps the previous by 2 pixels.
    v128_t offsets = vidup_u32_unaligned(0, 2);

    for (int32_t y = 0, y_end = roi->h - 1; y < roi->h; y += VBAYER_Y_STRIDE) {
        v4x_row_ptrs_t rowptrs = vdebayer_rowptrs_init(src, y + roi->y);
        uint8_t *Y0p0 = ((uint8_t *) Y0) + (y * JPEG_MCU_W);
        uint8_t *CBp0 = ((uint8_t *) CB) + (y * JPEG_MCU_W);
        uint8_t *CRp0 = ((uint8_t *) CR) + (y * JPEG_MCU_W);
        uint8_t *Y0p1 = Y0p0 + JPEG_MCU_W;
        uint8_t *CBp1 = CBp0 + JPEG_MCU_W;
        uint8_t *CRp1 = CRp0 + JPEG_MCU_W;

        for (int32_t x = 0; x < roi->w; x += VBAYER_X_STRIDE) {
            v4x_rows_t rows = vdebayer_load_rows(src, rowptrs, x + roi->x, offsets);
            v128_predicate_t pred = vdebayer_store_pred(roi->w, x);

            vrgb_pixels_t pixels0 = vdebayer_gbrg(rows.r0, rows.r1, rows.r2);
            vstr_u16_narrow_u8_pred(Y0p0 + x, vdebayer_to_y(pixels0), pred);
            vstr_u16_narrow_u8_pred(CBp0 + x, vdebayer_to_cb(pixels0), pred);
            vstr_u16_narrow_u8_pred(CRp0 + x, vdebayer_to_cr(pixels0), pred);

            if (y == y_end) {
                continue;
            }

            // PIXFORMAT_BAYER_GBRG shifted down by 1 becomes PIXFORMAT_BAYER_RGGB
            vrgb_pixels_t pixels1 = vdebayer_rggb(rows.r1, rows.r2, rows.r3);
            vstr_u16_narrow_u8_pred(Y0p1 + x, vdebayer_to_y(pixels1), pred);
            vstr_u16_narrow_u8_pred(CBp1 + x, vdebayer_to_cb(pixels1), pred);
            vstr_u16_narrow_u8_pred(CRp1 + x, vdebayer_to_cr(pixels1), pred);
        }
    }
}

static void vdebayer_grbg_to_ycbcr(image_t *src, rectangle_t *roi, int8_t *Y0, int8_t *CB, int8_t *CR) {
    // Load pixels, but, each set of 4 pixels overlaps the previous by 2 pixels.
    v128_t offsets = vidup_u32_unaligned(0, 2);

    for (int32_t y = 0, y_end = roi->h - 1; y < roi->h; y += VBAYER_Y_STRIDE) {
        v4x_row_ptrs_t rowptrs = vdebayer_rowptrs_init(src, y + roi->y);
        uint8_t *Y0p0 = ((uint8_t *) Y0) + (y * JPEG_MCU_W);
        uint8_t *CBp0 = ((uint8_t *) CB) + (y * JPEG_MCU_W);
        uint8_t *CRp0 = ((uint8_t *) CR) + (y * JPEG_MCU_W);
        uint8_t *Y0p1 = Y0p0 + JPEG_MCU_W;
        uint8_t *CBp1 = CBp0 + JPEG_MCU_W;
        uint8_t *CRp1 = CRp0 + JPEG_MCU_W;

        for (int32_t x = 0; x < roi->w; x += VBAYER_X_STRIDE) {
            v4x_rows_t rows = vdebayer_load_rows(src, rowptrs, x + roi->x, offsets);
            v128_predicate_t pred = vdebayer_store_pred(roi->w, x);

            vrgb_pixels_t pixels0 = vdebayer_grbg(rows.r0, rows.r1, rows.r2);
            vstr_u16_narrow_u8_pred(Y0p0 + x, vdebayer_to_y(pixels0), pred);
            vstr_u16_narrow_u8_pred(CBp0 + x, vdebayer_to_cb(pixels0), pred);
            vstr_u16_narrow_u8_pred(CRp0 + x, vdebayer_to_cr(pixels0), pred);

            if (y == y_end) {
                continue;
            }

            // PIXFORMAT_BAYER_GRBG shifted down by 1 becomes PIXFORMAT_BAYER_BGGR
            vrgb_pixels_t pixels1 = vdebayer_bggr(rows.r1, rows.r2, rows.r3);
            vstr_u16_narrow_u8_pred(Y0p1 + x, vdebayer_to_y(pixels1), pred);
            vstr_u16_narrow_u8_pred(CBp1 + x, vdebayer_to_cb(pixels1), pred);
            vstr_u16_narrow_u8_pred(CRp1 + x, vdebayer_to_cr(pixels1), pred);
        }
    }
}

static void vdebayer_rggb_to_ycbcr(image_t *src, rectangle_t *roi, int8_t *Y0, int8_t *CB, int8_t *CR) {
    // Load pixels, but, each set of 4 pixels overlaps the previous by 2 pixels.
    v128_t offsets = vidup_u32_unaligned(0, 2);

    for (int32_t y = 0, y_end = roi->h - 1; y < roi->h; y += VBAYER_Y_STRIDE) {
        v4x_row_ptrs_t rowptrs = vdebayer_rowptrs_init(src, y + roi->y);
        uint8_t *Y0p0 = ((uint8_t *) Y0) + (y * JPEG_MCU_W);
        uint8_t *CBp0 = ((uint8_t *) CB) + (y * JPEG_MCU_W);
        uint8_t *CRp0 = ((uint8_t *) CR) + (y * JPEG_MCU_W);
        uint8_t *Y0p1 = Y0p0 + JPEG_MCU_W;
        uint8_t *CBp1 = CBp0 + JPEG_MCU_W;
        uint8_t *CRp1 = CRp0 + JPEG_MCU_W;

        for (int32_t x = 0; x < roi->w; x += VBAYER_X_STRIDE) {
            v4x_rows_t rows = vdebayer_load_rows(src, rowptrs, x + roi->x, offsets);
            v128_predicate_t pred = vdebayer_store_pred(roi->w, x);

            vrgb_pixels_t pixels0 = vdebayer_rggb(rows.r0, rows.r1, rows.r2);
            vstr_u16_narrow_u8_pred(Y0p0 + x, vdebayer_to_y(pixels0), pred);
            vstr_u16_narrow_u8_pred(CBp0 + x, vdebayer_to_cb(pixels0), pred);
            vstr_u16_narrow_u8_pred(CRp0 + x, vdebayer_to_cr(pixels0), pred);

            if (y == y_end) {
                continue;
            }

            // PIXFORMAT_BAYER_RGGB shifted down by 1 becomes PIXFORMAT_BAYER_GBRG
            vrgb_pixels_t pixels1 = vdebayer_gbrg(rows.r1, rows.r2, rows.r3);
            vstr_u16_narrow_u8_pred(Y0p1 + x, vdebayer_to_y(pixels1), pred);
            vstr_u16_narrow_u8_pred(CBp1 + x, vdebayer_to_cb(pixels1), pred);
            vstr_u16_narrow_u8_pred(CRp1 + x, vdebayer_to_cr(pixels1), pred);
        }
    }
}

void imlib_debayer_ycbcr(image_t *src, rectangle_t *roi, int8_t *Y0, int8_t *CB, int8_t *CR) {
    switch (src->pixfmt) {
        case PIXFORMAT_BAYER_BGGR: {
            vdebayer_bggr_to_ycbcr(src, roi, Y0, CB, CR);
            break;
        }
        case PIXFORMAT_BAYER_GBRG: {
            vdebayer_gbrg_to_ycbcr(src, roi, Y0, CB, CR);
            break;
        }
        case PIXFORMAT_BAYER_GRBG: {
            vdebayer_grbg_to_ycbcr(src, roi, Y0, CB, CR);
            break;
        }
        case PIXFORMAT_BAYER_RGGB: {
            vdebayer_rggb_to_ycbcr(src, roi, Y0, CB, CR);
            break;
        }
        default: {
            __builtin_unreachable();
        }
    }
}
#else
void imlib_debayer_ycbcr(image_t *src, rectangle_t *roi, int8_t *Y0, int8_t *CB, int8_t *CR) {
    // Load pixels, but, each set of 4 pixels overlaps the previous by 2 pixels.
    v128_t offsets = vidup_u32_unaligned(0, 2);

    for (int32_t y = 0, y_end = roi->h - 1; y < roi->h; y += VBAYER_Y_STRIDE) {
        v4x_row_ptrs_t rowptrs = vdebayer_rowptrs_init(src, y + roi->y);
        uint8_t *Y0p0 = ((uint8_t *) Y0) + (y * JPEG_MCU_W);
        uint8_t *CBp0 = ((uint8_t *) CB) + (y * JPEG_MCU_W);
        uint8_t *CRp0 = ((uint8_t *) CR) + (y * JPEG_MCU_W);
        uint8_t *Y0p1 = Y0p0 + JPEG_MCU_W;
        uint8_t *CBp1 = CBp0 + JPEG_MCU_W;
        uint8_t *CRp1 = CRp0 + JPEG_MCU_W;

        for (int32_t x = 0; x < roi->w; x += VBAYER_X_STRIDE) {
            v4x_rows_t rows = vdebayer_load_rows(src, rowptrs, x + roi->x, offsets);
            v128_predicate_t pred = vdebayer_store_pred(roi->w, x);

            vrgb_pixels_t pixels0 = vdebayer_all_0(src, rows.r0, rows.r1, rows.r2);
            vstr_u16_narrow_u8_pred(Y0p0 + x, vdebayer_to_y(pixels0), pred);
            vstr_u16_narrow_u8_pred(CBp0 + x, vdebayer_to_cb(pixels0), pred);
            vstr_u16_narrow_u8_pred(CRp0 + x, vdebayer_to_cr(pixels0), pred);

            if (y == y_end) {
                continue;
            }

            // PIXFORMAT_BAYER_RGGB shifted down by 1 becomes PIXFORMAT_BAYER_GBRG
            vrgb_pixels_t pixels1 = vdebayer_all_1(src, rows.r1, rows.r2, rows.r3);
            vstr_u16_narrow_u8_pred(Y0p1 + x, vdebayer_to_y(pixels1), pred);
            vstr_u16_narrow_u8_pred(CBp1 + x, vdebayer_to_cb(pixels1), pred);
            vstr_u16_narrow_u8_pred(CRp1 + x, vdebayer_to_cr(pixels1), pred);
        }
    }
}
#endif // IMLIB_ENABLE_DEBAYER_OPTIMIZATION

static void vdebayer_bggr_to_grayscale_awb(image_t *src, image_t *dst, image_t *buf,
                                           uint32_t red_gain, uint32_t blue_gain) {
    // Load pixels, but, each set of 4 pixels overlaps the previous by 2 pixels.
    v128_t offsets = vidup_u32_unaligned(0, 2);

    for (int32_t y = 0, y_end = (src->h / VBAYER_Y_STRIDE) * VBAYER_Y_STRIDE; y < y_end; y += VBAYER_Y_STRIDE) {
        v4x_row_ptrs_t rowptrs = vdebayer_rowptrs_init(src, y);
        uint8_t *p0 = IMAGE_COMPUTE_GRAYSCALE_PIXEL_ROW_PTR(buf, (y % VBAYER_BUF_BROWS));
        uint8_t *p1 = IMAGE_COMPUTE_GRAYSCALE_PIXEL_ROW_PTR(buf, ((y + 1) % VBAYER_BUF_BROWS));

        v4x_rows_t rows = vdebayer_load_rows_inner(rowptrs, 0, offsets, vdebayer_load_pred(src, 0));
        v128_predicate_t pred = vdebayer_store_pred(src->w, 0);

        vrgb_pixels_t pixels0 = vdebayer_bggr(rows.r0, rows.r1, rows.r2);
        vrgb_pixels_store_grayscale(p0, 0, vdebayer_apply_rb_gain(pixels0, red_gain, blue_gain), pred);

        // PIXFORMAT_BAYER_BGGR shifted down by 1 becomes PIXFORMAT_BAYER_GRBG
        vrgb_pixels_t pixels1 = vdebayer_grbg(rows.r1, rows.r2, rows.r3);
        vrgb_pixels_store_grayscale(p1, 0, vdebayer_apply_rb_gain(pixels1, red_gain, blue_gain), pred);

        int32_t x = VBAYER_X_STRIDE;
        for (int32_t x_end = src->w - VBAYER_X_STRIDE; x < x_end; x += VBAYER_X_STRIDE) {
            v4x_rows_t rows;
            rows.r0 = vldr_u32_gather_unaligned(rowptrs.p0.u8 + x, offsets);
            rows.r1 = vldr_u32_gather_unaligned(rowptrs.p1.u8 + x, offsets);
            rows.r2 = vldr_u32_gather_unaligned(rowptrs.p2.u8 + x, offsets);
            rows.r3 = vldr_u32_gather_unaligned(rowptrs.p3.u8 + x, offsets);

            vrgb_pixels_t pixels0 = vdebayer_bggr(rows.r0, rows.r1, rows.r2);
            vrgb_pixels_store_grayscale(p0, x, vdebayer_apply_rb_gain(pixels0, red_gain, blue_gain), pred);

            // PIXFORMAT_BAYER_BGGR shifted down by 1 becomes PIXFORMAT_BAYER_GRBG
            vrgb_pixels_t pixels1 = vdebayer_grbg(rows.r1, rows.r2, rows.r3);
            vrgb_pixels_store_grayscale(p1, x, vdebayer_apply_rb_gain(pixels1, red_gain, blue_gain), pred);
        }

        if (x < src->w) {
            v4x_rows_t rows = vdebayer_load_rows_inner(rowptrs, x, offsets, vdebayer_load_pred(src, x));
            v128_predicate_t pred = vdebayer_store_pred(src->w, x);

            vrgb_pixels_t pixels0 = vdebayer_bggr(rows.r0, rows.r1, rows.r2);
            vrgb_pixels_store_grayscale(p0, x, vdebayer_apply_rb_gain(pixels0, red_gain, blue_gain), pred);

            // PIXFORMAT_BAYER_BGGR shifted down by 1 becomes PIXFORMAT_BAYER_GRBG
            vrgb_pixels_t pixels1 = vdebayer_grbg(rows.r1, rows.r2, rows.r3);
            vrgb_pixels_store_grayscale(p1, x, vdebayer_apply_rb_gain(pixels1, red_gain, blue_gain), pred);
        }

        vdebayer_grayscale_buf_copy(y, buf, dst);
    }

    // Last odd row.
    if (src->h % VBAYER_Y_STRIDE) {
        int32_t y = src->h - 1;
        v4x_row_ptrs_t rowptrs = vdebayer_rowptrs_init(src, y);
        uint8_t *p0 = IMAGE_COMPUTE_GRAYSCALE_PIXEL_ROW_PTR(buf, (y % VBAYER_BUF_BROWS));

        v4x_rows_t rows = vdebayer_load_rows_inner(rowptrs, 0, offsets, vdebayer_load_pred(src, 0));
        v128_predicate_t pred = vdebayer_store_pred(src->w, 0);

        vrgb_pixels_t pixels0 = vdebayer_bggr(rows.r0, rows.r1, rows.r2);
        vrgb_pixels_store_grayscale(p0, 0, vdebayer_apply_rb_gain(pixels0, red_gain, blue_gain), pred);

        int32_t x = VBAYER_X_STRIDE;
        for (int32_t x_end = src->w - VBAYER_X_STRIDE; x < x_end; x += VBAYER_X_STRIDE) {
            v4x_rows_t rows;
            rows.r0 = vldr_u32_gather_unaligned(rowptrs.p0.u8 + x, offsets);
            rows.r1 = vldr_u32_gather_unaligned(rowptrs.p1.u8 + x, offsets);
            rows.r2 = vldr_u32_gather_unaligned(rowptrs.p2.u8 + x, offsets);

            vrgb_pixels_t pixels0 = vdebayer_bggr(rows.r0, rows.r1, rows.r2);
            vrgb_pixels_store_grayscale(p0, x, vdebayer_apply_rb_gain(pixels0, red_gain, blue_gain), pred);

        }

        if (x < src->w) {
            v4x_rows_t rows = vdebayer_load_rows_inner(rowptrs, x, offsets, vdebayer_load_pred(src, x));
            v128_predicate_t pred = vdebayer_store_pred(src->w, x);

            vrgb_pixels_t pixels0 = vdebayer_bggr(rows.r0, rows.r1, rows.r2);
            vrgb_pixels_store_grayscale(p0, x, vdebayer_apply_rb_gain(pixels0, red_gain, blue_gain), pred);

        }

        vdebayer_grayscale_buf_copy(y, buf, dst);
    }
}

static void vdebayer_gbrg_to_grayscale_awb(image_t *src, image_t *dst, image_t *buf,
                                           uint32_t red_gain, uint32_t blue_gain) {
    // Load pixels, but, each set of 4 pixels overlaps the previous by 2 pixels.
    v128_t offsets = vidup_u32_unaligned(0, 2);

    for (int32_t y = 0, y_end = (src->h / VBAYER_Y_STRIDE) * VBAYER_Y_STRIDE; y < y_end; y += VBAYER_Y_STRIDE) {
        v4x_row_ptrs_t rowptrs = vdebayer_rowptrs_init(src, y);
        uint8_t *p0 = IMAGE_COMPUTE_GRAYSCALE_PIXEL_ROW_PTR(buf, (y % VBAYER_BUF_BROWS));
        uint8_t *p1 = IMAGE_COMPUTE_GRAYSCALE_PIXEL_ROW_PTR(buf, ((y + 1) % VBAYER_BUF_BROWS));

        v4x_rows_t rows = vdebayer_load_rows_inner(rowptrs, 0, offsets, vdebayer_load_pred(src, 0));
        v128_predicate_t pred = vdebayer_store_pred(src->w, 0);

        vrgb_pixels_t pixels0 = vdebayer_gbrg(rows.r0, rows.r1, rows.r2);
        vrgb_pixels_store_grayscale(p0, 0, vdebayer_apply_rb_gain(pixels0, red_gain, blue_gain), pred);

        // PIXFORMAT_BAYER_GBRG shifted down by 1 becomes PIXFORMAT_BAYER_RGGB
        vrgb_pixels_t pixels1 = vdebayer_rggb(rows.r1, rows.r2, rows.r3);
        vrgb_pixels_store_grayscale(p1, 0, vdebayer_apply_rb_gain(pixels1, red_gain, blue_gain), pred);

        int32_t x = VBAYER_X_STRIDE;
        for (int32_t x_end = src->w - VBAYER_X_STRIDE; x < x_end; x += VBAYER_X_STRIDE) {
            v4x_rows_t rows;
            rows.r0 = vldr_u32_gather_unaligned(rowptrs.p0.u8 + x, offsets);
            rows.r1 = vldr_u32_gather_unaligned(rowptrs.p1.u8 + x, offsets);
            rows.r2 = vldr_u32_gather_unaligned(rowptrs.p2.u8 + x, offsets);
            rows.r3 = vldr_u32_gather_unaligned(rowptrs.p3.u8 + x, offsets);

            vrgb_pixels_t pixels0 = vdebayer_gbrg(rows.r0, rows.r1, rows.r2);
            vrgb_pixels_store_grayscale(p0, x, vdebayer_apply_rb_gain(pixels0, red_gain, blue_gain), pred);

            // PIXFORMAT_BAYER_GBRG shifted down by 1 becomes PIXFORMAT_BAYER_RGGB
            vrgb_pixels_t pixels1 = vdebayer_rggb(rows.r1, rows.r2, rows.r3);
            vrgb_pixels_store_grayscale(p1, x, vdebayer_apply_rb_gain(pixels1, red_gain, blue_gain), pred);
        }

        if (x < src->w) {
            v4x_rows_t rows = vdebayer_load_rows_inner(rowptrs, x, offsets, vdebayer_load_pred(src, x));
            v128_predicate_t pred = vdebayer_store_pred(src->w, x);

            vrgb_pixels_t pixels0 = vdebayer_gbrg(rows.r0, rows.r1, rows.r2);
            vrgb_pixels_store_grayscale(p0, x, vdebayer_apply_rb_gain(pixels0, red_gain, blue_gain), pred);

            // PIXFORMAT_BAYER_GBRG shifted down by 1 becomes PIXFORMAT_BAYER_RGGB
            vrgb_pixels_t pixels1 = vdebayer_rggb(rows.r1, rows.r2, rows.r3);
            vrgb_pixels_store_grayscale(p1, x, vdebayer_apply_rb_gain(pixels1, red_gain, blue_gain), pred);
        }

        vdebayer_grayscale_buf_copy(y, buf, dst);
    }

    // Last odd row.
    if (src->h % VBAYER_Y_STRIDE) {
        int32_t y = src->h - 1;
        v4x_row_ptrs_t rowptrs = vdebayer_rowptrs_init(src, y);
        uint8_t *p0 = IMAGE_COMPUTE_GRAYSCALE_PIXEL_ROW_PTR(buf, (y % VBAYER_BUF_BROWS));

        v4x_rows_t rows = vdebayer_load_rows_inner(rowptrs, 0, offsets, vdebayer_load_pred(src, 0));
        v128_predicate_t pred = vdebayer_store_pred(src->w, 0);

        vrgb_pixels_t pixels0 = vdebayer_gbrg(rows.r0, rows.r1, rows.r2);
        vrgb_pixels_store_grayscale(p0, 0, vdebayer_apply_rb_gain(pixels0, red_gain, blue_gain), pred);

        int32_t x = VBAYER_X_STRIDE;
        for (int32_t x_end = src->w - VBAYER_X_STRIDE; x < x_end; x += VBAYER_X_STRIDE) {
            v4x_rows_t rows;
            rows.r0 = vldr_u32_gather_unaligned(rowptrs.p0.u8 + x, offsets);
            rows.r1 = vldr_u32_gather_unaligned(rowptrs.p1.u8 + x, offsets);
            rows.r2 = vldr_u32_gather_unaligned(rowptrs.p2.u8 + x, offsets);

            vrgb_pixels_t pixels0 = vdebayer_gbrg(rows.r0, rows.r1, rows.r2);
            vrgb_pixels_store_grayscale(p0, x, vdebayer_apply_rb_gain(pixels0, red_gain, blue_gain), pred);
        }

        if (x < src->w) {
            v4x_rows_t rows = vdebayer_load_rows_inner(rowptrs, x, offsets, vdebayer_load_pred(src, x));
            v128_predicate_t pred = vdebayer_store_pred(src->w, x);

            vrgb_pixels_t pixels0 = vdebayer_gbrg(rows.r0, rows.r1, rows.r2);
            vrgb_pixels_store_grayscale(p0, x, vdebayer_apply_rb_gain(pixels0, red_gain, blue_gain), pred);
        }

        vdebayer_grayscale_buf_copy(y, buf, dst);
    }
}

static void vdebayer_grbg_to_grayscale_awb(image_t *src, image_t *dst, image_t *buf,
                                           uint32_t red_gain, uint32_t blue_gain) {
    // Load pixels, but, each set of 4 pixels overlaps the previous by 2 pixels.
    v128_t offsets = vidup_u32_unaligned(0, 2);

    for (int32_t y = 0, y_end = (src->h / VBAYER_Y_STRIDE) * VBAYER_Y_STRIDE; y < y_end; y += VBAYER_Y_STRIDE) {
        v4x_row_ptrs_t rowptrs = vdebayer_rowptrs_init(src, y);
        uint8_t *p0 = IMAGE_COMPUTE_GRAYSCALE_PIXEL_ROW_PTR(buf, (y % VBAYER_BUF_BROWS));
        uint8_t *p1 = IMAGE_COMPUTE_GRAYSCALE_PIXEL_ROW_PTR(buf, ((y + 1) % VBAYER_BUF_BROWS));

        v4x_rows_t rows = vdebayer_load_rows_inner(rowptrs, 0, offsets, vdebayer_load_pred(src, 0));
        v128_predicate_t pred = vdebayer_store_pred(src->w, 0);

        vrgb_pixels_t pixels0 = vdebayer_grbg(rows.r0, rows.r1, rows.r2);
        vrgb_pixels_store_grayscale(p0, 0, vdebayer_apply_rb_gain(pixels0, red_gain, blue_gain), pred);

        // PIXFORMAT_BAYER_GRBG shifted down by 1 becomes PIXFORMAT_BAYER_BGGR
        vrgb_pixels_t pixels1 = vdebayer_bggr(rows.r1, rows.r2, rows.r3);
        vrgb_pixels_store_grayscale(p1, 0, vdebayer_apply_rb_gain(pixels1, red_gain, blue_gain), pred);

        int32_t x = VBAYER_X_STRIDE;
        for (int32_t x_end = src->w - VBAYER_X_STRIDE; x < x_end; x += VBAYER_X_STRIDE) {
            v4x_rows_t rows;
            rows.r0 = vldr_u32_gather_unaligned(rowptrs.p0.u8 + x, offsets);
            rows.r1 = vldr_u32_gather_unaligned(rowptrs.p1.u8 + x, offsets);
            rows.r2 = vldr_u32_gather_unaligned(rowptrs.p2.u8 + x, offsets);
            rows.r3 = vldr_u32_gather_unaligned(rowptrs.p3.u8 + x, offsets);

            vrgb_pixels_t pixels0 = vdebayer_grbg(rows.r0, rows.r1, rows.r2);
            vrgb_pixels_store_grayscale(p0, x, vdebayer_apply_rb_gain(pixels0, red_gain, blue_gain), pred);

            // PIXFORMAT_BAYER_GRBG shifted down by 1 becomes PIXFORMAT_BAYER_BGGR
            vrgb_pixels_t pixels1 = vdebayer_bggr(rows.r1, rows.r2, rows.r3);
            vrgb_pixels_store_grayscale(p1, x, vdebayer_apply_rb_gain(pixels1, red_gain, blue_gain), pred);
        }

        if (x < src->w) {
            v4x_rows_t rows = vdebayer_load_rows_inner(rowptrs, x, offsets, vdebayer_load_pred(src, x));
            v128_predicate_t pred = vdebayer_store_pred(src->w, x);

            vrgb_pixels_t pixels0 = vdebayer_grbg(rows.r0, rows.r1, rows.r2);
            vrgb_pixels_store_grayscale(p0, x, vdebayer_apply_rb_gain(pixels0, red_gain, blue_gain), pred);

            // PIXFORMAT_BAYER_GRBG shifted down by 1 becomes PIXFORMAT_BAYER_BGGR
            vrgb_pixels_t pixels1 = vdebayer_bggr(rows.r1, rows.r2, rows.r3);
            vrgb_pixels_store_grayscale(p1, x, vdebayer_apply_rb_gain(pixels1, red_gain, blue_gain), pred);
        }

        vdebayer_grayscale_buf_copy(y, buf, dst);
    }

    // Last odd row.
    if (src->h % VBAYER_Y_STRIDE) {
        int32_t y = src->h - 1;
        v4x_row_ptrs_t rowptrs = vdebayer_rowptrs_init(src, y);
        uint8_t *p0 = IMAGE_COMPUTE_GRAYSCALE_PIXEL_ROW_PTR(buf, (y % VBAYER_BUF_BROWS));

        v4x_rows_t rows = vdebayer_load_rows_inner(rowptrs, 0, offsets, vdebayer_load_pred(src, 0));
        v128_predicate_t pred = vdebayer_store_pred(src->w, 0);

        vrgb_pixels_t pixels0 = vdebayer_grbg(rows.r0, rows.r1, rows.r2);
        vrgb_pixels_store_grayscale(p0, 0, vdebayer_apply_rb_gain(pixels0, red_gain, blue_gain), pred);

        int32_t x = VBAYER_X_STRIDE;
        for (int32_t x_end = src->w - VBAYER_X_STRIDE; x < x_end; x += VBAYER_X_STRIDE) {
            v4x_rows_t rows;
            rows.r0 = vldr_u32_gather_unaligned(rowptrs.p0.u8 + x, offsets);
            rows.r1 = vldr_u32_gather_unaligned(rowptrs.p1.u8 + x, offsets);
            rows.r2 = vldr_u32_gather_unaligned(rowptrs.p2.u8 + x, offsets);

            vrgb_pixels_t pixels0 = vdebayer_grbg(rows.r0, rows.r1, rows.r2);
            vrgb_pixels_store_grayscale(p0, x, vdebayer_apply_rb_gain(pixels0, red_gain, blue_gain), pred);
        }

        if (x < src->w) {
            v4x_rows_t rows = vdebayer_load_rows_inner(rowptrs, x, offsets, vdebayer_load_pred(src, x));
            v128_predicate_t pred = vdebayer_store_pred(src->w, x);

            vrgb_pixels_t pixels0 = vdebayer_grbg(rows.r0, rows.r1, rows.r2);
            vrgb_pixels_store_grayscale(p0, x, vdebayer_apply_rb_gain(pixels0, red_gain, blue_gain), pred);
        }

        vdebayer_grayscale_buf_copy(y, buf, dst);
    }
}

static void vdebayer_rggb_to_grayscale_awb(image_t *src, image_t *dst, image_t *buf,
                                           uint32_t red_gain, uint32_t blue_gain) {
    // Load pixels, but, each set of 4 pixels overlaps the previous by 2 pixels.
    v128_t offsets = vidup_u32_unaligned(0, 2);

    for (int32_t y = 0, y_end = (src->h / VBAYER_Y_STRIDE) * VBAYER_Y_STRIDE; y < y_end; y += VBAYER_Y_STRIDE) {
        v4x_row_ptrs_t rowptrs = vdebayer_rowptrs_init(src, y);
        uint8_t *p0 = IMAGE_COMPUTE_GRAYSCALE_PIXEL_ROW_PTR(buf, (y % VBAYER_BUF_BROWS));
        uint8_t *p1 = IMAGE_COMPUTE_GRAYSCALE_PIXEL_ROW_PTR(buf, ((y + 1) % VBAYER_BUF_BROWS));

        v4x_rows_t rows = vdebayer_load_rows_inner(rowptrs, 0, offsets, vdebayer_load_pred(src, 0));
        v128_predicate_t pred = vdebayer_store_pred(src->w, 0);

        vrgb_pixels_t pixels0 = vdebayer_rggb(rows.r0, rows.r1, rows.r2);
        vrgb_pixels_store_grayscale(p0, 0, vdebayer_apply_rb_gain(pixels0, red_gain, blue_gain), pred);

        // PIXFORMAT_BAYER_RGGB shifted down by 1 becomes PIXFORMAT_BAYER_GBRG
        vrgb_pixels_t pixels1 = vdebayer_gbrg(rows.r1, rows.r2, rows.r3);
        vrgb_pixels_store_grayscale(p1, 0, vdebayer_apply_rb_gain(pixels1, red_gain, blue_gain), pred);

        int32_t x = VBAYER_X_STRIDE;
        for (int32_t x_end = src->w - VBAYER_X_STRIDE; x < x_end; x += VBAYER_X_STRIDE) {
            v4x_rows_t rows;
            rows.r0 = vldr_u32_gather_unaligned(rowptrs.p0.u8 + x, offsets);
            rows.r1 = vldr_u32_gather_unaligned(rowptrs.p1.u8 + x, offsets);
            rows.r2 = vldr_u32_gather_unaligned(rowptrs.p2.u8 + x, offsets);
            rows.r3 = vldr_u32_gather_unaligned(rowptrs.p3.u8 + x, offsets);

            vrgb_pixels_t pixels0 = vdebayer_rggb(rows.r0, rows.r1, rows.r2);
            vrgb_pixels_store_grayscale(p0, x, vdebayer_apply_rb_gain(pixels0, red_gain, blue_gain), pred);

            // PIXFORMAT_BAYER_RGGB shifted down by 1 becomes PIXFORMAT_BAYER_GBRG
            vrgb_pixels_t pixels1 = vdebayer_gbrg(rows.r1, rows.r2, rows.r3);
            vrgb_pixels_store_grayscale(p1, x, vdebayer_apply_rb_gain(pixels1, red_gain, blue_gain), pred);
        }

        if (x < src->w) {
            v4x_rows_t rows = vdebayer_load_rows_inner(rowptrs, x, offsets, vdebayer_load_pred(src, x));
            v128_predicate_t pred = vdebayer_store_pred(src->w, x);

            vrgb_pixels_t pixels0 = vdebayer_rggb(rows.r0, rows.r1, rows.r2);
            vrgb_pixels_store_grayscale(p0, x, vdebayer_apply_rb_gain(pixels0, red_gain, blue_gain), pred);

            // PIXFORMAT_BAYER_RGGB shifted down by 1 becomes PIXFORMAT_BAYER_GBRG
            vrgb_pixels_t pixels1 = vdebayer_gbrg(rows.r1, rows.r2, rows.r3);
            vrgb_pixels_store_grayscale(p1, x, vdebayer_apply_rb_gain(pixels1, red_gain, blue_gain), pred);
        }

        vdebayer_grayscale_buf_copy(y, buf, dst);
    }

    // Last odd row.
    if (src->h % VBAYER_Y_STRIDE) {
        int32_t y = src->h - 1;
        v4x_row_ptrs_t rowptrs = vdebayer_rowptrs_init(src, y);
        uint8_t *p0 = IMAGE_COMPUTE_GRAYSCALE_PIXEL_ROW_PTR(buf, (y % VBAYER_BUF_BROWS));

        v4x_rows_t rows = vdebayer_load_rows_inner(rowptrs, 0, offsets, vdebayer_load_pred(src, 0));
        v128_predicate_t pred = vdebayer_store_pred(src->w, 0);

        vrgb_pixels_t pixels0 = vdebayer_rggb(rows.r0, rows.r1, rows.r2);
        vrgb_pixels_store_grayscale(p0, 0, vdebayer_apply_rb_gain(pixels0, red_gain, blue_gain), pred);

        int32_t x = VBAYER_X_STRIDE;
        for (int32_t x_end = src->w - VBAYER_X_STRIDE; x < x_end; x += VBAYER_X_STRIDE) {
            v4x_rows_t rows;
            rows.r0 = vldr_u32_gather_unaligned(rowptrs.p0.u8 + x, offsets);
            rows.r1 = vldr_u32_gather_unaligned(rowptrs.p1.u8 + x, offsets);
            rows.r2 = vldr_u32_gather_unaligned(rowptrs.p2.u8 + x, offsets);

            vrgb_pixels_t pixels0 = vdebayer_rggb(rows.r0, rows.r1, rows.r2);
            vrgb_pixels_store_grayscale(p0, x, vdebayer_apply_rb_gain(pixels0, red_gain, blue_gain), pred);
        }

        if (x < src->w) {
            v4x_rows_t rows = vdebayer_load_rows_inner(rowptrs, x, offsets, vdebayer_load_pred(src, x));
            v128_predicate_t pred = vdebayer_store_pred(src->w, x);

            vrgb_pixels_t pixels0 = vdebayer_rggb(rows.r0, rows.r1, rows.r2);
            vrgb_pixels_store_grayscale(p0, x, vdebayer_apply_rb_gain(pixels0, red_gain, blue_gain), pred);
        }

        vdebayer_grayscale_buf_copy(y, buf, dst);
    }
}

static void vdebayer_bggr_to_rgb565_awb(image_t *src, image_t *dst, image_t *buf,
                                        uint32_t red_gain, uint32_t blue_gain) {
    // Load pixels, but, each set of 4 pixels overlaps the previous by 2 pixels.
    v128_t offsets = vidup_u32_unaligned(0, 2);

    for (int32_t y = 0, y_end = (src->h / VBAYER_Y_STRIDE) * VBAYER_Y_STRIDE; y < y_end; y += VBAYER_Y_STRIDE) {
        v4x_row_ptrs_t rowptrs = vdebayer_rowptrs_init(src, y);
        uint16_t *p0 = IMAGE_COMPUTE_RGB565_PIXEL_ROW_PTR(buf, (y % VBAYER_BUF_BROWS));
        uint16_t *p1 = IMAGE_COMPUTE_RGB565_PIXEL_ROW_PTR(buf, ((y + 1) % VBAYER_BUF_BROWS));

        v4x_rows_t rows = vdebayer_load_rows_inner(rowptrs, 0, offsets, vdebayer_load_pred(src, 0));
        v128_predicate_t pred = vdebayer_store_pred(src->w, 0);

        vrgb_pixels_t pixels0 = vdebayer_bggr(rows.r0, rows.r1, rows.r2);
        vrgb_pixels_store_rgb565(p0, 0, vdebayer_apply_rb_gain(pixels0, red_gain, blue_gain), pred);

        // PIXFORMAT_BAYER_BGGR shifted down by 1 becomes PIXFORMAT_BAYER_GRBG
        vrgb_pixels_t pixels1 = vdebayer_grbg(rows.r1, rows.r2, rows.r3);
        vrgb_pixels_store_rgb565(p1, 0, vdebayer_apply_rb_gain(pixels1, red_gain, blue_gain), pred);

        int32_t x = VBAYER_X_STRIDE;
        for (int32_t x_end = src->w - VBAYER_X_STRIDE; x < x_end; x += VBAYER_X_STRIDE) {
            v4x_rows_t rows;
            rows.r0 = vldr_u32_gather_unaligned(rowptrs.p0.u8 + x, offsets);
            rows.r1 = vldr_u32_gather_unaligned(rowptrs.p1.u8 + x, offsets);
            rows.r2 = vldr_u32_gather_unaligned(rowptrs.p2.u8 + x, offsets);
            rows.r3 = vldr_u32_gather_unaligned(rowptrs.p3.u8 + x, offsets);

            vrgb_pixels_t pixels0 = vdebayer_bggr(rows.r0, rows.r1, rows.r2);
            vstr_u16(p0 + x, vrgb_pixels888_to_rgb565(vdebayer_apply_rb_gain(pixels0, red_gain, blue_gain)));

            // PIXFORMAT_BAYER_BGGR shifted down by 1 becomes PIXFORMAT_BAYER_GRBG
            vrgb_pixels_t pixels1 = vdebayer_grbg(rows.r1, rows.r2, rows.r3);
            vstr_u16(p1 + x, vrgb_pixels888_to_rgb565(vdebayer_apply_rb_gain(pixels1, red_gain, blue_gain)));
        }

        if (x < src->w) {
            v4x_rows_t rows = vdebayer_load_rows_inner(rowptrs, x, offsets, vdebayer_load_pred(src, x));
            v128_predicate_t pred = vdebayer_store_pred(src->w, x);

            vrgb_pixels_t pixels0 = vdebayer_bggr(rows.r0, rows.r1, rows.r2);
            vrgb_pixels_store_rgb565(p0, x, vdebayer_apply_rb_gain(pixels0, red_gain, blue_gain), pred);

            // PIXFORMAT_BAYER_BGGR shifted down by 1 becomes PIXFORMAT_BAYER_GRBG
            vrgb_pixels_t pixels1 = vdebayer_grbg(rows.r1, rows.r2, rows.r3);
            vrgb_pixels_store_rgb565(p1, x, vdebayer_apply_rb_gain(pixels1, red_gain, blue_gain), pred);
        }

        vdebayer_rgb565_buf_copy(y, buf, dst);
    }

    // Last odd row.
    if (src->h % VBAYER_Y_STRIDE) {
        int32_t y = src->h - 1;
        v4x_row_ptrs_t rowptrs = vdebayer_rowptrs_init(src, y);
        uint16_t *p0 = IMAGE_COMPUTE_RGB565_PIXEL_ROW_PTR(buf, (y % VBAYER_BUF_BROWS));

        v4x_rows_t rows = vdebayer_load_rows_inner(rowptrs, 0, offsets, vdebayer_load_pred(src, 0));
        v128_predicate_t pred = vdebayer_store_pred(src->w, 0);

        vrgb_pixels_t pixels0 = vdebayer_bggr(rows.r0, rows.r1, rows.r2);
        vrgb_pixels_store_rgb565(p0, 0, vdebayer_apply_rb_gain(pixels0, red_gain, blue_gain), pred);

        int32_t x = VBAYER_X_STRIDE;
        for (int32_t x_end = src->w - VBAYER_X_STRIDE; x < x_end; x += VBAYER_X_STRIDE) {
            v4x_rows_t rows;
            rows.r0 = vldr_u32_gather_unaligned(rowptrs.p0.u8 + x, offsets);
            rows.r1 = vldr_u32_gather_unaligned(rowptrs.p1.u8 + x, offsets);
            rows.r2 = vldr_u32_gather_unaligned(rowptrs.p2.u8 + x, offsets);

            vrgb_pixels_t pixels0 = vdebayer_bggr(rows.r0, rows.r1, rows.r2);
            vstr_u16(p0 + x, vrgb_pixels888_to_rgb565(vdebayer_apply_rb_gain(pixels0, red_gain, blue_gain)));
        }

        if (x < src->w) {
            v4x_rows_t rows = vdebayer_load_rows_inner(rowptrs, x, offsets, vdebayer_load_pred(src, x));
            v128_predicate_t pred = vdebayer_store_pred(src->w, x);

            vrgb_pixels_t pixels0 = vdebayer_bggr(rows.r0, rows.r1, rows.r2);
            vrgb_pixels_store_rgb565(p0, x, vdebayer_apply_rb_gain(pixels0, red_gain, blue_gain), pred);
        }

        vdebayer_rgb565_buf_copy(y, buf, dst);
    }
}

static void vdebayer_gbrg_to_rgb565_awb(image_t *src, image_t *dst, image_t *buf,
                                        uint32_t red_gain, uint32_t blue_gain) {
    // Load pixels, but, each set of 4 pixels overlaps the previous by 2 pixels.
    v128_t offsets = vidup_u32_unaligned(0, 2);

    for (int32_t y = 0, y_end = (src->h / VBAYER_Y_STRIDE) * VBAYER_Y_STRIDE; y < y_end; y += VBAYER_Y_STRIDE) {
        v4x_row_ptrs_t rowptrs = vdebayer_rowptrs_init(src, y);
        uint16_t *p0 = IMAGE_COMPUTE_RGB565_PIXEL_ROW_PTR(buf, (y % VBAYER_BUF_BROWS));
        uint16_t *p1 = IMAGE_COMPUTE_RGB565_PIXEL_ROW_PTR(buf, ((y + 1) % VBAYER_BUF_BROWS));

        v4x_rows_t rows = vdebayer_load_rows_inner(rowptrs, 0, offsets, vdebayer_load_pred(src, 0));
        v128_predicate_t pred = vdebayer_store_pred(src->w, 0);

        vrgb_pixels_t pixels0 = vdebayer_gbrg(rows.r0, rows.r1, rows.r2);
        vrgb_pixels_store_rgb565(p0, 0, vdebayer_apply_rb_gain(pixels0, red_gain, blue_gain), pred);

        // PIXFORMAT_BAYER_GBRG shifted down by 1 becomes PIXFORMAT_BAYER_RGGB
        vrgb_pixels_t pixels1 = vdebayer_rggb(rows.r1, rows.r2, rows.r3);
        vrgb_pixels_store_rgb565(p1, 0, vdebayer_apply_rb_gain(pixels1, red_gain, blue_gain), pred);

        int32_t x = VBAYER_X_STRIDE;
        for (int32_t x_end = src->w - VBAYER_X_STRIDE; x < x_end; x += VBAYER_X_STRIDE) {
            v4x_rows_t rows;
            rows.r0 = vldr_u32_gather_unaligned(rowptrs.p0.u8 + x, offsets);
            rows.r1 = vldr_u32_gather_unaligned(rowptrs.p1.u8 + x, offsets);
            rows.r2 = vldr_u32_gather_unaligned(rowptrs.p2.u8 + x, offsets);
            rows.r3 = vldr_u32_gather_unaligned(rowptrs.p3.u8 + x, offsets);

            vrgb_pixels_t pixels0 = vdebayer_gbrg(rows.r0, rows.r1, rows.r2);
            vstr_u16(p0 + x, vrgb_pixels888_to_rgb565(vdebayer_apply_rb_gain(pixels0, red_gain, blue_gain)));

            // PIXFORMAT_BAYER_GBRG shifted down by 1 becomes PIXFORMAT_BAYER_RGGB
            vrgb_pixels_t pixels1 = vdebayer_rggb(rows.r1, rows.r2, rows.r3);
            vstr_u16(p1 + x, vrgb_pixels888_to_rgb565(vdebayer_apply_rb_gain(pixels1, red_gain, blue_gain)));
        }

        if (x < src->w) {
            v4x_rows_t rows = vdebayer_load_rows_inner(rowptrs, x, offsets, vdebayer_load_pred(src, x));
            v128_predicate_t pred = vdebayer_store_pred(src->w, x);

            vrgb_pixels_t pixels0 = vdebayer_gbrg(rows.r0, rows.r1, rows.r2);
            vrgb_pixels_store_rgb565(p0, x, vdebayer_apply_rb_gain(pixels0, red_gain, blue_gain), pred);

            // PIXFORMAT_BAYER_GBRG shifted down by 1 becomes PIXFORMAT_BAYER_RGGB
            vrgb_pixels_t pixels1 = vdebayer_rggb(rows.r1, rows.r2, rows.r3);
            vrgb_pixels_store_rgb565(p1, x, vdebayer_apply_rb_gain(pixels1, red_gain, blue_gain), pred);
        }

        vdebayer_rgb565_buf_copy(y, buf, dst);
    }

    // Last odd row.
    if (src->h % VBAYER_Y_STRIDE) {
        int32_t y = src->h - 1;
        v4x_row_ptrs_t rowptrs = vdebayer_rowptrs_init(src, y);
        uint16_t *p0 = IMAGE_COMPUTE_RGB565_PIXEL_ROW_PTR(buf, (y % VBAYER_BUF_BROWS));

        v4x_rows_t rows = vdebayer_load_rows_inner(rowptrs, 0, offsets, vdebayer_load_pred(src, 0));
        v128_predicate_t pred = vdebayer_store_pred(src->w, 0);

        vrgb_pixels_t pixels0 = vdebayer_gbrg(rows.r0, rows.r1, rows.r2);
        vrgb_pixels_store_rgb565(p0, 0, vdebayer_apply_rb_gain(pixels0, red_gain, blue_gain), pred);

        int32_t x = VBAYER_X_STRIDE;
        for (int32_t x_end = src->w - VBAYER_X_STRIDE; x < x_end; x += VBAYER_X_STRIDE) {
            v4x_rows_t rows;
            rows.r0 = vldr_u32_gather_unaligned(rowptrs.p0.u8 + x, offsets);
            rows.r1 = vldr_u32_gather_unaligned(rowptrs.p1.u8 + x, offsets);
            rows.r2 = vldr_u32_gather_unaligned(rowptrs.p2.u8 + x, offsets);

            vrgb_pixels_t pixels0 = vdebayer_gbrg(rows.r0, rows.r1, rows.r2);
            vstr_u16(p0 + x, vrgb_pixels888_to_rgb565(vdebayer_apply_rb_gain(pixels0, red_gain, blue_gain)));
        }

        if (x < src->w) {
            v4x_rows_t rows = vdebayer_load_rows_inner(rowptrs, x, offsets, vdebayer_load_pred(src, x));
            v128_predicate_t pred = vdebayer_store_pred(src->w, x);

            vrgb_pixels_t pixels0 = vdebayer_gbrg(rows.r0, rows.r1, rows.r2);
            vrgb_pixels_store_rgb565(p0, x, vdebayer_apply_rb_gain(pixels0, red_gain, blue_gain), pred);
        }

        vdebayer_rgb565_buf_copy(y, buf, dst);
    }
}

static void vdebayer_grbg_to_rgb565_awb(image_t *src, image_t *dst, image_t *buf,
                                        uint32_t red_gain, uint32_t blue_gain) {
    // Load pixels, but, each set of 4 pixels overlaps the previous by 2 pixels.
    v128_t offsets = vidup_u32_unaligned(0, 2);

    for (int32_t y = 0, y_end = (src->h / VBAYER_Y_STRIDE) * VBAYER_Y_STRIDE; y < y_end; y += VBAYER_Y_STRIDE) {
        v4x_row_ptrs_t rowptrs = vdebayer_rowptrs_init(src, y);
        uint16_t *p0 = IMAGE_COMPUTE_RGB565_PIXEL_ROW_PTR(buf, (y % VBAYER_BUF_BROWS));
        uint16_t *p1 = IMAGE_COMPUTE_RGB565_PIXEL_ROW_PTR(buf, ((y + 1) % VBAYER_BUF_BROWS));

        v4x_rows_t rows = vdebayer_load_rows_inner(rowptrs, 0, offsets, vdebayer_load_pred(src, 0));
        v128_predicate_t pred = vdebayer_store_pred(src->w, 0);

        vrgb_pixels_t pixels0 = vdebayer_grbg(rows.r0, rows.r1, rows.r2);
        vrgb_pixels_store_rgb565(p0, 0, vdebayer_apply_rb_gain(pixels0, red_gain, blue_gain), pred);

        // PIXFORMAT_BAYER_GRBG shifted down by 1 becomes PIXFORMAT_BAYER_BGGR
        vrgb_pixels_t pixels1 = vdebayer_bggr(rows.r1, rows.r2, rows.r3);
        vrgb_pixels_store_rgb565(p1, 0, vdebayer_apply_rb_gain(pixels1, red_gain, blue_gain), pred);

        int32_t x = VBAYER_X_STRIDE;
        for (int32_t x_end = src->w - VBAYER_X_STRIDE; x < x_end; x += VBAYER_X_STRIDE) {
            v4x_rows_t rows;
            rows.r0 = vldr_u32_gather_unaligned(rowptrs.p0.u8 + x, offsets);
            rows.r1 = vldr_u32_gather_unaligned(rowptrs.p1.u8 + x, offsets);
            rows.r2 = vldr_u32_gather_unaligned(rowptrs.p2.u8 + x, offsets);
            rows.r3 = vldr_u32_gather_unaligned(rowptrs.p3.u8 + x, offsets);

            vrgb_pixels_t pixels0 = vdebayer_grbg(rows.r0, rows.r1, rows.r2);
            vstr_u16(p0 + x, vrgb_pixels888_to_rgb565(vdebayer_apply_rb_gain(pixels0, red_gain, blue_gain)));

            // PIXFORMAT_BAYER_GRBG shifted down by 1 becomes PIXFORMAT_BAYER_BGGR
            vrgb_pixels_t pixels1 = vdebayer_bggr(rows.r1, rows.r2, rows.r3);
            vstr_u16(p1 + x, vrgb_pixels888_to_rgb565(vdebayer_apply_rb_gain(pixels1, red_gain, blue_gain)));
        }

        if (x < src->w) {
            v4x_rows_t rows = vdebayer_load_rows_inner(rowptrs, x, offsets, vdebayer_load_pred(src, x));
            v128_predicate_t pred = vdebayer_store_pred(src->w, x);

            vrgb_pixels_t pixels0 = vdebayer_grbg(rows.r0, rows.r1, rows.r2);
            vrgb_pixels_store_rgb565(p0, x, vdebayer_apply_rb_gain(pixels0, red_gain, blue_gain), pred);

            // PIXFORMAT_BAYER_GRBG shifted down by 1 becomes PIXFORMAT_BAYER_BGGR
            vrgb_pixels_t pixels1 = vdebayer_bggr(rows.r1, rows.r2, rows.r3);
            vrgb_pixels_store_rgb565(p1, x, vdebayer_apply_rb_gain(pixels1, red_gain, blue_gain), pred);
        }

        vdebayer_rgb565_buf_copy(y, buf, dst);
    }

    // Last odd row.
    if (src->h % VBAYER_Y_STRIDE) {
        int32_t y = src->h - 1;
        v4x_row_ptrs_t rowptrs = vdebayer_rowptrs_init(src, y);
        uint16_t *p0 = IMAGE_COMPUTE_RGB565_PIXEL_ROW_PTR(buf, (y % VBAYER_BUF_BROWS));

        v4x_rows_t rows = vdebayer_load_rows_inner(rowptrs, 0, offsets, vdebayer_load_pred(src, 0));
        v128_predicate_t pred = vdebayer_store_pred(src->w, 0);

        vrgb_pixels_t pixels0 = vdebayer_grbg(rows.r0, rows.r1, rows.r2);
        vrgb_pixels_store_rgb565(p0, 0, vdebayer_apply_rb_gain(pixels0, red_gain, blue_gain), pred);

        int32_t x = VBAYER_X_STRIDE;
        for (int32_t x_end = src->w - VBAYER_X_STRIDE; x < x_end; x += VBAYER_X_STRIDE) {
            v4x_rows_t rows;
            rows.r0 = vldr_u32_gather_unaligned(rowptrs.p0.u8 + x, offsets);
            rows.r1 = vldr_u32_gather_unaligned(rowptrs.p1.u8 + x, offsets);
            rows.r2 = vldr_u32_gather_unaligned(rowptrs.p2.u8 + x, offsets);

            vrgb_pixels_t pixels0 = vdebayer_grbg(rows.r0, rows.r1, rows.r2);
            vstr_u16(p0 + x, vrgb_pixels888_to_rgb565(vdebayer_apply_rb_gain(pixels0, red_gain, blue_gain)));
        }

        if (x < src->w) {
            v4x_rows_t rows = vdebayer_load_rows_inner(rowptrs, x, offsets, vdebayer_load_pred(src, x));
            v128_predicate_t pred = vdebayer_store_pred(src->w, x);

            vrgb_pixels_t pixels0 = vdebayer_grbg(rows.r0, rows.r1, rows.r2);
            vrgb_pixels_store_rgb565(p0, x, vdebayer_apply_rb_gain(pixels0, red_gain, blue_gain), pred);
        }

        vdebayer_rgb565_buf_copy(y, buf, dst);
    }
}

static void vdebayer_rggb_to_rgb565_awb(image_t *src, image_t *dst, image_t *buf,
                                        uint32_t red_gain, uint32_t blue_gain) {
    // Load pixels, but, each set of 4 pixels overlaps the previous by 2 pixels.
    v128_t offsets = vidup_u32_unaligned(0, 2);

    for (int32_t y = 0, y_end = (src->h / VBAYER_Y_STRIDE) * VBAYER_Y_STRIDE; y < y_end; y += VBAYER_Y_STRIDE) {
        v4x_row_ptrs_t rowptrs = vdebayer_rowptrs_init(src, y);
        uint16_t *p0 = IMAGE_COMPUTE_RGB565_PIXEL_ROW_PTR(buf, (y % VBAYER_BUF_BROWS));
        uint16_t *p1 = IMAGE_COMPUTE_RGB565_PIXEL_ROW_PTR(buf, ((y + 1) % VBAYER_BUF_BROWS));

        v4x_rows_t rows = vdebayer_load_rows_inner(rowptrs, 0, offsets, vdebayer_load_pred(src, 0));
        v128_predicate_t pred = vdebayer_store_pred(src->w, 0);

        vrgb_pixels_t pixels0 = vdebayer_rggb(rows.r0, rows.r1, rows.r2);
        vrgb_pixels_store_rgb565(p0, 0, vdebayer_apply_rb_gain(pixels0, red_gain, blue_gain), pred);

        // PIXFORMAT_BAYER_RGGB shifted down by 1 becomes PIXFORMAT_BAYER_GBRG
        vrgb_pixels_t pixels1 = vdebayer_gbrg(rows.r1, rows.r2, rows.r3);
        vrgb_pixels_store_rgb565(p1, 0, vdebayer_apply_rb_gain(pixels1, red_gain, blue_gain), pred);

        int32_t x = VBAYER_X_STRIDE;
        for (int32_t x_end = src->w - VBAYER_X_STRIDE; x < x_end; x += VBAYER_X_STRIDE) {
            v4x_rows_t rows;
            rows.r0 = vldr_u32_gather_unaligned(rowptrs.p0.u8 + x, offsets);
            rows.r1 = vldr_u32_gather_unaligned(rowptrs.p1.u8 + x, offsets);
            rows.r2 = vldr_u32_gather_unaligned(rowptrs.p2.u8 + x, offsets);
            rows.r3 = vldr_u32_gather_unaligned(rowptrs.p3.u8 + x, offsets);

            vrgb_pixels_t pixels0 = vdebayer_rggb(rows.r0, rows.r1, rows.r2);
            vstr_u16(p0 + x, vrgb_pixels888_to_rgb565(vdebayer_apply_rb_gain(pixels0, red_gain, blue_gain)));

            // PIXFORMAT_BAYER_RGGB shifted down by 1 becomes PIXFORMAT_BAYER_GBRG
            vrgb_pixels_t pixels1 = vdebayer_gbrg(rows.r1, rows.r2, rows.r3);
            vstr_u16(p1 + x, vrgb_pixels888_to_rgb565(vdebayer_apply_rb_gain(pixels1, red_gain, blue_gain)));
        }

        if (x < src->w) {
            v4x_rows_t rows = vdebayer_load_rows_inner(rowptrs, x, offsets, vdebayer_load_pred(src, x));
            v128_predicate_t pred = vdebayer_store_pred(src->w, x);

            vrgb_pixels_t pixels0 = vdebayer_rggb(rows.r0, rows.r1, rows.r2);
            vrgb_pixels_store_rgb565(p0, x, vdebayer_apply_rb_gain(pixels0, red_gain, blue_gain), pred);

            // PIXFORMAT_BAYER_RGGB shifted down by 1 becomes PIXFORMAT_BAYER_GBRG
            vrgb_pixels_t pixels1 = vdebayer_gbrg(rows.r1, rows.r2, rows.r3);
            vrgb_pixels_store_rgb565(p1, x, vdebayer_apply_rb_gain(pixels1, red_gain, blue_gain), pred);
        }

        vdebayer_rgb565_buf_copy(y, buf, dst);
    }

    // Last odd row.
    if (src->h % VBAYER_Y_STRIDE) {
        int32_t y = src->h - 1;
        v4x_row_ptrs_t rowptrs = vdebayer_rowptrs_init(src, y);
        uint16_t *p0 = IMAGE_COMPUTE_RGB565_PIXEL_ROW_PTR(buf, (y % VBAYER_BUF_BROWS));

        v4x_rows_t rows = vdebayer_load_rows_inner(rowptrs, 0, offsets, vdebayer_load_pred(src, 0));
        v128_predicate_t pred = vdebayer_store_pred(src->w, 0);

        vrgb_pixels_t pixels0 = vdebayer_rggb(rows.r0, rows.r1, rows.r2);
        vrgb_pixels_store_rgb565(p0, 0, vdebayer_apply_rb_gain(pixels0, red_gain, blue_gain), pred);

        int32_t x = VBAYER_X_STRIDE;
        for (int32_t x_end = src->w - VBAYER_X_STRIDE; x < x_end; x += VBAYER_X_STRIDE) {
            v4x_rows_t rows;
            rows.r0 = vldr_u32_gather_unaligned(rowptrs.p0.u8 + x, offsets);
            rows.r1 = vldr_u32_gather_unaligned(rowptrs.p1.u8 + x, offsets);
            rows.r2 = vldr_u32_gather_unaligned(rowptrs.p2.u8 + x, offsets);

            vrgb_pixels_t pixels0 = vdebayer_rggb(rows.r0, rows.r1, rows.r2);
            vstr_u16(p0 + x, vrgb_pixels888_to_rgb565(vdebayer_apply_rb_gain(pixels0, red_gain, blue_gain)));
        }

        if (x < src->w) {
            v4x_rows_t rows = vdebayer_load_rows_inner(rowptrs, x, offsets, vdebayer_load_pred(src, x));
            v128_predicate_t pred = vdebayer_store_pred(src->w, x);

            vrgb_pixels_t pixels0 = vdebayer_rggb(rows.r0, rows.r1, rows.r2);
            vrgb_pixels_store_rgb565(p0, x, vdebayer_apply_rb_gain(pixels0, red_gain, blue_gain), pred);
        }

        vdebayer_rgb565_buf_copy(y, buf, dst);
    }
}

static void vdebayer_bggr_to_grayscale_awb_quarter(image_t *src, image_t *dst, uint32_t red_gain, uint32_t blue_gain) {
    for (int32_t y = 0; y < src->h; y += VBAYER_Y_STRIDE) {
        v2x_row_ptrs_t rowptrs = vdebayer_quarter_rowptrs_init(src, y);
        uint8_t *p = IMAGE_COMPUTE_GRAYSCALE_PIXEL_ROW_PTR(dst, (y / 2));

        for (int32_t x = 0; x < src->w; x += VBAYER_X_STRIDE_2X2) {
            int32_t len = src->w - x;
            v2x_rows_t bg, gr;

            if (len >= VBAYER_X_STRIDE_2X2) {
                bg = vld2_u8(rowptrs.p0.u8 + x);
                gr = vld2_u8(rowptrs.p1.u8 + x);
            } else {
                bg = vld2_u8_len(rowptrs.p0.u8 + x, len);
                gr = vld2_u8_len(rowptrs.p1.u8 + x, len);
            }

            vrgb_pixels_t packed = {
                .r = gr.r1,
                .g = vhadd_u8(gr.r0, bg.r1),
                .b = bg.r0
            };

            vdebayer_store_packed_grayscale(p + (x / 2), packed, red_gain, blue_gain, dst->w - (x / 2));
        }
    }
}

static void vdebayer_gbrg_to_grayscale_awb_quarter(image_t *src, image_t *dst, uint32_t red_gain, uint32_t blue_gain) {
    for (int32_t y = 0; y < src->h; y += VBAYER_Y_STRIDE) {
        v2x_row_ptrs_t rowptrs = vdebayer_quarter_rowptrs_init(src, y);
        uint8_t *p = IMAGE_COMPUTE_GRAYSCALE_PIXEL_ROW_PTR(dst, (y / 2));

        for (int32_t x = 0; x < src->w; x += VBAYER_X_STRIDE_2X2) {
            int32_t len = src->w - x;
            v2x_rows_t gb, rg;

            if (len >= VBAYER_X_STRIDE_2X2) {
                gb = vld2_u8(rowptrs.p0.u8 + x);
                rg = vld2_u8(rowptrs.p1.u8 + x);
            } else {
                gb = vld2_u8_len(rowptrs.p0.u8 + x, len);
                rg = vld2_u8_len(rowptrs.p1.u8 + x, len);
            }

            vrgb_pixels_t packed = {
                .r = rg.r0,
                .g = vhadd_u8(gb.r0, rg.r1),
                .b = gb.r1
            };

            vdebayer_store_packed_grayscale(p + (x / 2), packed, red_gain, blue_gain, dst->w - (x / 2));
        }
    }
}

static void vdebayer_grbg_to_grayscale_awb_quarter(image_t *src, image_t *dst, uint32_t red_gain, uint32_t blue_gain) {
    for (int32_t y = 0; y < src->h; y += VBAYER_Y_STRIDE) {
        v2x_row_ptrs_t rowptrs = vdebayer_quarter_rowptrs_init(src, y);
        uint8_t *p = IMAGE_COMPUTE_GRAYSCALE_PIXEL_ROW_PTR(dst, (y / 2));

        for (int32_t x = 0; x < src->w; x += VBAYER_X_STRIDE_2X2) {
            int32_t len = src->w - x;
            v2x_rows_t gr, bg;

            if (len >= VBAYER_X_STRIDE_2X2) {
                gr = vld2_u8(rowptrs.p0.u8 + x);
                bg = vld2_u8(rowptrs.p1.u8 + x);
            } else {
                gr = vld2_u8_len(rowptrs.p0.u8 + x, len);
                bg = vld2_u8_len(rowptrs.p1.u8 + x, len);
            }

            vrgb_pixels_t packed = {
                .r = gr.r1,
                .g = vhadd_u8(gr.r0, bg.r1),
                .b = bg.r0
            };

            vdebayer_store_packed_grayscale(p + (x / 2), packed, red_gain, blue_gain, dst->w - (x / 2));
        }
    }
}

static void vdebayer_rggb_to_grayscale_awb_quarter(image_t *src, image_t *dst, uint32_t red_gain, uint32_t blue_gain) {
    for (int32_t y = 0; y < src->h; y += VBAYER_Y_STRIDE) {
        v2x_row_ptrs_t rowptrs = vdebayer_quarter_rowptrs_init(src, y);
        uint8_t *p = IMAGE_COMPUTE_GRAYSCALE_PIXEL_ROW_PTR(dst, (y / 2));

        for (int32_t x = 0; x < src->w; x += VBAYER_X_STRIDE_2X2) {
            int32_t len = src->w - x;
            v2x_rows_t rg, gb;

            if (len >= VBAYER_X_STRIDE_2X2) {
                rg = vld2_u8(rowptrs.p0.u8 + x);
                gb = vld2_u8(rowptrs.p1.u8 + x);
            } else {
                rg = vld2_u8_len(rowptrs.p0.u8 + x, len);
                gb = vld2_u8_len(rowptrs.p1.u8 + x, len);
            }

            vrgb_pixels_t packed = {
                .r = rg.r0,
                .g = vhadd_u8(gb.r0, rg.r1),
                .b = gb.r1
            };

            vdebayer_store_packed_grayscale(p + (x / 2), packed, red_gain, blue_gain, dst->w - (x / 2));
        }
    }
}

static void vdebayer_bggr_to_rgb565_awb_quarter(image_t *src, image_t *dst, uint32_t red_gain, uint32_t blue_gain) {
    for (int32_t y = 0; y < src->h; y += VBAYER_Y_STRIDE) {
        v2x_row_ptrs_t rowptrs = vdebayer_quarter_rowptrs_init(src, y);
        uint16_t *p = IMAGE_COMPUTE_RGB565_PIXEL_ROW_PTR(dst, (y / 2));

        for (int32_t x = 0; x < src->w; x += VBAYER_X_STRIDE_2X2) {
            int32_t len = src->w - x;
            v2x_rows_t bg, gr;

            if (len >= VBAYER_X_STRIDE_2X2) {
                bg = vld2_u8(rowptrs.p0.u8 + x);
                gr = vld2_u8(rowptrs.p1.u8 + x);
            } else {
                bg = vld2_u8_len(rowptrs.p0.u8 + x, len);
                gr = vld2_u8_len(rowptrs.p1.u8 + x, len);
            }

            vrgb_pixels_t packed = {
                .r = gr.r1,
                .g = vhadd_u8(gr.r0, bg.r1),
                .b = bg.r0
            };

            vdebayer_store_packed_rgb565(p + (x / 2), packed, red_gain, blue_gain, dst->w - (x / 2));
        }
    }
}

static void vdebayer_gbrg_to_rgb565_awb_quarter(image_t *src, image_t *dst, uint32_t red_gain, uint32_t blue_gain) {
    for (int32_t y = 0; y < src->h; y += VBAYER_Y_STRIDE) {
        v2x_row_ptrs_t rowptrs = vdebayer_quarter_rowptrs_init(src, y);
        uint16_t *p = IMAGE_COMPUTE_RGB565_PIXEL_ROW_PTR(dst, (y / 2));

        for (int32_t x = 0; x < src->w; x += VBAYER_X_STRIDE_2X2) {
            int32_t len = src->w - x;
            v2x_rows_t gb, rg;

            if (len >= VBAYER_X_STRIDE_2X2) {
                gb = vld2_u8(rowptrs.p0.u8 + x);
                rg = vld2_u8(rowptrs.p1.u8 + x);
            } else {
                gb = vld2_u8_len(rowptrs.p0.u8 + x, len);
                rg = vld2_u8_len(rowptrs.p1.u8 + x, len);
            }

            vrgb_pixels_t packed = {
                .r = rg.r0,
                .g = vhadd_u8(gb.r0, rg.r1),
                .b = gb.r1
            };

            vdebayer_store_packed_rgb565(p + (x / 2), packed, red_gain, blue_gain, dst->w - (x / 2));
        }
    }
}

static void vdebayer_grbg_to_rgb565_awb_quarter(image_t *src, image_t *dst, uint32_t red_gain, uint32_t blue_gain) {
    for (int32_t y = 0; y < src->h; y += VBAYER_Y_STRIDE) {
        v2x_row_ptrs_t rowptrs = vdebayer_quarter_rowptrs_init(src, y);
        uint16_t *p = IMAGE_COMPUTE_RGB565_PIXEL_ROW_PTR(dst, (y / 2));

        for (int32_t x = 0; x < src->w; x += VBAYER_X_STRIDE_2X2) {
            int32_t len = src->w - x;
            v2x_rows_t gr, bg;

            if (len >= VBAYER_X_STRIDE_2X2) {
                gr = vld2_u8(rowptrs.p0.u8 + x);
                bg = vld2_u8(rowptrs.p1.u8 + x);
            } else {
                gr = vld2_u8_len(rowptrs.p0.u8 + x, len);
                bg = vld2_u8_len(rowptrs.p1.u8 + x, len);
            }

            vrgb_pixels_t packed = {
                .r = gr.r1,
                .g = vhadd_u8(gr.r0, bg.r1),
                .b = bg.r0
            };

            vdebayer_store_packed_rgb565(p + (x / 2), packed, red_gain, blue_gain, dst->w - (x / 2));
        }
    }
}

static void vdebayer_rggb_to_rgb565_awb_quarter(image_t *src, image_t *dst, uint32_t red_gain, uint32_t blue_gain) {
    for (int32_t y = 0; y < src->h; y += VBAYER_Y_STRIDE) {
        v2x_row_ptrs_t rowptrs = vdebayer_quarter_rowptrs_init(src, y);
        uint16_t *p = IMAGE_COMPUTE_RGB565_PIXEL_ROW_PTR(dst, (y / 2));

        for (int32_t x = 0; x < src->w; x += VBAYER_X_STRIDE_2X2) {
            int32_t len = src->w - x;
            v2x_rows_t rg, gb;

            if (len >= VBAYER_X_STRIDE_2X2) {
                rg = vld2_u8(rowptrs.p0.u8 + x);
                gb = vld2_u8(rowptrs.p1.u8 + x);
            } else {
                rg = vld2_u8_len(rowptrs.p0.u8 + x, len);
                gb = vld2_u8_len(rowptrs.p1.u8 + x, len);
            }

            vrgb_pixels_t packed = {
                .r = rg.r0,
                .g = vhadd_u8(gb.r0, rg.r1),
                .b = gb.r1
            };

            vdebayer_store_packed_rgb565(p + (x / 2), packed, red_gain, blue_gain, dst->w - (x / 2));
        }
    }
}

// assumes dst->w == src->w (or dst->w == src->w / 2)
// assumes dst->h == src->h (or dst->h == src->h / 2)
// src and dst may overlap, but, slower than imlib_debayer_image
// BINARY: Not supported
// GRAYSCALE: src->data == dst->data
// RGB565: src->data == dst->data + image_size(src)
// YUV422: Not supported
void imlib_debayer_image_awb(image_t *dst, image_t *src, bool fast, uint32_t r_out, uint32_t g_out, uint32_t b_out) {
    OMV_PROFILE_START();

    uint32_t red_gain = IM_DIV(g_out * 32, r_out);
    red_gain = IM_MIN(red_gain, 128U);

    uint32_t blue_gain = IM_DIV(g_out * 32, b_out);
    blue_gain = IM_MIN(blue_gain, 128U);

    if (fast) {
        switch (src->pixfmt) {
            case PIXFORMAT_BAYER_BGGR: {
                switch (dst->pixfmt) {
                    case PIXFORMAT_GRAYSCALE: {
                        vdebayer_bggr_to_grayscale_awb_quarter(src, dst, red_gain, blue_gain);
                        break;
                    }
                    case PIXFORMAT_RGB565: {
                        vdebayer_bggr_to_rgb565_awb_quarter(src, dst, red_gain, blue_gain);
                        break;
                    }
                    default: {
                        __builtin_unreachable();
                    }
                }
                break;
            }
            case PIXFORMAT_BAYER_GBRG: {
                switch (dst->pixfmt) {
                    case PIXFORMAT_GRAYSCALE: {
                        vdebayer_gbrg_to_grayscale_awb_quarter(src, dst, red_gain, blue_gain);
                        break;
                    }
                    case PIXFORMAT_RGB565: {
                        vdebayer_gbrg_to_rgb565_awb_quarter(src, dst, red_gain, blue_gain);
                        break;
                    }
                    default: {
                        __builtin_unreachable();
                    }
                }
                break;
            }
            case PIXFORMAT_BAYER_GRBG: {
                switch (dst->pixfmt) {
                    case PIXFORMAT_GRAYSCALE: {
                        vdebayer_grbg_to_grayscale_awb_quarter(src, dst, red_gain, blue_gain);
                        break;
                    }
                    case PIXFORMAT_RGB565: {
                        vdebayer_grbg_to_rgb565_awb_quarter(src, dst, red_gain, blue_gain);
                        break;
                    }
                    default: {
                        __builtin_unreachable();
                    }
                }
                break;
            }
            case PIXFORMAT_BAYER_RGGB: {
                switch (dst->pixfmt) {
                    case PIXFORMAT_GRAYSCALE: {
                        vdebayer_rggb_to_grayscale_awb_quarter(src, dst, red_gain, blue_gain);
                        break;
                    }
                    case PIXFORMAT_RGB565: {
                        vdebayer_rggb_to_rgb565_awb_quarter(src, dst, red_gain, blue_gain);
                        break;
                    }
                    default: {
                        __builtin_unreachable();
                    }
                }
                break;
            }
            default: {
                __builtin_unreachable();
            }
        }
    } else {
        switch (dst->pixfmt) {
            case PIXFORMAT_GRAYSCALE: {
                image_t buf = {
                    .w = dst->w,
                    .h = VBAYER_BUF_BROWS,
                    .pixfmt = PIXFORMAT_GRAYSCALE,
                    .data = fb_alloc(dst->w * VBAYER_BUF_BROWS * sizeof(uint8_t), FB_ALLOC_PREFER_SPEED),
                };

                switch (src->pixfmt) {
                    case PIXFORMAT_BAYER_BGGR: {
                        vdebayer_bggr_to_grayscale_awb(src, dst, &buf, red_gain, blue_gain);
                        break;
                    }
                    case PIXFORMAT_BAYER_GBRG: {
                        vdebayer_gbrg_to_grayscale_awb(src, dst, &buf, red_gain, blue_gain);
                        break;
                    }
                    case PIXFORMAT_BAYER_GRBG: {
                        vdebayer_grbg_to_grayscale_awb(src, dst, &buf, red_gain, blue_gain);
                        break;
                    }
                    case PIXFORMAT_BAYER_RGGB: {
                        vdebayer_rggb_to_grayscale_awb(src, dst, &buf, red_gain, blue_gain);
                        break;
                    }
                    default: {
                        __builtin_unreachable();
                    }
                }

                // Copy any remaining lines from the buffer image...
                for (int32_t y = IM_MAX(dst->h - VBAYER_BUF_KSIZE, 0); y < dst->h; y++) {
                    vmemcpy_8(IMAGE_COMPUTE_GRAYSCALE_PIXEL_ROW_PTR(dst, y),
                              IMAGE_COMPUTE_GRAYSCALE_PIXEL_ROW_PTR(&buf, (y % VBAYER_BUF_BROWS)),
                              IMAGE_GRAYSCALE_LINE_LEN_BYTES(dst));
                }

                fb_free(); // buf.data
                break;
            }
            case PIXFORMAT_RGB565: {
                image_t buf = {
                    .w = dst->w,
                    .h = VBAYER_BUF_BROWS,
                    .pixfmt = PIXFORMAT_RGB565,
                    .data = fb_alloc(dst->w * VBAYER_BUF_BROWS * sizeof(uint16_t), FB_ALLOC_PREFER_SPEED),
                };

                switch (src->pixfmt) {
                    case PIXFORMAT_BAYER_BGGR: {
                        vdebayer_bggr_to_rgb565_awb(src, dst, &buf, red_gain, blue_gain);
                        break;
                    }
                    case PIXFORMAT_BAYER_GBRG: {
                        vdebayer_gbrg_to_rgb565_awb(src, dst, &buf, red_gain, blue_gain);
                        break;
                    }
                    case PIXFORMAT_BAYER_GRBG: {
                        vdebayer_grbg_to_rgb565_awb(src, dst, &buf, red_gain, blue_gain);
                        break;
                    }
                    case PIXFORMAT_BAYER_RGGB: {
                        vdebayer_rggb_to_rgb565_awb(src, dst, &buf, red_gain, blue_gain);
                        break;
                    }
                    default: {
                        __builtin_unreachable();
                    }
                }

                // Copy any remaining lines from the buffer image...
                for (int32_t y = IM_MAX(dst->h - VBAYER_BUF_KSIZE, 0); y < dst->h; y++) {
                    vmemcpy_16(IMAGE_COMPUTE_RGB565_PIXEL_ROW_PTR(dst, y),
                               IMAGE_COMPUTE_RGB565_PIXEL_ROW_PTR(&buf, (y % VBAYER_BUF_BROWS)),
                               IMAGE_RGB565_LINE_LEN_BYTES(dst));
                }

                fb_free(); // buf.data
                break;
            }
            default: {
                __builtin_unreachable();
            }
        }
    }

    OMV_PROFILE_PRINT();
}
