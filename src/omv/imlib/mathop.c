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
 * Image math operations.
 */
#include "imlib.h"

#ifdef IMLIB_ENABLE_MATH_OPS
void imlib_add_line_op(int x, int x_end, int y_row, imlib_draw_row_data_t *data) {
    image_t *mask = (image_t *) data->callback_arg;

    switch (data->dst_img->pixfmt) {
        case PIXFORMAT_BINARY: {
            imlib_b_or_line_op(x, x_end, y_row, data);
            break;
        }
        case PIXFORMAT_GRAYSCALE: {
            uint8_t *row0 = IMAGE_COMPUTE_GRAYSCALE_PIXEL_ROW_PTR(data->dst_img, y_row);
            uint8_t *row1 = (uint8_t *) data->dst_row_override;

            if (!mask) {
                uint32_t n = x_end - x;

                for (; n >= UINT8_VECTOR_SIZE; n -= UINT8_VECTOR_SIZE, x += UINT8_VECTOR_SIZE) {
                    vstr_u8(row0 + x, vqadd_u8(vldr_u8(row0 + x), vldr_u8(row1 + x)));
                }

                if (n) {
                    v128_predicate_t pred = vpredicate_8(n);
                    v2x_rows_t data = vldr_u8_pred_x2((v2x_row_ptrs_t) {.p0.u8 = row0, .p1.u8 = row1}, x, pred);
                    vstr_u8_pred(row0 + x, vqadd_u8(data.r0, data.r1), pred);
                }
            } else {
                for (; x < x_end; x++) {
                    if (image_get_mask_pixel(mask, x, y_row)) {
                        uint32_t p0 = IMAGE_GET_GRAYSCALE_PIXEL_FAST(row0, x);
                        uint32_t p1 = IMAGE_GET_GRAYSCALE_PIXEL_FAST(row1, x);
                        uint32_t p = __USAT(p0 + p1, 8);
                        IMAGE_PUT_GRAYSCALE_PIXEL_FAST(row0, x, p);
                    }
                }
            }
            break;
        }
        case PIXFORMAT_RGB565: {
            uint16_t *row0 = IMAGE_COMPUTE_RGB565_PIXEL_ROW_PTR(data->dst_img, y_row);
            uint16_t *row1 = (uint16_t *) data->dst_row_override;

            if (!mask) {
                uint32_t n = x_end - x;

                for (; n >= UINT16_VECTOR_SIZE; n -= UINT16_VECTOR_SIZE, x += UINT16_VECTOR_SIZE) {
                    vrgb_pixels_t p0 = vrgb_rgb565_to_pixels565(vldr_u16(row0 + x));
                    vrgb_pixels_t p1 = vrgb_rgb565_to_pixels565(vldr_u16(row1 + x));
                    vrgb_pixels_t p = {
                        .r = vusat_s16_narrow_u8(vadd_u32(p0.r, p1.r), 5),
                        .g = vusat_s16_narrow_u8(vadd_u32(p0.g, p1.g), 6),
                        .b = vusat_s16_narrow_u8(vadd_u32(p0.b, p1.b), 5)
                    };
                    vstr_u16(row0 + x, vrgb_pixels565_to_rgb565(p));
                }

                if (n) {
                    v128_predicate_t pred = vpredicate_16(n);
                    v2x_rows_t data = vldr_u16_pred_x2((v2x_row_ptrs_t) {.p0.u16 = row0, .p1.u16 = row1}, x, pred);
                    vrgb_pixels_t p0 = vrgb_rgb565_to_pixels565(data.r0);
                    vrgb_pixels_t p1 = vrgb_rgb565_to_pixels565(data.r1);
                    vrgb_pixels_t p = {
                        .r = vusat_s16_narrow_u8(vadd_u32(p0.r, p1.r), 5),
                        .g = vusat_s16_narrow_u8(vadd_u32(p0.g, p1.g), 6),
                        .b = vusat_s16_narrow_u8(vadd_u32(p0.b, p1.b), 5)
                    };
                    vstr_u16_pred(row0 + x, vrgb_pixels565_to_rgb565(p), pred);
                }
            } else {
                for (; x < x_end; x++) {
                    if (image_get_mask_pixel(mask, x, y_row)) {
                        uint32_t p0 = IMAGE_GET_RGB565_PIXEL_FAST(row0, x);
                        uint32_t p1 = IMAGE_GET_RGB565_PIXEL_FAST(row1, x);
                        uint32_t r = __USAT(COLOR_RGB565_TO_R5(p0) + COLOR_RGB565_TO_R5(p1), 5);
                        uint32_t g = __USAT(COLOR_RGB565_TO_G6(p0) + COLOR_RGB565_TO_G6(p1), 6);
                        uint32_t b = __USAT(COLOR_RGB565_TO_B5(p0) + COLOR_RGB565_TO_B5(p1), 5);
                        IMAGE_PUT_RGB565_PIXEL_FAST(row0, x, COLOR_R5_G6_B5_TO_RGB565(r, g, b));
                    }
                }
            }
            break;
        }
        default: {
            break;
        }
    }
}

