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
 * Image filtering functions.
 */
#include "fsort.h"
#include "imlib.h"

void imlib_histeq(image_t *img, image_t *mask) {
    switch (img->pixfmt) {
        case PIXFORMAT_BINARY: {
            int a = img->w * img->h;
            float s = (COLOR_BINARY_MAX - COLOR_BINARY_MIN) / ((float) a);
            uint32_t *hist = fb_alloc0((COLOR_BINARY_MAX - COLOR_BINARY_MIN + 1) * sizeof(uint32_t), FB_ALLOC_NO_HINT);

            for (int y = 0, yy = img->h; y < yy; y++) {
                uint32_t *row_ptr = IMAGE_COMPUTE_BINARY_PIXEL_ROW_PTR(img, y);
                for (int x = 0, xx = img->w; x < xx; x++) {
                    hist[IMAGE_GET_BINARY_PIXEL_FAST(row_ptr, x) - COLOR_BINARY_MIN] += 1;
                }
            }

            for (int i = 0, sum = 0, ii = COLOR_BINARY_MAX - COLOR_BINARY_MIN + 1; i < ii; i++) {
                sum += hist[i];
                hist[i] = sum;
            }

            for (int y = 0, yy = img->h; y < yy; y++) {
                uint32_t *row_ptr = IMAGE_COMPUTE_BINARY_PIXEL_ROW_PTR(img, y);
                for (int x = 0, xx = img->w; x < xx; x++) {
                    if (mask && (!image_get_mask_pixel(mask, x, y))) {
                        continue;
                    }
                    int pixel = IMAGE_GET_BINARY_PIXEL_FAST(row_ptr, x);
                    IMAGE_PUT_BINARY_PIXEL_FAST(row_ptr, x,
                                                fast_floorf((s * hist[pixel - COLOR_BINARY_MIN]) + COLOR_BINARY_MIN));
                }
            }

            fb_free();
            break;
        }
        case PIXFORMAT_GRAYSCALE: {
            int a = img->w * img->h;
            float s = (COLOR_GRAYSCALE_MAX - COLOR_GRAYSCALE_MIN) / ((float) a);
            uint32_t *hist = fb_alloc0((COLOR_GRAYSCALE_MAX - COLOR_GRAYSCALE_MIN + 1) * sizeof(uint32_t), FB_ALLOC_NO_HINT);

            for (int y = 0, yy = img->h; y < yy; y++) {
                uint8_t *row_ptr = IMAGE_COMPUTE_GRAYSCALE_PIXEL_ROW_PTR(img, y);
                for (int x = 0, xx = img->w; x < xx; x++) {
                    hist[IMAGE_GET_GRAYSCALE_PIXEL_FAST(row_ptr, x) - COLOR_GRAYSCALE_MIN] += 1;
                }
            }

            for (int i = 0, sum = 0, ii = COLOR_GRAYSCALE_MAX - COLOR_GRAYSCALE_MIN + 1; i < ii; i++) {
                sum += hist[i];
                hist[i] = sum;
            }

            for (int y = 0, yy = img->h; y < yy; y++) {
                uint8_t *row_ptr = IMAGE_COMPUTE_GRAYSCALE_PIXEL_ROW_PTR(img, y);
                for (int x = 0, xx = img->w; x < xx; x++) {
                    if (mask && (!image_get_mask_pixel(mask, x, y))) {
                        continue;
                    }
                    int pixel = IMAGE_GET_GRAYSCALE_PIXEL_FAST(row_ptr, x);
                    IMAGE_PUT_GRAYSCALE_PIXEL_FAST(row_ptr, x,
                                                   fast_floorf((s * hist[pixel - COLOR_GRAYSCALE_MIN]) + COLOR_GRAYSCALE_MIN));
                }
            }

            fb_free();
            break;
        }
        case PIXFORMAT_RGB565: {
            int a = img->w * img->h;
            float s = (COLOR_Y_MAX - COLOR_Y_MIN) / ((float) a);
            uint32_t *hist = fb_alloc0((COLOR_Y_MAX - COLOR_Y_MIN + 1) * sizeof(uint32_t), FB_ALLOC_NO_HINT);

            for (int y = 0, yy = img->h; y < yy; y++) {
                uint16_t *row_ptr = IMAGE_COMPUTE_RGB565_PIXEL_ROW_PTR(img, y);
                for (int x = 0, xx = img->w; x < xx; x++) {
                    hist[COLOR_RGB565_TO_Y(IMAGE_GET_RGB565_PIXEL_FAST(row_ptr, x)) - COLOR_Y_MIN] += 1;
                }
            }

            for (int i = 0, sum = 0, ii = COLOR_Y_MAX - COLOR_Y_MIN + 1; i < ii; i++) {
                sum += hist[i];
                hist[i] = sum;
            }

            for (int y = 0, yy = img->h; y < yy; y++) {
                uint16_t *row_ptr = IMAGE_COMPUTE_RGB565_PIXEL_ROW_PTR(img, y);
                for (int x = 0, xx = img->w; x < xx; x++) {
                    if (mask && (!image_get_mask_pixel(mask, x, y))) {
                        continue;
                    }
                    int pixel = IMAGE_GET_RGB565_PIXEL_FAST(row_ptr, x);
                    int r = COLOR_RGB565_TO_R8(pixel);
                    int g = COLOR_RGB565_TO_G8(pixel);
                    int b = COLOR_RGB565_TO_B8(pixel);
                    uint8_t y, u, v;
                    y = (uint8_t) (((r * 9770) + (g * 19182) + (b * 3736)) >> 15); // .299*r + .587*g + .114*b
                    u = (uint8_t) (((b << 14) - (r * 5529) - (g * 10855)) >> 15);  // -0.168736*r + -0.331264*g + 0.5*b
                    v = (uint8_t) (((r << 14) - (g * 13682) - (b * 2664)) >> 15);  // 0.5*r + -0.418688*g + -0.081312*b
                    IMAGE_PUT_RGB565_PIXEL_FAST(row_ptr, x, imlib_yuv_to_rgb(fast_floorf(s * hist[y]), u, v));
                }
            }

            fb_free();
            break;
        }
        default: {
            break;
        }
    }
}

