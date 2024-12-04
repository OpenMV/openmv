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
 * Stero Image Disparity
 */
#include "imlib.h"

#ifdef IMLIB_ENABLE_STEREO_DISPARITY

#define BLOCK_W       4
#define BLOCK_H       4

#define BLOCK_W_L     (((BLOCK_W) / 2) - 1)
#define BLOCK_W_R     ((BLOCK_W) / 2)
#define BLOCK_H_U     (((BLOCK_H) / 2) - 1)
#define BLOCK_H_D     ((BLOCK_H) / 2)

#define BLOCK_SIZE    ((BLOCK_W) *(BLOCK_H))

static inline void shift(uint8_t *data, image_t *img, int x, int y) {
#if defined(ARM_MATH_DSP) && (!(BLOCK_SIZE % 4))
    int x_2 = x + BLOCK_W_R;
    uint32_t *data_32 = (uint32_t *) data;

    #if BLOCK_SIZE == 4
    uint32_t temp = __UXTB16_RORn(*data_32, 8);
    temp |= IMAGE_GET_GRAYSCALE_PIXEL(img, x_2, y) << 8;
    temp |= IMAGE_GET_GRAYSCALE_PIXEL(img, x_2, y + 1) << 24;
    *data_32 = temp;
    #elif BLOCK_SIZE == 16
    data_32[0] = (data_32[0] >> 8) | (IMAGE_GET_GRAYSCALE_PIXEL(img, x_2, y - 1) << 24);
    data_32[1] = (data_32[1] >> 8) | (IMAGE_GET_GRAYSCALE_PIXEL(img, x_2, y) << 24);
    data_32[2] = (data_32[2] >> 8) | (IMAGE_GET_GRAYSCALE_PIXEL(img, x_2, y + 1) << 24);
    data_32[3] = (data_32[3] >> 8) | (IMAGE_GET_GRAYSCALE_PIXEL(img, x_2, y + 2) << 24);
    #elif BLOCK_SIZE == 64
    data_32[0] = (data_32[0] >> 8) | (data_32[1] << 24);
    data_32[1] = (data_32[1] >> 8) | (IMAGE_GET_GRAYSCALE_PIXEL(img, x_2, y - 3) << 24);
    data_32[2] = (data_32[2] >> 8) | (data_32[3] << 24);
    data_32[3] = (data_32[3] >> 8) | (IMAGE_GET_GRAYSCALE_PIXEL(img, x_2, y - 2) << 24);
    data_32[4] = (data_32[4] >> 8) | (data_32[5] << 24);
    data_32[5] = (data_32[5] >> 8) | (IMAGE_GET_GRAYSCALE_PIXEL(img, x_2, y - 1) << 24);
    data_32[6] = (data_32[6] >> 8) | (data_32[7] << 24);
    data_32[7] = (data_32[7] >> 8) | (IMAGE_GET_GRAYSCALE_PIXEL(img, x_2, y) << 24);
    data_32[8] = (data_32[8] >> 8) | (data_32[9] << 24);
    data_32[9] = (data_32[9] >> 8) | (IMAGE_GET_GRAYSCALE_PIXEL(img, x_2, y + 1) << 24);
    data_32[10] = (data_32[10] >> 8) | (data_32[11] << 24);
    data_32[11] = (data_32[11] >> 8) | (IMAGE_GET_GRAYSCALE_PIXEL(img, x_2, y + 2) << 24);
    data_32[12] = (data_32[12] >> 8) | (data_32[13] << 24);
    data_32[13] = (data_32[13] >> 8) | (IMAGE_GET_GRAYSCALE_PIXEL(img, x_2, y + 3) << 24);
    data_32[14] = (data_32[14] >> 8) | (data_32[15] << 24);
    data_32[15] = (data_32[15] >> 8) | (IMAGE_GET_GRAYSCALE_PIXEL(img, x_2, y + 4) << 24);
    #else
    for (int j = -BLOCK_H_U, last = (BLOCK_W / sizeof(uint32_t)) - 1; j <= BLOCK_H_D; j++, data_32 += last + 1) {
        for (int i = 0; i < last; i++) {
            data_32[i] = (data_32[i] >> 8) | (data_32[i + 1] << 24);
        }
        data_32[last] = (data_32[last] >> 8) | (IMAGE_GET_GRAYSCALE_PIXEL(img, x_2, y + j) << 24);
    }
    #endif
#else
    for (int i = 0, j = -BLOCK_H_U; j <= BLOCK_H_D; j++) {
        int y_p = y + j;
        uint8_t *k_row_ptr = IMAGE_COMPUTE_GRAYSCALE_PIXEL_ROW_PTR(img, y_p);
        for (int k = -BLOCK_W_L; k <= BLOCK_W_R; k++) {
            int x_p = x + k;
            data[i++] = IMAGE_GET_GRAYSCALE_PIXEL_FAST(k_row_ptr, x_p);
        }
    }
#endif
}