void imlib_sub_line_op(int x, int x_end, int y_row, imlib_draw_row_data_t *data) {
    image_t *mask = (image_t *) data->callback_arg;

    switch (data->dst_img->pixfmt) {
        case PIXFORMAT_BINARY: {
            uint32_t *row0 = IMAGE_COMPUTE_BINARY_PIXEL_ROW_PTR(data->dst_img, y_row);
            uint32_t *row1 = (uint32_t *) data->dst_row_override;

            if (!mask) {
                uint32_t n = x_end - x;
                uint32_t unaligned = x % UINT32_T_BITS;

                if (unaligned) {
                    uint32_t unaligned_len = IM_MIN(UINT32_T_BITS - unaligned, n);
                    uint32_t vp0 = vldr_binary_bits(row0, x, unaligned_len);
                    uint32_t vp1 = vldr_binary_bits(row1, x, unaligned_len);
                    vstr_binary_bits(row0, x, unaligned_len, vp0 & ~vp1);
                    x += unaligned_len;
                    n -= unaligned_len;
                }

                if (UINT32_VECTOR_BITS > UINT32_T_BITS) {
                    for (uint32_t i = x / UINT32_T_BITS; n >= UINT32_VECTOR_BITS;
                         n -= UINT32_VECTOR_BITS, x += UINT32_VECTOR_BITS, i += UINT32_VECTOR_SIZE) {
                        vstr_u32(row0 + i, vbic_u32(vldr_u32(row0 + i), vldr_u32(row1 + i)));
                    }
                }

                for (uint32_t i = x / UINT32_T_BITS; n >= UINT32_T_BITS;
                     n -= UINT32_T_BITS, x += UINT32_T_BITS, i++) {
                    row0[i] = row0[i] & ~row1[i];
                }

                if (n) {
                    uint32_t vp0 = vldr_binary_bits(row0, x, n);
                    uint32_t vp1 = vldr_binary_bits(row1, x, n);
                    vstr_binary_bits(row0, x, n, vp0 & ~vp1);
                }
            } else {
                for (; x < x_end; x++) {
                    if (image_get_mask_pixel(mask, x, y_row)) {
                        uint32_t p0 = IMAGE_GET_BINARY_PIXEL_FAST(row0, x);
                        uint32_t p1 = IMAGE_GET_BINARY_PIXEL_FAST(row1, x);
                        uint32_t p = p0 & ~p1;
                        IMAGE_PUT_BINARY_PIXEL_FAST(row0, x, p);
                    }
                }
            }
            break;
        }
        case PIXFORMAT_GRAYSCALE: {
            uint8_t *row0 = IMAGE_COMPUTE_GRAYSCALE_PIXEL_ROW_PTR(data->dst_img, y_row);
            uint8_t *row1 = (uint8_t *) data->dst_row_override;

            if (!mask) {
                uint32_t n = x_end - x;

                for (; n >= UINT8_VECTOR_SIZE; n -= UINT8_VECTOR_SIZE, x += UINT8_VECTOR_SIZE) {
                    vstr_u8(row0 + x, vqsub_u8(vldr_u8(row0 + x), vldr_u8(row1 + x)));
                }

                if (n) {
                    v128_predicate_t pred = vpredicate_8(n);
                    v2x_rows_t data = vldr_u8_pred_x2((v2x_row_ptrs_t) {.p0.u8 = row0, .p1.u8 = row1}, x, pred);
                    vstr_u8_pred(row0 + x, vqsub_u8(data.r0, data.r1), pred);
                }
            } else {
                for (; x < x_end; x++) {
                    if (image_get_mask_pixel(mask, x, y_row)) {
                        uint32_t p0 = IMAGE_GET_GRAYSCALE_PIXEL_FAST(row0, x);
                        uint32_t p1 = IMAGE_GET_GRAYSCALE_PIXEL_FAST(row1, x);
                        uint32_t p = __USAT(((int32_t) p0) - ((int32_t) p1), 8);
                        IMAGE_PUT_GRAYSCALE_PIXEL_FAST(row0, x, p);
                    }
                }
            }
            break;
        }
        case PIXFORMAT_RGB565: {
            uint16_t *row0 = IMAGE_COMPUTE_RGB565_PIXEL_ROW_PTR(data->dst_img, y_row);
            uint16_t *row1 = (uint16_t *) data->dst_row_override;

            if (!mask) {
                uint32_t n = x_end - x;

                for (; n >= UINT16_VECTOR_SIZE; n -= UINT16_VECTOR_SIZE, x += UINT16_VECTOR_SIZE) {
                    vrgb_pixels_t p0 = vrgb_rgb565_to_pixels565(vldr_u16(row0 + x));
                    vrgb_pixels_t p1 = vrgb_rgb565_to_pixels565(vldr_u16(row1 + x));
                    vrgb_pixels_t p = {
                        .r = vusat_s16_narrow_u8(vsub_s16(p0.r, p1.r), 5),
                        .g = vusat_s16_narrow_u8(vsub_s16(p0.g, p1.g), 6),
                        .b = vusat_s16_narrow_u8(vsub_s16(p0.b, p1.b), 5)
                    };
                    vstr_u16(row0 + x, vrgb_pixels565_to_rgb565(p));
                }

                if (n) {
                    v128_predicate_t pred = vpredicate_16(n);
                    v2x_rows_t data = vldr_u16_pred_x2((v2x_row_ptrs_t) {.p0.u16 = row0, .p1.u16 = row1}, x, pred);
                    vrgb_pixels_t p0 = vrgb_rgb565_to_pixels565(data.r0);
                    vrgb_pixels_t p1 = vrgb_rgb565_to_pixels565(data.r1);
                    vrgb_pixels_t p = {
                        .r = vusat_s16_narrow_u8(vsub_s16(p0.r, p1.r), 5),
                        .g = vusat_s16_narrow_u8(vsub_s16(p0.g, p1.g), 6),
                        .b = vusat_s16_narrow_u8(vsub_s16(p0.b, p1.b), 5)
                    };
                    vstr_u16_pred(row0 + x, vrgb_pixels565_to_rgb565(p), pred);
                }
            } else {
                for (; x < x_end; x++) {
                    if (image_get_mask_pixel(mask, x, y_row)) {
                        uint32_t p0 = IMAGE_GET_RGB565_PIXEL_FAST(row0, x);
                        uint32_t p1 = IMAGE_GET_RGB565_PIXEL_FAST(row1, x);
                        uint32_t r = __USAT(((int32_t) COLOR_RGB565_TO_R5(p0)) - ((int32_t) COLOR_RGB565_TO_R5(p1)), 5);
                        uint32_t g = __USAT(((int32_t) COLOR_RGB565_TO_G6(p0)) - ((int32_t) COLOR_RGB565_TO_G6(p1)), 6);
                        uint32_t b = __USAT(((int32_t) COLOR_RGB565_TO_B5(p0)) - ((int32_t) COLOR_RGB565_TO_B5(p1)), 5);
                        IMAGE_PUT_RGB565_PIXEL_FAST(row0, x, COLOR_R5_G6_B5_TO_RGB565(r, g, b));
                    }
                }
            }
            break;
        }
        default: {
            break;
        }
    }
}

