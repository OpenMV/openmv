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
 * Statistics functions.
 */
#include "imlib.h"

#ifdef IMLIB_ENABLE_GET_SIMILARITY
typedef struct imlib_similarity_line_op_state {
    bool dssim;
    int *sumBucketsOfX, *sumBucketsOfY, *sum2BucketsOfX, *sum2BucketsOfY, *sum2Buckets;
    float similarity_sum, similarity_sum_2, similarity_min, similarity_max;
    int lines_processed, lines;
} imlib_similarity_line_op_state_t;

static void imlib_similarity_line_op(int x, int x_end, int y_row, imlib_draw_row_data_t *data) {
    imlib_similarity_line_op_state_t *state = data->callback_arg;
    float c1 = 0, c2 = 0;
    int x_start = x;

    switch (data->dst_img->pixfmt) {
        case PIXFORMAT_BINARY: {
            uint32_t *row_ptr = IMAGE_COMPUTE_BINARY_PIXEL_ROW_PTR(data->dst_img, y_row);
            uint32_t *other_row_ptr = (uint32_t *) data->dst_row_override;
            for (; x < x_end; x++) {
                int pixel = IMAGE_GET_BINARY_PIXEL_FAST(row_ptr, x);
                int other_pixel = IMAGE_GET_BINARY_PIXEL_FAST(other_row_ptr, x);
                int bucket = (x - x_start) / 8;
                state->sumBucketsOfX[bucket] += pixel;
                state->sumBucketsOfY[bucket] += other_pixel;
                state->sum2BucketsOfX[bucket] += pixel * pixel;
                state->sum2BucketsOfY[bucket] += other_pixel * other_pixel;
                state->sum2Buckets[bucket] += pixel * other_pixel;
            }
            c1 = (COLOR_BINARY_MAX * 0.01f) * (COLOR_BINARY_MAX * 0.01f);
            c2 = (COLOR_BINARY_MAX * 0.03f) * (COLOR_BINARY_MAX * 0.03f);
            break;
        }
        case PIXFORMAT_GRAYSCALE: {
            uint8_t *row_ptr = IMAGE_COMPUTE_GRAYSCALE_PIXEL_ROW_PTR(data->dst_img, y_row);
            uint8_t *other_row_ptr = (uint8_t *) data->dst_row_override;
            for (; x < x_end; x++) {
                int pixel = IMAGE_GET_GRAYSCALE_PIXEL_FAST(row_ptr, x);
                int other_pixel = IMAGE_GET_GRAYSCALE_PIXEL_FAST(other_row_ptr, x);
                int bucket = (x - x_start) / 8;
                state->sumBucketsOfX[bucket] += pixel;
                state->sumBucketsOfY[bucket] += other_pixel;
                state->sum2BucketsOfX[bucket] += pixel * pixel;
                state->sum2BucketsOfY[bucket] += other_pixel * other_pixel;
                state->sum2Buckets[bucket] += pixel * other_pixel;
            }
            c1 = (COLOR_GRAYSCALE_MAX * 0.01f) * (COLOR_GRAYSCALE_MAX * 0.01f);
            c2 = (COLOR_GRAYSCALE_MAX * 0.03f) * (COLOR_GRAYSCALE_MAX * 0.03f);
            break;
        }
        case PIXFORMAT_RGB565: {
            uint16_t *row_ptr = IMAGE_COMPUTE_RGB565_PIXEL_ROW_PTR(data->dst_img, y_row);
            uint16_t *other_row_ptr = (uint16_t *) data->dst_row_override;
            for (; x < x_end; x++) {
                int pixel = COLOR_RGB565_TO_Y(IMAGE_GET_RGB565_PIXEL_FAST(row_ptr, x));
                int other_pixel = COLOR_RGB565_TO_Y(IMAGE_GET_RGB565_PIXEL_FAST(other_row_ptr, x));
                int bucket = (x - x_start) / 8;
                state->sumBucketsOfX[bucket] += pixel;
                state->sumBucketsOfY[bucket] += other_pixel;
                state->sum2BucketsOfX[bucket] += pixel * pixel;
                state->sum2BucketsOfY[bucket] += other_pixel * other_pixel;
                state->sum2Buckets[bucket] += pixel * other_pixel;
            }
            c1 = (COLOR_Y_MAX * 0.01f) * (COLOR_Y_MAX * 0.01f);
            c2 = (COLOR_Y_MAX * 0.03f) * (COLOR_Y_MAX * 0.03f);
            break;
        }
        default: {
            break;
        }
    }

    // https://en.wikipedia.org/wiki/Structural_similarity
    if ((!((state->lines_processed + 1) % 8)) || ((state->lines_processed + 1) == state->lines)) {
        for (x = x_start; x < x_end; x += 8) {
            int bucket = (x - x_start) / 8;
            int w = IM_MIN((x_end - x), 8);
            int h = IM_MIN((state->lines - state->lines_processed), 8);
            float size = w * h;

            // Dividng the sum squared buckets by size causes a loss of accuracy which results in
            // the single pass standard deviation formula giving the wrong answer. To bypass this
            // vx, vy, vxy have been multiplied by size which will be divided back out in the final
            // ssim calculation (given c1/c2 ~= 0).

            float mx = state->sumBucketsOfX[bucket] / size;
            float my = state->sumBucketsOfY[bucket] / size;
            float vx = state->sum2BucketsOfX[bucket] - (size * mx * mx);
            float vy = state->sum2BucketsOfY[bucket] - (size * my * my);
            float vxy = state->sum2Buckets[bucket] - (size * mx * my);

            float ssim = (((2 * mx * my) + c1) * ((2 * vxy) + c2)) /
                         (((mx * mx) + (my * my) + c1) * (vx + vy + c2));

            if (state->dssim) {
                ssim = (1.0f - ssim) / 2.0f;
            }

            state->similarity_sum += ssim;
            state->similarity_sum_2 += ssim * ssim;
            state->similarity_min = IM_MIN(state->similarity_min, ssim);
            state->similarity_max = IM_MAX(state->similarity_max, ssim);

            state->sumBucketsOfX[bucket] = 0;
            state->sumBucketsOfY[bucket] = 0;
            state->sum2BucketsOfX[bucket] = 0;
            state->sum2BucketsOfY[bucket] = 0;
            state->sum2Buckets[bucket] = 0;
        }
    }

    state->lines_processed += 1;
}