static inline uint32_t sad(uint8_t *data_l, uint8_t *data_r) {
    uint32_t diff = 0;

#if defined(ARM_MATH_DSP) && (!(BLOCK_SIZE % 4))
    uint32_t *data_l_32 = (uint32_t *) data_l;
    uint32_t *data_r_32 = (uint32_t *) data_r;

    #if BLOCK_SIZE == 4
    diff = __USADA8(*data_l_32, *data_r_32, diff);
    #elif BLOCK_SIZE == 16
    diff = __USADA8(data_l_32[0], data_r_32[0], diff);
    diff = __USADA8(data_l_32[1], data_r_32[1], diff);
    diff = __USADA8(data_l_32[2], data_r_32[2], diff);
    diff = __USADA8(data_l_32[3], data_r_32[3], diff);
    #elif BLOCK_SIZE == 64
    diff = __USADA8(data_l_32[0],  data_r_32[0],  diff);
    diff = __USADA8(data_l_32[1],  data_r_32[1],  diff);
    diff = __USADA8(data_l_32[2],  data_r_32[2],  diff);
    diff = __USADA8(data_l_32[3],  data_r_32[3],  diff);
    diff = __USADA8(data_l_32[4],  data_r_32[4],  diff);
    diff = __USADA8(data_l_32[5],  data_r_32[5],  diff);
    diff = __USADA8(data_l_32[6],  data_r_32[6],  diff);
    diff = __USADA8(data_l_32[7],  data_r_32[7],  diff);
    diff = __USADA8(data_l_32[8],  data_r_32[8],  diff);
    diff = __USADA8(data_l_32[9],  data_r_32[9],  diff);
    diff = __USADA8(data_l_32[10], data_r_32[10], diff);
    diff = __USADA8(data_l_32[11], data_r_32[11], diff);
    diff = __USADA8(data_l_32[12], data_r_32[12], diff);
    diff = __USADA8(data_l_32[13], data_r_32[13], diff);
    diff = __USADA8(data_l_32[14], data_r_32[14], diff);
    diff = __USADA8(data_l_32[15], data_r_32[15], diff);
    #else
    for (int i = 0; i < (BLOCK_SIZE / sizeof(uint32_t)); i++) {
        diff = __USADA8(data_l_32[i], data_r_32[i], diff);
    }
    #endif
#else
    for (int i = 0; i < BLOCK_SIZE; i++) {
        int d = data_l[i] - data_r[i];
        diff += abs(d);
    }
#endif

    return diff;
}