void imlib_rsub_line_op(int x, int x_end, int y_row, imlib_draw_row_data_t *data) {
    image_t *mask = (image_t *) data->callback_arg;

    switch (data->dst_img->pixfmt) {
        case PIXFORMAT_BINARY: {
            uint32_t *row0 = IMAGE_COMPUTE_BINARY_PIXEL_ROW_PTR(data->dst_img, y_row);
            uint32_t *row1 = (uint32_t *) data->dst_row_override;

            if (!mask) {
                uint32_t n = x_end - x;
                uint32_t unaligned = x % UINT32_T_BITS;

                if (unaligned) {
                    uint32_t unaligned_len = IM_MIN(UINT32_T_BITS - unaligned, n);
                    uint32_t vp0 = vldr_binary_bits(row0, x, unaligned_len);
                    uint32_t vp1 = vldr_binary_bits(row1, x, unaligned_len);
                    vstr_binary_bits(row0, x, unaligned_len, vp1 & ~vp0);
                    x += unaligned_len;
                    n -= unaligned_len;
                }

                if (UINT32_VECTOR_BITS > UINT32_T_BITS) {
                    for (uint32_t i = x / UINT32_T_BITS; n >= UINT32_VECTOR_BITS;
                         n -= UINT32_VECTOR_BITS, x += UINT32_VECTOR_BITS, i += UINT32_VECTOR_SIZE) {
                        vstr_u32(row0 + i, vbic_u32(vldr_u32(row1 + i), vldr_u32(row0 + i)));
                    }
                }

                for (uint32_t i = x / UINT32_T_BITS; n >= UINT32_T_BITS;
                     n -= UINT32_T_BITS, x += UINT32_T_BITS, i++) {
                    row0[i] = row1[i] & ~row0[i];
                }

                if (n) {
                    uint32_t vp0 = vldr_binary_bits(row0, x, n);
                    uint32_t vp1 = vldr_binary_bits(row1, x, n);
                    vstr_binary_bits(row0, x, n, vp1 & ~vp0);
                }
            } else {
                for (; x < x_end; x++) {
                    if (image_get_mask_pixel(mask, x, y_row)) {
                        uint32_t p0 = IMAGE_GET_BINARY_PIXEL_FAST(row0, x);
                        uint32_t p1 = IMAGE_GET_BINARY_PIXEL_FAST(row1, x);
                        uint32_t p = p1 & ~p0;
                        IMAGE_PUT_BINARY_PIXEL_FAST(row0, x, p);
                    }
                }
            }
            break;
        }
        case PIXFORMAT_GRAYSCALE: {
            uint8_t *row0 = IMAGE_COMPUTE_GRAYSCALE_PIXEL_ROW_PTR(data->dst_img, y_row);
            uint8_t *row1 = (uint8_t *) data->dst_row_override;

            if (!mask) {
                uint32_t n = x_end - x;

                for (; n >= UINT8_VECTOR_SIZE; n -= UINT8_VECTOR_SIZE, x += UINT8_VECTOR_SIZE) {
                    vstr_u8(row0 + x, vqsub_u8(vldr_u8(row1 + x), vldr_u8(row0 + x)));
                }

                if (n) {
                    v128_predicate_t pred = vpredicate_8(n);
                    v2x_rows_t data = vldr_u8_pred_x2((v2x_row_ptrs_t) {.p0.u8 = row0, .p1.u8 = row1}, x, pred);
                    vstr_u8_pred(row0 + x, vqsub_u8(data.r1, data.r0), pred);
                }
            } else {
                for (; x < x_end; x++) {
                    if (image_get_mask_pixel(mask, x, y_row)) {
                        uint32_t p0 = IMAGE_GET_GRAYSCALE_PIXEL_FAST(row0, x);
                        uint32_t p1 = IMAGE_GET_GRAYSCALE_PIXEL_FAST(row1, x);
                        uint32_t p = __USAT(((int32_t) p1) - ((int32_t) p0), 8);
                        IMAGE_PUT_GRAYSCALE_PIXEL_FAST(row0, x, p);
                    }
                }
            }
            break;
        }
        case PIXFORMAT_RGB565: {
            uint16_t *row0 = IMAGE_COMPUTE_RGB565_PIXEL_ROW_PTR(data->dst_img, y_row);
            uint16_t *row1 = (uint16_t *) data->dst_row_override;

            if (!mask) {
                uint32_t n = x_end - x;

                for (; n >= UINT16_VECTOR_SIZE; n -= UINT16_VECTOR_SIZE, x += UINT16_VECTOR_SIZE) {
                    vrgb_pixels_t p0 = vrgb_rgb565_to_pixels565(vldr_u16(row0 + x));
                    vrgb_pixels_t p1 = vrgb_rgb565_to_pixels565(vldr_u16(row1 + x));
                    vrgb_pixels_t p = {
                        .r = vusat_s16_narrow_u8(vsub_s16(p1.r, p0.r), 5),
                        .g = vusat_s16_narrow_u8(vsub_s16(p1.g, p0.g), 6),
                        .b = vusat_s16_narrow_u8(vsub_s16(p1.b, p0.b), 5)
                    };
                    vstr_u16(row0 + x, vrgb_pixels565_to_rgb565(p));
                }

                if (n) {
                    v128_predicate_t pred = vpredicate_16(n);
                    v2x_rows_t data = vldr_u16_pred_x2((v2x_row_ptrs_t) {.p0.u16 = row0, .p1.u16 = row1}, x, pred);
                    vrgb_pixels_t p0 = vrgb_rgb565_to_pixels565(data.r0);
                    vrgb_pixels_t p1 = vrgb_rgb565_to_pixels565(data.r1);
                    vrgb_pixels_t p = {
                        .r = vusat_s16_narrow_u8(vsub_s16(p1.r, p0.r), 5),
                        .g = vusat_s16_narrow_u8(vsub_s16(p1.g, p0.g), 6),
                        .b = vusat_s16_narrow_u8(vsub_s16(p1.b, p0.b), 5)
                    };
                    vstr_u16_pred(row0 + x, vrgb_pixels565_to_rgb565(p), pred);
                }
            } else {
                for (; x < x_end; x++) {
                    if (image_get_mask_pixel(mask, x, y_row)) {
                        uint32_t p0 = IMAGE_GET_RGB565_PIXEL_FAST(row0, x);
                        uint32_t p1 = IMAGE_GET_RGB565_PIXEL_FAST(row1, x);
                        uint32_t r = __USAT(((int32_t) COLOR_RGB565_TO_R5(p1)) - ((int32_t) COLOR_RGB565_TO_R5(p0)), 5);
                        uint32_t g = __USAT(((int32_t) COLOR_RGB565_TO_G6(p1)) - ((int32_t) COLOR_RGB565_TO_G6(p0)), 6);
                        uint32_t b = __USAT(((int32_t) COLOR_RGB565_TO_B5(p1)) - ((int32_t) COLOR_RGB565_TO_B5(p0)), 5);
                        IMAGE_PUT_RGB565_PIXEL_FAST(row0, x, COLOR_R5_G6_B5_TO_RGB565(r, g, b));
                    }
                }
            }
            break;
        }
        default: {
            break;
        }
    }
}