void imlib_get_similarity(image_t *img,
                          image_t *other,
                          int x_start,
                          int y_start,
                          float x_scale,
                          float y_scale,
                          rectangle_t *roi,
                          int rgb_channel,
                          int alpha,
                          const uint16_t *color_palette,
                          const uint8_t *alpha_palette,
                          image_hint_t hint,
                          bool dssim,
                          float *avg,
                          float *std,
                          float *min,
                          float *max) {
    point_t p0, p1;
    imlib_draw_image_get_bounds(img, other, x_start, y_start, x_scale, y_scale, roi,
                                alpha, alpha_palette, hint, &p0, &p1);
    int h_blocks = ((p1.x - p0.x) + 7) / 8;
    int v_blocks = ((p1.y - p0.y) + 7) / 8;
    int blocks = h_blocks * v_blocks;

    if (!blocks) {
        return;
    }

    int int_h_blocks = h_blocks * sizeof(int);
    imlib_similarity_line_op_state_t state;
    state.dssim = dssim;
    state.sumBucketsOfX = fb_alloc0(int_h_blocks * 5, FB_ALLOC_NO_HINT);
    state.sumBucketsOfY = state.sumBucketsOfX + int_h_blocks;
    state.sum2BucketsOfX = state.sumBucketsOfY + int_h_blocks;
    state.sum2BucketsOfY = state.sum2BucketsOfX + int_h_blocks;
    state.sum2Buckets = state.sum2BucketsOfY + int_h_blocks;
    state.similarity_sum = 0.0f;
    state.similarity_sum_2 = 0.0f;
    state.similarity_min = FLT_MAX;
    state.similarity_max = -FLT_MAX;
    state.lines_processed = 0;
    state.lines = p1.y - p0.y;

    void *dst_row_override = fb_alloc0(image_line_size(img), FB_ALLOC_NO_HINT);
    imlib_draw_image(img, other, x_start, y_start, x_scale, y_scale, roi,
                     rgb_channel, alpha, color_palette, alpha_palette,
                     hint, imlib_similarity_line_op, &state, dst_row_override);

    *avg = state.similarity_sum / blocks;
    *std = fast_sqrtf((state.similarity_sum_2 / blocks) - ((*avg) * (*avg)));
    *min = state.similarity_min;
    *max = state.similarity_max;

    fb_free(); // dst_row_override
    fb_free(); // sumBucketsOfX
}
#endif // IMLIB_ENABLE_GET_SIMILARITY

