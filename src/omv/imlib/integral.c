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
 * Integral image.
 */
#include <stdlib.h>
#include <string.h>
#include <arm_math.h>
#include "imlib.h"
#include "fb_alloc.h"

void imlib_integral_image_alloc(i_image_t *sum, int w, int h) {
    sum->w = w;
    sum->h = h;
    sum->data = fb_alloc(w * h * sizeof(*sum->data));
}

void imlib_integral_image_free(i_image_t *sum) {
    // 1 allocation
    fb_free();
}

void imlib_integral_image(image_t *src, i_image_t *sum) {
    typeof(*src->data) * img_data = src->data;
    typeof(*sum->data) * sum_data = sum->data;

    // Compute first column to avoid branching
    for (int s = 0, x = 0; x < src->w; x++) {
        /* sum of the current row (integer) */
        s += img_data[x];
        sum_data[x] = s;
    }

    for (int y = 1; y < src->h; y++) {
        /* loop over the number of columns */
        for (int s = 0, x = 0; x < src->w; x++) {
            /* sum of the current row (integer) */
            s += img_data[y * src->w + x];
            sum_data[y * src->w + x] = s + sum_data[(y - 1) * src->w + x];
        }
    }
}

void imlib_integral_image_scaled(image_t *src, i_image_t *sum) {
    typeof(*src->data) * img_data = src->data;
    typeof(*sum->data) * sum_data = sum->data;

    int x_ratio = (int) ((src->w << 16) / sum->w) + 1;
    int y_ratio = (int) ((src->h << 16) / sum->h) + 1;

    // Compute first column to avoid branching
    for (int s = 0, x = 0; x < sum->w; x++) {
        int sx = (x * x_ratio) >> 16;
        /* sum of the current row (integer) */
        s += img_data[sx];
        sum_data[x] = s;
    }

    for (int y = 1; y < sum->h; y++) {
        int sy = (y * y_ratio) >> 16;
        /* loop over the number of columns */
        for (int s = 0, x = 0; x < sum->w; x++) {
            int sx = (x * x_ratio) >> 16;

            /* sum of the current row (integer) */
            s += img_data[sy * src->w + sx];
            sum_data[y * sum->w + x] = s + sum_data[(y - 1) * sum->w + x];
        }
    }
}

void imlib_integral_image_sq(image_t *src, i_image_t *sum) {
    typeof(*src->data) * img_data = src->data;
    typeof(*sum->data) * sum_data = sum->data;

    // Compute first column to avoid branching
    for (uint32_t s = 0, x = 0; x < src->w; x++) {
        /* sum of the current row (integer) */
        s += img_data[x] * img_data[x];
        sum_data[x] = s;
    }

    for (uint32_t y = 1; y < src->h; y++) {
        /* loop over the number of columns */
        for (uint32_t s = 0, x = 0; x < src->w; x++) {
            /* sum of the current row (integer) */
            s += img_data[y * src->w + x] * img_data[y * src->w + x];
            sum_data[y * src->w + x] = s + sum_data[(y - 1) * src->w + x];
        }
    }

}

uint32_t imlib_integral_lookup(i_image_t *sum, int x, int y, int w, int h) {
#define PIXEL_AT(x, y) \
    (sum->data[((y) - 1) * sum->w + ((x) - 1)])
    if (x == 0 && y == 0) {
        return PIXEL_AT(w, h);
    } else if (y == 0) {
        return PIXEL_AT(w + x, h) - PIXEL_AT(x, h);
    } else if (x == 0) {
        return PIXEL_AT(w, h + y) - PIXEL_AT(w, y);
    } else {
        return PIXEL_AT(w + x, h + y) + PIXEL_AT(x, y) - PIXEL_AT(w + x, y) - PIXEL_AT(x, h + y);
    }
#undef  PIXEL_AT
}
