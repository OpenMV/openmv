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
 * Binary image operations.
 */
#include "imlib.h"

void imlib_zero_line_op(int x, int x_end, int y_row, imlib_draw_row_data_t *data) {
    image_t *mask = data->callback_arg;

    switch (data->dst_img->pixfmt) {
        case PIXFORMAT_BINARY: {
            uint32_t *row = IMAGE_COMPUTE_BINARY_PIXEL_ROW_PTR(data->dst_img, y_row);

            if (!mask) {
                for (; x < x_end; x++) {
                    if (IMAGE_GET_BINARY_PIXEL_FAST(((uint32_t *) data->dst_row_override), x)) {
                        IMAGE_CLEAR_BINARY_PIXEL_FAST(row, x);
                    }
                }
            } else {
                for (; x < x_end; x++) {
                    if (image_get_mask_pixel(mask, x, y_row) &&
                        IMAGE_GET_BINARY_PIXEL_FAST(((uint32_t *) data->dst_row_override), x)) {
                        IMAGE_CLEAR_BINARY_PIXEL_FAST(row, x);
                    }
                }
            }
            break;
        }
        case PIXFORMAT_GRAYSCALE: {
            uint8_t *row = IMAGE_COMPUTE_GRAYSCALE_PIXEL_ROW_PTR(data->dst_img, y_row);

            if (!mask) {
                for (; x < x_end; x++) {
                    if (IMAGE_GET_GRAYSCALE_PIXEL_FAST(((uint8_t *) data->dst_row_override), x)) {
                        IMAGE_PUT_GRAYSCALE_PIXEL_FAST(row, x, 0);
                    }
                }
            } else {
                for (; x < x_end; x++) {
                    if (image_get_mask_pixel(mask, x, y_row) &&
                        IMAGE_GET_GRAYSCALE_PIXEL_FAST(((uint8_t *) data->dst_row_override), x)) {
                        IMAGE_PUT_GRAYSCALE_PIXEL_FAST(row, x, 0);
                    }
                }
            }
            break;
        }
        case PIXFORMAT_RGB565: {
            uint16_t *row = IMAGE_COMPUTE_RGB565_PIXEL_ROW_PTR(data->dst_img, y_row);

            if (!mask) {
                for (; x < x_end; x++) {
                    if (IMAGE_GET_RGB565_PIXEL_FAST(((uint16_t *) data->dst_row_override), x)) {
                        IMAGE_PUT_RGB565_PIXEL_FAST(row, x, 0);
                    }
                }
            } else {
                for (; x < x_end; x++) {
                    if (image_get_mask_pixel(mask, x, y_row) &&
                        IMAGE_GET_RGB565_PIXEL_FAST(((uint16_t *) data->dst_row_override), x)) {
                        IMAGE_PUT_RGB565_PIXEL_FAST(row, x, 0);
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

void imlib_mask_line_op(int x, int x_end, int y_row, imlib_draw_row_data_t *data) {
    image_t *mask = data->callback_arg;

    switch (data->dst_img->pixfmt) {
        case PIXFORMAT_BINARY: {
            uint32_t *row = IMAGE_COMPUTE_BINARY_PIXEL_ROW_PTR(data->dst_img, y_row);
            for (; x < x_end; x++) {
                if (image_get_mask_pixel(mask, x, y_row)) {
                    int otherPixel = IMAGE_GET_BINARY_PIXEL_FAST(((uint32_t *) data->dst_row_override), x);
                    IMAGE_PUT_BINARY_PIXEL_FAST(row, x, otherPixel);
                }
            }
            break;
        }
        case PIXFORMAT_GRAYSCALE: {
            uint8_t *row = IMAGE_COMPUTE_GRAYSCALE_PIXEL_ROW_PTR(data->dst_img, y_row);
            for (; x < x_end; x++) {
                if (image_get_mask_pixel(mask, x, y_row)) {
                    int otherPixel = IMAGE_GET_GRAYSCALE_PIXEL_FAST(((uint8_t *) data->dst_row_override), x);
                    IMAGE_PUT_GRAYSCALE_PIXEL_FAST(row, x, otherPixel);
                }
            }
            break;
        }
        case PIXFORMAT_RGB565: {
            uint16_t *row = IMAGE_COMPUTE_RGB565_PIXEL_ROW_PTR(data->dst_img, y_row);
            for (; x < x_end; x++) {
                if (image_get_mask_pixel(mask, x, y_row)) {
                    int otherPixel = IMAGE_GET_RGB565_PIXEL_FAST(((uint16_t *) data->dst_row_override), x);
                    IMAGE_PUT_RGB565_PIXEL_FAST(row, x, otherPixel);
                }
            }
            break;
        }
        default: {
            break;
        }
    }
}

#ifdef IMLIB_ENABLE_BINARY_OPS
void imlib_binary(image_t *out, image_t *img, list_t *thresholds, bool invert, bool zero, image_t *mask) {
    image_t bmp;
    bmp.w = img->w;
    bmp.h = img->h;
    bmp.pixfmt = PIXFORMAT_BINARY;
    bmp.data = fb_alloc0(image_size(&bmp), FB_ALLOC_NO_HINT);

    list_for_each(it, thresholds) {
        color_thresholds_list_lnk_data_t *lnk_data = list_get_data(it);

        switch (img->pixfmt) {
            case PIXFORMAT_BINARY: {
                for (int y = 0, yy = img->h; y < yy; y++) {
                    uint32_t *old_row_ptr = IMAGE_COMPUTE_BINARY_PIXEL_ROW_PTR(img, y);
                    uint32_t *bmp_row_ptr = IMAGE_COMPUTE_BINARY_PIXEL_ROW_PTR(&bmp, y);
                    for (int x = 0, xx = img->w; x < xx; x++) {
                        if (COLOR_THRESHOLD_BINARY(IMAGE_GET_BINARY_PIXEL_FAST(old_row_ptr, x), lnk_data, invert)) {
                            IMAGE_SET_BINARY_PIXEL_FAST(bmp_row_ptr, x);
                        }
                    }
                }
                break;
            }
            case PIXFORMAT_GRAYSCALE: {
                for (int y = 0, yy = img->h; y < yy; y++) {
                    uint8_t *old_row_ptr = IMAGE_COMPUTE_GRAYSCALE_PIXEL_ROW_PTR(img, y);
                    uint32_t *bmp_row_ptr = IMAGE_COMPUTE_BINARY_PIXEL_ROW_PTR(&bmp, y);
                    for (int x = 0, xx = img->w; x < xx; x++) {
                        if (COLOR_THRESHOLD_GRAYSCALE(IMAGE_GET_GRAYSCALE_PIXEL_FAST(old_row_ptr, x), lnk_data, invert)) {
                            IMAGE_SET_BINARY_PIXEL_FAST(bmp_row_ptr, x);
                        }
                    }
                }
                break;
            }
            case PIXFORMAT_RGB565: {
                for (int y = 0, yy = img->h; y < yy; y++) {
                    uint16_t *old_row_ptr = IMAGE_COMPUTE_RGB565_PIXEL_ROW_PTR(img, y);
                    uint32_t *bmp_row_ptr = IMAGE_COMPUTE_BINARY_PIXEL_ROW_PTR(&bmp, y);
                    for (int x = 0, xx = img->w; x < xx; x++) {
                        if (COLOR_THRESHOLD_RGB565(IMAGE_GET_RGB565_PIXEL_FAST(old_row_ptr, x), lnk_data, invert)) {
                            IMAGE_SET_BINARY_PIXEL_FAST(bmp_row_ptr, x);
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

    imlib_draw_row_callback_t callback = NULL;
    if (zero) {
        callback = imlib_zero_line_op;
    }

    if ((!callback) && mask) {
        callback = imlib_mask_line_op;
    }

    void *dst_row_override = NULL;
    if (callback) {
        dst_row_override = fb_alloc0(image_line_size(out), FB_ALLOC_CACHE_ALIGN);
    }

    imlib_draw_image(out, &bmp, 0, 0, 1.0f, 1.0f, NULL, -1, 255, NULL, NULL, 0, callback, mask, dst_row_override);

    if (dst_row_override) {
        fb_free(); // dst_row_override
    }

    fb_free(); // bmp.data
}

void imlib_invert(image_t *img) {
    uint32_t n = image_size(img);
    uint32_t *p = (uint32_t *) img->data;

    for (; n >= UINT8_VECTOR_SIZE; n -= UINT8_VECTOR_SIZE, p += UINT32_VECTOR_SIZE) {
        vstr_u32(p, vmvn_u32(vldr_u32(p)));
    }

    v128_predicate_t pred = vpredicate_8(n);
    vstr_u8_pred((uint8_t *) p, vmvn_u32(vldr_u8_pred((uint8_t *) p, pred)), pred);
}

void imlib_b_and_line_op(int x, int x_end, int y_row, imlib_draw_row_data_t *data) {
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
                    vstr_binary_bits(row0, x, unaligned_len, vp0 & vp1);
                    x += unaligned_len;
                    n -= unaligned_len;
                }

                if (UINT32_VECTOR_BITS > UINT32_T_BITS) {
                    for (uint32_t i = x / UINT32_T_BITS; n >= UINT32_VECTOR_BITS;
                         n -= UINT32_VECTOR_BITS, x += UINT32_VECTOR_BITS, i += UINT32_VECTOR_SIZE) {
                        vstr_u32(row0 + i, vand_u32(vldr_u32(row0 + i), vldr_u32(row1 + i)));
                    }
                }

                for (uint32_t i = x / UINT32_T_BITS; n >= UINT32_T_BITS;
                     n -= UINT32_T_BITS, x += UINT32_T_BITS, i++) {
                    row0[i] = row0[i] & row1[i];
                }

                if (n) {
                    uint32_t vp0 = vldr_binary_bits(row0, x, n);
                    uint32_t vp1 = vldr_binary_bits(row1, x, n);
                    vstr_binary_bits(row0, x, n, vp0 & vp1);
                }
            } else {
                for (; x < x_end; x++) {
                    if (image_get_mask_pixel(mask, x, y_row)) {
                        uint32_t p0 = IMAGE_GET_BINARY_PIXEL_FAST(row0, x);
                        uint32_t p1 = IMAGE_GET_BINARY_PIXEL_FAST(row1, x);
                        uint32_t p = p0 & p1;
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
                    vstr_u8(row0 + x, vand_u32(vldr_u8(row0 + x), vldr_u8(row1 + x)));
                }

                if (n) {
                    v128_predicate_t pred = vpredicate_8(n);
                    v2x_rows_t data = vldr_u8_pred_x2((v2x_row_ptrs_t) {.p0.u8 = row0, .p1.u8 = row1}, x, pred);
                    vstr_u8_pred(row0 + x, vand_u32(data.r0, data.r1), pred);
                }
            } else {
                for (; x < x_end; x++) {
                    if (image_get_mask_pixel(mask, x, y_row)) {
                        uint32_t p0 = IMAGE_GET_GRAYSCALE_PIXEL_FAST(row0, x);
                        uint32_t p1 = IMAGE_GET_GRAYSCALE_PIXEL_FAST(row1, x);
                        uint32_t p = p0 & p1;
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
                    vstr_u16(row0 + x, vand_u32(vldr_u16(row0 + x), vldr_u16(row1 + x)));
                }

                if (n) {
                    v128_predicate_t pred = vpredicate_16(n);
                    v2x_rows_t data = vldr_u16_pred_x2((v2x_row_ptrs_t) {.p0.u16 = row0, .p1.u16 = row1}, x, pred);
                    vstr_u16_pred(row0 + x, vand_u32(data.r0, data.r1), pred);
                }
            } else {
                for (; x < x_end; x++) {
                    if (image_get_mask_pixel(mask, x, y_row)) {
                        uint32_t p0 = IMAGE_GET_RGB565_PIXEL_FAST(row0, x);
                        uint32_t p1 = IMAGE_GET_RGB565_PIXEL_FAST(row1, x);
                        uint32_t p = p0 & p1;
                        IMAGE_PUT_RGB565_PIXEL_FAST(row0, x, p);
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

void imlib_b_nand_line_op(int x, int x_end, int y_row, imlib_draw_row_data_t *data) {
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
                    vstr_binary_bits(row0, x, unaligned_len, ~(vp0 & vp1));
                    x += unaligned_len;
                    n -= unaligned_len;
                }

                if (UINT32_VECTOR_BITS > UINT32_T_BITS) {
                    for (uint32_t i = x / UINT32_T_BITS; n >= UINT32_VECTOR_BITS;
                         n -= UINT32_VECTOR_BITS, x += UINT32_VECTOR_BITS, i += UINT32_VECTOR_SIZE) {
                        vstr_u32(row0 + i, vmvn_u32(vand_u32(vldr_u32(row0 + i), vldr_u32(row1 + i))));
                    }
                }

                for (uint32_t i = x / UINT32_T_BITS; n >= UINT32_T_BITS;
                     n -= UINT32_T_BITS, x += UINT32_T_BITS, i++) {
                    row0[i] = ~(row0[i] & row1[i]);
                }

                if (n) {
                    uint32_t vp0 = vldr_binary_bits(row0, x, n);
                    uint32_t vp1 = vldr_binary_bits(row1, x, n);
                    vstr_binary_bits(row0, x, n, ~(vp0 & vp1));
                }
            } else {
                for (; x < x_end; x++) {
                    if (image_get_mask_pixel(mask, x, y_row)) {
                        uint32_t p0 = IMAGE_GET_BINARY_PIXEL_FAST(row0, x);
                        uint32_t p1 = IMAGE_GET_BINARY_PIXEL_FAST(row1, x);
                        uint32_t p = ~(p0 & p1);
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
                    vstr_u8(row0 + x, vmvn_u32(vand_u32(vldr_u8(row0 + x), vldr_u8(row1 + x))));
                }

                if (n) {
                    v128_predicate_t pred = vpredicate_8(n);
                    v2x_rows_t data = vldr_u8_pred_x2((v2x_row_ptrs_t) {.p0.u8 = row0, .p1.u8 = row1}, x, pred);
                    vstr_u8_pred(row0 + x, vmvn_u32(vand_u32(data.r0, data.r1)), pred);
                }
            } else {
                for (; x < x_end; x++) {
                    if (image_get_mask_pixel(mask, x, y_row)) {
                        uint32_t p0 = IMAGE_GET_GRAYSCALE_PIXEL_FAST(row0, x);
                        uint32_t p1 = IMAGE_GET_GRAYSCALE_PIXEL_FAST(row1, x);
                        uint32_t p = ~(p0 & p1);
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
                    vstr_u16(row0 + x, vmvn_u32(vand_u32(vldr_u16(row0 + x), vldr_u16(row1 + x))));
                }

                if (n) {
                    v128_predicate_t pred = vpredicate_16(n);
                    v2x_rows_t data = vldr_u16_pred_x2((v2x_row_ptrs_t) {.p0.u16 = row0, .p1.u16 = row1}, x, pred);
                    vstr_u16_pred(row0 + x, vmvn_u32(vand_u32(data.r0, data.r1)), pred);
                }
            } else {
                for (; x < x_end; x++) {
                    if (image_get_mask_pixel(mask, x, y_row)) {
                        uint32_t p0 = IMAGE_GET_RGB565_PIXEL_FAST(row0, x);
                        uint32_t p1 = IMAGE_GET_RGB565_PIXEL_FAST(row1, x);
                        uint32_t p = ~(p0 & p1);
                        IMAGE_PUT_RGB565_PIXEL_FAST(row0, x, p);
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

void imlib_b_or_line_op(int x, int x_end, int y_row, imlib_draw_row_data_t *data) {
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
                    vstr_binary_bits(row0, x, unaligned_len, vp0 | vp1);
                    x += unaligned_len;
                    n -= unaligned_len;
                }

                if (UINT32_VECTOR_BITS > UINT32_T_BITS) {
                    for (uint32_t i = x / UINT32_T_BITS; n >= UINT32_VECTOR_BITS;
                         n -= UINT32_VECTOR_BITS, x += UINT32_VECTOR_BITS, i += UINT32_VECTOR_SIZE) {
                        vstr_u32(row0 + i, vorr_u32(vldr_u32(row0 + i), vldr_u32(row1 + i)));
                    }
                }

                for (uint32_t i = x / UINT32_T_BITS; n >= UINT32_T_BITS;
                     n -= UINT32_T_BITS, x += UINT32_T_BITS, i++) {
                    row0[i] = row0[i] | row1[i];
                }

                if (n) {
                    uint32_t vp0 = vldr_binary_bits(row0, x, n);
                    uint32_t vp1 = vldr_binary_bits(row1, x, n);
                    vstr_binary_bits(row0, x, n, vp0 | vp1);
                }
            } else {
                for (; x < x_end; x++) {
                    if (image_get_mask_pixel(mask, x, y_row)) {
                        uint32_t p0 = IMAGE_GET_BINARY_PIXEL_FAST(row0, x);
                        uint32_t p1 = IMAGE_GET_BINARY_PIXEL_FAST(row1, x);
                        uint32_t p = p0 | p1;
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
                    vstr_u8(row0 + x, vorr_u32(vldr_u8(row0 + x), vldr_u8(row1 + x)));
                }

                if (n) {
                    v128_predicate_t pred = vpredicate_8(n);
                    v2x_rows_t data = vldr_u8_pred_x2((v2x_row_ptrs_t) {.p0.u8 = row0, .p1.u8 = row1}, x, pred);
                    vstr_u8_pred(row0 + x, vorr_u32(data.r0, data.r1), pred);
                }
            } else {
                for (; x < x_end; x++) {
                    if (image_get_mask_pixel(mask, x, y_row)) {
                        uint32_t p0 = IMAGE_GET_GRAYSCALE_PIXEL_FAST(row0, x);
                        uint32_t p1 = IMAGE_GET_GRAYSCALE_PIXEL_FAST(row1, x);
                        uint32_t p = p0 | p1;
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
                    vstr_u16(row0 + x, vorr_u32(vldr_u16(row0 + x), vldr_u16(row1 + x)));
                }

                if (n) {
                    v128_predicate_t pred = vpredicate_16(n);
                    v2x_rows_t data = vldr_u16_pred_x2((v2x_row_ptrs_t) {.p0.u16 = row0, .p1.u16 = row1}, x, pred);
                    vstr_u16_pred(row0 + x, vorr_u32(data.r0, data.r1), pred);
                }
            } else {
                for (; x < x_end; x++) {
                    if (image_get_mask_pixel(mask, x, y_row)) {
                        uint32_t p0 = IMAGE_GET_RGB565_PIXEL_FAST(row0, x);
                        uint32_t p1 = IMAGE_GET_RGB565_PIXEL_FAST(row1, x);
                        uint32_t p = p0 | p1;
                        IMAGE_PUT_RGB565_PIXEL_FAST(row0, x, p);
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

void imlib_b_nor_line_op(int x, int x_end, int y_row, imlib_draw_row_data_t *data) {
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
                    vstr_binary_bits(row0, x, unaligned_len, ~(vp0 | vp1));
                    x += unaligned_len;
                    n -= unaligned_len;
                }

                if (UINT32_VECTOR_BITS > UINT32_T_BITS) {
                    for (uint32_t i = x / UINT32_T_BITS; n >= UINT32_VECTOR_BITS;
                         n -= UINT32_VECTOR_BITS, x += UINT32_VECTOR_BITS, i += UINT32_VECTOR_SIZE) {
                        vstr_u32(row0 + i, vmvn_u32(vorr_u32(vldr_u32(row0 + i), vldr_u32(row1 + i))));
                    }
                }

                for (uint32_t i = x / UINT32_T_BITS; n >= UINT32_T_BITS;
                     n -= UINT32_T_BITS, x += UINT32_T_BITS, i++) {
                    row0[i] = ~(row0[i] | row1[i]);
                }

                if (n) {
                    uint32_t vp0 = vldr_binary_bits(row0, x, n);
                    uint32_t vp1 = vldr_binary_bits(row1, x, n);
                    vstr_binary_bits(row0, x, n, ~(vp0 | vp1));
                }
            } else {
                for (; x < x_end; x++) {
                    if (image_get_mask_pixel(mask, x, y_row)) {
                        uint32_t p0 = IMAGE_GET_BINARY_PIXEL_FAST(row0, x);
                        uint32_t p1 = IMAGE_GET_BINARY_PIXEL_FAST(row1, x);
                        uint32_t p = ~(p0 | p1);
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
                    vstr_u8(row0 + x, vmvn_u32(vorr_u32(vldr_u8(row0 + x), vldr_u8(row1 + x))));
                }

                if (n) {
                    v128_predicate_t pred = vpredicate_8(n);
                    v2x_rows_t data = vldr_u8_pred_x2((v2x_row_ptrs_t) {.p0.u8 = row0, .p1.u8 = row1}, x, pred);
                    vstr_u8_pred(row0 + x, vmvn_u32(vorr_u32(data.r0, data.r1)), pred);
                }
            } else {
                for (; x < x_end; x++) {
                    if (image_get_mask_pixel(mask, x, y_row)) {
                        uint32_t p0 = IMAGE_GET_GRAYSCALE_PIXEL_FAST(row0, x);
                        uint32_t p1 = IMAGE_GET_GRAYSCALE_PIXEL_FAST(row1, x);
                        uint32_t p = ~(p0 | p1);
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
                    vstr_u16(row0 + x, vmvn_u32(vorr_u32(vldr_u16(row0 + x), vldr_u16(row1 + x))));
                }

                if (n) {
                    v128_predicate_t pred = vpredicate_16(n);
                    v2x_rows_t data = vldr_u16_pred_x2((v2x_row_ptrs_t) {.p0.u16 = row0, .p1.u16 = row1}, x, pred);
                    vstr_u16_pred(row0 + x, vmvn_u32(vorr_u32(data.r0, data.r1)), pred);
                }
            } else {
                for (; x < x_end; x++) {
                    if (image_get_mask_pixel(mask, x, y_row)) {
                        uint32_t p0 = IMAGE_GET_RGB565_PIXEL_FAST(row0, x);
                        uint32_t p1 = IMAGE_GET_RGB565_PIXEL_FAST(row1, x);
                        uint32_t p = ~(p0 | p1);
                        IMAGE_PUT_RGB565_PIXEL_FAST(row0, x, p);
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

void imlib_b_xor_line_op(int x, int x_end, int y_row, imlib_draw_row_data_t *data) {
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
                    vstr_binary_bits(row0, x, unaligned_len, vp0 ^ vp1);
                    x += unaligned_len;
                    n -= unaligned_len;
                }

                if (UINT32_VECTOR_BITS > UINT32_T_BITS) {
                    for (uint32_t i = x / UINT32_T_BITS; n >= UINT32_VECTOR_BITS;
                         n -= UINT32_VECTOR_BITS, x += UINT32_VECTOR_BITS, i += UINT32_VECTOR_SIZE) {
                        vstr_u32(row0 + i, veor_u32(vldr_u32(row0 + i), vldr_u32(row1 + i)));
                    }
                }

                for (uint32_t i = x / UINT32_T_BITS; n >= UINT32_T_BITS;
                     n -= UINT32_T_BITS, x += UINT32_T_BITS, i++) {
                    row0[i] = row0[i] ^ row1[i];
                }

                if (n) {
                    uint32_t vp0 = vldr_binary_bits(row0, x, n);
                    uint32_t vp1 = vldr_binary_bits(row1, x, n);
                    vstr_binary_bits(row0, x, n, vp0 ^ vp1);
                }
            } else {
                for (; x < x_end; x++) {
                    if (image_get_mask_pixel(mask, x, y_row)) {
                        uint32_t p0 = IMAGE_GET_BINARY_PIXEL_FAST(row0, x);
                        uint32_t p1 = IMAGE_GET_BINARY_PIXEL_FAST(row1, x);
                        uint32_t p = p0 ^ p1;
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
                    vstr_u8(row0 + x, veor_u32(vldr_u8(row0 + x), vldr_u8(row1 + x)));
                }

                if (n) {
                    v128_predicate_t pred = vpredicate_8(n);
                    v2x_rows_t data = vldr_u8_pred_x2((v2x_row_ptrs_t) {.p0.u8 = row0, .p1.u8 = row1}, x, pred);
                    vstr_u8_pred(row0 + x, veor_u32(data.r0, data.r1), pred);
                }
            } else {
                for (; x < x_end; x++) {
                    if (image_get_mask_pixel(mask, x, y_row)) {
                        uint32_t p0 = IMAGE_GET_GRAYSCALE_PIXEL_FAST(row0, x);
                        uint32_t p1 = IMAGE_GET_GRAYSCALE_PIXEL_FAST(row1, x);
                        uint32_t p = p0 ^ p1;
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
                    vstr_u16(row0 + x, veor_u32(vldr_u16(row0 + x), vldr_u16(row1 + x)));
                }

                if (n) {
                    v128_predicate_t pred = vpredicate_16(n);
                    v2x_rows_t data = vldr_u16_pred_x2((v2x_row_ptrs_t) {.p0.u16 = row0, .p1.u16 = row1}, x, pred);
                    vstr_u16_pred(row0 + x, veor_u32(data.r0, data.r1), pred);
                }
            } else {
                for (; x < x_end; x++) {
                    if (image_get_mask_pixel(mask, x, y_row)) {
                        uint32_t p0 = IMAGE_GET_RGB565_PIXEL_FAST(row0, x);
                        uint32_t p1 = IMAGE_GET_RGB565_PIXEL_FAST(row1, x);
                        uint32_t p = p0 ^ p1;
                        IMAGE_PUT_RGB565_PIXEL_FAST(row0, x, p);
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

void imlib_b_xnor_line_op(int x, int x_end, int y_row, imlib_draw_row_data_t *data) {
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
                    vstr_binary_bits(row0, x, unaligned_len, ~(vp0 ^ vp1));
                    x += unaligned_len;
                    n -= unaligned_len;
                }

                if (UINT32_VECTOR_BITS > UINT32_T_BITS) {
                    for (uint32_t i = x / UINT32_T_BITS; n >= UINT32_VECTOR_BITS;
                         n -= UINT32_VECTOR_BITS, x += UINT32_VECTOR_BITS, i += UINT32_VECTOR_SIZE) {
                        vstr_u32(row0 + i, vmvn_u32(veor_u32(vldr_u32(row0 + i), vldr_u32(row1 + i))));
                    }
                }

                for (uint32_t i = x / UINT32_T_BITS; n >= UINT32_T_BITS;
                     n -= UINT32_T_BITS, x += UINT32_T_BITS, i++) {
                    row0[i] = ~(row0[i] ^ row1[i]);
                }

                if (n) {
                    uint32_t vp0 = vldr_binary_bits(row0, x, n);
                    uint32_t vp1 = vldr_binary_bits(row1, x, n);
                    vstr_binary_bits(row0, x, n, ~(vp0 ^ vp1));
                }
            } else {
                for (; x < x_end; x++) {
                    if (image_get_mask_pixel(mask, x, y_row)) {
                        uint32_t p0 = IMAGE_GET_BINARY_PIXEL_FAST(row0, x);
                        uint32_t p1 = IMAGE_GET_BINARY_PIXEL_FAST(row1, x);
                        uint32_t p = ~(p0 ^ p1);
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
                    vstr_u8(row0 + x, vmvn_u32(veor_u32(vldr_u8(row0 + x), vldr_u8(row1 + x))));
                }

                if (n) {
                    v128_predicate_t pred = vpredicate_8(n);
                    v2x_rows_t data = vldr_u8_pred_x2((v2x_row_ptrs_t) {.p0.u8 = row0, .p1.u8 = row1}, x, pred);
                    vstr_u8_pred(row0 + x, vmvn_u32(veor_u32(data.r0, data.r1)), pred);
                }
            } else {
                for (; x < x_end; x++) {
                    if (image_get_mask_pixel(mask, x, y_row)) {
                        uint32_t p0 = IMAGE_GET_GRAYSCALE_PIXEL_FAST(row0, x);
                        uint32_t p1 = IMAGE_GET_GRAYSCALE_PIXEL_FAST(row1, x);
                        uint32_t p = ~(p0 ^ p1);
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
                    vstr_u16(row0 + x, vmvn_u32(veor_u32(vldr_u16(row0 + x), vldr_u16(row1 + x))));
                }

                if (n) {
                    v128_predicate_t pred = vpredicate_16(n);
                    v2x_rows_t data = vldr_u16_pred_x2((v2x_row_ptrs_t) {.p0.u16 = row0, .p1.u16 = row1}, x, pred);
                    vstr_u16_pred(row0 + x, vmvn_u32(veor_u32(data.r0, data.r1)), pred);
                }
            } else {
                for (; x < x_end; x++) {
                    if (image_get_mask_pixel(mask, x, y_row)) {
                        uint32_t p0 = IMAGE_GET_RGB565_PIXEL_FAST(row0, x);
                        uint32_t p1 = IMAGE_GET_RGB565_PIXEL_FAST(row1, x);
                        uint32_t p = ~(p0 ^ p1);
                        IMAGE_PUT_RGB565_PIXEL_FAST(row0, x, p);
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

static void imlib_erode_dilate(image_t *img, int ksize, int threshold, int e_or_d, image_t *mask) {
    int brows = ksize + 1;
    image_t buf;
    buf.w = img->w;
    buf.h = brows;
    buf.pixfmt = img->pixfmt;

    switch (img->pixfmt) {
        case PIXFORMAT_BINARY: {
            buf.data = fb_alloc(IMAGE_BINARY_LINE_LEN_BYTES(img) * brows, FB_ALLOC_NO_HINT);

            for (int y = 0; y < img->h; y++) {
                uint32_t *row_ptr = IMAGE_COMPUTE_BINARY_PIXEL_ROW_PTR(img, y);
                uint32_t *buf_row_ptr = IMAGE_COMPUTE_BINARY_PIXEL_ROW_PTR(&buf, (y % brows));
                int acc = 0;

                for (int x = 0; x < img->w; x++) {
                    int pixel = IMAGE_GET_BINARY_PIXEL_FAST(row_ptr, x);
                    IMAGE_PUT_BINARY_PIXEL_FAST(buf_row_ptr, x, pixel);

                    if (mask && (!image_get_mask_pixel(mask, x, y))) {
                        continue; // Short circuit.
                    }

                    if (!mask && x > ksize && x < img->w - ksize && y >= ksize && y < img->h - ksize) {
                        // faster
                        for (int j = -ksize; j <= ksize; j++) {
                            uint32_t *k_row_ptr = IMAGE_COMPUTE_BINARY_PIXEL_ROW_PTR(img, y + j);
                            // subtract old left column and add new right column
                            acc -= IMAGE_GET_BINARY_PIXEL_FAST(k_row_ptr, x - ksize - 1);
                            acc += IMAGE_GET_BINARY_PIXEL_FAST(k_row_ptr, x + ksize);
                        }
                    } else {
                        // slower (checks boundaries per pixel)
                        acc = e_or_d ? 0 : -1; // Don't count center pixel...
                        for (int j = -ksize; j <= ksize; j++) {
                            int y_j = IM_CLAMP(y + j, 0, (img->h - 1));
                            uint32_t *k_row_ptr = IMAGE_COMPUTE_BINARY_PIXEL_ROW_PTR(img, y_j);

                            for (int k = -ksize; k <= ksize; k++) {
                                int x_k = IM_CLAMP(x + k, 0, (img->w - 1));
                                acc += IMAGE_GET_BINARY_PIXEL_FAST(k_row_ptr, x_k);
                            }
                        }
                    }

                    if (!e_or_d) {
                        // Preserve original pixel value... or clear it.
                        if (acc < threshold) {
                            IMAGE_CLEAR_BINARY_PIXEL_FAST(buf_row_ptr, x);
                        }
                    } else {
                        // Preserve original pixel value... or set it.
                        if (acc > threshold) {
                            IMAGE_SET_BINARY_PIXEL_FAST(buf_row_ptr, x);
                        }
                    }
                }

                if (y >= ksize) {
                    // Transfer buffer lines...
                    memcpy(IMAGE_COMPUTE_BINARY_PIXEL_ROW_PTR(img, (y - ksize)),
                           IMAGE_COMPUTE_BINARY_PIXEL_ROW_PTR(&buf, ((y - ksize) % brows)),
                           IMAGE_BINARY_LINE_LEN_BYTES(img));
                }
            }

            // Copy any remaining lines from the buffer image...
            for (int y = IM_MAX(img->h - ksize, 0); y < img->h; y++) {
                memcpy(IMAGE_COMPUTE_BINARY_PIXEL_ROW_PTR(img, y),
                       IMAGE_COMPUTE_BINARY_PIXEL_ROW_PTR(&buf, (y % brows)),
                       IMAGE_BINARY_LINE_LEN_BYTES(img));
            }

            fb_free();
            break;
        }
        case PIXFORMAT_GRAYSCALE: {
            buf.data = fb_alloc(IMAGE_GRAYSCALE_LINE_LEN_BYTES(img) * brows, FB_ALLOC_NO_HINT);

            #if defined(ARM_MATH_DSP)
            int32_t threshold_x4 = __USAT(threshold, 7) * 0x01010101;
            uint32_t e_or_d_mask_x4 = e_or_d ? 0xFFFFFFFF : 0x00000000;
            #endif

            for (int y = 0; y < img->h; y++) {
                uint8_t *row_ptr = IMAGE_COMPUTE_GRAYSCALE_PIXEL_ROW_PTR(img, y);
                uint8_t *buf_row_ptr = IMAGE_COMPUTE_GRAYSCALE_PIXEL_ROW_PTR(&buf, (y % brows));
                int acc = 0;

                for (int x = 0; x < img->w; x++) {
                    int pixel = IMAGE_GET_GRAYSCALE_PIXEL_FAST(row_ptr, x);
                    IMAGE_PUT_GRAYSCALE_PIXEL_FAST(buf_row_ptr, x, pixel);

                    if (mask && (!image_get_mask_pixel(mask, x, y))) {
                        continue; // Short circuit.
                    }

                    if (!mask && x > ksize && x < img->w - ksize && y >= ksize && y < img->h - ksize) {
                        if (0) {
                        #if defined(ARM_MATH_DSP)
                            // acc will never be larger than 121 (<= 127) for ksize <= 5.
                        } else if ((x < img->w - ksize - 3) && (ksize <= 5)) {
                            int32_t acc_x4 = acc & 0xFF;

                            for (int j = -ksize; j <= ksize; j++) {
                                uint8_t *k_row_ptr = IMAGE_COMPUTE_GRAYSCALE_PIXEL_ROW_PTR(img, y + j);
                                // subtract old left edge 4x
                                __USUB8(*((uint32_t *) (k_row_ptr + x - ksize - 1)), 0x01010101);
                                acc_x4 = __SSUB8(acc_x4, __SEL(0x01010101, 0));
                                // add new right edge to sum 4x
                                __USUB8(*((uint32_t *) (k_row_ptr + x + ksize)), 0x01010101);
                                acc_x4 = __SADD8(acc_x4, __SEL(0x01010101, 0));
                            }

                            // After the loop the lowest 8-bits contain a valid acc value. We
                            // compute the next 3 acc values by adding the sums up.

                            acc_x4 = __SADD8(acc_x4, acc_x4 << 8);
                            acc_x4 = __SADD8(acc_x4, acc_x4 << 16);
                            acc = acc_x4 >> 24;

                            // Compute all erode/dilate thresholds at once.

                            uint32_t pixel_x4 = *((uint32_t *) (row_ptr + x));
                            __SSUB8(e_or_d ? threshold_x4 : acc_x4, e_or_d ? acc_x4 : threshold_x4);
                            *((uint32_t *) (buf_row_ptr + x)) = __SEL(pixel_x4, e_or_d_mask_x4);

                            x += 3;
                            continue;
                        #endif
                        } else {
                            // faster
                            for (int j = -ksize; j <= ksize; j++) {
                                uint8_t *k_row_ptr = IMAGE_COMPUTE_GRAYSCALE_PIXEL_ROW_PTR(img, y + j);
                                // subtract old left edge and add new right edge to sum
                                acc -= (IMAGE_GET_GRAYSCALE_PIXEL_FAST(k_row_ptr, x - ksize - 1) > 0);
                                acc += (IMAGE_GET_GRAYSCALE_PIXEL_FAST(k_row_ptr, x + ksize) > 0);
                            }
                        }
                    } else {
                        // slower way which checks boundaries per pixel
                        acc = e_or_d ? 0 : -1; // Don't count center pixel...
                        for (int j = -ksize; j <= ksize; j++) {
                            int y_j = IM_CLAMP(y + j, 0, (img->h - 1));
                            uint8_t *k_row_ptr = IMAGE_COMPUTE_GRAYSCALE_PIXEL_ROW_PTR(img, y_j);

                            for (int k = -ksize; k <= ksize; k++) {
                                int x_k = IM_CLAMP(x + k, 0, (img->w - 1));
                                acc += IMAGE_GET_GRAYSCALE_PIXEL_FAST(k_row_ptr, x_k) > 0;
                            }
                        }
                    }

                    if (!e_or_d) {
                        // Preserve original pixel value... or clear it.
                        if (acc < threshold) {
                            IMAGE_PUT_GRAYSCALE_PIXEL_FAST(buf_row_ptr, x, COLOR_GRAYSCALE_BINARY_MIN);
                        }
                    } else {
                        // Preserve original pixel value... or set it.
                        if (acc > threshold) {
                            IMAGE_PUT_GRAYSCALE_PIXEL_FAST(buf_row_ptr, x, COLOR_GRAYSCALE_BINARY_MAX);
                        }
                    }
                }

                if (y >= ksize) {
                    // Transfer buffer lines...
                    memcpy(IMAGE_COMPUTE_GRAYSCALE_PIXEL_ROW_PTR(img, (y - ksize)),
                           IMAGE_COMPUTE_GRAYSCALE_PIXEL_ROW_PTR(&buf, ((y - ksize) % brows)),
                           IMAGE_GRAYSCALE_LINE_LEN_BYTES(img));
                }
            }

            // Copy any remaining lines from the buffer image...
            for (int y = IM_MAX(img->h - ksize, 0); y < img->h; y++) {
                memcpy(IMAGE_COMPUTE_GRAYSCALE_PIXEL_ROW_PTR(img, y),
                       IMAGE_COMPUTE_GRAYSCALE_PIXEL_ROW_PTR(&buf, (y % brows)),
                       IMAGE_GRAYSCALE_LINE_LEN_BYTES(img));
            }

            fb_free();
            break;
        }
        case PIXFORMAT_RGB565: {
            buf.data = fb_alloc(IMAGE_RGB565_LINE_LEN_BYTES(img) * brows, FB_ALLOC_NO_HINT);

            #if defined(ARM_MATH_DSP)
            int32_t threshold_x2 = __USAT(threshold, 15) * 0x00010001;
            uint32_t e_or_d_mask_x2 = e_or_d ? 0xFFFFFFFF : 0x00000000;
            #endif

            for (int y = 0; y < img->h; y++) {
                uint16_t *row_ptr = IMAGE_COMPUTE_RGB565_PIXEL_ROW_PTR(img, y);
                uint16_t *buf_row_ptr = IMAGE_COMPUTE_RGB565_PIXEL_ROW_PTR(&buf, (y % brows));
                int acc = 0;

                for (int x = 0; x < img->w; x++) {
                    int pixel = IMAGE_GET_RGB565_PIXEL_FAST(row_ptr, x);
                    IMAGE_PUT_RGB565_PIXEL_FAST(buf_row_ptr, x, pixel);

                    if (mask && (!image_get_mask_pixel(mask, x, y))) {
                        continue; // Short circuit.
                    }

                    if (!mask && x > ksize && x < img->w - ksize && y >= ksize && y < img->h - ksize) {
                        if (0) {
                        #if defined(ARM_MATH_DSP)
                            // acc will never be larger than 32,761 (<= 32767) for ksize <= 90.
                        } else if ((x < img->w - ksize - 1) && (ksize <= 90)) {
                            int32_t acc_x2 = acc & 0xFFFF;

                            for (int j = -ksize; j <= ksize; j++) {
                                uint16_t *k_row_ptr = IMAGE_COMPUTE_RGB565_PIXEL_ROW_PTR(img, y + j);
                                // subtract old left edge 2x
                                __USUB16(*((uint32_t *) (k_row_ptr + x - ksize - 1)), 0x00010001);
                                acc_x2 = __SSUB16(acc_x2, __SEL(0x00010001, 0));
                                // add new right edge to sum 2x
                                __USUB16(*((uint32_t *) (k_row_ptr + x + ksize)), 0x00010001);
                                acc_x2 = __SADD16(acc_x2, __SEL(0x00010001, 0));
                            }

                            // After the loop the lowest 16-bits contain a valid acc value. We
                            // compute the next acc value by adding the sums up.

                            acc_x2 = __SADD16(acc_x2, acc_x2 << 16);
                            acc = acc_x2 >> 16;

                            // Compute all erode/dilate thresholds at once.

                            uint32_t pixel_x2 = *((uint32_t *) (row_ptr + x));
                            __SSUB16(e_or_d ? threshold_x2 : acc_x2, e_or_d ? acc_x2 : threshold_x2);
                            *((uint32_t *) (buf_row_ptr + x)) = __SEL(pixel_x2, e_or_d_mask_x2);

                            x += 1;
                            continue;
                        #endif
                        } else {
                            // faster
                            for (int j = -ksize; j <= ksize; j++) {
                                uint16_t *k_row_ptr = IMAGE_COMPUTE_RGB565_PIXEL_ROW_PTR(img, y + j);
                                // subtract old left column and add new right column
                                acc -= IMAGE_GET_RGB565_PIXEL_FAST(k_row_ptr, x - ksize - 1) > 0;
                                acc += IMAGE_GET_RGB565_PIXEL_FAST(k_row_ptr, x + ksize) > 0;
                            }
                        }
                    } else {
                        // need to check boundary conditions for each pixel
                        acc = e_or_d ? 0 : -1; // Don't count center pixel...
                        for (int j = -ksize; j <= ksize; j++) {
                            int y_j = IM_CLAMP(y + j, 0, (img->h - 1));
                            uint16_t *k_row_ptr = IMAGE_COMPUTE_RGB565_PIXEL_ROW_PTR(img, y_j);

                            for (int k = -ksize; k <= ksize; k++) {
                                int x_k = IM_CLAMP(x + k, 0, (img->w - 1));
                                acc += IMAGE_GET_RGB565_PIXEL_FAST(k_row_ptr, x_k) > 0;
                            }
                        }
                    }

                    if (!e_or_d) {
                        // Preserve original pixel value... or clear it.
                        if (acc < threshold) {
                            IMAGE_PUT_RGB565_PIXEL_FAST(buf_row_ptr, x, COLOR_RGB565_BINARY_MIN);
                        }
                    } else {
                        // Preserve original pixel value... or set it.
                        if (acc > threshold) {
                            IMAGE_PUT_RGB565_PIXEL_FAST(buf_row_ptr, x, COLOR_RGB565_BINARY_MAX);
                        }
                    }
                }

                if (y >= ksize) {
                    // Transfer buffer lines...
                    memcpy(IMAGE_COMPUTE_RGB565_PIXEL_ROW_PTR(img, (y - ksize)),
                           IMAGE_COMPUTE_RGB565_PIXEL_ROW_PTR(&buf, ((y - ksize) % brows)),
                           IMAGE_RGB565_LINE_LEN_BYTES(img));
                }
            }

            // Copy any remaining lines from the buffer image...
            for (int y = IM_MAX(img->h - ksize, 0); y < img->h; y++) {
                memcpy(IMAGE_COMPUTE_RGB565_PIXEL_ROW_PTR(img, y),
                       IMAGE_COMPUTE_RGB565_PIXEL_ROW_PTR(&buf, (y % brows)),
                       IMAGE_RGB565_LINE_LEN_BYTES(img));
            }

            fb_free();
            break;
        }
        default: {
            break;
        }
    }
}

void imlib_erode(image_t *img, int ksize, int threshold, image_t *mask) {
    // Threshold should be equal to 0
    // for normal operation. E.g. for ksize==3 -> threshold==0
    // Basically you're adjusting the number of data that
    // must be set in the kernel (besides the center) for the output to be 1.
    // Erode normally requires all data to be 1.
    imlib_erode_dilate(img, ksize, imlib_ksize_to_n(ksize) - 1 - threshold, 0, mask);
}

void imlib_dilate(image_t *img, int ksize, int threshold, image_t *mask) {
    // Threshold should be equal to 0
    // for normal operation. E.g. for ksize==3 -> threshold==0
    // Basically you're adjusting the number of data that
    // must be set in the kernel (besides the center) for the output to be 1.
    // Dilate normally requires one pixel to be 1.
    imlib_erode_dilate(img, ksize, threshold, 1, mask);
}

void imlib_open(image_t *img, int ksize, int threshold, image_t *mask) {
    imlib_erode(img, ksize, threshold, mask);
    imlib_dilate(img, ksize, threshold, mask);
}

void imlib_close(image_t *img, int ksize, int threshold, image_t *mask) {
    imlib_dilate(img, ksize, threshold, mask);
    imlib_erode(img, ksize, threshold, mask);
}

static void imlib_hat(image_t *img, int ksize, int threshold, image_t *mask, binary_morph_op_t op) {
    image_t temp;
    temp.w = img->w;
    temp.h = img->h;
    temp.pixfmt = img->pixfmt;
    temp.data = fb_alloc(image_size(img), FB_ALLOC_CACHE_ALIGN);
    memcpy(temp.data, img->data, image_size(img));
    op(&temp, ksize, threshold, mask);
    void *dst_row_override = fb_alloc0(image_line_size(img), FB_ALLOC_CACHE_ALIGN);
    imlib_draw_image(img, &temp, 0, 0, 1.0f, 1.0f, NULL, -1, 255, NULL, NULL, 0,
                     imlib_difference_line_op, mask, dst_row_override);
    fb_free(); // dst_row_override
    fb_free(); // temp.data
}

void imlib_top_hat(image_t *img, int ksize, int threshold, image_t *mask) {
    imlib_hat(img, ksize, threshold, mask, imlib_open);
}

void imlib_black_hat(image_t *img, int ksize, int threshold, image_t *mask) {
    imlib_hat(img, ksize, threshold, mask, imlib_close);
}
#endif