void imlib_get_histogram(histogram_t *out, image_t *ptr, rectangle_t *roi, list_t *thresholds, bool invert, image_t *other) {
    switch (ptr->pixfmt) {
        case PIXFORMAT_BINARY: {
            memset(out->LBins, 0, out->LBinCount * sizeof(uint32_t));

            int pixel_count = roi->w * roi->h;
            float mult = (out->LBinCount - 1) / ((float) (COLOR_BINARY_MAX - COLOR_BINARY_MIN));

            if ((!thresholds) || (!list_size(thresholds))) {
                // Fast histogram code when no color thresholds list...
                if (!other) {
                    for (int y = roi->y, yy = roi->y + roi->h; y < yy; y++) {
                        uint32_t *row_ptr = IMAGE_COMPUTE_BINARY_PIXEL_ROW_PTR(ptr, y);
                        for (int x = roi->x, xx = roi->x + roi->w; x < xx; x++) {
                            int pixel = IMAGE_GET_BINARY_PIXEL_FAST(row_ptr, x);
                            ((uint32_t *) out->LBins)[fast_roundf((pixel - COLOR_BINARY_MIN) * mult)]++;
                        }
                    }
                } else {
                    for (int y = roi->y, yy = roi->y + roi->h; y < yy; y++) {
                        uint32_t *row_ptr = IMAGE_COMPUTE_BINARY_PIXEL_ROW_PTR(ptr, y),
                                 *other_row_ptr = IMAGE_COMPUTE_BINARY_PIXEL_ROW_PTR(other, y);
                        for (int x = roi->x, xx = roi->x + roi->w; x < xx; x++) {
                            int pixel = IMAGE_GET_BINARY_PIXEL_FAST(row_ptr, x) ^ IMAGE_GET_BINARY_PIXEL_FAST(other_row_ptr, x);
                            ((uint32_t *) out->LBins)[fast_roundf((pixel - COLOR_BINARY_MIN) * mult)]++;
                        }
                    }
                }
            } else {
                // Reset pixel count.
                pixel_count = 0;
                if (!other) {
                    list_for_each(it, thresholds) {
                        color_thresholds_list_lnk_data_t *lnk_data = list_get_data(it);
                        for (int y = roi->y, yy = roi->y + roi->h; y < yy; y++) {
                            uint32_t *row_ptr = IMAGE_COMPUTE_BINARY_PIXEL_ROW_PTR(ptr, y);
                            for (int x = roi->x, xx = roi->x + roi->w; x < xx; x++) {
                                int pixel = IMAGE_GET_BINARY_PIXEL_FAST(row_ptr, x);
                                if (COLOR_THRESHOLD_BINARY(pixel, lnk_data, invert)) {
                                    ((uint32_t *) out->LBins)[fast_roundf((pixel - COLOR_BINARY_MIN) * mult)]++;
                                    pixel_count++;
                                }
                            }
                        }
                    }
                } else {
                    list_for_each(it, thresholds) {
                        color_thresholds_list_lnk_data_t *lnk_data = list_get_data(it);

                        for (int y = roi->y, yy = roi->y + roi->h; y < yy; y++) {
                            uint32_t *row_ptr = IMAGE_COMPUTE_BINARY_PIXEL_ROW_PTR(ptr, y),
                                     *other_row_ptr = IMAGE_COMPUTE_BINARY_PIXEL_ROW_PTR(other, y);
                            for (int x = roi->x, xx = roi->x + roi->w; x < xx; x++) {
                                int pixel = IMAGE_GET_BINARY_PIXEL_FAST(row_ptr, x) ^ IMAGE_GET_BINARY_PIXEL_FAST(other_row_ptr,
                                                                                                                  x);
                                if (COLOR_THRESHOLD_BINARY(pixel, lnk_data, invert)) {
                                    ((uint32_t *) out->LBins)[fast_roundf((pixel - COLOR_BINARY_MIN) * mult)]++;
                                    pixel_count++;
                                }
                            }
                        }
                    }
                }
            }

            float pixels = IM_DIV(1, ((float) pixel_count));

            for (int i = 0, j = out->LBinCount; i < j; i++) {
                out->LBins[i] = ((uint32_t *) out->LBins)[i] * pixels;
            }

            break;
        }
        case PIXFORMAT_GRAYSCALE: {
            memset(out->LBins, 0, out->LBinCount * sizeof(uint32_t));

            int pixel_count = roi->w * roi->h;
            float mult = (out->LBinCount - 1) / ((float) (COLOR_GRAYSCALE_MAX - COLOR_GRAYSCALE_MIN));

            if ((!thresholds) || (!list_size(thresholds))) {
                // Fast histogram code when no color thresholds list...
                if (!other) {
                    for (int y = roi->y, yy = roi->y + roi->h; y < yy; y++) {
                        uint8_t *row_ptr = IMAGE_COMPUTE_GRAYSCALE_PIXEL_ROW_PTR(ptr, y);
                        for (int x = roi->x, xx = roi->x + roi->w; x < xx; x++) {
                            int pixel = IMAGE_GET_GRAYSCALE_PIXEL_FAST(row_ptr, x);
                            ((uint32_t *) out->LBins)[fast_roundf((pixel - COLOR_GRAYSCALE_MIN) * mult)]++;
                        }
                    }
                } else {
                    for (int y = roi->y, yy = roi->y + roi->h; y < yy; y++) {
                        uint8_t *row_ptr = IMAGE_COMPUTE_GRAYSCALE_PIXEL_ROW_PTR(ptr, y),
                                *other_row_ptr = IMAGE_COMPUTE_GRAYSCALE_PIXEL_ROW_PTR(other, y);
                        for (int x = roi->x, xx = roi->x + roi->w; x < xx; x++) {
                            int pixel =
                                abs(IMAGE_GET_GRAYSCALE_PIXEL_FAST(row_ptr, x) - IMAGE_GET_GRAYSCALE_PIXEL_FAST(other_row_ptr,
                                                                                                                x));
                            ((uint32_t *) out->LBins)[fast_roundf((pixel - COLOR_GRAYSCALE_MIN) * mult)]++;
                        }
                    }
                }
            } else {
                // Reset pixel count.
                pixel_count = 0;
                if (!other) {
                    list_for_each(it, thresholds) {
                        color_thresholds_list_lnk_data_t *lnk_data = list_get_data(it);

                        for (int y = roi->y, yy = roi->y + roi->h; y < yy; y++) {
                            uint8_t *row_ptr = IMAGE_COMPUTE_GRAYSCALE_PIXEL_ROW_PTR(ptr, y);
                            for (int x = roi->x, xx = roi->x + roi->w; x < xx; x++) {
                                int pixel = IMAGE_GET_GRAYSCALE_PIXEL_FAST(row_ptr, x);
                                if (COLOR_THRESHOLD_GRAYSCALE(pixel, lnk_data, invert)) {
                                    ((uint32_t *) out->LBins)[fast_roundf((pixel - COLOR_GRAYSCALE_MIN) * mult)]++;
                                    pixel_count++;
                                }
                            }
                        }
                    }
                } else {
                    list_for_each(it, thresholds) {
                        color_thresholds_list_lnk_data_t *lnk_data = list_get_data(it);

                        for (int y = roi->y, yy = roi->y + roi->h; y < yy; y++) {
                            uint8_t *row_ptr = IMAGE_COMPUTE_GRAYSCALE_PIXEL_ROW_PTR(ptr, y),
                                    *other_row_ptr = IMAGE_COMPUTE_GRAYSCALE_PIXEL_ROW_PTR(other, y);
                            for (int x = roi->x, xx = roi->x + roi->w; x < xx; x++) {
                                int pixel =
                                    abs(IMAGE_GET_GRAYSCALE_PIXEL_FAST(row_ptr,
                                                                       x) - IMAGE_GET_GRAYSCALE_PIXEL_FAST(other_row_ptr,
                                                                                                           x));
                                if (COLOR_THRESHOLD_GRAYSCALE(pixel, lnk_data, invert)) {
                                    ((uint32_t *) out->LBins)[fast_roundf((pixel - COLOR_GRAYSCALE_MIN) * mult)]++;
                                    pixel_count++;
                                }
                            }
                        }
                    }
                }
            }

            float pixels = IM_DIV(1, ((float) pixel_count));

            for (int i = 0, j = out->LBinCount; i < j; i++) {
                out->LBins[i] = ((uint32_t *) out->LBins)[i] * pixels;
            }

            break;
        }
        case PIXFORMAT_RGB565: {
            memset(out->LBins, 0, out->LBinCount * sizeof(uint32_t));
            memset(out->ABins, 0, out->ABinCount * sizeof(uint32_t));
            memset(out->BBins, 0, out->BBinCount * sizeof(uint32_t));

            int pixel_count = roi->w * roi->h;
            float l_mult = (out->LBinCount - 1) / ((float) (COLOR_L_MAX - COLOR_L_MIN));
            float a_mult = (out->ABinCount - 1) / ((float) (COLOR_A_MAX - COLOR_A_MIN));
            float b_mult = (out->BBinCount - 1) / ((float) (COLOR_B_MAX - COLOR_B_MIN));

            if ((!thresholds) || (!list_size(thresholds))) {
                // Fast histogram code when no color thresholds list...
                if (!other) {
                    for (int y = roi->y, yy = roi->y + roi->h; y < yy; y++) {
                        uint16_t *row_ptr = IMAGE_COMPUTE_RGB565_PIXEL_ROW_PTR(ptr, y);
                        for (int x = roi->x, xx = roi->x + roi->w; x < xx; x++) {
                            int pixel = IMAGE_GET_RGB565_PIXEL_FAST(row_ptr, x);
                            ((uint32_t *) out->LBins)[fast_roundf((COLOR_RGB565_TO_L(pixel) - COLOR_L_MIN) * l_mult)]++;
                            ((uint32_t *) out->ABins)[fast_roundf((COLOR_RGB565_TO_A(pixel) - COLOR_A_MIN) * a_mult)]++;
                            ((uint32_t *) out->BBins)[fast_roundf((COLOR_RGB565_TO_B(pixel) - COLOR_B_MIN) * b_mult)]++;
                        }
                    }
                } else {
                    for (int y = roi->y, yy = roi->y + roi->h; y < yy; y++) {
                        uint16_t *row_ptr = IMAGE_COMPUTE_RGB565_PIXEL_ROW_PTR(ptr, y),
                                 *other_row_ptr = IMAGE_COMPUTE_RGB565_PIXEL_ROW_PTR(other, y);
                        for (int x = roi->x, xx = roi->x + roi->w; x < xx; x++) {
                            int pixel = IMAGE_GET_RGB565_PIXEL_FAST(row_ptr, x);
                            int other_pixel = IMAGE_GET_RGB565_PIXEL_FAST(other_row_ptr, x);
                            int r = abs(COLOR_RGB565_TO_R5(pixel) - COLOR_RGB565_TO_R5(other_pixel));
                            int g = abs(COLOR_RGB565_TO_G6(pixel) - COLOR_RGB565_TO_G6(other_pixel));
                            int b = abs(COLOR_RGB565_TO_B5(pixel) - COLOR_RGB565_TO_B5(other_pixel));
                            pixel = COLOR_R5_G6_B5_TO_RGB565(r, g, b);
                            ((uint32_t *) out->LBins)[fast_roundf((COLOR_RGB565_TO_L(pixel) - COLOR_L_MIN) * l_mult)]++;
                            ((uint32_t *) out->ABins)[fast_roundf((COLOR_RGB565_TO_A(pixel) - COLOR_A_MIN) * a_mult)]++;
                            ((uint32_t *) out->BBins)[fast_roundf((COLOR_RGB565_TO_B(pixel) - COLOR_B_MIN) * b_mult)]++;
                        }
                    }
                }
            } else {
                // Reset pixel count.
                pixel_count = 0;
                if (!other) {
                    list_for_each(it, thresholds) {
                        color_thresholds_list_lnk_data_t *lnk_data = list_get_data(it);

                        for (int y = roi->y, yy = roi->y + roi->h; y < yy; y++) {
                            uint16_t *row_ptr = IMAGE_COMPUTE_RGB565_PIXEL_ROW_PTR(ptr, y);
                            for (int x = roi->x, xx = roi->x + roi->w; x < xx; x++) {
                                int pixel = IMAGE_GET_RGB565_PIXEL_FAST(row_ptr, x);
                                if (COLOR_THRESHOLD_RGB565(pixel, lnk_data, invert)) {
                                    ((uint32_t *) out->LBins)[fast_roundf((COLOR_RGB565_TO_L(pixel) - COLOR_L_MIN) * l_mult)]++;
                                    ((uint32_t *) out->ABins)[fast_roundf((COLOR_RGB565_TO_A(pixel) - COLOR_A_MIN) * a_mult)]++;
                                    ((uint32_t *) out->BBins)[fast_roundf((COLOR_RGB565_TO_B(pixel) - COLOR_B_MIN) * b_mult)]++;
                                    pixel_count++;
                                }
                            }
                        }
                    }
                } else {
                    list_for_each(it, thresholds) {
                        color_thresholds_list_lnk_data_t *lnk_data = list_get_data(it);

                        for (int y = roi->y, yy = roi->y + roi->h; y < yy; y++) {
                            uint16_t *row_ptr = IMAGE_COMPUTE_RGB565_PIXEL_ROW_PTR(ptr, y),
                                     *other_row_ptr = IMAGE_COMPUTE_RGB565_PIXEL_ROW_PTR(other, y);
                            for (int x = roi->x, xx = roi->x + roi->w; x < xx; x++) {
                                int pixel = IMAGE_GET_RGB565_PIXEL_FAST(row_ptr, x);
                                int other_pixel = IMAGE_GET_RGB565_PIXEL_FAST(other_row_ptr, x);
                                int r = abs(COLOR_RGB565_TO_R5(pixel) - COLOR_RGB565_TO_R5(other_pixel));
                                int g = abs(COLOR_RGB565_TO_G6(pixel) - COLOR_RGB565_TO_G6(other_pixel));
                                int b = abs(COLOR_RGB565_TO_B5(pixel) - COLOR_RGB565_TO_B5(other_pixel));
                                pixel = COLOR_R5_G6_B5_TO_RGB565(r, g, b);
                                if (COLOR_THRESHOLD_RGB565(pixel, lnk_data, invert)) {
                                    ((uint32_t *) out->LBins)[fast_roundf((COLOR_RGB565_TO_L(pixel) - COLOR_L_MIN) * l_mult)]++;
                                    ((uint32_t *) out->ABins)[fast_roundf((COLOR_RGB565_TO_A(pixel) - COLOR_A_MIN) * a_mult)]++;
                                    ((uint32_t *) out->BBins)[fast_roundf((COLOR_RGB565_TO_B(pixel) - COLOR_B_MIN) * b_mult)]++;
                                    pixel_count++;
                                }
                            }
                        }
                    }
                }
            }

            float pixels = IM_DIV(1, ((float) pixel_count));

            for (int i = 0, j = out->LBinCount; i < j; i++) {
                out->LBins[i] = ((uint32_t *) out->LBins)[i] * pixels;
            }

            for (int i = 0, j = out->ABinCount; i < j; i++) {
                out->ABins[i] = ((uint32_t *) out->ABins)[i] * pixels;
            }

            for (int i = 0, j = out->BBinCount; i < j; i++) {
                out->BBins[i] = ((uint32_t *) out->BBins)[i] * pixels;
            }

            break;
        }
        default: {
            break;
        }
    }
}