void imlib_min_line_op(int x, int x_end, int y_row, imlib_draw_row_data_t *data) {
    image_t *mask = (image_t *) data->callback_arg;

    switch (data->dst_img->pixfmt) {
        case PIXFORMAT_BINARY: {
            imlib_b_and_line_op(x, x_end, y_row, data);
        }
        case PIXFORMAT_GRAYSCALE: {
            uint8_t *row0 = IMAGE_COMPUTE_GRAYSCALE_PIXEL_ROW_PTR(data->dst_img, y_row);
            uint8_t *row1 = (uint8_t *) data->dst_row_override;

            if (!mask) {
                uint32_t n = x_end - x;

                for (; n >= UINT8_VECTOR_SIZE; n -= UINT8_VECTOR_SIZE, x += UINT8_VECTOR_SIZE) {
                    vstr_u8(row0 + x, vmin_u8(vldr_u8(row1 + x), vldr_u8(row0 + x)));
                }

                if (n) {
                    v128_predicate_t pred = vpredicate_8(n);
                    v2x_rows_t data = vldr_u8_pred_x2((v2x_row_ptrs_t) {.p0.u8 = row0, .p1.u8 = row1}, x, pred);
                    vstr_u8_pred(row0 + x, vmin_u8(data.r1, data.r0), pred);
                }
            } else {
                for (; x < x_end; x++) {
                    if (image_get_mask_pixel(mask, x, y_row)) {
                        uint32_t p0 = IMAGE_GET_GRAYSCALE_PIXEL_FAST(row0, x);
                        uint32_t p1 = IMAGE_GET_GRAYSCALE_PIXEL_FAST(row1, x);
                        uint32_t p = IM_MIN(p0, p1);
                        IMAGE_PUT_GRAYSCALE_PIXEL_FAST(row0, x, p);
                    }
                }
            }
            break;
        }
        case PIXFORMAT_RGB565: {
            uint16_t *row0 = IMAGE_COMPUTE_RGB565_PIXEL_ROW_PTR(data->dst_img, y_row);
            uint16_t *row1 = (uint16_t *) data->dst_row_override;

            if (!mask) {
                uint32_t n = x_end - x;

                for (; n >= UINT16_VECTOR_SIZE; n -= UINT16_VECTOR_SIZE, x += UINT16_VECTOR_SIZE) {
                    v128_t p0 = vldr_u16(row0 + x);
                    v128_t p1 = vldr_u16(row1 + x);
                    v128_t r0_rb = vand_u32(p0, vdup_u16(0xf81f));
                    v128_t r1_rb = vand_u32(p1, vdup_u16(0xf81f));
                    v128_t r_rb = vmin_u8(r0_rb, r1_rb);
                    v128_t r0_g = vand_u32(p0, vdup_u16(0x07e0));
                    v128_t r1_g = vand_u32(p1, vdup_u16(0x07e0));
                    v128_t r_g = vmin_u16(r0_g, r1_g);
                    vstr_u16(row0 + x, vorr_u32(r_rb, r_g));
                }

                if (n) {
                    v128_predicate_t pred = vpredicate_16(n);
                    v2x_rows_t data = vldr_u16_pred_x2((v2x_row_ptrs_t) {.p0.u16 = row0, .p1.u16 = row1}, x, pred);
                    v128_t r0_rb = vand_u32(data.r0, vdup_u16(0xf81f));
                    v128_t r1_rb = vand_u32(data.r1, vdup_u16(0xf81f));
                    v128_t r_rb = vmin_u8(r0_rb, r1_rb);
                    v128_t r0_g = vand_u32(data.r0, vdup_u16(0x07e0));
                    v128_t r1_g = vand_u32(data.r1, vdup_u16(0x07e0));
                    v128_t r_g = vmin_u16(r0_g, r1_g);
                    vstr_u16_pred(row0 + x, vorr_u32(r_rb, r_g), pred);
                }
            } else {
                for (; x < x_end; x++) {
                    if (image_get_mask_pixel(mask, x, y_row)) {
                        uint32_t p0 = IMAGE_GET_RGB565_PIXEL_FAST(row0, x);
                        uint32_t p1 = IMAGE_GET_RGB565_PIXEL_FAST(row1, x);
                        uint32_t r = IM_MIN(COLOR_RGB565_TO_R5(p0), COLOR_RGB565_TO_R5(p1));
                        uint32_t g = IM_MIN(COLOR_RGB565_TO_G6(p0), COLOR_RGB565_TO_G6(p1));
                        uint32_t b = IM_MIN(COLOR_RGB565_TO_B5(p0), COLOR_RGB565_TO_B5(p1));
                        IMAGE_PUT_RGB565_PIXEL_FAST(row0, x, COLOR_R5_G6_B5_TO_RGB565(r, g, b));
                    }
                }
            }
            break;
        }
        default: {
            break;
        }
    }
}

