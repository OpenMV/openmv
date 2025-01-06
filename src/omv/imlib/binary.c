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
    uint32_t *p32 = (uint32_t *) img->data;

    switch (img->pixfmt) {
        case PIXFORMAT_BINARY: {
            for (; n >= 4; n -= 4, p32++) {
                *p32 = ~*p32;
            }
            break;
        }
        case PIXFORMAT_GRAYSCALE: {
            #if (__ARM_ARCH > 6)
            for (; n >= 4; n -= 4, p32++) {
                *p32 = ~*p32;
            }
            #endif

            uint8_t *p8 = (uint8_t *) p32;

            for (; n >= 1; n -= 1, p8++) {
                *p8 = ~*p8;
            }
            break;
        }
        case PIXFORMAT_RGB565: {
            #if (__ARM_ARCH > 6)
            for (; n >= 4; n -= 4, p32++) {
                *p32 = ~*p32;
            }
            #endif

            uint16_t *p16 = (uint16_t *) p32;

            for (; n >= 2; n -= 2, p16++) {
                *p16 = ~*p16;
            }
            break;
        }
        default: {
            break;
        }
    }
}

void imlib_b_and_line_op(int x, int x_end, int y_row, imlib_draw_row_data_t *data) {
    image_t *mask = (image_t *) data->callback_arg;

    switch (data->dst_img->pixfmt) {
        case PIXFORMAT_BINARY: {
            uint32_t *row0 = IMAGE_COMPUTE_BINARY_PIXEL_ROW_PTR(data->dst_img, y_row);
            uint32_t *row1 = (uint32_t *) data->dst_row_override;

            if (!mask) {
                #if (__ARM_ARCH > 6)
                // Align to 32-bit boundary.
                for (; (x % 32) && (x < x_end); x++) {
                    uint32_t p0 = IMAGE_GET_BINARY_PIXEL_FAST(row0, x);
                    uint32_t p1 = IMAGE_GET_BINARY_PIXEL_FAST(row1, x);
                    uint32_t p = p0 & p1;
                    IMAGE_PUT_BINARY_PIXEL_FAST(row0, x, p);
                }

                for (; (x_end - x) >= 32; x += 32) {
                    uint32_t p0 = row0[x / 32];
                    uint32_t p1 = ((uint32_t *) row1)[x / 32];
                    row0[x / 32] = p0 & p1;
                }
                #endif

                for (; x < x_end; x++) {
                    uint32_t p0 = IMAGE_GET_BINARY_PIXEL_FAST(row0, x);
                    uint32_t p1 = IMAGE_GET_BINARY_PIXEL_FAST(row1, x);
                    uint32_t p = p0 & p1;
                    IMAGE_PUT_BINARY_PIXEL_FAST(row0, x, p);
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
                #if (__ARM_ARCH > 6)
                for (; (x_end - x) >= 4; x += 4) {
                    uint32_t p0 = *((uint32_t *) (row0 + x));
                    uint32_t p1 = *((uint32_t *) (row1 + x));
                    *((uint32_t *) (row0 + x)) = p0 & p1;
                }
                #endif

                for (; x < x_end; x++) {
                    uint32_t p0 = IMAGE_GET_GRAYSCALE_PIXEL_FAST(row0, x);
                    uint32_t p1 = IMAGE_GET_GRAYSCALE_PIXEL_FAST(row1, x);
                    uint32_t p = p0 & p1;
                    IMAGE_PUT_GRAYSCALE_PIXEL_FAST(row0, x, p);
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
                #if (__ARM_ARCH > 6)
                for (; (x_end - x) >= 2; x += 2) {
                    uint32_t p0 = *((uint32_t *) (row0 + x));
                    uint32_t p1 = *((uint32_t *) (row1 + x));
                    *((uint32_t *) (row0 + x)) = p0 & p1;
                }
                #endif

                for (; x < x_end; x++) {
                    uint32_t p0 = IMAGE_GET_RGB565_PIXEL_FAST(row0, x);
                    uint32_t p1 = IMAGE_GET_RGB565_PIXEL_FAST(row1, x);
                    uint32_t p = p0 & p1;
                    IMAGE_PUT_RGB565_PIXEL_FAST(row0, x, p);
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
                #if (__ARM_ARCH > 6)
                // Align to 32-bit boundary.
                for (; (x % 32) && (x < x_end); x++) {
                    uint32_t p0 = IMAGE_GET_BINARY_PIXEL_FAST(row0, x);
                    uint32_t p1 = IMAGE_GET_BINARY_PIXEL_FAST(row1, x);
                    uint32_t p = ~(p0 & p1);
                    IMAGE_PUT_BINARY_PIXEL_FAST(row0, x, p);
                }

                for (; (x_end - x) >= 32; x += 32) {
                    uint32_t p0 = row0[x / 32];
                    uint32_t p1 = row1[x / 32];
                    row0[x / 32] = ~(p0 & p1);
                }
                #endif

                for (; x < x_end; x++) {
                    uint32_t p0 = IMAGE_GET_BINARY_PIXEL_FAST(row0, x);
                    uint32_t p1 = IMAGE_GET_BINARY_PIXEL_FAST(row1, x);
                    uint32_t p = ~(p0 & p1);
                    IMAGE_PUT_BINARY_PIXEL_FAST(row0, x, p);
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
                #if (__ARM_ARCH > 6)
                for (; (x_end - x) >= 4; x += 4) {
                    uint32_t p0 = *((uint32_t *) (row0 + x));
                    uint32_t p1 = *((uint32_t *) (row1 + x));
                    *((uint32_t *) (row0 + x)) = ~(p0 & p1);
                }
                #endif

                for (; x < x_end; x++) {
                    uint32_t p0 = IMAGE_GET_GRAYSCALE_PIXEL_FAST(row0, x);
                    uint32_t p1 = IMAGE_GET_GRAYSCALE_PIXEL_FAST(row1, x);
                    uint32_t p = ~(p0 & p1);
                    IMAGE_PUT_GRAYSCALE_PIXEL_FAST(row0, x, p);
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
                #if (__ARM_ARCH > 6)
                for (; (x_end - x) >= 2; x += 2) {
                    uint32_t p0 = *((uint32_t *) (row0 + x));
                    uint32_t p1 = *((uint32_t *) (row1 + x));
                    *((uint32_t *) (row0 + x)) = ~(p0 & p1);
                }
                #endif

                for (; x < x_end; x++) {
                    uint32_t p0 = IMAGE_GET_RGB565_PIXEL_FAST(row0, x);
                    uint32_t p1 = IMAGE_GET_RGB565_PIXEL_FAST(row1, x);
                    uint32_t p = ~(p0 & p1);
                    IMAGE_PUT_RGB565_PIXEL_FAST(row0, x, p);
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
                #if (__ARM_ARCH > 6)
                // Align to 32-bit boundary.
                for (; (x % 32) && (x < x_end); x++) {
                    uint32_t p0 = IMAGE_GET_BINARY_PIXEL_FAST(row0, x);
                    uint32_t p1 = IMAGE_GET_BINARY_PIXEL_FAST(row1, x);
                    uint32_t p = p0 | p1;
                    IMAGE_PUT_BINARY_PIXEL_FAST(row0, x, p);
                }

                for (; (x_end - x) >= 32; x += 32) {
                    uint32_t p0 = row0[x / 32];
                    uint32_t p1 = row1[x / 32];
                    row0[x / 32] = p0 | p1;
                }
                #endif

                for (; x < x_end; x++) {
                    uint32_t p0 = IMAGE_GET_BINARY_PIXEL_FAST(row0, x);
                    uint32_t p1 = IMAGE_GET_BINARY_PIXEL_FAST(row1, x);
                    uint32_t p = p0 | p1;
                    IMAGE_PUT_BINARY_PIXEL_FAST(row0, x, p);
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
                #if (__ARM_ARCH > 6)
                for (; (x_end - x) >= 4; x += 4) {
                    uint32_t p0 = *((uint32_t *) (row0 + x));
                    uint32_t p1 = *((uint32_t *) (row1 + x));
                    *((uint32_t *) (row0 + x)) = p0 | p1;
                }
                #endif

                for (; x < x_end; x++) {
                    uint32_t p0 = IMAGE_GET_GRAYSCALE_PIXEL_FAST(row0, x);
                    uint32_t p1 = IMAGE_GET_GRAYSCALE_PIXEL_FAST(row1, x);
                    uint32_t p = p0 | p1;
                    IMAGE_PUT_GRAYSCALE_PIXEL_FAST(row0, x, p);
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
                #if (__ARM_ARCH > 6)
                for (; (x_end - x) >= 2; x += 2) {
                    uint32_t p0 = *((uint32_t *) (row0 + x));
                    uint32_t p1 = *((uint32_t *) (row1 + x));
                    *((uint32_t *) (row0 + x)) = p0 | p1;
                }
                #endif

                for (; x < x_end; x++) {
                    uint32_t p0 = IMAGE_GET_RGB565_PIXEL_FAST(row0, x);
                    uint32_t p1 = IMAGE_GET_RGB565_PIXEL_FAST(row1, x);
                    uint32_t p = p0 | p1;
                    IMAGE_PUT_RGB565_PIXEL_FAST(row0, x, p);
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
                #if (__ARM_ARCH > 6)
                // Align to 32-bit boundary.
                for (; (x % 32) && (x < x_end); x++) {
                    uint32_t p0 = IMAGE_GET_BINARY_PIXEL_FAST(row0, x);
                    uint32_t p1 = IMAGE_GET_BINARY_PIXEL_FAST(row1, x);
                    uint32_t p = ~(p0 | p1);
                    IMAGE_PUT_BINARY_PIXEL_FAST(row0, x, p);
                }

                for (; (x_end - x) >= 32; x += 32) {
                    uint32_t p0 = row0[x / 32];
                    uint32_t p1 = row1[x / 32];
                    row0[x / 32] = ~(p0 | p1);
                }
                #endif

                for (; x < x_end; x++) {
                    uint32_t p0 = IMAGE_GET_BINARY_PIXEL_FAST(row0, x);
                    uint32_t p1 = IMAGE_GET_BINARY_PIXEL_FAST(row1, x);
                    uint32_t p = ~(p0 | p1);
                    IMAGE_PUT_BINARY_PIXEL_FAST(row0, x, p);
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
                #if (__ARM_ARCH > 6)
                for (; (x_end - x) >= 4; x += 4) {
                    uint32_t p0 = *((uint32_t *) (row0 + x));
                    uint32_t p1 = *((uint32_t *) (row1 + x));
                    *((uint32_t *) (row0 + x)) = ~(p0 | p1);
                }
                #endif

                for (; x < x_end; x++) {
                    uint32_t p0 = IMAGE_GET_GRAYSCALE_PIXEL_FAST(row0, x);
                    uint32_t p1 = IMAGE_GET_GRAYSCALE_PIXEL_FAST(row1, x);
                    uint32_t p = ~(p0 | p1);
                    IMAGE_PUT_GRAYSCALE_PIXEL_FAST(row0, x, p);
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
                #if (__ARM_ARCH > 6)
                for (; (x_end - x) >= 2; x += 2) {
                    uint32_t p0 = *((uint32_t *) (row0 + x));
                    uint32_t p1 = *((uint32_t *) (row1 + x));
                    *((uint32_t *) (row0 + x)) = ~(p0 | p1);
                }
                #endif

                for (; x < x_end; x++) {
                    uint32_t p0 = IMAGE_GET_RGB565_PIXEL_FAST(row0, x);
                    uint32_t p1 = IMAGE_GET_RGB565_PIXEL_FAST(row1, x);
                    uint32_t p = ~(p0 | p1);
                    IMAGE_PUT_RGB565_PIXEL_FAST(row0, x, p);
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
                #if (__ARM_ARCH > 6)
                // Align to 32-bit boundary.
                for (; (x % 32) && (x < x_end); x++) {
                    uint32_t p0 = IMAGE_GET_BINARY_PIXEL_FAST(row0, x);
                    uint32_t p1 = IMAGE_GET_BINARY_PIXEL_FAST(row1, x);
                    uint32_t p = p0 ^ p1;
                    IMAGE_PUT_BINARY_PIXEL_FAST(row0, x, p);
                }

                for (; (x_end - x) >= 32; x += 32) {
                    uint32_t p0 = row0[x / 32];
                    uint32_t p1 = row1[x / 32];
                    row0[x / 32] = p0 ^ p1;
                }
                #endif

                for (; x < x_end; x++) {
                    uint32_t p0 = IMAGE_GET_BINARY_PIXEL_FAST(row0, x);
                    uint32_t p1 = IMAGE_GET_BINARY_PIXEL_FAST(row1, x);
                    uint32_t p = p0 ^ p1;
                    IMAGE_PUT_BINARY_PIXEL_FAST(row0, x, p);
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
                #if (__ARM_ARCH > 6)
                for (; (x_end - x) >= 4; x += 4) {
                    uint32_t p0 = *((uint32_t *) (row0 + x));
                    uint32_t p1 = *((uint32_t *) (row1 + x));
                    *((uint32_t *) (row0 + x)) = p0 ^ p1;
                }
                #endif

                for (; x < x_end; x++) {
                    uint32_t p0 = IMAGE_GET_GRAYSCALE_PIXEL_FAST(row0, x);
                    uint32_t p1 = IMAGE_GET_GRAYSCALE_PIXEL_FAST(row1, x);
                    uint32_t p = p0 ^ p1;
                    IMAGE_PUT_GRAYSCALE_PIXEL_FAST(row0, x, p);
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
                #if (__ARM_ARCH > 6)
                for (; (x_end - x) >= 2; x += 2) {
                    uint32_t p0 = *((uint32_t *) (row0 + x));
                    uint32_t p1 = *((uint32_t *) (row1 + x));
                    *((uint32_t *) (row0 + x)) = p0 ^ p1;
                }
                #endif

                for (; x < x_end; x++) {
                    uint32_t p0 = IMAGE_GET_RGB565_PIXEL_FAST(row0, x);
                    uint32_t p1 = IMAGE_GET_RGB565_PIXEL_FAST(row1, x);
                    uint32_t p = p0 ^ p1;
                    IMAGE_PUT_RGB565_PIXEL_FAST(row0, x, p);
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
                #if (__ARM_ARCH > 6)
                // Align to 32-bit boundary.
                for (; (x % 32) && (x < x_end); x++) {
                    uint32_t p0 = IMAGE_GET_BINARY_PIXEL_FAST(row0, x);
                    uint32_t p1 = IMAGE_GET_BINARY_PIXEL_FAST(row1, x);
                    uint32_t p = ~(p0 ^ p1);
                    IMAGE_PUT_BINARY_PIXEL_FAST(row0, x, p);
                }

                for (; (x_end - x) >= 32; x += 32) {
                    uint32_t p0 = row0[x / 32];
                    uint32_t p1 = row1[x / 32];
                    row0[x / 32] = ~(p0 ^ p1);
                }
                #endif

                for (; x < x_end; x++) {
                    uint32_t p0 = IMAGE_GET_BINARY_PIXEL_FAST(row0, x);
                    uint32_t p1 = IMAGE_GET_BINARY_PIXEL_FAST(row1, x);
                    uint32_t p = ~(p0 ^ p1);
                    IMAGE_PUT_BINARY_PIXEL_FAST(row0, x, p);
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
                #if (__ARM_ARCH > 6)
                for (; (x_end - x) >= 4; x += 4) {
                    uint32_t p0 = *((uint32_t *) (row0 + x));
                    uint32_t p1 = *((uint32_t *) (row1 + x));
                    *((uint32_t *) (row0 + x)) = ~(p0 ^ p1);
                }
                #endif

                for (; x < x_end; x++) {
                    uint32_t p0 = IMAGE_GET_GRAYSCALE_PIXEL_FAST(row0, x);
                    uint32_t p1 = IMAGE_GET_GRAYSCALE_PIXEL_FAST(row1, x);
                    uint32_t p = ~(p0 ^ p1);
                    IMAGE_PUT_GRAYSCALE_PIXEL_FAST(row0, x, p);
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
                #if (__ARM_ARCH > 6)
                for (; (x_end - x) >= 2; x += 2) {
                    uint32_t p0 = *((uint32_t *) (row0 + x));
                    uint32_t p1 = *((uint32_t *) (row1 + x));
                    *((uint32_t *) (row0 + x)) = ~(p0 ^ p1);
                }
                #endif

                for (; x < x_end; x++) {
                    uint32_t p0 = IMAGE_GET_RGB565_PIXEL_FAST(row0, x);
                    uint32_t p1 = IMAGE_GET_RGB565_PIXEL_FAST(row1, x);
                    uint32_t p = ~(p0 ^ p1);
                    IMAGE_PUT_RGB565_PIXEL_FAST(row0, x, p);
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
    int32_t brows = ksize + 1;
    image_t buf;
    buf.w = img->w;
    buf.h = brows;
    buf.pixfmt = img->pixfmt;
    int32_t sum = imlib_ksize_to_n(ksize);

    switch (img->pixfmt) {
        case PIXFORMAT_BINARY: {
            buf.data = fb_alloc(IMAGE_BINARY_LINE_LEN_BYTES(img) * brows, FB_ALLOC_PREFER_SPEED);
            int32_t out = e_or_d ? COLOR_BINARY_MAX : COLOR_BINARY_MIN;

            for (int32_t y = 0; y < img->h; y++) {
                uint32_t *row_ptr = IMAGE_COMPUTE_BINARY_PIXEL_ROW_PTR(img, y);
                uint32_t *buf_row_ptr = IMAGE_COMPUTE_BINARY_PIXEL_ROW_PTR(&buf, (y % brows));
                int32_t acc = 0;

                for (int32_t x = 0; x < img->w; x++) {
                    if (mask && (!image_get_mask_pixel(mask, x, y))) {
                        int32_t p = IMAGE_GET_BINARY_PIXEL_FAST(row_ptr, x);
                        IMAGE_PUT_BINARY_PIXEL_FAST(buf_row_ptr, x, p);
                        continue; // Short circuit.
                    }

                    if (!mask && x > ksize && x < img->w - ksize && y >= ksize && y < img->h - ksize) {
                        for (int32_t j = -ksize; j <= ksize; j++) {
                            uint32_t *k_row_ptr = IMAGE_COMPUTE_BINARY_PIXEL_ROW_PTR(img, y + j);
                            // subtract old left column and add new right column
                            acc -= IMAGE_GET_BINARY_PIXEL_FAST(k_row_ptr, x - ksize - 1);
                            acc += IMAGE_GET_BINARY_PIXEL_FAST(k_row_ptr, x + ksize);
                        }
                    } else {
                        acc = e_or_d ? 0 : -1; // Don't count center pixel...
                        for (int32_t j = -ksize; j <= ksize; j++) {
                            int32_t y_j = IM_CLAMP(y + j, 0, (img->h - 1));
                            uint32_t *k_row_ptr = IMAGE_COMPUTE_BINARY_PIXEL_ROW_PTR(img, y_j);

                            for (int32_t k = -ksize; k <= ksize; k++) {
                                int32_t x_k = IM_CLAMP(x + k, 0, (img->w - 1));
                                acc += IMAGE_GET_BINARY_PIXEL_FAST(k_row_ptr, x_k);
                            }
                        }
                    }

                    int32_t old_pixel = IMAGE_GET_BINARY_PIXEL_FAST(row_ptr, x);
                    int32_t v0 = e_or_d ? threshold : acc;
                    int32_t v1 = e_or_d ? acc : threshold;
                    int32_t pixel = (v0 >= v1) ? old_pixel : out;
                    IMAGE_PUT_BINARY_PIXEL_FAST(buf_row_ptr, x, pixel);
                }

                linebuf_copy_binary(y, ksize, brows, &buf, img);
            }

            linebuf_copy_binary_tail(ksize, brows, &buf, img);
            fb_free();
            break;
        }
        case PIXFORMAT_GRAYSCALE: {
            buf.data = fb_alloc(IMAGE_GRAYSCALE_LINE_LEN_BYTES(img) * brows, FB_ALLOC_PREFER_SPEED);
            // Usage of singed math in the loop doesn't matter as the final compare is unsigned.
            bool u8_overflow = sum > UINT8_MAX;
            int32_t out = e_or_d ? COLOR_GRAYSCALE_BINARY_MAX : COLOR_GRAYSCALE_BINARY_MIN;
            v128_t vthreshold = vdup_u8(threshold);
            v128_t vout = vdup_u8(out);

            for (int32_t y = 0; y < img->h; y++) {
                uint8_t *row_ptr = IMAGE_COMPUTE_GRAYSCALE_PIXEL_ROW_PTR(img, y);
                uint8_t *buf_row_ptr = IMAGE_COMPUTE_GRAYSCALE_PIXEL_ROW_PTR(&buf, (y % brows));
                int32_t acc = 0;

                for (int32_t x = 0; x < img->w; x++) {
                    if (mask && (!image_get_mask_pixel(mask, x, y))) {
                        int32_t p = IMAGE_GET_GRAYSCALE_PIXEL_FAST(row_ptr, x);
                        IMAGE_PUT_GRAYSCALE_PIXEL_FAST(buf_row_ptr, x, p);
                        continue; // Short circuit.
                    }

                    if (!mask && x > ksize && x < img->w - ksize && y >= ksize && y < img->h - ksize) {
                        int32_t remaining = (img->w - ksize) - x;
                        if ((!u8_overflow) && (remaining >= vpredicate_8_maybe_min_elements())) {
                            for (;;) {
                                v128_predicate_t pred = vpredicate_8_maybe(remaining);
                                v128_t vacc = vset_u8(vdup_u8(0), 0, acc);

                                for (int32_t j = -ksize; j <= ksize; j++) {
                                    uint8_t *k_row_ptr = IMAGE_COMPUTE_GRAYSCALE_PIXEL_ROW_PTR(img, y + j) + x;
                                    // subtract old left edge from sum
                                    v128_t sub_pixels = vldr_u8_pred(k_row_ptr - ksize - 1, pred);
                                    sub_pixels = vcmpgesel_u8(sub_pixels, vdup_u8(1), vdup_u8(1), vdup_u8(0));
                                    vacc = vsub_s8(vacc, sub_pixels);
                                    // add new right edge to sum
                                    v128_t add_pixels = vldr_u8_pred(k_row_ptr + ksize, pred);
                                    add_pixels = vcmpgesel_u8(add_pixels, vdup_u8(1), vdup_u8(1), vdup_u8(0));
                                    vacc = vadd_s8(vacc, add_pixels);
                                }

                                // After the loop the lowest 8-bits contain a valid acc value. We
                                // compute the next acc values by adding the sums up.

                                vacc = vsum_up_s8(vacc);

                                // Extract final acc value for the next pass.

                                acc = vget_s8(vacc, INT8_VECTOR_SIZE - 1);

                                // Compute all erode/dilate thresholds at once.

                                v128_t old_pixels = vldr_u8_pred(row_ptr + x, pred);
                                v128_t v0 = e_or_d ? vthreshold : vacc;
                                v128_t v1 = e_or_d ? vacc : vthreshold;
                                v128_t pixels = vcmpgesel_u8(v0, v1, old_pixels, vout);

                                vstr_u8_pred(buf_row_ptr + x, pixels, pred);
                                uint32_t inc = vpredicate_8_get_n(pred);
                                remaining -= inc;

                                if (remaining < vpredicate_8_maybe_min_elements()) {
                                    x += inc - 1;
                                    break;
                                }

                                x += inc;
                            }

                            continue;
                        } else {
                            for (int32_t j = -ksize; j <= ksize; j++) {
                                uint8_t *k_row_ptr = IMAGE_COMPUTE_GRAYSCALE_PIXEL_ROW_PTR(img, y + j) + x;
                                // subtract old left edge and add new right edge to sum
                                acc -= IMAGE_GET_GRAYSCALE_PIXEL_FAST(k_row_ptr, -ksize - 1) > 0;
                                acc += IMAGE_GET_GRAYSCALE_PIXEL_FAST(k_row_ptr, ksize) > 0;
                            }
                        }
                    } else {
                        acc = e_or_d ? 0 : -1; // Don't count center pixel...
                        for (int32_t j = -ksize; j <= ksize; j++) {
                            int32_t y_j = IM_CLAMP(y + j, 0, (img->h - 1));
                            uint8_t *k_row_ptr = IMAGE_COMPUTE_GRAYSCALE_PIXEL_ROW_PTR(img, y_j);

                            for (int32_t k = -ksize; k <= ksize; k++) {
                                int32_t x_k = IM_CLAMP(x + k, 0, (img->w - 1));
                                acc += IMAGE_GET_GRAYSCALE_PIXEL_FAST(k_row_ptr, x_k) > 0;
                            }
                        }
                    }

                    int32_t old_pixel = IMAGE_GET_GRAYSCALE_PIXEL_FAST(row_ptr, x);
                    int32_t v0 = e_or_d ? threshold : acc;
                    int32_t v1 = e_or_d ? acc : threshold;
                    int32_t pixel = (v0 >= v1) ? old_pixel : out;
                    IMAGE_PUT_GRAYSCALE_PIXEL_FAST(buf_row_ptr, x, pixel);
                }

                linebuf_copy_grayscale(y, ksize, brows, &buf, img);
            }

            linebuf_copy_grayscale_tail(ksize, brows, &buf, img);
            fb_free();
            break;
        }
        case PIXFORMAT_RGB565: {
            buf.data = fb_alloc(IMAGE_RGB565_LINE_LEN_BYTES(img) * brows, FB_ALLOC_PREFER_SPEED);
            // Usage of singed math in the loop doesn't matter as the final compare is unsigned.
            bool u16_overflow = sum > UINT16_MAX;
            int32_t out = e_or_d ? COLOR_RGB565_BINARY_MAX : COLOR_RGB565_BINARY_MIN;
            v128_t vthreshold = vdup_u16(threshold);
            v128_t vout = vdup_u16(out);

            for (int32_t y = 0; y < img->h; y++) {
                uint16_t *row_ptr = IMAGE_COMPUTE_RGB565_PIXEL_ROW_PTR(img, y);
                uint16_t *buf_row_ptr = IMAGE_COMPUTE_RGB565_PIXEL_ROW_PTR(&buf, (y % brows));
                int32_t acc = 0;

                for (int32_t x = 0; x < img->w; x++) {
                    if (mask && (!image_get_mask_pixel(mask, x, y))) {
                        int32_t p = IMAGE_GET_RGB565_PIXEL_FAST(row_ptr, x);
                        IMAGE_PUT_RGB565_PIXEL_FAST(buf_row_ptr, x, p);
                        continue; // Short circuit.
                    }

                    if (!mask && x > ksize && x < img->w - ksize && y >= ksize && y < img->h - ksize) {
                        int32_t remaining = (img->w - ksize) - x;
                        if ((!u16_overflow) && (remaining >= vpredicate_16_maybe_min_elements())) {
                            for (;;) {
                                v128_predicate_t pred = vpredicate_16_maybe(remaining);
                                v128_t vacc = vset_u16(vdup_u16(0), 0, acc);

                                for (int32_t j = -ksize; j <= ksize; j++) {
                                    uint16_t *k_row_ptr = IMAGE_COMPUTE_RGB565_PIXEL_ROW_PTR(img, y + j) + x;
                                    // subtract old left edge from sum
                                    v128_t sub_pixels = vldr_u16_pred(k_row_ptr - ksize - 1, pred);
                                    sub_pixels = vcmpgesel_u16(sub_pixels, vdup_u16(1), vdup_u16(1), vdup_u16(0));
                                    vacc = vsub_s16(vacc, sub_pixels);
                                    // add new right edge to sum
                                    v128_t add_pixels = vldr_u16_pred(k_row_ptr + ksize, pred);
                                    add_pixels = vcmpgesel_u16(add_pixels, vdup_u16(1), vdup_u16(1), vdup_u16(0));
                                    vacc = vadd_s16(vacc, add_pixels);
                                }

                                // After the loop the lowest 16-bits contain a valid acc value. We
                                // compute the next acc values by adding the sums up.

                                vacc = vsum_up_s16(vacc);

                                // Extract final acc value for the next pass.

                                acc = vget_s16(vacc, INT16_VECTOR_SIZE - 1);

                                // Compute all erode/dilate thresholds at once.

                                v128_t old_pixels = vldr_u16_pred(row_ptr + x, pred);
                                v128_t v0 = e_or_d ? vthreshold : vacc;
                                v128_t v1 = e_or_d ? vacc : vthreshold;
                                v128_t pixels = vcmpgesel_u16(v0, v1, old_pixels, vout);

                                vstr_u16_pred(buf_row_ptr + x, pixels, pred);
                                uint32_t inc = vpredicate_16_get_n(pred);
                                remaining -= inc;

                                if (remaining < vpredicate_16_maybe_min_elements()) {
                                    x += inc - 1;
                                    break;
                                }

                                x += inc;
                            }

                            continue;
                        } else {
                            for (int32_t j = -ksize; j <= ksize; j++) {
                                uint16_t *k_row_ptr = IMAGE_COMPUTE_RGB565_PIXEL_ROW_PTR(img, y + j) + x;
                                // subtract old left column and add new right column
                                acc -= IMAGE_GET_RGB565_PIXEL_FAST(k_row_ptr, -ksize - 1) > 0;
                                acc += IMAGE_GET_RGB565_PIXEL_FAST(k_row_ptr, ksize) > 0;
                            }
                        }
                    } else {
                        acc = e_or_d ? 0 : -1; // Don't count center pixel...
                        for (int32_t j = -ksize; j <= ksize; j++) {
                            int32_t y_j = IM_CLAMP(y + j, 0, (img->h - 1));
                            uint16_t *k_row_ptr = IMAGE_COMPUTE_RGB565_PIXEL_ROW_PTR(img, y_j);

                            for (int32_t k = -ksize; k <= ksize; k++) {
                                int32_t x_k = IM_CLAMP(x + k, 0, (img->w - 1));
                                acc += IMAGE_GET_RGB565_PIXEL_FAST(k_row_ptr, x_k) > 0;
                            }
                        }
                    }

                    int32_t old_pixel = IMAGE_GET_RGB565_PIXEL_FAST(row_ptr, x);
                    int32_t v0 = e_or_d ? threshold : acc;
                    int32_t v1 = e_or_d ? acc : threshold;
                    int32_t pixel = (v0 >= v1) ? old_pixel : out;
                    IMAGE_PUT_RGB565_PIXEL_FAST(buf_row_ptr, x, pixel);
                }

                linebuf_copy_rgb565(y, ksize, brows, &buf, img);
            }

            linebuf_copy_rgb565_tail(ksize, brows, &buf, img);
            fb_free();
            break;
        }
        default: {
            __builtin_unreachable();
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