void imlib_get_percentile(percentile_t *out, pixformat_t pixfmt, histogram_t *ptr, float percentile) {
    memset(out, 0, sizeof(percentile_t));
    switch (pixfmt) {
        case PIXFORMAT_BINARY: {
            float mult = (COLOR_BINARY_MAX - COLOR_BINARY_MIN) / ((float) (ptr->LBinCount - 1));
            float median_count = 0;

            for (int i = 0, j = ptr->LBinCount; i < j; i++) {
                if ((median_count < percentile) && (percentile <= (median_count + ptr->LBins[i]))) {
                    out->LValue = fast_floorf((i * mult) + COLOR_BINARY_MIN);
                    break;
                }

                median_count += ptr->LBins[i];
            }
            break;
        }
        case PIXFORMAT_GRAYSCALE: {
            float mult = (COLOR_GRAYSCALE_MAX - COLOR_GRAYSCALE_MIN) / ((float) (ptr->LBinCount - 1));
            float median_count = 0;

            for (int i = 0, j = ptr->LBinCount; i < j; i++) {
                if ((median_count < percentile) && (percentile <= (median_count + ptr->LBins[i]))) {
                    out->LValue = fast_floorf((i * mult) + COLOR_GRAYSCALE_MIN);
                    break;
                }

                median_count += ptr->LBins[i];
            }
            break;
        }
        case PIXFORMAT_RGB565: {
            {
                float mult = (COLOR_L_MAX - COLOR_L_MIN) / ((float) (ptr->LBinCount - 1));
                float median_count = 0;

                for (int i = 0, j = ptr->LBinCount; i < j; i++) {
                    if ((median_count < percentile) && (percentile <= (median_count + ptr->LBins[i]))) {
                        out->LValue = fast_floorf((i * mult) + COLOR_L_MIN);
                        break;
                    }

                    median_count += ptr->LBins[i];
                }
            }
            {
                float mult = (COLOR_A_MAX - COLOR_A_MIN) / ((float) (ptr->ABinCount - 1));
                float median_count = 0;

                for (int i = 0, j = ptr->ABinCount; i < j; i++) {
                    if ((median_count < percentile) && (percentile <= (median_count + ptr->ABins[i]))) {
                        out->AValue = fast_floorf((i * mult) + COLOR_A_MIN);
                        break;
                    }

                    median_count += ptr->ABins[i];
                }
            }
            {
                float mult = (COLOR_B_MAX - COLOR_B_MIN) / ((float) (ptr->BBinCount - 1));
                float median_count = 0;

                for (int i = 0, j = ptr->BBinCount; i < j; i++) {
                    if ((median_count < percentile) && (percentile <= (median_count + ptr->BBins[i]))) {
                        out->BValue = fast_floorf((i * mult) + COLOR_B_MIN);
                        break;
                    }

                    median_count += ptr->BBins[i];
                }
            }
            break;
        }
        default: {
            break;
        }
    }
}