void imlib_max_line_op(int x, int x_end, int y_row, imlib_draw_row_data_t *data) {
    image_t *mask = (image_t *) data->callback_arg;

    switch (data->dst_img->pixfmt) {
        case PIXFORMAT_BINARY: {
            imlib_b_or_line_op(x, x_end, y_row, data);
            break;
        }
        case PIXFORMAT_GRAYSCALE: {
            uint8_t *row0 = IMAGE_COMPUTE_GRAYSCALE_PIXEL_ROW_PTR(data->dst_img, y_row);
            uint8_t *row1 = (uint8_t *) data->dst_row_override;

            if (!mask) {
                uint32_t n = x_end - x;

                for (; n >= UINT8_VECTOR_SIZE; n -= UINT8_VECTOR_SIZE, x += UINT8_VECTOR_SIZE) {
                    vstr_u8(row0 + x, vmax_u8(vldr_u8(row1 + x), vldr_u8(row0 + x)));
                }

                if (n) {
                    v128_predicate_t pred = vpredicate_8(n);
                    v2x_rows_t data = vldr_u8_pred_x2((v2x_row_ptrs_t) {.p0.u8 = row0, .p1.u8 = row1}, x, pred);
                    vstr_u8_pred(row0 + x, vmax_u8(data.r1, data.r0), pred);
                }
            } else {
                for (; x < x_end; x++) {
                    if (image_get_mask_pixel(mask, x, y_row)) {
                        uint32_t p0 = IMAGE_GET_GRAYSCALE_PIXEL_FAST(row0, x);
                        uint32_t p1 = IMAGE_GET_GRAYSCALE_PIXEL_FAST(row1, x);
                        uint32_t p = IM_MAX(p0, p1);
                        IMAGE_PUT_GRAYSCALE_PIXEL_FAST(row0, x, p);
                    }
                }
            }
            break;
        }
        case PIXFORMAT_RGB565: {
            uint16_t *row0 = IMAGE_COMPUTE_RGB565_PIXEL_ROW_PTR(data->dst_img, y_row);
            uint16_t *row1 = (uint16_t *) data->dst_row_override;

            if (!mask) {
                uint32_t n = x_end - x;

                for (; n >= UINT16_VECTOR_SIZE; n -= UINT16_VECTOR_SIZE, x += UINT16_VECTOR_SIZE) {
                    v128_t p0 = vldr_u16(row0 + x);
                    v128_t p1 = vldr_u16(row1 + x);
                    v128_t r0_rb = vand_u32(p0, vdup_u16(0xf81f));
                    v128_t r1_rb = vand_u32(p1, vdup_u16(0xf81f));
                    v128_t r_rb = vmax_u8(r0_rb, r1_rb);
                    v128_t r0_g = vand_u32(p0, vdup_u16(0x07e0));
                    v128_t r1_g = vand_u32(p1, vdup_u16(0x07e0));
                    v128_t r_g = vmax_u16(r0_g, r1_g);
                    vstr_u16(row0 + x, vorr_u32(r_rb, r_g));
                }

                if (n) {
                    v128_predicate_t pred = vpredicate_16(n);
                    v2x_rows_t data = vldr_u16_pred_x2((v2x_row_ptrs_t) {.p0.u16 = row0, .p1.u16 = row1}, x, pred);
                    v128_t r0_rb = vand_u32(data.r0, vdup_u16(0xf81f));
                    v128_t r1_rb = vand_u32(data.r1, vdup_u16(0xf81f));
                    v128_t r_rb = vmax_u8(r0_rb, r1_rb);
                    v128_t r0_g = vand_u32(data.r0, vdup_u16(0x07e0));
                    v128_t r1_g = vand_u32(data.r1, vdup_u16(0x07e0));
                    v128_t r_g = vmax_u16(r0_g, r1_g);
                    vstr_u16_pred(row0 + x, vorr_u32(r_rb, r_g), pred);
                }
            } else {
                for (; x < x_end; x++) {
                    if (image_get_mask_pixel(mask, x, y_row)) {
                        uint32_t p0 = IMAGE_GET_RGB565_PIXEL_FAST(row0, x);
                        uint32_t p1 = IMAGE_GET_RGB565_PIXEL_FAST(row1, x);
                        uint32_t r = IM_MAX(COLOR_RGB565_TO_R5(p0), COLOR_RGB565_TO_R5(p1));
                        uint32_t g = IM_MAX(COLOR_RGB565_TO_G6(p0), COLOR_RGB565_TO_G6(p1));
                        uint32_t b = IM_MAX(COLOR_RGB565_TO_B5(p0), COLOR_RGB565_TO_B5(p1));
                        IMAGE_PUT_RGB565_PIXEL_FAST(row0, x, COLOR_R5_G6_B5_TO_RGB565(r, g, b));
                    }
                }
            }
            break;
        }
        default: {
            break;
        }
    }
}