void imlib_stereo_disparity(image_t *img, bool reversed, int max_disparity, int threshold) {
    int width_2 = img->w / 2, width_2_m_1 = width_2 - 1;
    int height_1 = img->h, height_1_m_1 = height_1 - 1;

    int xl_offset = 0;
    int xr_offset = width_2;

    if (reversed) {
        xl_offset = xr_offset;
        xr_offset = 0;
    }

    float disparity_scale = COLOR_GRAYSCALE_MAX / max_disparity;

    image_t buf;
    buf.w = width_2;
    buf.h = BLOCK_H_D;
    buf.pixfmt = img->pixfmt;

    switch (img->pixfmt) {
        case PIXFORMAT_BINARY: {
            break;
        }
        case PIXFORMAT_GRAYSCALE: {
            buf.data = fb_alloc(IMAGE_GRAYSCALE_LINE_LEN_BYTES(&buf) * BLOCK_H_D);
            uint8_t *data_l = fb_alloc(BLOCK_SIZE);
            uint8_t *data_r = fb_alloc(BLOCK_SIZE);

            for (int y = 0, yy = height_1 - BLOCK_H_D; y < height_1; y++) {
                for (int xl = 0, xx = width_2 - BLOCK_W_R; xl < width_2; xl++) {
                    if ((xl >= BLOCK_W_L) && (xl < xx) && (y >= BLOCK_H_U) && (y < yy)) {
                        // fast way
                        shift(data_l, img, xl + xl_offset, y);
                    } else {
                        // slow way
                        for (int i = 0, j = -BLOCK_H_U; j <= BLOCK_H_D; j++) {
                            int y_p = y + j;
                            int y = IM_CLAMP(y_p, 0, height_1_m_1);
                            uint8_t *k_row_ptr = IMAGE_COMPUTE_GRAYSCALE_PIXEL_ROW_PTR(img, y);
                            for (int k = -BLOCK_W_L; k <= BLOCK_W_R; k++) {
                                int x_p = xl + k;
                                int x = IM_CLAMP(x_p, 0, width_2_m_1) + xl_offset;
                                data_l[i++] = IMAGE_GET_GRAYSCALE_PIXEL_FAST(k_row_ptr, x);
                            }
                        }
                    }

                    uint32_t min_diff = UINT32_MAX;
                    int min_disparity = 0;

                    for (int xr = xl, disparity = 0;
                         (xr < width_2) && (disparity <= max_disparity); xr++, disparity++) {
                        if (disparity && (xr >= BLOCK_W_L) && (xr < xx) && (y >= BLOCK_H_U) && (y < yy)) {
                            // fast way
                            shift(data_r, img, xr + xr_offset, y);
                        } else {
                            // slow way
                            for (int i = 0, j = -BLOCK_H_U; j <= BLOCK_H_D; j++) {
                                int y_p = y + j;
                                int y = IM_CLAMP(y_p, 0, height_1_m_1);
                                uint8_t *k_row_ptr = IMAGE_COMPUTE_GRAYSCALE_PIXEL_ROW_PTR(img, y);
                                for (int k = -BLOCK_W_L; k <= BLOCK_W_R; k++) {
                                    int x_p = xr + k;
                                    int x = IM_CLAMP(x_p, 0, width_2_m_1) + xr_offset;
                                    data_r[i++] = IMAGE_GET_GRAYSCALE_PIXEL_FAST(k_row_ptr, x);
                                }
                            }
                        }

                        // Record the closest matching block in the scan line.
                        uint32_t diff = sad(data_l, data_r);
                        if (diff < min_diff) {
                            min_diff = diff;
                            min_disparity = disparity;
                        }

                        if (min_diff <= threshold) {
                            break;
                        }
                    }

                    uint8_t *buf_row_ptr = IMAGE_COMPUTE_GRAYSCALE_PIXEL_ROW_PTR(&buf, (y % BLOCK_H_D));
                    IMAGE_PUT_GRAYSCALE_PIXEL_FAST(buf_row_ptr, xl, fast_floorf(min_disparity * disparity_scale));
                }

                if (y >= BLOCK_H_U) {
                    // Transfer buffer lines...
                    memcpy(IMAGE_COMPUTE_GRAYSCALE_PIXEL_ROW_PTR(img, (y - BLOCK_H_U)) + xr_offset,
                           IMAGE_COMPUTE_GRAYSCALE_PIXEL_ROW_PTR(&buf, ((y - BLOCK_H_U) % BLOCK_H_D)),
                           IMAGE_GRAYSCALE_LINE_LEN_BYTES(&buf));
                }
            }

            // Copy any remaining lines from the buffer image...
            for (int y = IM_MAX(height_1 - BLOCK_H_U, 0); y < height_1; y++) {
                memcpy(IMAGE_COMPUTE_GRAYSCALE_PIXEL_ROW_PTR(img, y) + xr_offset,
                       IMAGE_COMPUTE_GRAYSCALE_PIXEL_ROW_PTR(&buf, (y % BLOCK_H_D)),
                       IMAGE_GRAYSCALE_LINE_LEN_BYTES(&buf));
            }

            fb_free(); // data_r
            fb_free(); // data_l
            fb_free(); // buf
            break;
        }
        case PIXFORMAT_RGB565: {
            break;
        }
        default: {
            break;
        }
    }
}

#endif // IMLIB_ENABLE_STEREO_DISPARITY