static int ostu(int bincount, float *bins) {
    float cdf[bincount]; memset(cdf, 0, bincount * sizeof(float));
    float weighted_cdf[bincount]; memset(weighted_cdf, 0, bincount * sizeof(float));

    cdf[0] = bins[0];
    weighted_cdf[0] = 0 * bins[0];

    for (int i = 1; i < bincount; i++) {
        cdf[i] = cdf[i - 1] + bins[i];
        weighted_cdf[i] = weighted_cdf[i - 1] + (i * bins[i]);
    }

    float variance[bincount]; memset(variance, 0, bincount * sizeof(float));
    float max_variance = 0.0f;
    int threshold = 0;

    for (int i = 0, ii = bincount - 1; i < ii; i++) {

        if ((cdf[i] != 0.0f) && (cdf[i] != 1.0f)) {
            variance[i] = powf((cdf[i] * weighted_cdf[bincount - 1]) - weighted_cdf[i], 2.0f) / (cdf[i] * (1.0f - cdf[i]));
        } else {
            variance[i] = 0.0f;
        }

        if (variance[i] > max_variance) {
            max_variance = variance[i];
            threshold = i;
        }
    }

    return threshold;
}

void imlib_get_threshold(threshold_t *out, pixformat_t pixfmt, histogram_t *ptr) {
    memset(out, 0, sizeof(threshold_t));
    switch (pixfmt) {
        case PIXFORMAT_BINARY: {
            out->LValue = (ostu(ptr->LBinCount, ptr->LBins) * (COLOR_BINARY_MAX - COLOR_BINARY_MIN)) / (ptr->LBinCount - 1);
            break;
        }
        case PIXFORMAT_GRAYSCALE: {
            out->LValue =
                (ostu(ptr->LBinCount, ptr->LBins) * (COLOR_GRAYSCALE_MAX - COLOR_GRAYSCALE_MIN)) / (ptr->LBinCount - 1);
            break;
        }
        case PIXFORMAT_RGB565: {
            out->LValue = (ostu(ptr->LBinCount, ptr->LBins) * (COLOR_L_MAX - COLOR_L_MIN)) / (ptr->LBinCount - 1);
            out->AValue = ((ostu(ptr->ABinCount, ptr->ABins) * (COLOR_A_MAX - COLOR_A_MIN)) / (ptr->ABinCount - 1)) +
                          COLOR_A_MIN;
            out->BValue = ((ostu(ptr->BBinCount, ptr->BBins) * (COLOR_B_MAX - COLOR_B_MIN)) / (ptr->BBinCount - 1)) +
                          COLOR_B_MIN;
            break;
        }
        default: {
            break;
        }
    }
}

void imlib_get_statistics(statistics_t *out, pixformat_t pixfmt, histogram_t *ptr) {
    memset(out, 0, sizeof(statistics_t));
    switch (pixfmt) {
        case PIXFORMAT_BINARY: {
            float mult = (COLOR_BINARY_MAX - COLOR_BINARY_MIN) / ((float) (ptr->LBinCount - 1));

            float avg = 0;
            float stdev = 0;
            float median_count = 0;
            float mode_count = 0;
            bool min_flag = false;

            for (int i = 0, j = ptr->LBinCount; i < j; i++) {
                float value_f = (i * mult) + COLOR_BINARY_MIN;
                int value = fast_floorf(value_f);

                avg += value_f * ptr->LBins[i];
                stdev += value_f * value_f * ptr->LBins[i];

                if ((median_count < 0.25f) && (0.25f <= (median_count + ptr->LBins[i]))) {
                    out->LLQ = value;
                }

                if ((median_count < 0.5f) && (0.5f <= (median_count + ptr->LBins[i]))) {
                    out->LMedian = value;
                }

                if ((median_count < 0.75f) && (0.75f <= (median_count + ptr->LBins[i]))) {
                    out->LUQ = value;
                }

                if (ptr->LBins[i] > mode_count) {
                    mode_count = ptr->LBins[i];
                    out->LMode = value;
                }

                if ((ptr->LBins[i] > 0.0f) && (!min_flag)) {
                    min_flag = true;
                    out->LMin = value;
                }

                if (ptr->LBins[i] > 0.0f) {
                    out->LMax = value;
                }

                median_count += ptr->LBins[i];
            }

            out->LMean = fast_floorf(avg);
            out->LSTDev = fast_floorf(fast_sqrtf(stdev - (avg * avg)));
            break;
        }
        case PIXFORMAT_GRAYSCALE: {
            float mult = (COLOR_GRAYSCALE_MAX - COLOR_GRAYSCALE_MIN) / ((float) (ptr->LBinCount - 1));

            float avg = 0;
            float stdev = 0;
            float median_count = 0;
            float mode_count = 0;
            bool min_flag = false;

            for (int i = 0, j = ptr->LBinCount; i < j; i++) {
                float value_f = (i * mult) + COLOR_GRAYSCALE_MIN;
                int value = fast_floorf(value_f);

                avg += value_f * ptr->LBins[i];
                stdev += value_f * value_f * ptr->LBins[i];

                if ((median_count < 0.25f) && (0.25f <= (median_count + ptr->LBins[i]))) {
                    out->LLQ = value;
                }

                if ((median_count < 0.5f) && (0.5f <= (median_count + ptr->LBins[i]))) {
                    out->LMedian = value;
                }

                if ((median_count < 0.75f) && (0.75f <= (median_count + ptr->LBins[i]))) {
                    out->LUQ = value;
                }

                if (ptr->LBins[i] > mode_count) {
                    mode_count = ptr->LBins[i];
                    out->LMode = value;
                }

                if ((ptr->LBins[i] > 0.0f) && (!min_flag)) {
                    min_flag = true;
                    out->LMin = value;
                }

                if (ptr->LBins[i] > 0.0f) {
                    out->LMax = value;
                }

                median_count += ptr->LBins[i];
            }

            out->LMean = fast_floorf(avg);
            out->LSTDev = fast_floorf(fast_sqrtf(stdev - (avg * avg)));
            break;
        }
        case PIXFORMAT_RGB565: {
            {
                float mult = (COLOR_L_MAX - COLOR_L_MIN) / ((float) (ptr->LBinCount - 1));

                float avg = 0;
                float stdev = 0;
                float median_count = 0;
                float mode_count = 0;
                bool min_flag = false;

                for (int i = 0, j = ptr->LBinCount; i < j; i++) {
                    float value_f = (i * mult) + COLOR_L_MIN;
                    int value = fast_floorf(value_f);

                    avg += value_f * ptr->LBins[i];
                    stdev += value_f * value_f * ptr->LBins[i];

                    if ((median_count < 0.25f) && (0.25f <= (median_count + ptr->LBins[i]))) {
                        out->LLQ = value;
                    }

                    if ((median_count < 0.5f) && (0.5f <= (median_count + ptr->LBins[i]))) {
                        out->LMedian = value;
                    }

                    if ((median_count < 0.75f) && (0.75f <= (median_count + ptr->LBins[i]))) {
                        out->LUQ = value;
                    }

                    if (ptr->LBins[i] > mode_count) {
                        mode_count = ptr->LBins[i];
                        out->LMode = value;
                    }

                    if ((ptr->LBins[i] > 0.0f) && (!min_flag)) {
                        min_flag = true;
                        out->LMin = value;
                    }

                    if (ptr->LBins[i] > 0.0f) {
                        out->LMax = value;
                    }

                    median_count += ptr->LBins[i];
                }

                out->LMean = fast_floorf(avg);
                out->LSTDev = fast_floorf(fast_sqrtf(stdev - (avg * avg)));
            }
            {
                float mult = (COLOR_A_MAX - COLOR_A_MIN) / ((float) (ptr->ABinCount - 1));

                float avg = 0;
                float stdev = 0;
                float median_count = 0;
                float mode_count = 0;
                bool min_flag = false;

                for (int i = 0, j = ptr->ABinCount; i < j; i++) {
                    float value_f = (i * mult) + COLOR_A_MIN;
                    int value = fast_floorf(value_f);

                    avg += value_f * ptr->ABins[i];
                    stdev += value_f * value_f * ptr->ABins[i];

                    if ((median_count < 0.25f) && (0.25f <= (median_count + ptr->ABins[i]))) {
                        out->ALQ = value;
                    }

                    if ((median_count < 0.5f) && (0.5f <= (median_count + ptr->ABins[i]))) {
                        out->AMedian = value;
                    }

                    if ((median_count < 0.75f) && (0.75f <= (median_count + ptr->ABins[i]))) {
                        out->AUQ = value;
                    }

                    if (ptr->ABins[i] > mode_count) {
                        mode_count = ptr->ABins[i];
                        out->AMode = value;
                    }

                    if ((ptr->ABins[i] > 0.0f) && (!min_flag)) {
                        min_flag = true;
                        out->AMin = value;
                    }

                    if (ptr->ABins[i] > 0.0f) {
                        out->AMax = value;
                    }

                    median_count += ptr->ABins[i];
                }

                out->AMean = fast_floorf(avg);
                out->ASTDev = fast_floorf(fast_sqrtf(stdev - (avg * avg)));
            }
            {
                float mult = (COLOR_B_MAX - COLOR_B_MIN) / ((float) (ptr->BBinCount - 1));

                float avg = 0;
                float stdev = 0;
                float median_count = 0;
                float mode_count = 0;
                bool min_flag = false;

                for (int i = 0, j = ptr->BBinCount; i < j; i++) {
                    float value_f = (i * mult) + COLOR_B_MIN;
                    int value = fast_floorf(value_f);

                    avg += value_f * ptr->BBins[i];
                    stdev += value_f * value_f * ptr->BBins[i];

                    if ((median_count < 0.25f) && (0.25f <= (median_count + ptr->BBins[i]))) {
                        out->BLQ = value;
                    }

                    if ((median_count < 0.5f) && (0.5f <= (median_count + ptr->BBins[i]))) {
                        out->BMedian = value;
                    }

                    if ((median_count < 0.75f) && (0.75f <= (median_count + ptr->BBins[i]))) {
                        out->BUQ = value;
                    }

                    if (ptr->BBins[i] > mode_count) {
                        mode_count = ptr->BBins[i];
                        out->BMode = value;
                    }

                    if ((ptr->BBins[i] > 0.0f) && (!min_flag)) {
                        min_flag = true;
                        out->BMin = value;
                    }

                    if (ptr->BBins[i] > 0.0f) {
                        out->BMax = value;
                    }

                    median_count += ptr->BBins[i];
                }

                out->BMean = fast_floorf(avg);
                out->BSTDev = fast_floorf(fast_sqrtf(stdev - (avg * avg)));
            }
            break;
        }
        default: {
            break;
        }
    }
}