// ksize == 0 -> 1x1 kernel
// ksize == 1 -> 3x3 kernel
// ...
// ksize == n -> ((n*2)+1)x((n*2)+1) kernel
//
// To speed up this filter, we can help in two ways:
// 1) For the 'center portion' of the image area, we don't need to check
//   the x+y values against the boundary conditions on each pixel.
// 2) In that same region we can take advantage of the filter property being
//   the sum of all of the pixels by subtracting the last left edge values
//   and adding the new right edge values instead of re-calculating the sum
//   of every pixel. This will allow very large filters to be used without
//   much change in performance.
//
#ifdef IMLIB_ENABLE_MEAN
void imlib_mean_filter(image_t *img, const int ksize, bool threshold, int offset, bool invert, image_t *mask) {
    int brows = ksize + 1;
    image_t buf;
    buf.w = img->w;
    buf.h = brows;
    buf.pixfmt = img->pixfmt;

    int32_t over32_n = 65536 / (((ksize * 2) + 1) * ((ksize * 2) + 1));

    switch (img->pixfmt) {
        case PIXFORMAT_BINARY: {
            buf.data = fb_alloc(IMAGE_BINARY_LINE_LEN_BYTES(img) * brows, FB_ALLOC_NO_HINT);

            for (int y = 0, yy = img->h; y < yy; y++) {
                int pixel, acc = 0;
                uint32_t *row_ptr = IMAGE_COMPUTE_BINARY_PIXEL_ROW_PTR(img, y);
                uint32_t *buf_row_ptr = IMAGE_COMPUTE_BINARY_PIXEL_ROW_PTR(&buf, (y % brows));

                for (int x = 0, xx = img->w; x < xx; x++) {
                    if (mask && (!image_get_mask_pixel(mask, x, y))) {
                        IMAGE_PUT_BINARY_PIXEL_FAST(buf_row_ptr, x, IMAGE_GET_BINARY_PIXEL_FAST(row_ptr, x));
                        continue; // Short circuit.
                    }

                    if (!mask && x > ksize && x < img->w - ksize && y >= ksize && y < img->h - ksize) {
                        for (int j = -ksize; j <= ksize; j++) {
                            uint32_t *k_row_ptr = IMAGE_COMPUTE_BINARY_PIXEL_ROW_PTR(img, y + j);
                            acc -= IMAGE_GET_BINARY_PIXEL_FAST(k_row_ptr, x - ksize - 1);
                            acc += IMAGE_GET_BINARY_PIXEL_FAST(k_row_ptr, x + ksize);
                        }
                    } else {
                        acc = 0;

                        for (int j = -ksize; j <= ksize; j++) {
                            int y_j = IM_CLAMP(y + j, 0, (img->h - 1));
                            uint32_t *k_row_ptr = IMAGE_COMPUTE_BINARY_PIXEL_ROW_PTR(img, y_j);

                            for (int k = -ksize; k <= ksize; k++) {
                                int x_k = IM_CLAMP(x + k, 0, (img->w - 1));
                                acc += IMAGE_GET_BINARY_PIXEL_FAST(k_row_ptr, x_k);
                            }
                        }
                    }

                    pixel = (int) ((acc * over32_n) >> 16);

                    if (threshold) {
                        if (((pixel - offset) < IMAGE_GET_BINARY_PIXEL_FAST(row_ptr, x)) ^ invert) {
                            pixel = COLOR_BINARY_MAX;
                        } else {
                            pixel = COLOR_BINARY_MIN;
                        }
                    }

                    IMAGE_PUT_BINARY_PIXEL_FAST(buf_row_ptr, x, pixel);
                }

                if (y >= ksize) {
                    // Transfer buffer lines...
                    memcpy(IMAGE_COMPUTE_BINARY_PIXEL_ROW_PTR(img, (y - ksize)),
                           IMAGE_COMPUTE_BINARY_PIXEL_ROW_PTR(&buf, ((y - ksize) % brows)),
                           IMAGE_BINARY_LINE_LEN_BYTES(img));
                }
            }

            // Copy any remaining lines from the buffer image...
            for (int y = IM_MAX(img->h - ksize, 0), yy = img->h; y < yy; y++) {
                memcpy(IMAGE_COMPUTE_BINARY_PIXEL_ROW_PTR(img, y),
                       IMAGE_COMPUTE_BINARY_PIXEL_ROW_PTR(&buf, (y % brows)),
                       IMAGE_BINARY_LINE_LEN_BYTES(img));
            }

            fb_free();
            break;
        }
        case PIXFORMAT_GRAYSCALE: {
            buf.data = fb_alloc(IMAGE_GRAYSCALE_LINE_LEN_BYTES(img) * brows, FB_ALLOC_NO_HINT);

            for (int y = 0, yy = img->h; y < yy; y++) {
                int pixel, acc = 0;
                uint8_t *row_ptr = IMAGE_COMPUTE_GRAYSCALE_PIXEL_ROW_PTR(img, y);
                uint8_t *buf_row_ptr = IMAGE_COMPUTE_GRAYSCALE_PIXEL_ROW_PTR(&buf, (y % brows));

                for (int x = 0, xx = img->w; x < xx; x++) {
                    if (mask && (!image_get_mask_pixel(mask, x, y))) {
                        IMAGE_PUT_GRAYSCALE_PIXEL_FAST(buf_row_ptr, x, IMAGE_GET_GRAYSCALE_PIXEL_FAST(row_ptr, x));
                        continue; // Short circuit.
                    }
                    if (!mask && x > ksize && x < img->w - ksize && y >= ksize && y < img->h - ksize) {
                        for (int j = -ksize; j <= ksize; j++) {
                            uint8_t *k_row_ptr = IMAGE_COMPUTE_GRAYSCALE_PIXEL_ROW_PTR(img, y + j);
                            acc -= IMAGE_GET_GRAYSCALE_PIXEL_FAST(k_row_ptr, x - ksize - 1);
                            acc += IMAGE_GET_GRAYSCALE_PIXEL_FAST(k_row_ptr, x + ksize);
                        }
                    } else {
                        acc = 0;
                        for (int j = -ksize; j <= ksize; j++) {
                            int y_j = IM_CLAMP(y + j, 0, (img->h - 1));
                            uint8_t *k_row_ptr = IMAGE_COMPUTE_GRAYSCALE_PIXEL_ROW_PTR(img, y_j);

                            for (int k = -ksize; k <= ksize; k++) {
                                int x_k = IM_CLAMP(x + k, 0, (img->w - 1));
                                acc += IMAGE_GET_GRAYSCALE_PIXEL_FAST(k_row_ptr, x_k);
                            }
                        }
                    }

                    pixel = (int) ((acc * over32_n) >> 16);

                    if (threshold) {
                        if (((pixel - offset) < IMAGE_GET_GRAYSCALE_PIXEL_FAST(row_ptr, x)) ^ invert) {
                            pixel = COLOR_GRAYSCALE_BINARY_MAX;
                        } else {
                            pixel = COLOR_GRAYSCALE_BINARY_MIN;
                        }
                    }

                    IMAGE_PUT_GRAYSCALE_PIXEL_FAST(buf_row_ptr, x, pixel);
                }

                if (y >= ksize) {
                    // Transfer buffer lines...
                    memcpy(IMAGE_COMPUTE_GRAYSCALE_PIXEL_ROW_PTR(img, (y - ksize)),
                           IMAGE_COMPUTE_GRAYSCALE_PIXEL_ROW_PTR(&buf, ((y - ksize) % brows)),
                           IMAGE_GRAYSCALE_LINE_LEN_BYTES(img));
                }
            }

            // Copy any remaining lines from the buffer image...
            for (int y = IM_MAX(img->h - ksize, 0), yy = img->h; y < yy; y++) {
                memcpy(IMAGE_COMPUTE_GRAYSCALE_PIXEL_ROW_PTR(img, y),
                       IMAGE_COMPUTE_GRAYSCALE_PIXEL_ROW_PTR(&buf, (y % brows)),
                       IMAGE_GRAYSCALE_LINE_LEN_BYTES(img));
            }

            fb_free();
            break;
        }
        case PIXFORMAT_RGB565: {
            int pixel, r, g, b, r_acc, g_acc, b_acc;
            buf.data = fb_alloc(IMAGE_RGB565_LINE_LEN_BYTES(img) * brows, FB_ALLOC_NO_HINT);

            for (int y = 0, yy = img->h; y < yy; y++) {
                uint16_t *row_ptr = IMAGE_COMPUTE_RGB565_PIXEL_ROW_PTR(img, y);
                uint16_t *buf_row_ptr = IMAGE_COMPUTE_RGB565_PIXEL_ROW_PTR(&buf, (y % brows));

                r_acc = g_acc = b_acc = 0;
                for (int x = 0, xx = img->w; x < xx; x++) {
                    if (mask && (!image_get_mask_pixel(mask, x, y))) {
                        IMAGE_PUT_RGB565_PIXEL_FAST(buf_row_ptr, x, IMAGE_GET_RGB565_PIXEL_FAST(row_ptr, x));
                        continue; // Short circuit.
                    }
                    if (!mask && x > ksize && x < img->w - ksize && y >= ksize && y < img->h - ksize) {
                        for (int j = -ksize; j <= ksize; j++) {
                            uint16_t *k_row_ptr = IMAGE_COMPUTE_RGB565_PIXEL_ROW_PTR(img, y + j);
                            // subtract last left-most pixel from the sums
                            pixel = IMAGE_GET_RGB565_PIXEL_FAST(k_row_ptr, x - ksize - 1);
                            r_acc -= COLOR_RGB565_TO_R5(pixel);
                            g_acc -= COLOR_RGB565_TO_G6(pixel);
                            b_acc -= COLOR_RGB565_TO_B5(pixel);
                            // add new right edge pixel to the sums
                            pixel = IMAGE_GET_RGB565_PIXEL_FAST(k_row_ptr, x + ksize);
                            r_acc += COLOR_RGB565_TO_R5(pixel);
                            g_acc += COLOR_RGB565_TO_G6(pixel);
                            b_acc += COLOR_RGB565_TO_B5(pixel);
                        }
                    } else {
                        // check bounds and do full sum calculations
                        r_acc = g_acc = b_acc = 0;
                        for (int j = -ksize; j <= ksize; j++) {
                            int y_j = IM_CLAMP(y + j, 0, (img->h - 1));
                            uint16_t *k_row_ptr = IMAGE_COMPUTE_RGB565_PIXEL_ROW_PTR(img, y_j);

                            for (int k = -ksize; k <= ksize; k++) {
                                int x_k = IM_CLAMP(x + k, 0, (img->w - 1));
                                pixel = IMAGE_GET_RGB565_PIXEL_FAST(k_row_ptr, x_k);
                                r_acc += COLOR_RGB565_TO_R5(pixel);
                                g_acc += COLOR_RGB565_TO_G6(pixel);
                                b_acc += COLOR_RGB565_TO_B5(pixel);
                            }
                        }
                    }
                    int pixel;
                    r = (int) ((r_acc * over32_n) >> 16);
                    g = (int) ((g_acc * over32_n) >> 16);
                    b = (int) ((b_acc * over32_n) >> 16);
                    pixel = COLOR_R5_G6_B5_TO_RGB565(r, g, b);

                    if (threshold) {
                        if (((COLOR_RGB565_TO_Y(pixel) - offset) <
                             COLOR_RGB565_TO_Y(IMAGE_GET_RGB565_PIXEL_FAST(row_ptr, x))) ^ invert) {
                            pixel = COLOR_RGB565_BINARY_MAX;
                        } else {
                            pixel = COLOR_RGB565_BINARY_MIN;
                        }
                    }

                    IMAGE_PUT_RGB565_PIXEL_FAST(buf_row_ptr, x, pixel);
                }

                if (y >= ksize) {
                    // Transfer buffer lines...
                    memcpy(IMAGE_COMPUTE_RGB565_PIXEL_ROW_PTR(img, (y - ksize)),
                           IMAGE_COMPUTE_RGB565_PIXEL_ROW_PTR(&buf, ((y - ksize) % brows)),
                           IMAGE_RGB565_LINE_LEN_BYTES(img));
                }
            }

            // Copy any remaining lines from the buffer image...
            for (int y = IM_MAX(img->h - ksize, 0), yy = img->h; y < yy; y++) {
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
#endif // IMLIB_ENABLE_MEAN

#ifdef IMLIB_ENABLE_MEDIAN
static uint8_t hist_median(uint8_t *data, int len, const int cutoff) {
    int i;
#if defined(ARM_MATH_DSP)
    uint32_t oldsum = 0, sum32 = 0;

    for (i = 0; i < len; i += 4) {
        // work 4 at time with SIMD
        sum32 = __USADA8(*(uint32_t *) &data[i], 0, sum32);
        if (sum32 >= cutoff) {
            // within this group
            while (oldsum < cutoff && i < len) {
                oldsum += data[i++];
            }
            break;
        } // if we're at the last 4 values
        oldsum = sum32;
    } // for each group of 4 elements
#else // generic C version
    int sum = 0;
    for (i = 0; i < len && sum < cutoff; i++) {
        sum += data[i];
    }
#endif
    return i - 1;
} /* hist_median() */

void imlib_median_filter(image_t *img, const int ksize, float percentile, bool threshold, int offset, bool invert,
                         image_t *mask) {
    int brows = ksize + 1;
    image_t buf;
    buf.w = img->w;
    buf.h = brows;
    buf.pixfmt = img->pixfmt;

    const int n = ((ksize * 2) + 1) * ((ksize * 2) + 1);
    const int median_cutoff = fast_floorf(percentile * (float) n);

    switch (img->pixfmt) {
        case PIXFORMAT_BINARY: {
            buf.data = fb_alloc(IMAGE_BINARY_LINE_LEN_BYTES(img) * brows, FB_ALLOC_NO_HINT);
            int sum = 0;

            for (int y = 0, yy = img->h; y < yy; y++) {
                uint32_t *row_ptr = IMAGE_COMPUTE_BINARY_PIXEL_ROW_PTR(img, y);
                uint32_t *buf_row_ptr = IMAGE_COMPUTE_BINARY_PIXEL_ROW_PTR(&buf, (y % brows));

                for (int x = 0, xx = img->w; x < xx; x++) {
                    if (mask && (!image_get_mask_pixel(mask, x, y))) {
                        IMAGE_PUT_BINARY_PIXEL_FAST(buf_row_ptr, x, IMAGE_GET_BINARY_PIXEL_FAST(row_ptr, x));
                        continue; // Short circuit.
                    }
                    if (!mask && x > ksize && x < img->w - ksize && y >= ksize && y < img->h - ksize) {
                        for (int j = -ksize; j <= ksize; j++) {
                            uint32_t *k_row_ptr = IMAGE_COMPUTE_BINARY_PIXEL_ROW_PTR(img, y + j);
                            sum -= IMAGE_GET_BINARY_PIXEL_FAST(k_row_ptr, x - ksize - 1);
                            sum += IMAGE_GET_BINARY_PIXEL_FAST(k_row_ptr, x + ksize);
                        }
                    } else {
                        sum = 0;
                        for (int j = -ksize; j <= ksize; j++) {
                            int y_j = IM_CLAMP(y + j, 0, (img->h - 1));
                            uint32_t *k_row_ptr = IMAGE_COMPUTE_BINARY_PIXEL_ROW_PTR(img, y_j);

                            for (int k = -ksize; k <= ksize; k++) {
                                int x_k = IM_CLAMP(x + k, 0, (img->w - 1));
                                sum += IMAGE_GET_BINARY_PIXEL_FAST(k_row_ptr, x_k);
                            }
                        }
                    }
                    int pixel = (sum >= median_cutoff);

                    if (threshold) {
                        if (((pixel - offset) < IMAGE_GET_BINARY_PIXEL_FAST(row_ptr, x)) ^ invert) {
                            pixel = COLOR_BINARY_MAX;
                        } else {
                            pixel = COLOR_BINARY_MIN;
                        }
                    }

                    IMAGE_PUT_BINARY_PIXEL_FAST(buf_row_ptr, x, pixel);
                }

                if (y >= ksize) {
                    // Transfer buffer lines...
                    memcpy(IMAGE_COMPUTE_BINARY_PIXEL_ROW_PTR(img, (y - ksize)),
                           IMAGE_COMPUTE_BINARY_PIXEL_ROW_PTR(&buf, ((y - ksize) % brows)),
                           IMAGE_BINARY_LINE_LEN_BYTES(img));
                }
            }

            // Copy any remaining lines from the buffer image...
            for (int y = IM_MAX(img->h - ksize, 0), yy = img->h; y < yy; y++) {
                memcpy(IMAGE_COMPUTE_BINARY_PIXEL_ROW_PTR(img, y),
                       IMAGE_COMPUTE_BINARY_PIXEL_ROW_PTR(&buf, (y % brows)),
                       IMAGE_BINARY_LINE_LEN_BYTES(img));
            }

            fb_free();
            break;
        }
        case PIXFORMAT_GRAYSCALE: {
            buf.data = fb_alloc(IMAGE_GRAYSCALE_LINE_LEN_BYTES(img) * brows, FB_ALLOC_NO_HINT);
            uint8_t *data = fb_alloc(64, FB_ALLOC_NO_HINT);
            uint8_t pixel;
            for (int y = 0, yy = img->h; y < yy; y++) {
                uint8_t *row_ptr = IMAGE_COMPUTE_GRAYSCALE_PIXEL_ROW_PTR(img, y);
                uint8_t *buf_row_ptr = IMAGE_COMPUTE_GRAYSCALE_PIXEL_ROW_PTR(&buf, (y % brows));

                for (int x = 0, xx = img->w; x < xx; x++) {
                    if (mask && (!image_get_mask_pixel(mask, x, y))) {
                        IMAGE_PUT_GRAYSCALE_PIXEL_FAST(buf_row_ptr, x, IMAGE_GET_GRAYSCALE_PIXEL_FAST(row_ptr, x));
                        continue; // Short circuit.
                    }

                    if (!mask && x > ksize && x < img->w - ksize && y >= ksize && y < img->h - ksize) {
                        // update histogram edges
                        for (int j = -ksize; j <= ksize; j++) {
                            uint8_t *k_row_ptr = IMAGE_COMPUTE_GRAYSCALE_PIXEL_ROW_PTR(img, y + j);
                            pixel = IMAGE_GET_GRAYSCALE_PIXEL_FAST(k_row_ptr, x - ksize - 1);
                            data[pixel >> 2]--; // remove old pixels
                            pixel = IMAGE_GET_GRAYSCALE_PIXEL_FAST(k_row_ptr, x + ksize);
                            data[pixel >> 2]++; // add new pixels
                        } // for j
                    } else {
                        // slow way
                        memset(data, 0, 64);
                        for (int j = -ksize; j <= ksize; j++) {
                            int y_j = IM_CLAMP(y + j, 0, (img->h - 1));
                            uint8_t *k_row_ptr = IMAGE_COMPUTE_GRAYSCALE_PIXEL_ROW_PTR(img, y_j);

                            for (int k = -ksize; k <= ksize; k++) {
                                int x_k = IM_CLAMP(x + k, 0, (img->w - 1));
                                pixel = IMAGE_GET_GRAYSCALE_PIXEL_FAST(k_row_ptr, x_k);
                                data[pixel >> 2]++;
                            }
                        }
                    }

                    pixel = hist_median(data, 64, median_cutoff); // find the median
                    pixel <<= 2; // scale it back up
                    if (threshold) {
                        if (((pixel - offset) < IMAGE_GET_GRAYSCALE_PIXEL_FAST(row_ptr, x)) ^ invert) {
                            pixel = COLOR_GRAYSCALE_BINARY_MAX;
                        } else {
                            pixel = COLOR_GRAYSCALE_BINARY_MIN;
                        }
                    }

                    IMAGE_PUT_GRAYSCALE_PIXEL_FAST(buf_row_ptr, x, pixel);
                }

                if (y >= ksize) {
                    // Transfer buffer lines...
                    memcpy(IMAGE_COMPUTE_GRAYSCALE_PIXEL_ROW_PTR(img, (y - ksize)),
                           IMAGE_COMPUTE_GRAYSCALE_PIXEL_ROW_PTR(&buf, ((y - ksize) % brows)),
                           IMAGE_GRAYSCALE_LINE_LEN_BYTES(img));
                }
            }

            // Copy any remaining lines from the buffer image...
            for (int y = IM_MAX(img->h - ksize, 0), yy = img->h; y < yy; y++) {
                memcpy(IMAGE_COMPUTE_GRAYSCALE_PIXEL_ROW_PTR(img, y),
                       IMAGE_COMPUTE_GRAYSCALE_PIXEL_ROW_PTR(&buf, (y % brows)),
                       IMAGE_GRAYSCALE_LINE_LEN_BYTES(img));
            }

            fb_free();
            fb_free();
            break;
        }
        case PIXFORMAT_RGB565: {
            buf.data = fb_alloc(IMAGE_RGB565_LINE_LEN_BYTES(img) * brows, FB_ALLOC_NO_HINT);
            uint8_t *r_data = fb_alloc(32, FB_ALLOC_NO_HINT);
            uint8_t *g_data = fb_alloc(64, FB_ALLOC_NO_HINT);
            uint8_t *b_data = fb_alloc(32, FB_ALLOC_NO_HINT);
            uint8_t r, g, b;
            for (int y = 0, yy = img->h; y < yy; y++) {
                uint16_t *row_ptr = IMAGE_COMPUTE_RGB565_PIXEL_ROW_PTR(img, y);
                uint16_t *buf_row_ptr = IMAGE_COMPUTE_RGB565_PIXEL_ROW_PTR(&buf, (y % brows));

                for (int x = 0, xx = img->w; x < xx; x++) {
                    if (mask && (!image_get_mask_pixel(mask, x, y))) {
                        IMAGE_PUT_RGB565_PIXEL_FAST(buf_row_ptr, x, IMAGE_GET_RGB565_PIXEL_FAST(row_ptr, x));
                        continue; // Short circuit.
                    }


                    if (!mask && x > ksize && x < img->w - ksize && y >= ksize && y < img->h - ksize) {
                        for (int j = -ksize; j <= ksize; j++) {
                            uint16_t *k_row_ptr = IMAGE_COMPUTE_RGB565_PIXEL_ROW_PTR(img, y + j);
                            int pixel = IMAGE_GET_RGB565_PIXEL_FAST(k_row_ptr, x - ksize - 1);
                            r_data[COLOR_RGB565_TO_R5(pixel)]--; // remove left pixel
                            g_data[COLOR_RGB565_TO_G6(pixel)]--;
                            b_data[COLOR_RGB565_TO_B5(pixel)]--;
                            pixel = IMAGE_GET_RGB565_PIXEL_FAST(k_row_ptr, x + ksize);
                            r_data[COLOR_RGB565_TO_R5(pixel)]++; // add right pixel
                            g_data[COLOR_RGB565_TO_G6(pixel)]++;
                            b_data[COLOR_RGB565_TO_B5(pixel)]++;
                        }
                    } else {
                        // need to check bounds
                        memset(r_data, 0, 32); memset(g_data, 0, 64); memset(b_data, 0, 32);
                        for (int j = -ksize; j <= ksize; j++) {
                            int y_j = IM_CLAMP(y + j, 0, (img->h - 1));
                            uint16_t *k_row_ptr = IMAGE_COMPUTE_RGB565_PIXEL_ROW_PTR(img, y_j);

                            for (int k = -ksize; k <= ksize; k++) {
                                int x_k = IM_CLAMP(x + k, 0, (img->w - 1));
                                int pixel = IMAGE_GET_RGB565_PIXEL_FAST(k_row_ptr, x_k);
                                r_data[COLOR_RGB565_TO_R5(pixel)]++;
                                g_data[COLOR_RGB565_TO_G6(pixel)]++;
                                b_data[COLOR_RGB565_TO_B5(pixel)]++;
                            }
                        }
                    }

                    r = hist_median(r_data, 32, median_cutoff);
                    g = hist_median(g_data, 64, median_cutoff);
                    b = hist_median(b_data, 32, median_cutoff);

                    int pixel = COLOR_R5_G6_B5_TO_RGB565(r, g, b);

                    if (threshold) {
                        if (((COLOR_RGB565_TO_Y(pixel) - offset) <
                             COLOR_RGB565_TO_Y(IMAGE_GET_RGB565_PIXEL_FAST(row_ptr, x))) ^ invert) {
                            pixel = COLOR_RGB565_BINARY_MAX;
                        } else {
                            pixel = COLOR_RGB565_BINARY_MIN;
                        }
                    }

                    IMAGE_PUT_RGB565_PIXEL_FAST(buf_row_ptr, x, pixel);
                }

                if (y >= ksize) {
                    // Transfer buffer lines...
                    memcpy(IMAGE_COMPUTE_RGB565_PIXEL_ROW_PTR(img, (y - ksize)),
                           IMAGE_COMPUTE_RGB565_PIXEL_ROW_PTR(&buf, ((y - ksize) % brows)),
                           IMAGE_RGB565_LINE_LEN_BYTES(img));
                }
            }

            // Copy any remaining lines from the buffer image...
            for (int y = IM_MAX(img->h - ksize, 0), yy = img->h; y < yy; y++) {
                memcpy(IMAGE_COMPUTE_RGB565_PIXEL_ROW_PTR(img, y),
                       IMAGE_COMPUTE_RGB565_PIXEL_ROW_PTR(&buf, (y % brows)),
                       IMAGE_RGB565_LINE_LEN_BYTES(img));
            }

            fb_free();
            fb_free();
            fb_free();
            fb_free();
            break;
        }
        default: {
            break;
        }
    }
}
#endif // IMLIB_ENABLE_MEDIAN

#ifdef IMLIB_ENABLE_MODE
static uint8_t find_mode(uint8_t *bins, int len) {
    int i, j;
    uint8_t mode = 0, mcount = 0;
    for (i = 0; i < len; i += 4) {
        if (*(uint32_t *) &bins[i] == 0) {
            continue; // skip empty bins quickly
        }
        for (j = i; j < i + 4; j++) {
            if (bins[j] > mcount) {
                mcount = bins[j];
                mode = j;
            }
        }
    }
    return mode;
} /* find_mode() */

void imlib_mode_filter(image_t *img, const int ksize, bool threshold, int offset, bool invert, image_t *mask) {
    int brows = ksize + 1;
    image_t buf;
    buf.w = img->w;
    buf.h = brows;
    buf.pixfmt = img->pixfmt;
    const uint8_t n2 = (((ksize * 2) + 1) * ((ksize * 2) + 1)) / 2;
    switch (img->pixfmt) {
        case PIXFORMAT_BINARY: {
            buf.data = fb_alloc(IMAGE_BINARY_LINE_LEN_BYTES(img) * brows, FB_ALLOC_NO_HINT);
            int bins = 0;

            for (int y = 0, yy = img->h; y < yy; y++) {
                uint32_t *row_ptr = IMAGE_COMPUTE_BINARY_PIXEL_ROW_PTR(img, y);
                uint32_t *buf_row_ptr = IMAGE_COMPUTE_BINARY_PIXEL_ROW_PTR(&buf, (y % brows));

                for (int x = 0, xx = img->w; x < xx; x++) {
                    if (mask && (!image_get_mask_pixel(mask, x, y))) {
                        IMAGE_PUT_BINARY_PIXEL_FAST(buf_row_ptr, x, IMAGE_GET_BINARY_PIXEL_FAST(row_ptr, x));
                        continue; // Short circuit.
                    }
                    if (!mask && x > ksize && x < img->w - ksize && y >= ksize && y < img->h - ksize) {
                        for (int j = -ksize; j <= ksize; j++) {
                            uint32_t *k_row_ptr = IMAGE_COMPUTE_BINARY_PIXEL_ROW_PTR(img, y + j);
                            bins -= IMAGE_GET_BINARY_PIXEL_FAST(k_row_ptr, x - ksize - 1);
                            bins += IMAGE_GET_BINARY_PIXEL_FAST(k_row_ptr, x + ksize);
                        }
                    } else {
                        bins = 0;
                        for (int j = -ksize; j <= ksize; j++) {
                            int y_j = IM_CLAMP(y + j, 0, (img->h - 1));
                            uint32_t *k_row_ptr = IMAGE_COMPUTE_BINARY_PIXEL_ROW_PTR(img, y_j);
                            for (int k = -ksize; k <= ksize; k++) {
                                int x_k = IM_CLAMP(x + k, 0, (img->w - 1));
                                bins += IMAGE_GET_BINARY_PIXEL_FAST(k_row_ptr, x_k);
                            }
                        }
                    }

                    uint8_t pixel = (bins > n2);

                    if (threshold) {
                        if (((pixel - offset) < IMAGE_GET_BINARY_PIXEL_FAST(row_ptr, x)) ^ invert) {
                            pixel = COLOR_BINARY_MAX;
                        } else {
                            pixel = COLOR_BINARY_MIN;
                        }
                    }

                    IMAGE_PUT_BINARY_PIXEL_FAST(buf_row_ptr, x, pixel);
                }

                if (y >= ksize) {
                    // Transfer buffer lines...
                    memcpy(IMAGE_COMPUTE_BINARY_PIXEL_ROW_PTR(img, (y - ksize)),
                           IMAGE_COMPUTE_BINARY_PIXEL_ROW_PTR(&buf, ((y - ksize) % brows)),
                           IMAGE_BINARY_LINE_LEN_BYTES(img));
                }
            }

            // Copy any remaining lines from the buffer image...
            for (int y = IM_MAX(img->h - ksize, 0), yy = img->h; y < yy; y++) {
                memcpy(IMAGE_COMPUTE_BINARY_PIXEL_ROW_PTR(img, y),
                       IMAGE_COMPUTE_BINARY_PIXEL_ROW_PTR(&buf, (y % brows)),
                       IMAGE_BINARY_LINE_LEN_BYTES(img));
            }

            fb_free();
            fb_free();
            break;
        }
        case PIXFORMAT_GRAYSCALE: {
            buf.data = fb_alloc(IMAGE_GRAYSCALE_LINE_LEN_BYTES(img) * brows, FB_ALLOC_NO_HINT);
            uint8_t *bins = fb_alloc((COLOR_GRAYSCALE_MAX - COLOR_GRAYSCALE_MIN + 1), FB_ALLOC_NO_HINT);

            for (int y = 0, yy = img->h; y < yy; y++) {
                uint8_t *row_ptr = IMAGE_COMPUTE_GRAYSCALE_PIXEL_ROW_PTR(img, y);
                uint8_t *buf_row_ptr = IMAGE_COMPUTE_GRAYSCALE_PIXEL_ROW_PTR(&buf, (y % brows));
                uint8_t pixel = 0, mode = 0;
                int mcount = -1;
                for (int x = 0, xx = img->w; x < xx; x++) {
                    if (mask && (!image_get_mask_pixel(mask, x, y))) {
                        IMAGE_PUT_GRAYSCALE_PIXEL_FAST(buf_row_ptr, x, IMAGE_GET_GRAYSCALE_PIXEL_FAST(row_ptr, x));
                        continue; // Short circuit.
                    }
                    if (!mask && x > ksize && x < img->w - ksize && y >= ksize && y < img->h - ksize) {
                        for (int j = -ksize; j <= ksize; j++) {
                            uint8_t m, *k_row_ptr = IMAGE_COMPUTE_GRAYSCALE_PIXEL_ROW_PTR(img, y + j);
                            pixel = IMAGE_GET_GRAYSCALE_PIXEL_FAST(k_row_ptr, x - ksize - 1);
                            m = --bins[pixel];
                            if (pixel == mode) {
                                if (m < n2) {
                                    mcount = 256; // need to search later
                                } else {
                                    mcount = m; // we're still the mode
                                }
                            }
                            pixel = IMAGE_GET_GRAYSCALE_PIXEL_FAST(k_row_ptr, x + ksize);
                            m = ++bins[pixel];
                            if (m > mcount) {
                                mcount = m;
                                mode = pixel;
                            }
                        }
                        if (mcount == 256) {
                            // need to find max
                            mode = find_mode(bins, 256);
                            mcount = bins[mode];
                        }
                    } else {
                        // slow way
                        mcount = -1;
                        memset(bins, 0, (COLOR_GRAYSCALE_MAX - COLOR_GRAYSCALE_MIN + 1));
                        for (int j = -ksize; j <= ksize; j++) {
                            int y_j = IM_CLAMP(y + j, 0, (img->h - 1));
                            uint8_t *k_row_ptr = IMAGE_COMPUTE_GRAYSCALE_PIXEL_ROW_PTR(img, y_j);

                            for (int k = -ksize; k <= ksize; k++) {
                                int x_k = IM_CLAMP(x + k, 0, (img->w - 1));
                                pixel = IMAGE_GET_GRAYSCALE_PIXEL_FAST(k_row_ptr, x_k);
                                bins[pixel]++;

                                if (bins[pixel] > mcount) {
                                    mcount = bins[pixel];
                                    mode = pixel;
                                }
                            }
                        }
                    }
                    pixel = mode;

                    if (threshold) {
                        if (((pixel - offset) < IMAGE_GET_GRAYSCALE_PIXEL_FAST(row_ptr, x)) ^ invert) {
                            pixel = COLOR_GRAYSCALE_BINARY_MAX;
                        } else {
                            pixel = COLOR_GRAYSCALE_BINARY_MIN;
                        }
                    }

                    IMAGE_PUT_GRAYSCALE_PIXEL_FAST(buf_row_ptr, x, pixel);
                }

                if (y >= ksize) {
                    // Transfer buffer lines...
                    memcpy(IMAGE_COMPUTE_GRAYSCALE_PIXEL_ROW_PTR(img, (y - ksize)),
                           IMAGE_COMPUTE_GRAYSCALE_PIXEL_ROW_PTR(&buf, ((y - ksize) % brows)),
                           IMAGE_GRAYSCALE_LINE_LEN_BYTES(img));
                }
            }

            // Copy any remaining lines from the buffer image...
            for (int y = IM_MAX(img->h - ksize, 0), yy = img->h; y < yy; y++) {
                memcpy(IMAGE_COMPUTE_GRAYSCALE_PIXEL_ROW_PTR(img, y),
                       IMAGE_COMPUTE_GRAYSCALE_PIXEL_ROW_PTR(&buf, (y % brows)),
                       IMAGE_GRAYSCALE_LINE_LEN_BYTES(img));
            }

            fb_free();
            fb_free();
            break;
        }
        case PIXFORMAT_RGB565: {
            buf.data = fb_alloc(IMAGE_RGB565_LINE_LEN_BYTES(img) * brows, FB_ALLOC_NO_HINT);
            uint8_t *r_bins = fb_alloc((COLOR_R5_MAX - COLOR_R5_MIN + 1), FB_ALLOC_NO_HINT);
            uint8_t *g_bins = fb_alloc((COLOR_G6_MAX - COLOR_G6_MIN + 1), FB_ALLOC_NO_HINT);
            uint8_t *b_bins = fb_alloc((COLOR_B5_MAX - COLOR_B5_MIN + 1), FB_ALLOC_NO_HINT);
            int r_pixel, g_pixel, b_pixel;

            for (int y = 0, yy = img->h; y < yy; y++) {
                uint16_t *row_ptr = IMAGE_COMPUTE_RGB565_PIXEL_ROW_PTR(img, y);
                uint16_t *buf_row_ptr = IMAGE_COMPUTE_RGB565_PIXEL_ROW_PTR(&buf, (y % brows));
                int r_mcount = 0, g_mcount = 0, b_mcount = 0;
                int pixel, r_mode, g_mode, b_mode;
                r_mode = g_mode = b_mode = 0;
                for (int x = 0, xx = img->w; x < xx; x++) {
                    if (mask && (!image_get_mask_pixel(mask, x, y))) {
                        IMAGE_PUT_RGB565_PIXEL_FAST(buf_row_ptr, x, IMAGE_GET_RGB565_PIXEL_FAST(row_ptr, x));
                        continue; // Short circuit.
                    }

                    if (!mask && x > ksize && x < img->w - ksize && y >= ksize && y < img->h - ksize) {
                        for (int j = -ksize; j <= ksize; j++) {
                            uint16_t *k_row_ptr = IMAGE_COMPUTE_RGB565_PIXEL_ROW_PTR(img, y + j);
                            pixel = IMAGE_GET_RGB565_PIXEL_FAST(k_row_ptr, x - ksize - 1);
                            r_pixel = COLOR_RGB565_TO_R5(pixel);
                            g_pixel = COLOR_RGB565_TO_G6(pixel);
                            b_pixel = COLOR_RGB565_TO_B5(pixel);
                            r_bins[r_pixel]--;
                            g_bins[g_pixel]--;
                            b_bins[b_pixel]--;
                            if (r_pixel == r_mode) {
                                if (r_bins[r_pixel] < n2) {
                                    r_mcount = 256; // need to search later
                                } else {
                                    r_mcount = r_bins[r_pixel]; // we're still the mode
                                }
                            }
                            if (g_pixel == g_mode) {
                                if (g_bins[g_pixel] < n2) {
                                    g_mcount = 256; // need to search later
                                } else {
                                    g_mcount = g_bins[g_pixel]; // we're still the mode
                                }
                            }
                            if (b_pixel == b_mode) {
                                if (b_bins[b_pixel] < n2) {
                                    b_mcount = 256; // need to search later
                                } else {
                                    b_mcount = b_bins[b_pixel]; // we're still the mode
                                }
                            }

                            pixel = IMAGE_GET_RGB565_PIXEL_FAST(k_row_ptr, x + ksize);
                            r_pixel = COLOR_RGB565_TO_R5(pixel);
                            g_pixel = COLOR_RGB565_TO_G6(pixel);
                            b_pixel = COLOR_RGB565_TO_B5(pixel);
                            r_bins[r_pixel]++;
                            g_bins[g_pixel]++;
                            b_bins[b_pixel]++;
                            if (r_bins[r_pixel] > r_mcount) {
                                r_mcount = r_bins[r_pixel];
                                r_mode = r_pixel;
                            }

                            if (g_bins[g_pixel] > g_mcount) {
                                g_mcount = g_bins[g_pixel];
                                g_mode = g_pixel;
                            }

                            if (b_bins[b_pixel] > b_mcount) {
                                b_mcount = b_bins[b_pixel];
                                b_mode = b_pixel;
                            }
                        } // for j
                        if (r_mcount == 256) {
                            // need to find max
                            r_mode = find_mode(r_bins, 32);
                            r_mcount = r_bins[r_mode];
                        }
                        if (g_mcount == 256) {
                            // need to find max
                            g_mode = find_mode(g_bins, 64);
                            g_mcount = g_bins[g_mode];
                        }
                        if (b_mcount == 256) {
                            // need to find max
                            b_mode = find_mode(b_bins, 32);
                            b_mcount = r_bins[b_mode];
                        }
                    } else {
                        // slower way
                        memset(r_bins, 0, (COLOR_R5_MAX - COLOR_R5_MIN + 1));
                        memset(g_bins, 0, (COLOR_G6_MAX - COLOR_G6_MIN + 1));
                        memset(b_bins, 0, (COLOR_B5_MAX - COLOR_B5_MIN + 1));
                        r_mcount = g_mcount = b_mcount = 0;
                        for (int j = -ksize; j <= ksize; j++) {
                            int y_j = IM_CLAMP(y + j, 0, (img->h - 1));
                            uint16_t *k_row_ptr = IMAGE_COMPUTE_RGB565_PIXEL_ROW_PTR(img, y_j);

                            for (int k = -ksize; k <= ksize; k++) {
                                int x_k = IM_CLAMP(x + k, 0, (img->w - 1));
                                pixel = IMAGE_GET_RGB565_PIXEL_FAST(k_row_ptr, x_k);
                                r_pixel = COLOR_RGB565_TO_R5(pixel);
                                g_pixel = COLOR_RGB565_TO_G6(pixel);
                                b_pixel = COLOR_RGB565_TO_B5(pixel);
                                r_bins[r_pixel]++;
                                g_bins[g_pixel]++;
                                b_bins[b_pixel]++;

                                if (r_bins[r_pixel] > r_mcount) {
                                    r_mcount = r_bins[r_pixel];
                                    r_mode = r_pixel;
                                }

                                if (g_bins[g_pixel] > g_mcount) {
                                    g_mcount = g_bins[g_pixel];
                                    g_mode = g_pixel;
                                }

                                if (b_bins[b_pixel] > b_mcount) {
                                    b_mcount = b_bins[b_pixel];
                                    b_mode = b_pixel;
                                }
                            } // for k
                        } // for j
                    } // slow/fast way
                    pixel = COLOR_R5_G6_B5_TO_RGB565(r_mode, g_mode, b_mode);
                    IMAGE_PUT_RGB565_PIXEL_FAST(buf_row_ptr, x, pixel);
                }

                if (y >= ksize) {
                    // Transfer buffer lines...
                    memcpy(IMAGE_COMPUTE_RGB565_PIXEL_ROW_PTR(img, (y - ksize)),
                           IMAGE_COMPUTE_RGB565_PIXEL_ROW_PTR(&buf, ((y - ksize) % brows)),
                           IMAGE_RGB565_LINE_LEN_BYTES(img));
                }
            }

            // Copy any remaining lines from the buffer image...
            for (int y = IM_MAX(img->h - ksize, 0), yy = img->h; y < yy; y++) {
                memcpy(IMAGE_COMPUTE_RGB565_PIXEL_ROW_PTR(img, y),
                       IMAGE_COMPUTE_RGB565_PIXEL_ROW_PTR(&buf, (y % brows)),
                       IMAGE_RGB565_LINE_LEN_BYTES(img));
            }

            fb_free();
            fb_free();
            fb_free();
            fb_free();
            break;
        }
        default: {
            break;
        }
    }
}
#endif // IMLIB_ENABLE_MODE

#ifdef IMLIB_ENABLE_MIDPOINT
void imlib_midpoint_filter(image_t *img, const int ksize, float bias, bool threshold, int offset, bool invert, image_t *mask) {
    int brows = ksize + 1;
    image_t buf;
    buf.w = img->w;
    buf.h = brows;
    buf.pixfmt = img->pixfmt;
    uint8_t *u8BiasTable;
    float max_bias = bias, min_bias = 1.0f - bias;

    u8BiasTable = fb_alloc(256, FB_ALLOC_NO_HINT);
    for (int i = 0; i < 256; i++) {
        u8BiasTable[i] = (uint8_t) fast_floorf((float) i * bias);
    }

    switch (img->pixfmt) {
        case PIXFORMAT_BINARY: {
            buf.data = fb_alloc(IMAGE_BINARY_LINE_LEN_BYTES(img) * brows, FB_ALLOC_NO_HINT);

            for (int y = 0, yy = img->h; y < yy; y++) {
                uint32_t *row_ptr = IMAGE_COMPUTE_BINARY_PIXEL_ROW_PTR(img, y);
                uint32_t *buf_row_ptr = IMAGE_COMPUTE_BINARY_PIXEL_ROW_PTR(&buf, (y % brows));

                for (int x = 0, xx = img->w; x < xx; x++) {
                    if (mask && (!image_get_mask_pixel(mask, x, y))) {
                        IMAGE_PUT_BINARY_PIXEL_FAST(buf_row_ptr, x, IMAGE_GET_BINARY_PIXEL_FAST(row_ptr, x));
                        continue; // Short circuit.
                    }

                    int min = COLOR_BINARY_MAX, max = COLOR_BINARY_MIN;

                    if (x >= ksize && x < img->w - ksize && y >= ksize && y < img->h - ksize) {
                        for (int j = -ksize; j <= ksize; j++) {
                            uint32_t *k_row_ptr = IMAGE_COMPUTE_BINARY_PIXEL_ROW_PTR(img, y + j);
                            for (int k = -ksize; k <= ksize; k++) {
                                int pixel = IMAGE_GET_BINARY_PIXEL_FAST(k_row_ptr, x + k);
                                min &= pixel;
                                max |= pixel;
                            }
                        }
                    } else {
                        for (int j = -ksize; j <= ksize; j++) {
                            int y_j = IM_CLAMP(y + j, 0, (img->h - 1));
                            uint32_t *k_row_ptr = IMAGE_COMPUTE_BINARY_PIXEL_ROW_PTR(img, y_j);

                            for (int k = -ksize; k <= ksize; k++) {
                                int x_k = IM_CLAMP(x + k, 0, (img->w - 1));
                                int pixel = IMAGE_GET_BINARY_PIXEL_FAST(k_row_ptr, x_k);
                                min &= pixel;
                                max |= pixel;
                            }
                        }
                    }

                    int pixel = fast_floorf((min * min_bias) + (max * max_bias));

                    if (threshold) {
                        if (((pixel - offset) < IMAGE_GET_BINARY_PIXEL_FAST(row_ptr, x)) ^ invert) {
                            pixel = COLOR_BINARY_MAX;
                        } else {
                            pixel = COLOR_BINARY_MIN;
                        }
                    }

                    IMAGE_PUT_BINARY_PIXEL_FAST(buf_row_ptr, x, pixel);
                }

                if (y >= ksize) {
                    // Transfer buffer lines...
                    memcpy(IMAGE_COMPUTE_BINARY_PIXEL_ROW_PTR(img, (y - ksize)),
                           IMAGE_COMPUTE_BINARY_PIXEL_ROW_PTR(&buf, ((y - ksize) % brows)),
                           IMAGE_BINARY_LINE_LEN_BYTES(img));
                }
            }

            // Copy any remaining lines from the buffer image...
            for (int y = IM_MAX(img->h - ksize, 0), yy = img->h; y < yy; y++) {
                memcpy(IMAGE_COMPUTE_BINARY_PIXEL_ROW_PTR(img, y),
                       IMAGE_COMPUTE_BINARY_PIXEL_ROW_PTR(&buf, (y % brows)),
                       IMAGE_BINARY_LINE_LEN_BYTES(img));
            }

            fb_free();
            break;
        }
        case PIXFORMAT_GRAYSCALE: {
            buf.data = fb_alloc(IMAGE_GRAYSCALE_LINE_LEN_BYTES(img) * brows, FB_ALLOC_NO_HINT);

            for (int y = 0, yy = img->h; y < yy; y++) {
                uint8_t *row_ptr = IMAGE_COMPUTE_GRAYSCALE_PIXEL_ROW_PTR(img, y);
                uint8_t *buf_row_ptr = IMAGE_COMPUTE_GRAYSCALE_PIXEL_ROW_PTR(&buf, (y % brows));

                for (int x = 0, xx = img->w; x < xx; x++) {
                    if (mask && (!image_get_mask_pixel(mask, x, y))) {
                        IMAGE_PUT_GRAYSCALE_PIXEL_FAST(buf_row_ptr, x, IMAGE_GET_GRAYSCALE_PIXEL_FAST(row_ptr, x));
                        continue; // Short circuit.
                    }

                    int min = COLOR_GRAYSCALE_MAX, max = COLOR_GRAYSCALE_MIN;
                    if (x >= ksize && x < img->w - ksize && y >= ksize && y < img->h - ksize) {
                        for (int j = -ksize; j <= ksize; j++) {
                            uint8_t *k_row_ptr = IMAGE_COMPUTE_GRAYSCALE_PIXEL_ROW_PTR(img, y + j);
                            for (int k = -ksize; k <= ksize; k++) {
                                int pixel = IMAGE_GET_GRAYSCALE_PIXEL_FAST(k_row_ptr, x + k);
                                if (pixel < min) {
                                    min = pixel;
                                } else if (pixel > max) {
                                    max = pixel;
                                }
                            }
                        }
                    } else {
                        for (int j = -ksize; j <= ksize; j++) {
                            int y_j = IM_CLAMP(y + j, 0, (img->h - 1));
                            uint8_t *k_row_ptr = IMAGE_COMPUTE_GRAYSCALE_PIXEL_ROW_PTR(img, y_j);

                            for (int k = -ksize; k <= ksize; k++) {
                                int x_k = IM_CLAMP(x + k, 0, (img->w - 1));
                                int pixel = IMAGE_GET_GRAYSCALE_PIXEL_FAST(k_row_ptr, x_k);
                                if (pixel < min) {
                                    min = pixel;
                                } else if (pixel > max) {
                                    max = pixel;
                                }
                            }
                        }
                    }

                    int pixel = min + u8BiasTable[max - min];

                    if (threshold) {
                        if (((pixel - offset) < IMAGE_GET_GRAYSCALE_PIXEL_FAST(row_ptr, x)) ^ invert) {
                            pixel = COLOR_GRAYSCALE_BINARY_MAX;
                        } else {
                            pixel = COLOR_GRAYSCALE_BINARY_MIN;
                        }
                    }

                    IMAGE_PUT_GRAYSCALE_PIXEL_FAST(buf_row_ptr, x, pixel);
                }

                if (y >= ksize) {
                    // Transfer buffer lines...
                    memcpy(IMAGE_COMPUTE_GRAYSCALE_PIXEL_ROW_PTR(img, (y - ksize)),
                           IMAGE_COMPUTE_GRAYSCALE_PIXEL_ROW_PTR(&buf, ((y - ksize) % brows)),
                           IMAGE_GRAYSCALE_LINE_LEN_BYTES(img));
                }
            }

            // Copy any remaining lines from the buffer image...
            for (int y = IM_MAX(img->h - ksize, 0), yy = img->h; y < yy; y++) {
                memcpy(IMAGE_COMPUTE_GRAYSCALE_PIXEL_ROW_PTR(img, y),
                       IMAGE_COMPUTE_GRAYSCALE_PIXEL_ROW_PTR(&buf, (y % brows)),
                       IMAGE_GRAYSCALE_LINE_LEN_BYTES(img));
            }

            fb_free();
            break;
        }
        case PIXFORMAT_RGB565: {
            buf.data = fb_alloc(IMAGE_RGB565_LINE_LEN_BYTES(img) * brows, FB_ALLOC_NO_HINT);

            for (int y = 0, yy = img->h; y < yy; y++) {
                uint16_t *row_ptr = IMAGE_COMPUTE_RGB565_PIXEL_ROW_PTR(img, y);
                uint16_t *buf_row_ptr = IMAGE_COMPUTE_RGB565_PIXEL_ROW_PTR(&buf, (y % brows));

                for (int x = 0, xx = img->w; x < xx; x++) {
                    if (mask && (!image_get_mask_pixel(mask, x, y))) {
                        IMAGE_PUT_RGB565_PIXEL_FAST(buf_row_ptr, x, IMAGE_GET_RGB565_PIXEL_FAST(row_ptr, x));
                        continue; // Short circuit.
                    }

                    int r_min = COLOR_R5_MAX, r_max = COLOR_R5_MIN;
                    int g_min = COLOR_G6_MAX, g_max = COLOR_G6_MIN;
                    int b_min = COLOR_B5_MAX, b_max = COLOR_B5_MIN;
                    if (x >= ksize && x < img->w - ksize && y >= ksize && y < img->h - ksize) {
                        for (int j = -ksize; j <= ksize; j++) {
                            uint16_t *k_row_ptr = IMAGE_COMPUTE_RGB565_PIXEL_ROW_PTR(img, y + j);
                            for (int k = -ksize; k <= ksize; k++) {
                                int pixel = IMAGE_GET_RGB565_PIXEL_FAST(k_row_ptr, x + k);
                                int r_pixel = COLOR_RGB565_TO_R5(pixel);
                                int g_pixel = COLOR_RGB565_TO_G6(pixel);
                                int b_pixel = COLOR_RGB565_TO_B5(pixel);
                                if (r_pixel < r_min) {
                                    r_min = r_pixel;
                                } else if (r_pixel > r_max) {
                                    r_max = r_pixel;
                                }
                                if (g_pixel < g_min) {
                                    g_min = g_pixel;
                                } else if (g_pixel > g_max) {
                                    g_max = g_pixel;
                                }
                                if (b_pixel < b_min) {
                                    b_min = b_pixel;
                                } else if (b_pixel > b_max) {
                                    b_max = b_pixel;
                                }
                            }
                        }
                    } else {
                        for (int j = -ksize; j <= ksize; j++) {
                            int y_j = IM_CLAMP(y + j, 0, (img->h - 1));
                            uint16_t *k_row_ptr = IMAGE_COMPUTE_RGB565_PIXEL_ROW_PTR(img, y_j);

                            for (int k = -ksize; k <= ksize; k++) {
                                int x_k = IM_CLAMP(x + k, 0, (img->w - 1));
                                int pixel = IMAGE_GET_RGB565_PIXEL_FAST(k_row_ptr, x_k);
                                int r_pixel = COLOR_RGB565_TO_R5(pixel);
                                int g_pixel = COLOR_RGB565_TO_G6(pixel);
                                int b_pixel = COLOR_RGB565_TO_B5(pixel);
                                if (r_pixel < r_min) {
                                    r_min = r_pixel;
                                } else if (r_pixel > r_max) {
                                    r_max = r_pixel;
                                }
                                if (g_pixel < g_min) {
                                    g_min = g_pixel;
                                } else if (g_pixel > g_max) {
                                    g_max = g_pixel;
                                }
                                if (b_pixel < b_min) {
                                    b_min = b_pixel;
                                } else if (b_pixel > b_max) {
                                    b_max = b_pixel;
                                }
                            }
                        }
                    }

                    r_min += u8BiasTable[r_max - r_min];
                    g_min += u8BiasTable[g_max - g_min];
                    b_min += u8BiasTable[b_max - b_min];
                    int pixel = COLOR_R5_G6_B5_TO_RGB565(r_min, g_min, b_min);

                    if (threshold) {
                        if (((COLOR_RGB565_TO_Y(pixel) - offset) <
                             COLOR_RGB565_TO_Y(IMAGE_GET_RGB565_PIXEL_FAST(row_ptr, x))) ^ invert) {
                            pixel = COLOR_RGB565_BINARY_MAX;
                        } else {
                            pixel = COLOR_RGB565_BINARY_MIN;
                        }
                    }

                    IMAGE_PUT_RGB565_PIXEL_FAST(buf_row_ptr, x, pixel);
                }

                if (y >= ksize) {
                    // Transfer buffer lines...
                    memcpy(IMAGE_COMPUTE_RGB565_PIXEL_ROW_PTR(img, (y - ksize)),
                           IMAGE_COMPUTE_RGB565_PIXEL_ROW_PTR(&buf, ((y - ksize) % brows)),
                           IMAGE_RGB565_LINE_LEN_BYTES(img));
                }
            }

            // Copy any remaining lines from the buffer image...
            for (int y = IM_MAX(img->h - ksize, 0), yy = img->h; y < yy; y++) {
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
    fb_free();
}
#endif // IMLIB_ENABLE_MIDPOINT

// http://www.fmwconcepts.com/imagemagick/digital_image_filtering.pdf

void imlib_morph(image_t *img,
                 const int ksize,
                 const int *krn,
                 const int sum,
                 bool threshold,
                 int offset,
                 bool invert,
                 image_t *mask) {
    int32_t brows = ksize + 1;
    image_t buf;
    buf.w = img->w;
    buf.h = brows;
    buf.pixfmt = img->pixfmt;
    v128_t voffset8 = vdup_s8(offset);
    v128_t voffset16 = vdup_s16(offset);
    v128_t vinvert_ge = vdup_u32(invert ? 0xFFFFFFFF : 0x00000000);
    v128_t vinvert_lt = vdup_u32(invert ? 0x00000000 : 0xFFFFFFFF);
    int32_t div_sum = div_fast_setup_s16(sum);
    v128_t vdiv_sum = vdiv_fast_setup_s16(div_sum);

    switch (img->pixfmt) {
        case PIXFORMAT_BINARY: {
            buf.data = fb_alloc(IMAGE_BINARY_LINE_LEN_BYTES(img) * brows, FB_ALLOC_PREFER_SPEED);

            for (int32_t y = 0; y < img->h; y++) {
                uint32_t *row_ptr = IMAGE_COMPUTE_BINARY_PIXEL_ROW_PTR(img, y);
                uint32_t *buf_row_ptr = IMAGE_COMPUTE_BINARY_PIXEL_ROW_PTR(&buf, (y % brows));

                for (int32_t x = 0; x < img->w; x++) {
                    if (mask && (!image_get_mask_pixel(mask, x, y))) {
                        int32_t p = IMAGE_GET_BINARY_PIXEL_FAST(row_ptr, x);
                        IMAGE_PUT_BINARY_PIXEL_FAST(buf_row_ptr, x, p);
                        continue; // Short circuit.
                    }

                    int32_t acc = 0, ptr = 0;

                    if (!mask && x >= ksize && x < img->w - ksize && y >= ksize && y < img->h - ksize) {
                        for (int32_t j = -ksize; j <= ksize; j++) {
                            uint32_t *k_row_ptr = IMAGE_COMPUTE_BINARY_PIXEL_ROW_PTR(img, y + j);
                            for (int32_t k = -ksize; k <= ksize; k++) {
                                acc += krn[ptr++] * IMAGE_GET_BINARY_PIXEL_FAST(k_row_ptr, x + k);
                            }
                        }
                    } else {
                        for (int32_t j = -ksize; j <= ksize; j++) {
                            int32_t y_j = IM_CLAMP(y + j, 0, (img->h - 1));
                            uint32_t *k_row_ptr = IMAGE_COMPUTE_BINARY_PIXEL_ROW_PTR(img, y_j);

                            for (int32_t k = -ksize; k <= ksize; k++) {
                                int32_t x_k = IM_CLAMP(x + k, 0, (img->w - 1));
                                acc += krn[ptr++] * IMAGE_GET_BINARY_PIXEL_FAST(k_row_ptr, x_k);
                            }
                        }
                    }

                    int32_t pixel = div_fast_binary_s16(acc, div_sum);

                    if (threshold) {
                        pixel -= offset;
                        pixel = pixel < IMAGE_GET_BINARY_PIXEL_FAST(row_ptr, x);
                        pixel = pixel ^ invert;
                    }

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
            int32_t s16_acc_max = sum * COLOR_GRAYSCALE_MAX;
            bool s16_overflow = (s16_acc_max > INT16_MAX) || (s16_acc_max < INT16_MIN);

            for (int32_t y = 0; y < img->h; y++) {
                uint8_t *row_ptr = IMAGE_COMPUTE_GRAYSCALE_PIXEL_ROW_PTR(img, y);
                uint8_t *buf_row_ptr = IMAGE_COMPUTE_GRAYSCALE_PIXEL_ROW_PTR(&buf, (y % brows));

                for (int32_t x = 0; x < img->w; x++) {
                    if (mask && (!image_get_mask_pixel(mask, x, y))) {
                        int p = IMAGE_GET_GRAYSCALE_PIXEL_FAST(row_ptr, x);
                        IMAGE_PUT_GRAYSCALE_PIXEL_FAST(buf_row_ptr, x, p);
                        continue; // Short circuit.
                    }

                    int32_t acc = 0, ptr = 0;

                    if (!mask && x >= ksize && x < img->w - ksize && y >= ksize && y < img->h - ksize) {
                        int32_t remaining = (img->w - ksize) - x;
                        if ((!s16_overflow) && (remaining >= vpredicate_8_maybe_min_elements())) {
                            for (;;) {
                                v128_predicate_t pred = vpredicate_8_maybe(remaining);
                                v128_t vacc0 = vdup_u16(0);
                                v128_t vacc1 = vdup_u16(0);
                                ptr = 0;

                                for (int32_t j = -ksize; j <= ksize; j++) {
                                    uint8_t *k_row_ptr = IMAGE_COMPUTE_GRAYSCALE_PIXEL_ROW_PTR(img, y + j) + x;
                                    for (int32_t k = -ksize; k <= ksize; k++) {
                                        v128_t pixels = vldr_u8_pred(k_row_ptr + k, pred);
                                        int32_t krn_val = krn[ptr++];
                                        vacc0 = vmla_n_s16(vuxtb16(pixels), krn_val, vacc0);
                                        vacc1 = vmla_n_s16(vuxtb16_ror8(pixels), krn_val, vacc1);
                                    }
                                }

                                // Divide by sum.

                                v128_t pixels0 = vdiv_fast_s16(vacc0, vdiv_sum);
                                v128_t pixels1 = vdiv_fast_s16(vacc1, vdiv_sum);
                                v128_t pixels = vusat_s16_narrow_u8_combine(pixels0, pixels1);

                                if (threshold) {
                                    pixels = vsub_s8(pixels, voffset8);
                                    v128_t old_pixels = vldr_u8_pred(row_ptr + x, pred);
                                    pixels = vcmpgesel_u8(pixels, old_pixels, vinvert_ge, vinvert_lt);
                                }

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
                                for (int32_t k = -ksize; k <= ksize; k++) {
                                    acc += krn[ptr++] * IMAGE_GET_GRAYSCALE_PIXEL_FAST(k_row_ptr, k);
                                }
                            }
                        }
                    } else {
                        for (int32_t j = -ksize; j <= ksize; j++) {
                            int32_t y_j = IM_CLAMP(y + j, 0, (img->h - 1));
                            uint8_t *k_row_ptr = IMAGE_COMPUTE_GRAYSCALE_PIXEL_ROW_PTR(img, y_j);

                            for (int32_t k = -ksize; k <= ksize; k++) {
                                int32_t x_k = IM_CLAMP(x + k, 0, (img->w - 1));
                                acc += krn[ptr++] * IMAGE_GET_GRAYSCALE_PIXEL_FAST(k_row_ptr, x_k);
                            }
                        }
                    }

                    int32_t pixel = div_fast_usat_s16(acc, div_sum, 8);

                    if (threshold) {
                        pixel -= offset;
                        pixel = pixel < IMAGE_GET_GRAYSCALE_PIXEL_FAST(row_ptr, x);
                        pixel = (pixel ^ invert) * COLOR_GRAYSCALE_BINARY_MAX;
                    }

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
            int32_t s16_acc_max = sum * COLOR_G6_MAX;
            bool s16_overflow = (s16_acc_max > INT16_MAX) || (s16_acc_max < INT16_MIN);

            for (int y = 0; y < img->h; y++) {
                uint16_t *row_ptr = IMAGE_COMPUTE_RGB565_PIXEL_ROW_PTR(img, y);
                uint16_t *buf_row_ptr = IMAGE_COMPUTE_RGB565_PIXEL_ROW_PTR(&buf, (y % brows));

                for (int32_t x = 0; x < img->w; x++) {
                    if (mask && (!image_get_mask_pixel(mask, x, y))) {
                        int p = IMAGE_GET_RGB565_PIXEL_FAST(row_ptr, x);
                        IMAGE_PUT_RGB565_PIXEL_FAST(buf_row_ptr, x, p);
                        continue; // Short circuit.
                    }

                    int32_t r_acc = 0, g_acc = 0, b_acc = 0, ptr = 0;

                    if (!mask && x >= ksize && x < img->w - ksize && y >= ksize && y < img->h - ksize) {
                        int32_t remaining = (img->w - ksize) - x;
                        if ((!s16_overflow) && (remaining >= vpredicate_16_maybe_min_elements())) {
                            for (;;) {
                                v128_predicate_t pred = vpredicate_16_maybe(remaining);
                                v128_t vr_acc = vdup_u16(0);
                                v128_t vg_acc = vdup_u16(0);
                                v128_t vb_acc = vdup_u16(0);
                                ptr = 0;

                                for (int32_t j = -ksize; j <= ksize; j++) {
                                    uint16_t *k_row_ptr = IMAGE_COMPUTE_RGB565_PIXEL_ROW_PTR(img, y + j) + x;
                                    for (int32_t k = -ksize; k <= ksize; k++) {
                                        v128_t pixels = vldr_u16_pred(k_row_ptr + k, pred);
                                        vrgb_pixels_t pixels_rgb = vrgb_rgb565_to_pixels565(pixels);
                                        int32_t krn_val = krn[ptr++];
                                        vr_acc = vmla_n_s16(pixels_rgb.r, krn_val, vr_acc);
                                        vg_acc = vmla_n_s16(pixels_rgb.g, krn_val, vg_acc);
                                        vb_acc = vmla_n_s16(pixels_rgb.b, krn_val, vb_acc);
                                    }
                                }

                                // Divide by sum and convert to RGB565.

                                vrgb_pixels_t pixels565;
                                pixels565.r = vusat_s16_narrow_u8(vdiv_fast_s16(vr_acc, vdiv_sum), 5);
                                pixels565.g = vusat_s16_narrow_u8(vdiv_fast_s16(vg_acc, vdiv_sum), 6);
                                pixels565.b = vusat_s16_narrow_u8(vdiv_fast_s16(vb_acc, vdiv_sum), 5);
                                v128_t pixels = vrgb_pixels565_to_rgb565(pixels565);

                                if (threshold) {
                                    v128_t pixels_y = vuxtb16(vrgb_pixels565_to_grayscale(pixels565));
                                    pixels_y = vsub_s16(pixels_y, voffset16);
                                    v128_t old_pixels = vldr_u16_pred(row_ptr + x, pred);
                                    vrgb_pixels_t old_pixels565 = vrgb_rgb565_to_pixels565(old_pixels);
                                    v128_t old_pixels_y = vuxtb16(vrgb_pixels565_to_grayscale(old_pixels565));
                                    pixels = vcmpgesel_u16(pixels_y, old_pixels_y, vinvert_ge, vinvert_lt);
                                }

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
                                for (int32_t k = -ksize; k <= ksize; k++) {
                                    int32_t pixel = IMAGE_GET_RGB565_PIXEL_FAST(k_row_ptr, k);
                                    int32_t krn_val = krn[ptr++];
                                    r_acc += krn_val * COLOR_RGB565_TO_R5(pixel);
                                    g_acc += krn_val * COLOR_RGB565_TO_G6(pixel);
                                    b_acc += krn_val * COLOR_RGB565_TO_B5(pixel);
                                }
                            }
                        }
                    } else {
                        for (int32_t j = -ksize; j <= ksize; j++) {
                            int32_t y_j = IM_CLAMP(y + j, 0, (img->h - 1));
                            uint16_t *k_row_ptr = IMAGE_COMPUTE_RGB565_PIXEL_ROW_PTR(img, y_j);

                            for (int32_t k = -ksize; k <= ksize; k++) {
                                int32_t x_k = IM_CLAMP(x + k, 0, (img->w - 1));
                                int32_t pixel = IMAGE_GET_RGB565_PIXEL_FAST(k_row_ptr, x_k);
                                int32_t krn_val = krn[ptr++];
                                r_acc += krn_val * COLOR_RGB565_TO_R5(pixel);
                                g_acc += krn_val * COLOR_RGB565_TO_G6(pixel);
                                b_acc += krn_val * COLOR_RGB565_TO_B5(pixel);
                            }
                        }
                    }

                    int32_t r_pixel = div_fast_usat_s16(r_acc, div_sum, 5);
                    int32_t g_pixel = div_fast_usat_s16(g_acc, div_sum, 6);
                    int32_t b_pixel = div_fast_usat_s16(b_acc, div_sum, 5);
                    int32_t pixel = COLOR_R5_G6_B5_TO_RGB565(r_pixel, g_pixel, b_pixel);

                    if (threshold) {
                        pixel = COLOR_RGB565_TO_Y(pixel) - offset;
                        pixel = pixel < COLOR_RGB565_TO_Y(IMAGE_GET_RGB565_PIXEL_FAST(row_ptr, x));
                        pixel = (pixel ^ invert) * COLOR_RGB565_BINARY_MAX;
                    }

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

#ifdef IMLIB_ENABLE_BILATERAL
static float gaussian(float x, float sigma) {
    return fast_expf((x * x) / (-2.0f * sigma * sigma)) / (fabsf(sigma) * 2.506628f); // sqrt(2 * PI)
}

static float distance(int x, int y) {
    return fast_sqrtf((x * x) + (y * y));
}

void imlib_bilateral_filter(image_t *img,
                            const int ksize,
                            float color_sigma,
                            float space_sigma,
                            bool threshold,
                            int offset,
                            bool invert,
                            image_t *mask) {
    int brows = ksize + 1;
    image_t buf;
    buf.w = img->w;
    buf.h = brows;
    buf.pixfmt = img->pixfmt;

    switch (img->pixfmt) {
        case PIXFORMAT_BINARY: {
            buf.data = fb_alloc(IMAGE_BINARY_LINE_LEN_BYTES(img) * brows, FB_ALLOC_NO_HINT);
            float *gi_lut_ptr = fb_alloc((COLOR_BINARY_MAX - COLOR_BINARY_MIN + 1) * sizeof(float) * 2, FB_ALLOC_NO_HINT);
            float *gi_lut = &gi_lut_ptr[1];
            float max_color = IM_DIV(1.0f, COLOR_BINARY_MAX - COLOR_BINARY_MIN);
            for (int i = COLOR_BINARY_MIN; i <= COLOR_BINARY_MAX; i++) {
                gi_lut[-i] = gi_lut[i] = gaussian(i * max_color, color_sigma);
            }

            int n = (ksize * 2) + 1;
            float *gs_lut = fb_alloc(n * n * sizeof(float), FB_ALLOC_NO_HINT);

            float max_space = IM_DIV(1.0f, distance(ksize, ksize));
            for (int y = -ksize; y <= ksize; y++) {
                for (int x = -ksize; x <= ksize; x++) {
                    gs_lut[(n * (y + ksize)) + (x + ksize)] = gaussian(distance(x, y) * max_space, space_sigma);
                }
            }

            for (int y = 0, yy = img->h; y < yy; y++) {
                uint32_t *row_ptr = IMAGE_COMPUTE_BINARY_PIXEL_ROW_PTR(img, y);
                uint32_t *buf_row_ptr = IMAGE_COMPUTE_BINARY_PIXEL_ROW_PTR(&buf, (y % brows));

                for (int x = 0, xx = img->w; x < xx; x++) {
                    if (mask && (!image_get_mask_pixel(mask, x, y))) {
                        IMAGE_PUT_BINARY_PIXEL_FAST(buf_row_ptr, x, IMAGE_GET_BINARY_PIXEL_FAST(row_ptr, x));
                        continue; // Short circuit.
                    }

                    int this_pixel = IMAGE_GET_BINARY_PIXEL_FAST(row_ptr, x);
                    float i_acc = 0, w_acc = 0;
                    int ptr = 0;
                    for (int j = -ksize; j <= ksize; j++) {
                        int y_j = IM_CLAMP(y + j, 0, (img->h - 1));
                        uint32_t *k_row_ptr = IMAGE_COMPUTE_BINARY_PIXEL_ROW_PTR(img, y_j);

                        for (int k = -ksize; k <= ksize; k++) {
                            int x_k = IM_CLAMP(x + k, 0, (img->w - 1));
                            int pixel = IMAGE_GET_BINARY_PIXEL_FAST(k_row_ptr, x_k);
                            float w = gi_lut[(this_pixel - pixel)] * gs_lut[ptr++];
                            i_acc += pixel * w;
                            w_acc += w;
                        }
                    }

                    int pixel = fast_floorf(IM_MIN(IM_DIV(i_acc, w_acc), COLOR_BINARY_MAX));

                    if (threshold) {
                        if (((pixel - offset) < IMAGE_GET_BINARY_PIXEL_FAST(row_ptr, x)) ^ invert) {
                            pixel = COLOR_BINARY_MAX;
                        } else {
                            pixel = COLOR_BINARY_MIN;
                        }
                    }

                    IMAGE_PUT_BINARY_PIXEL_FAST(buf_row_ptr, x, pixel);
                }

                if (y >= ksize) {
                    // Transfer buffer lines...
                    memcpy(IMAGE_COMPUTE_BINARY_PIXEL_ROW_PTR(img, (y - ksize)),
                           IMAGE_COMPUTE_BINARY_PIXEL_ROW_PTR(&buf, ((y - ksize) % brows)),
                           IMAGE_BINARY_LINE_LEN_BYTES(img));
                }
            }

            // Copy any remaining lines from the buffer image...
            for (int y = IM_MAX(img->h - ksize, 0), yy = img->h; y < yy; y++) {
                memcpy(IMAGE_COMPUTE_BINARY_PIXEL_ROW_PTR(img, y),
                       IMAGE_COMPUTE_BINARY_PIXEL_ROW_PTR(&buf, (y % brows)),
                       IMAGE_BINARY_LINE_LEN_BYTES(img));
            }

            fb_free();
            fb_free();
            fb_free();
            break;
        }
        case PIXFORMAT_GRAYSCALE: {
            buf.data = fb_alloc(IMAGE_GRAYSCALE_LINE_LEN_BYTES(img) * brows, FB_ALLOC_NO_HINT);
            float *gi_lut_ptr = fb_alloc((COLOR_GRAYSCALE_MAX - COLOR_GRAYSCALE_MIN + 1) * sizeof(float) * 2, FB_ALLOC_NO_HINT);
            float *gi_lut = &gi_lut_ptr[256]; // point to the middle
            float max_color = IM_DIV(1.0f, COLOR_GRAYSCALE_MAX - COLOR_GRAYSCALE_MIN);
            for (int i = COLOR_GRAYSCALE_MIN; i <= COLOR_GRAYSCALE_MAX; i++) {
                gi_lut[-i] = gi_lut[i] = gaussian(i * max_color, color_sigma);
            }

            int n = (ksize * 2) + 1;
            float *gs_lut = fb_alloc(n * n * sizeof(float), FB_ALLOC_NO_HINT);

            float max_space = IM_DIV(1.0f, distance(ksize, ksize));
            for (int y = -ksize; y <= ksize; y++) {
                for (int x = -ksize; x <= ksize; x++) {
                    gs_lut[(n * (y + ksize)) + (x + ksize)] = gaussian(distance(x, y) * max_space, space_sigma);
                }
            }

            for (int y = 0, yy = img->h; y < yy; y++) {
                uint8_t *row_ptr = IMAGE_COMPUTE_GRAYSCALE_PIXEL_ROW_PTR(img, y);
                uint8_t *buf_row_ptr = IMAGE_COMPUTE_GRAYSCALE_PIXEL_ROW_PTR(&buf, (y % brows));

                for (int x = 0, xx = img->w; x < xx; x++) {
                    if (mask && (!image_get_mask_pixel(mask, x, y))) {
                        IMAGE_PUT_GRAYSCALE_PIXEL_FAST(buf_row_ptr, x, IMAGE_GET_GRAYSCALE_PIXEL_FAST(row_ptr, x));
                        continue; // Short circuit.
                    }

                    int this_pixel = IMAGE_GET_GRAYSCALE_PIXEL_FAST(row_ptr, x);
                    float i_acc = 0, w_acc = 0;
                    int ptr = 0;
                    if (x >= ksize && x < img->w - ksize && y >= ksize && y < img->h - ksize) {
                        for (int j = -ksize; j <= ksize; j++) {
                            uint8_t *k_row_ptr = IMAGE_COMPUTE_GRAYSCALE_PIXEL_ROW_PTR(img, y + j);
                            for (int k = -ksize; k <= ksize; k++) {
                                int pixel = IMAGE_GET_GRAYSCALE_PIXEL_FAST(k_row_ptr, x + k);
                                float w = gi_lut[this_pixel - pixel] * gs_lut[ptr++];
                                i_acc += pixel * w;
                                w_acc += w;
                            }
                        }
                    } else {
                        for (int j = -ksize; j <= ksize; j++) {
                            int y_j = IM_CLAMP(y + j, 0, (img->h - 1));
                            uint8_t *k_row_ptr = IMAGE_COMPUTE_GRAYSCALE_PIXEL_ROW_PTR(img, y_j);

                            for (int k = -ksize; k <= ksize; k++) {
                                int x_k = IM_CLAMP(x + k, 0, (img->w - 1));
                                int pixel = IMAGE_GET_GRAYSCALE_PIXEL_FAST(k_row_ptr, x_k);
                                float w = gi_lut[(this_pixel - pixel)] * gs_lut[ptr++];
                                i_acc += pixel * w;
                                w_acc += w;
                            }
                        }
                    }

                    int pixel = fast_floorf(IM_MIN(IM_DIV(i_acc, w_acc), COLOR_GRAYSCALE_MAX));

                    if (threshold) {
                        if (((pixel - offset) < IMAGE_GET_GRAYSCALE_PIXEL_FAST(row_ptr, x)) ^ invert) {
                            pixel = COLOR_GRAYSCALE_BINARY_MAX;
                        } else {
                            pixel = COLOR_GRAYSCALE_BINARY_MIN;
                        }
                    }

                    IMAGE_PUT_GRAYSCALE_PIXEL_FAST(buf_row_ptr, x, pixel);
                }

                if (y >= ksize) {
                    // Transfer buffer lines...
                    memcpy(IMAGE_COMPUTE_GRAYSCALE_PIXEL_ROW_PTR(img, (y - ksize)),
                           IMAGE_COMPUTE_GRAYSCALE_PIXEL_ROW_PTR(&buf, ((y - ksize) % brows)),
                           IMAGE_GRAYSCALE_LINE_LEN_BYTES(img));
                }
            }

            // Copy any remaining lines from the buffer image...
            for (int y = IM_MAX(img->h - ksize, 0), yy = img->h; y < yy; y++) {
                memcpy(IMAGE_COMPUTE_GRAYSCALE_PIXEL_ROW_PTR(img, y),
                       IMAGE_COMPUTE_GRAYSCALE_PIXEL_ROW_PTR(&buf, (y % brows)),
                       IMAGE_GRAYSCALE_LINE_LEN_BYTES(img));
            }

            fb_free();
            fb_free();
            fb_free();
            break;
        }
        case PIXFORMAT_RGB565: {
            buf.data = fb_alloc(IMAGE_RGB565_LINE_LEN_BYTES(img) * brows, FB_ALLOC_NO_HINT);
            float *rb_gi_ptr = fb_alloc((COLOR_R5_MAX - COLOR_R5_MIN + 1) * sizeof(float) * 2, FB_ALLOC_NO_HINT);
            float *g_gi_ptr = fb_alloc((COLOR_G6_MAX - COLOR_G6_MIN + 1) * sizeof(float) * 2, FB_ALLOC_NO_HINT);
            float *rb_gi_lut = &rb_gi_ptr[32]; // center
            float *g_gi_lut = &g_gi_ptr[64];

            float r_max_color = IM_DIV(1.0f, COLOR_R5_MAX - COLOR_R5_MIN);
            for (int i = COLOR_R5_MIN; i <= COLOR_R5_MAX; i++) {
                rb_gi_lut[-i] = rb_gi_lut[i] = gaussian(i * r_max_color, color_sigma);
            }

            float g_max_color = IM_DIV(1.0f, COLOR_G6_MAX - COLOR_G6_MIN);
            for (int i = COLOR_G6_MIN; i <= COLOR_G6_MAX; i++) {
                g_gi_lut[-i] = g_gi_lut[i] = gaussian(i * g_max_color, color_sigma);
            }

            int n = (ksize * 2) + 1;
            float *gs_lut = fb_alloc(n * n * sizeof(float), FB_ALLOC_NO_HINT);

            float max_space = IM_DIV(1.0f, distance(ksize, ksize));
            for (int y = -ksize; y <= ksize; y++) {
                for (int x = -ksize; x <= ksize; x++) {
                    gs_lut[(n * (y + ksize)) + (x + ksize)] = gaussian(distance(x, y) * max_space, space_sigma);
                }
            }

            for (int y = 0, yy = img->h; y < yy; y++) {
                uint16_t *row_ptr = IMAGE_COMPUTE_RGB565_PIXEL_ROW_PTR(img, y);
                uint16_t *buf_row_ptr = IMAGE_COMPUTE_RGB565_PIXEL_ROW_PTR(&buf, (y % brows));

                for (int x = 0, xx = img->w; x < xx; x++) {
                    if (mask && (!image_get_mask_pixel(mask, x, y))) {
                        IMAGE_PUT_RGB565_PIXEL_FAST(buf_row_ptr, x, IMAGE_GET_RGB565_PIXEL_FAST(row_ptr, x));
                        continue; // Short circuit.
                    }

                    int this_pixel = IMAGE_GET_RGB565_PIXEL_FAST(row_ptr, x);
                    int r_this_pixel = COLOR_RGB565_TO_R5(this_pixel);
                    int g_this_pixel = COLOR_RGB565_TO_G6(this_pixel);
                    int b_this_pixel = COLOR_RGB565_TO_B5(this_pixel);
                    float r_i_acc = 0, r_w_acc = 0;
                    float g_i_acc = 0, g_w_acc = 0;
                    float b_i_acc = 0, b_w_acc = 0;
                    int ptr = 0;
                    if (x >= ksize && x < img->w - ksize && y >= ksize && y < img->h - ksize) {
                        for (int j = -ksize; j <= ksize; j++) {
                            uint16_t *k_row_ptr = IMAGE_COMPUTE_RGB565_PIXEL_ROW_PTR(img, y + j);
                            for (int k = -ksize; k <= ksize; k++) {
                                int pixel = IMAGE_GET_RGB565_PIXEL_FAST(k_row_ptr, x + k);
                                int r_pixel = COLOR_RGB565_TO_R5(pixel);
                                int g_pixel = COLOR_RGB565_TO_G6(pixel);
                                int b_pixel = COLOR_RGB565_TO_B5(pixel);
                                float gs = gs_lut[ptr++];
                                float r_w = rb_gi_lut[(r_this_pixel - r_pixel)] * gs;
                                float g_w = g_gi_lut[(g_this_pixel - g_pixel)] * gs;
                                float b_w = rb_gi_lut[(b_this_pixel - b_pixel)] * gs;
                                r_i_acc += r_pixel * r_w;
                                r_w_acc += r_w;
                                g_i_acc += g_pixel * g_w;
                                g_w_acc += g_w;
                                b_i_acc += b_pixel * b_w;
                                b_w_acc += b_w;
                            }
                        }
                    } else {
                        // check boundary conditions
                        for (int j = -ksize; j <= ksize; j++) {
                            int y_j = IM_CLAMP(y + j, 0, (img->h - 1));
                            uint16_t *k_row_ptr = IMAGE_COMPUTE_RGB565_PIXEL_ROW_PTR(img, y_j);

                            for (int k = -ksize; k <= ksize; k++) {
                                int x_k = IM_CLAMP(x + k, 0, (img->w - 1));
                                int pixel = IMAGE_GET_RGB565_PIXEL_FAST(k_row_ptr, x_k);
                                int r_pixel = COLOR_RGB565_TO_R5(pixel);
                                int g_pixel = COLOR_RGB565_TO_G6(pixel);
                                int b_pixel = COLOR_RGB565_TO_B5(pixel);
                                float gs = gs_lut[ptr++];
                                float r_w = rb_gi_lut[(r_this_pixel - r_pixel)] * gs;
                                float g_w = g_gi_lut[(g_this_pixel - g_pixel)] * gs;
                                float b_w = rb_gi_lut[(b_this_pixel - b_pixel)] * gs;
                                r_i_acc += r_pixel * r_w;
                                r_w_acc += r_w;
                                g_i_acc += g_pixel * g_w;
                                g_w_acc += g_w;
                                b_i_acc += b_pixel * b_w;
                                b_w_acc += b_w;
                            }
                        }
                    }

                    int pixel = COLOR_R5_G6_B5_TO_RGB565(fast_floorf(IM_MIN(IM_DIV(r_i_acc, r_w_acc), COLOR_R5_MAX)),
                                                         fast_floorf(IM_MIN(IM_DIV(g_i_acc, g_w_acc), COLOR_G6_MAX)),
                                                         fast_floorf(IM_MIN(IM_DIV(b_i_acc, b_w_acc), COLOR_B5_MAX)));

                    if (threshold) {
                        if (((COLOR_RGB565_TO_Y(pixel) - offset) <
                             COLOR_RGB565_TO_Y(IMAGE_GET_RGB565_PIXEL_FAST(row_ptr, x))) ^ invert) {
                            pixel = COLOR_RGB565_BINARY_MAX;
                        } else {
                            pixel = COLOR_RGB565_BINARY_MIN;
                        }
                    }

                    IMAGE_PUT_RGB565_PIXEL_FAST(buf_row_ptr, x, pixel);
                }

                if (y >= ksize) {
                    // Transfer buffer lines...
                    memcpy(IMAGE_COMPUTE_RGB565_PIXEL_ROW_PTR(img, (y - ksize)),
                           IMAGE_COMPUTE_RGB565_PIXEL_ROW_PTR(&buf, ((y - ksize) % brows)),
                           IMAGE_RGB565_LINE_LEN_BYTES(img));
                }
            }

            // Copy any remaining lines from the buffer image...
            for (int y = IM_MAX(img->h - ksize, 0), yy = img->h; y < yy; y++) {
                memcpy(IMAGE_COMPUTE_RGB565_PIXEL_ROW_PTR(img, y),
                       IMAGE_COMPUTE_RGB565_PIXEL_ROW_PTR(&buf, (y % brows)),
                       IMAGE_RGB565_LINE_LEN_BYTES(img));
            }

            fb_free();
            fb_free();
            fb_free();
            fb_free();
            break;
        }
        default: {
            break;
        }
    }
}
#endif // IMLIB_ENABLE_BILATERAL