void imlib_difference_line_op(int x, int x_end, int y_row, imlib_draw_row_data_t *data) {
    image_t *mask = (image_t *) data->callback_arg;

    switch (data->dst_img->pixfmt) {
        case PIXFORMAT_BINARY: {
            imlib_b_xor_line_op(x, x_end, y_row, data);
            break;
        }
        case PIXFORMAT_GRAYSCALE: {
            uint8_t *row0 = IMAGE_COMPUTE_GRAYSCALE_PIXEL_ROW_PTR(data->dst_img, y_row);
            uint8_t *row1 = (uint8_t *) data->dst_row_override;

            if (!mask) {
                uint32_t n = x_end - x;

                for (; n >= UINT8_VECTOR_SIZE; n -= UINT8_VECTOR_SIZE, x += UINT8_VECTOR_SIZE) {
                    vstr_u8(row0 + x, vabd_u8(vldr_u8(row1 + x), vldr_u8(row0 + x)));
                }

                if (n) {
                    v128_predicate_t pred = vpredicate_8(n);
                    v2x_rows_t data = vldr_u8_pred_x2((v2x_row_ptrs_t) {.p0.u8 = row0, .p1.u8 = row1}, x, pred);
                    vstr_u8_pred(row0 + x, vabd_u8(data.r1, data.r0), pred);
                }
            } else {
                for (; x < x_end; x++) {
                    if (image_get_mask_pixel(mask, x, y_row)) {
                        uint32_t p0 = IMAGE_GET_GRAYSCALE_PIXEL_FAST(row0, x);
                        uint32_t p1 = IMAGE_GET_GRAYSCALE_PIXEL_FAST(row1, x);
                        uint32_t p = abs(((int32_t) p0) - ((int32_t) p1));
                        IMAGE_PUT_GRAYSCALE_PIXEL_FAST(row0, x, p);
                    }
                }
            }
            break;
        }
        case PIXFORMAT_RGB565: {
            uint16_t *row0 = IMAGE_COMPUTE_RGB565_PIXEL_ROW_PTR(data->dst_img, y_row);
            uint16_t *row1 = (uint16_t *) data->dst_row_override;

            if (!mask) {
                uint32_t n = x_end - x;

                for (; n >= UINT16_VECTOR_SIZE; n -= UINT16_VECTOR_SIZE, x += UINT16_VECTOR_SIZE) {
                    v128_t p0 = vldr_u16(row0 + x);
                    v128_t p1 = vldr_u16(row1 + x);
                    v128_t r0_rb = vand_u32(p0, vdup_u16(0xf81f));
                    v128_t r1_rb = vand_u32(p1, vdup_u16(0xf81f));
                    v128_t r_rb = vabd_u8(r0_rb, r1_rb);
                    v128_t r0_g = vand_u32(p0, vdup_u16(0x07e0));
                    v128_t r1_g = vand_u32(p1, vdup_u16(0x07e0));
                    v128_t r_g = vabd_u16(r0_g, r1_g);
                    vstr_u16(row0 + x, vorr_u32(r_rb, r_g));
                }

                if (n) {
                    v128_predicate_t pred = vpredicate_16(n);
                    v2x_rows_t data = vldr_u16_pred_x2((v2x_row_ptrs_t) {.p0.u16 = row0, .p1.u16 = row1}, x, pred);
                    v128_t r0_rb = vand_u32(data.r0, vdup_u16(0xf81f));
                    v128_t r1_rb = vand_u32(data.r1, vdup_u16(0xf81f));
                    v128_t r_rb = vabd_u8(r0_rb, r1_rb);
                    v128_t r0_g = vand_u32(data.r0, vdup_u16(0x07e0));
                    v128_t r1_g = vand_u32(data.r1, vdup_u16(0x07e0));
                    v128_t r_g = vabd_u16(r0_g, r1_g);
                    vstr_u16_pred(row0 + x, vorr_u32(r_rb, r_g), pred);
                }
            } else {
                for (; x < x_end; x++) {
                    if (image_get_mask_pixel(mask, x, y_row)) {
                        uint32_t p0 = IMAGE_GET_RGB565_PIXEL_FAST(row0, x);
                        uint32_t p1 = IMAGE_GET_RGB565_PIXEL_FAST(row1, x);
                        uint32_t r = abs(((int32_t) COLOR_RGB565_TO_R5(p0)) - ((int32_t) COLOR_RGB565_TO_R5(p1)));
                        uint32_t g = abs(((int32_t) COLOR_RGB565_TO_G6(p0)) - ((int32_t) COLOR_RGB565_TO_G6(p1)));
                        uint32_t b = abs(((int32_t) COLOR_RGB565_TO_B5(p0)) - ((int32_t) COLOR_RGB565_TO_B5(p1)));
                        IMAGE_PUT_RGB565_PIXEL_FAST(row0, x, COLOR_R5_G6_B5_TO_RGB565(r, g, b));
                    }
                }
            }
            break;
        }
        default: {
            break;
        }
    }
}
#endif // IMLIB_ENABLE_MATH_OPS