static int get_median(int *array, int array_sum, int array_len) {
    const int median_threshold = (array_sum + 1) / 2;
    int median_count = 0;

    for (int i = 0; i < array_len; i++) {
        if ((median_count < median_threshold) && (median_threshold <= (median_count + array[i]))) {
            return i;
        }
        median_count += array[i];
    }

    return array_len - 1;
}

static int get_median_l(long long *array, long long array_sum, int array_len) {
    const long long median_threshold = (array_sum + 1) / 2;
    long long median_count = 0;

    for (int i = 0; i < array_len; i++) {
        if ((median_count < median_threshold) && (median_threshold <= (median_count + array[i]))) {
            return i;
        }
        median_count += array[i];
    }

    return array_len - 1;
}

bool imlib_get_regression(find_lines_list_lnk_data_t *out,
                          image_t *ptr,
                          rectangle_t *roi,
                          unsigned int x_stride,
                          unsigned int y_stride,
                          list_t *thresholds,
                          bool invert,
                          unsigned int area_threshold,
                          unsigned int pixels_threshold,
                          bool robust) {
    bool result = false;
    memset(out, 0, sizeof(find_lines_list_lnk_data_t));

    if (!robust) {
        // Least Squares
        int blob_x1 = roi->x + roi->w - 1;
        int blob_y1 = roi->y + roi->h - 1;
        int blob_x2 = roi->x;
        int blob_y2 = roi->y;
        int blob_pixels = 0;
        int blob_cx = 0;
        int blob_cy = 0;
        long long blob_a = 0;
        long long blob_b = 0;
        long long blob_c = 0;

        list_for_each(it, thresholds) {
            color_thresholds_list_lnk_data_t *lnk_data = list_get_data(it);

            switch (ptr->pixfmt) {
                case PIXFORMAT_BINARY: {
                    for (int y = roi->y, yy = roi->y + roi->h; y < yy; y += y_stride) {
                        uint32_t *row_ptr = IMAGE_COMPUTE_BINARY_PIXEL_ROW_PTR(ptr, y);
                        for (int x = roi->x + (y % x_stride), xx = roi->x + roi->w; x < xx; x += x_stride) {
                            if (COLOR_THRESHOLD_BINARY(IMAGE_GET_BINARY_PIXEL_FAST(row_ptr, x), lnk_data, invert)) {
                                blob_x1 = IM_MIN(blob_x1, x);
                                blob_y1 = IM_MIN(blob_y1, y);
                                blob_x2 = IM_MAX(blob_x2, x);
                                blob_y2 = IM_MAX(blob_y2, y);
                                blob_pixels += 1;
                                blob_cx += x;
                                blob_cy += y;
                                blob_a += x * x;
                                blob_b += x * y;
                                blob_c += y * y;
                            }
                        }
                    }
                    break;
                }
                case PIXFORMAT_GRAYSCALE: {
                    for (int y = roi->y, yy = roi->y + roi->h; y < yy; y += y_stride) {
                        uint8_t *row_ptr = IMAGE_COMPUTE_GRAYSCALE_PIXEL_ROW_PTR(ptr, y);
                        for (int x = roi->x + (y % x_stride), xx = roi->x + roi->w; x < xx; x += x_stride) {
                            if (COLOR_THRESHOLD_GRAYSCALE(IMAGE_GET_GRAYSCALE_PIXEL_FAST(row_ptr, x), lnk_data, invert)) {
                                blob_x1 = IM_MIN(blob_x1, x);
                                blob_y1 = IM_MIN(blob_y1, y);
                                blob_x2 = IM_MAX(blob_x2, x);
                                blob_y2 = IM_MAX(blob_y2, y);
                                blob_pixels += 1;
                                blob_cx += x;
                                blob_cy += y;
                                blob_a += x * x;
                                blob_b += x * y;
                                blob_c += y * y;
                            }
                        }
                    }
                    break;
                }
                case PIXFORMAT_RGB565: {
                    for (int y = roi->y, yy = roi->y + roi->h; y < yy; y += y_stride) {
                        uint16_t *row_ptr = IMAGE_COMPUTE_RGB565_PIXEL_ROW_PTR(ptr, y);
                        for (int x = roi->x + (y % x_stride), xx = roi->x + roi->w; x < xx; x += x_stride) {
                            if (COLOR_THRESHOLD_RGB565(IMAGE_GET_RGB565_PIXEL_FAST(row_ptr, x), lnk_data, invert)) {
                                blob_x1 = IM_MIN(blob_x1, x);
                                blob_y1 = IM_MIN(blob_y1, y);
                                blob_x2 = IM_MAX(blob_x2, x);
                                blob_y2 = IM_MAX(blob_y2, y);
                                blob_pixels += 1;
                                blob_cx += x;
                                blob_cy += y;
                                blob_a += x * x;
                                blob_b += x * y;
                                blob_c += y * y;
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

        int w = blob_x2 - blob_x1;
        int h = blob_y2 - blob_y1;
        if (blob_pixels && ((w * h) >= area_threshold) && (blob_pixels >= pixels_threshold)) {
            // http://www.cse.usf.edu/~r1k/MachineVisionBook/MachineVision.files/MachineVision_Chapter2.pdf
            // https://www.strchr.com/standard_deviation_in_one_pass
            //
            // a = sigma(x*x) + (mx*sigma(x)) + (mx*sigma(x)) + (sigma()*mx*mx)
            // b = sigma(x*y) + (mx*sigma(y)) + (my*sigma(x)) + (sigma()*mx*my)
            // c = sigma(y*y) + (my*sigma(y)) + (my*sigma(y)) + (sigma()*my*my)
            //
            // blob_a = sigma(x*x)
            // blob_b = sigma(x*y)
            // blob_c = sigma(y*y)
            // blob_cx = sigma(x)
            // blob_cy = sigma(y)
            // blob_pixels = sigma()

            int mx = blob_cx / blob_pixels; // x centroid
            int my = blob_cy / blob_pixels; // y centroid
            int small_blob_a = blob_a - ((mx * blob_cx) + (mx * blob_cx)) + (blob_pixels * mx * mx);
            int small_blob_b = blob_b - ((mx * blob_cy) + (my * blob_cx)) + (blob_pixels * mx * my);
            int small_blob_c = blob_c - ((my * blob_cy) + (my * blob_cy)) + (blob_pixels * my * my);

            float rotation =
                ((small_blob_a !=
                  small_blob_c) ? (fast_atan2f(2 * small_blob_b, small_blob_a - small_blob_c) / 2.0f) : 1.570796f) + 1.570796f;                              // PI/2

            out->theta = fast_roundf(rotation * 57.295780) % 180; // * (180 / PI)
            if (out->theta < 0) {
                out->theta += 180;
            }
            out->rho = fast_roundf(((mx - roi->x) * cos_table[out->theta]) + ((my - roi->y) * sin_table[out->theta]));

            float part0 = (small_blob_a + small_blob_c) / 2.0f;
            float f_b = (float) small_blob_b;
            float f_a_c = (float) (small_blob_a - small_blob_c);
            float part1 = fast_sqrtf((4 * f_b * f_b) + (f_a_c * f_a_c)) / 2.0f;
            float p_add = fast_sqrtf(part0 + part1);
            float p_sub = fast_sqrtf(part0 - part1);
            float e_min = IM_MIN(p_add, p_sub);
            float e_max = IM_MAX(p_add, p_sub);
            out->magnitude = fast_roundf(e_max / e_min) - 1; // Circle -> [0, INF) -> Line

            if ((45 <= out->theta) && (out->theta < 135)) {
                // y = (r - x cos(t)) / sin(t)
                out->line.x1 = 0;
                out->line.y1 = fast_roundf((out->rho - (out->line.x1 * cos_table[out->theta])) / sin_table[out->theta]);
                out->line.x2 = roi->w - 1;
                out->line.y2 = fast_roundf((out->rho - (out->line.x2 * cos_table[out->theta])) / sin_table[out->theta]);
            } else {
                // x = (r - y sin(t)) / cos(t);
                out->line.y1 = 0;
                out->line.x1 = fast_roundf((out->rho - (out->line.y1 * sin_table[out->theta])) / cos_table[out->theta]);
                out->line.y2 = roi->h - 1;
                out->line.x2 = fast_roundf((out->rho - (out->line.y2 * sin_table[out->theta])) / cos_table[out->theta]);
            }

            if (lb_clip_line(&out->line, 0, 0, roi->w, roi->h)) {
                out->line.x1 += roi->x;
                out->line.y1 += roi->y;
                out->line.x2 += roi->x;
                out->line.y2 += roi->y;
                // Move rho too.
                out->rho += fast_roundf((roi->x * cos_table[out->theta]) + (roi->y * sin_table[out->theta]));
                result = true;
            } else {
                memset(out, 0, sizeof(find_lines_list_lnk_data_t));
            }
        }
    } else {
        // Theil-Sen Estimator
        int *x_histogram = fb_alloc0(ptr->w * sizeof(int), FB_ALLOC_NO_HINT);
        int *y_histogram = fb_alloc0(ptr->h * sizeof(int), FB_ALLOC_NO_HINT);
        long long *x_delta_histogram = fb_alloc0((2 * ptr->w) * sizeof(long long), FB_ALLOC_NO_HINT);
        long long *y_delta_histogram = fb_alloc0((2 * ptr->h) * sizeof(long long), FB_ALLOC_NO_HINT);

        uint32_t size;
        point_t *points = (point_t *) fb_alloc_all(&size, FB_ALLOC_NO_HINT);
        size_t points_max = size / sizeof(point_t);
        size_t points_count = 0;

        if (points_max) {
            int blob_x1 = roi->x + roi->w - 1;
            int blob_y1 = roi->y + roi->h - 1;
            int blob_x2 = roi->x;
            int blob_y2 = roi->y;
            int blob_pixels = 0;

            list_for_each(it, thresholds) {
                color_thresholds_list_lnk_data_t *lnk_data = list_get_data(it);

                switch (ptr->pixfmt) {
                    case PIXFORMAT_BINARY: {
                        for (int y = roi->y, yy = roi->y + roi->h; y < yy; y += y_stride) {
                            uint32_t *row_ptr = IMAGE_COMPUTE_BINARY_PIXEL_ROW_PTR(ptr, y);
                            for (int x = roi->x + (y % x_stride), xx = roi->x + roi->w; x < xx; x += x_stride) {
                                if (COLOR_THRESHOLD_BINARY(IMAGE_GET_BINARY_PIXEL_FAST(row_ptr, x), lnk_data, invert)) {
                                    blob_x1 = IM_MIN(blob_x1, x);
                                    blob_y1 = IM_MIN(blob_y1, y);
                                    blob_x2 = IM_MAX(blob_x2, x);
                                    blob_y2 = IM_MAX(blob_y2, y);
                                    blob_pixels += 1;
                                    x_histogram[x]++;
                                    y_histogram[y]++;

                                    if (points_count < points_max) {
                                        point_init(&points[points_count], x, y);
                                        points_count += 1;
                                    }
                                }
                            }
                        }
                        break;
                    }
                    case PIXFORMAT_GRAYSCALE: {
                        for (int y = roi->y, yy = roi->y + roi->h; y < yy; y += y_stride) {
                            uint8_t *row_ptr = IMAGE_COMPUTE_GRAYSCALE_PIXEL_ROW_PTR(ptr, y);
                            for (int x = roi->x + (y % x_stride), xx = roi->x + roi->w; x < xx; x += x_stride) {
                                if (COLOR_THRESHOLD_GRAYSCALE(IMAGE_GET_GRAYSCALE_PIXEL_FAST(row_ptr, x), lnk_data, invert)) {
                                    blob_x1 = IM_MIN(blob_x1, x);
                                    blob_y1 = IM_MIN(blob_y1, y);
                                    blob_x2 = IM_MAX(blob_x2, x);
                                    blob_y2 = IM_MAX(blob_y2, y);
                                    blob_pixels += 1;
                                    x_histogram[x]++;
                                    y_histogram[y]++;

                                    if (points_count < points_max) {
                                        point_init(&points[points_count], x, y);
                                        points_count += 1;
                                    }
                                }
                            }
                        }
                        break;
                    }
                    case PIXFORMAT_RGB565: {
                        for (int y = roi->y, yy = roi->y + roi->h; y < yy; y += y_stride) {
                            uint16_t *row_ptr = IMAGE_COMPUTE_RGB565_PIXEL_ROW_PTR(ptr, y);
                            for (int x = roi->x + (y % x_stride), xx = roi->x + roi->w; x < xx; x += x_stride) {
                                if (COLOR_THRESHOLD_RGB565(IMAGE_GET_RGB565_PIXEL_FAST(row_ptr, x), lnk_data, invert)) {
                                    blob_x1 = IM_MIN(blob_x1, x);
                                    blob_y1 = IM_MIN(blob_y1, y);
                                    blob_x2 = IM_MAX(blob_x2, x);
                                    blob_y2 = IM_MAX(blob_y2, y);
                                    blob_pixels += 1;
                                    x_histogram[x]++;
                                    y_histogram[y]++;

                                    if (points_count < points_max) {
                                        point_init(&points[points_count], x, y);
                                        points_count += 1;
                                    }
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

            int w = blob_x2 - blob_x1;
            int h = blob_y2 - blob_y1;
            if (blob_pixels && ((w * h) >= area_threshold) && (blob_pixels >= pixels_threshold)) {
                long long delta_sum = (points_count * (points_count - 1)) / 2;

                if (delta_sum) {
                    // The code below computes the average slope between all pairs of points.
                    // This is a N^2 operation that can easily blow up if the image is not threshold carefully...

                    for (int i = 0; i < points_count; i++) {
                        point_t *p0 = &points[i];
                        for (int j = i + 1; j < points_count; j++) {
                            point_t *p1 = &points[j];
                            // Note we allocated 1 extra above so we can do ptr->w instead of (ptr->w-1).
                            x_delta_histogram[p0->x - p1->x + ptr->w]++;
                            // Note we allocated 1 extra above so we can do ptr->h instead of (ptr->h-1).
                            y_delta_histogram[p0->y - p1->y + ptr->h]++;
                        }
                    }

                    int mx = get_median(x_histogram, blob_pixels, ptr->w); // Output doesn't need adjustment.
                    int my = get_median(y_histogram, blob_pixels, ptr->h); // Output doesn't need adjustment.
                    int mdx = get_median_l(x_delta_histogram, delta_sum, 2 * ptr->w) - ptr->w; // Fix offset.
                    int mdy = get_median_l(y_delta_histogram, delta_sum, 2 * ptr->h) - ptr->h; // Fix offset.

                    float rotation = (mdx ? fast_atan2f(mdy, mdx) : 1.570796f) + 1.570796f; // PI/2

                    out->theta = fast_roundf(rotation * 57.295780) % 180; // * (180 / PI)
                    if (out->theta < 0) {
                        out->theta += 180;
                    }
                    out->rho = fast_roundf(((mx - roi->x) * cos_table[out->theta]) + ((my - roi->y) * sin_table[out->theta]));

                    out->magnitude = fast_roundf(fast_sqrtf((mdx * mdx) + (mdy * mdy)));

                    if ((45 <= out->theta) && (out->theta < 135)) {
                        // y = (r - x cos(t)) / sin(t)
                        out->line.x1 = 0;
                        out->line.y1 = fast_roundf((out->rho - (out->line.x1 * cos_table[out->theta])) / sin_table[out->theta]);
                        out->line.x2 = roi->w - 1;
                        out->line.y2 = fast_roundf((out->rho - (out->line.x2 * cos_table[out->theta])) / sin_table[out->theta]);
                    } else {
                        // x = (r - y sin(t)) / cos(t);
                        out->line.y1 = 0;
                        out->line.x1 = fast_roundf((out->rho - (out->line.y1 * sin_table[out->theta])) / cos_table[out->theta]);
                        out->line.y2 = roi->h - 1;
                        out->line.x2 = fast_roundf((out->rho - (out->line.y2 * sin_table[out->theta])) / cos_table[out->theta]);
                    }

                    if (lb_clip_line(&out->line, 0, 0, roi->w, roi->h)) {
                        out->line.x1 += roi->x;
                        out->line.y1 += roi->y;
                        out->line.x2 += roi->x;
                        out->line.y2 += roi->y;
                        // Move rho too.
                        out->rho += fast_roundf((roi->x * cos_table[out->theta]) + (roi->y * sin_table[out->theta]));
                        result = true;
                    } else {
                        memset(out, 0, sizeof(find_lines_list_lnk_data_t));
                    }
                }
            }
        }

        fb_free(); // points
        fb_free(); // y_delta_histogram
        fb_free(); // x_delta_histogram
        fb_free(); // y_histogram
        fb_free(); // x_histogram
    }

    return result;
}
