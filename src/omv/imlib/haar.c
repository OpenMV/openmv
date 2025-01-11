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
 * Viola-Jones object detector implementation.
 * Based on the work of Francesco Comaschi (f.comaschi@tue.nl)
 */
#include <stdio.h>
#include "py/obj.h"
#include "py/runtime.h"

#if MICROPY_VFS
#include "py/stream.h"
#include "extmod/vfs.h"
#endif

#include "xalloc.h"
#include "imlib.h"

#ifdef IMLIB_ENABLE_FEATURES
static int eval_weak_classifier(cascade_t *cascade, point_t pt, int t_idx, int w_idx, int r_idx) {
    int32_t sumw = 0;
    mw_image_t *sum = cascade->sum;

    /* The node threshold is multiplied by the standard deviation of the sub window */
    int32_t t = cascade->tree_thresh_array[t_idx] * cascade->std;

    for (int i = 0; i < cascade->num_rectangles_array[t_idx]; i++) {
        int x = cascade->rectangles_array[r_idx + (i << 2) + 0];
        int y = cascade->rectangles_array[r_idx + (i << 2) + 1];
        int w = cascade->rectangles_array[r_idx + (i << 2) + 2];
        int h = cascade->rectangles_array[r_idx + (i << 2) + 3];
        // Lookup the feature
        sumw += imlib_integral_mw_lookup(sum, pt.x + x, y, w, h) * (cascade->weights_array[w_idx + i] << 12);
    }

    if (sumw >= t) {
        return cascade->alpha2_array[t_idx];
    }

    return cascade->alpha1_array[t_idx];
}

static int run_cascade_classifier(cascade_t *cascade, point_t pt) {
    int win_w = cascade->window.w;
    int win_h = cascade->window.h;
    uint32_t n = (win_w * win_h);
    uint32_t i_s = imlib_integral_mw_lookup(cascade->sum, pt.x, 0, win_w, win_h);
    uint32_t i_sq = imlib_integral_mw_lookup(cascade->ssq, pt.x, 0, win_w, win_h);
    uint32_t m = i_s / n;
    uint32_t v = i_sq / n - (m * m);

    // Skip homogeneous regions.
    if (v < (50 * 50)) {
        return 0;
    }

    cascade->std = fast_sqrtf(i_sq * n - (i_s * i_s));
    for (int i = 0, w_idx = 0, r_idx = 0, t_idx = 0; i < cascade->n_stages; i++) {
        int stage_sum = 0;
        for (int j = 0; j < cascade->stages_array[i]; j++, t_idx++) {
            // Send the shifted window to a haar filter
            stage_sum += eval_weak_classifier(cascade, pt, t_idx, w_idx, r_idx);
            w_idx += cascade->num_rectangles_array[t_idx];
            r_idx += cascade->num_rectangles_array[t_idx] * 4;
        }
        // If the sum is below the stage threshold, no objects were detected
        if (stage_sum < (cascade->threshold * cascade->stages_thresh_array[i])) {
            return 0;
        }
    }
    return 1;
}

array_t *imlib_detect_objects(image_t *image, cascade_t *cascade, rectangle_t *roi) {
    // Integral images
    mw_image_t sum;
    mw_image_t ssq;

    // Detected objects array
    array_t *objects;

    // Allocate the objects array
    array_alloc(&objects, xfree);

    // Set cascade image pointers
    cascade->img = image;
    cascade->sum = &sum;
    cascade->ssq = &ssq;

    // Set scanning step.
    // Viola and Jones achieved best results using a scaling factor
    // of 1.25 and a scanning factor proportional to the current scale.
    // Start with a step of 5% of the image width and reduce at each scaling step
    cascade->step = (roi->w * 50) / 1000;

    // Make sure step is less than window height + 1
    if (cascade->step > cascade->window.h) {
        cascade->step = cascade->window.h;
    }

    // Allocate integral images
    imlib_integral_mw_alloc(&sum, roi->w, cascade->window.h + 1);
    imlib_integral_mw_alloc(&ssq, roi->w, cascade->window.h + 1);

    // Iterate over the image pyramid
    for (float factor = 1.0f; ; factor *= cascade->scale_factor) {
        // Set the scaled width and height
        int szw = roi->w / factor;
        int szh = roi->h / factor;

        // Break if scaled image is smaller than feature size
        if (szw < cascade->window.w || szh < cascade->window.h) {
            break;
        }

        // Set the integral images scale
        imlib_integral_mw_scale(roi, &sum, szw, szh);
        imlib_integral_mw_scale(roi, &ssq, szw, szh);

        // Compute new scaled integral images
        imlib_integral_mw_ss(image, &sum, &ssq, roi);

        // Scale the scanning step
        cascade->step = cascade->step / factor;
        cascade->step = (cascade->step == 0) ? 1 : cascade->step;

        // Process image at the current scale
        // When filter window shifts to borders, some margin need to be kept
        int y2 = szh - cascade->window.h;
        int x2 = szw - cascade->window.w;

        // Shift the filter window over the image.
        for (int y = 0; y < y2; y += cascade->step) {
            for (int x = 0; x < x2; x += cascade->step) {
                point_t p = {x, y};
                // If an object is detected, record the coordinates of the filter window
                if (run_cascade_classifier(cascade, p) > 0) {
                    array_push_back(objects,
                                    rectangle_alloc(fast_roundf(x * factor) + roi->x, fast_roundf(y * factor) + roi->y,
                                                    fast_roundf(cascade->window.w * factor),
                                                    fast_roundf(cascade->window.h * factor)));
                }
            }

            // If not last line, shift integral images
            if ((y + cascade->step) < y2) {
                imlib_integral_mw_shift_ss(image, &sum, &ssq, roi, cascade->step);
            }
        }
    }

    imlib_integral_mw_free(&ssq);
    imlib_integral_mw_free(&sum);

    if (array_length(objects) > 1) {
        // Merge objects detected at different scales
        objects = rectangle_merge(objects);
    }

    return objects;
}

#if MICROPY_VFS
static void *cascade_buffer_read(uint8_t **buf, size_t size) {
    uint8_t *buf8 = *buf;
    *buf += size;
    return buf8;
}

int imlib_load_cascade_from_file(cascade_t *cascade, const char *path) {
    int error = 0;
    mp_obj_t args[2] = {
        mp_obj_new_str_from_cstr(path),
        MP_OBJ_NEW_QSTR(MP_QSTR_rb),
    };

    memset(cascade, 0, sizeof(cascade_t));

    mp_buffer_info_t bufinfo;
    mp_obj_t file = mp_vfs_open(MP_ARRAY_SIZE(args), args, (mp_map_t *) &mp_const_empty_map);

    if (mp_get_buffer(file, &bufinfo, MP_BUFFER_READ)) {
        uint8_t *buf = (uint8_t *) bufinfo.buf + 12;
        // Set detection window size and the number of stages.
        cascade->window.w = ((uint32_t *) bufinfo.buf)[0];
        cascade->window.h = ((uint32_t *) bufinfo.buf)[1];
        cascade->n_stages = ((uint32_t *) bufinfo.buf)[2];

        // Set the number features in each stages
        cascade->stages_array = cascade_buffer_read(&buf, cascade->n_stages);
        // Skip alignment
        if ((uint32_t) buf % 4) {
            buf += 4 - ((uint32_t) buf % 4);
        }

        // Sum the number of features in each stages
        for (size_t i = 0; i < cascade->n_stages; i++) {
            cascade->n_features += cascade->stages_array[i];
        }

        // Set features thresh array, alpha1, alpha 2,rects weights and rects
        cascade->stages_thresh_array = cascade_buffer_read(&buf, sizeof(int16_t) * cascade->n_stages);
        cascade->tree_thresh_array = cascade_buffer_read(&buf, sizeof(int16_t) * cascade->n_features);
        cascade->alpha1_array = cascade_buffer_read(&buf, sizeof(int16_t) * cascade->n_features);
        cascade->alpha2_array = cascade_buffer_read(&buf, sizeof(int16_t) * cascade->n_features);
        cascade->num_rectangles_array = cascade_buffer_read(&buf, sizeof(int8_t) * cascade->n_features);

        // Sum the number of rectangles in all features
        for (size_t i = 0; i < cascade->n_features; i++) {
            cascade->n_rectangles += cascade->num_rectangles_array[i];
        }

        // Set rectangles weights and rectangles (number of rectangles * 4 points)
        cascade->weights_array = cascade_buffer_read(&buf, cascade->n_rectangles);
        cascade->rectangles_array = cascade_buffer_read(&buf, cascade->n_rectangles * 4);
    } else {
        // Read detection window size.
        mp_stream_read_exactly(file, &cascade->window, sizeof(cascade->window), &error);
        // Read the number of stages.
        mp_stream_read_exactly(file, &cascade->n_stages, sizeof(cascade->n_stages), &error);

        // Allocate stages array.
        cascade->stages_array = xalloc(sizeof(int8_t) * cascade->n_stages);

        // Read number of features in each stages
        mp_stream_read_exactly(file, cascade->stages_array, cascade->n_stages, &error);
        // Skip alignment
        uint8_t padding[4];
        if (cascade->n_stages % 4) {
            mp_stream_read_exactly(file, padding, 4 - (cascade->n_stages % 4), &error);
        }

        // Sum the number of features in each stages
        for (size_t i = 0; i < cascade->n_stages; i++) {
            cascade->n_features += cascade->stages_array[i];
        }

        // Alloc features thresh array, alpha1, alpha 2,rects weights and rects
        cascade->stages_thresh_array = xalloc(sizeof(int16_t) * cascade->n_stages);
        cascade->tree_thresh_array = xalloc(sizeof(int16_t) * cascade->n_features);
        cascade->alpha1_array = xalloc(sizeof(int16_t) * cascade->n_features);
        cascade->alpha2_array = xalloc(sizeof(int16_t) * cascade->n_features);
        cascade->num_rectangles_array = xalloc(sizeof(int8_t) * cascade->n_features);

        // Read features thresh array, alpha1, alpha 2,rects weights and rects
        mp_stream_read_exactly(file, cascade->stages_thresh_array, sizeof(int16_t) * cascade->n_stages, &error);
        mp_stream_read_exactly(file, cascade->tree_thresh_array, sizeof(int16_t) * cascade->n_features, &error);
        mp_stream_read_exactly(file, cascade->alpha1_array, sizeof(int16_t) * cascade->n_features, &error);
        mp_stream_read_exactly(file, cascade->alpha2_array, sizeof(int16_t) * cascade->n_features, &error);
        mp_stream_read_exactly(file, cascade->num_rectangles_array, cascade->n_features, &error);

        // Sum the number of rectangles per feature
        for (size_t i = 0; i < cascade->n_features; i++) {
            cascade->n_rectangles += cascade->num_rectangles_array[i];
        }

        // Allocate weights and rectangles arrays.
        cascade->weights_array = xalloc(cascade->n_rectangles);
        cascade->rectangles_array = xalloc(cascade->n_rectangles * 4);

        // Read rectangles weights and rectangles (number of rectangles * 4 points)
        mp_stream_read_exactly(file, cascade->weights_array, sizeof(int8_t) * cascade->n_rectangles, &error);
        mp_stream_read_exactly(file, cascade->rectangles_array, sizeof(int8_t) * cascade->n_rectangles * 4, &error);
    }

    if (error != 0) {
        mp_raise_OSError(error);
    }
    mp_stream_close(file);
    return 0;
}
#endif //(IMLIB_ENABLE_IMAGE_FILE_IO)

int imlib_load_cascade(cascade_t *cascade, const char *path) {
    #if MICROPY_VFS
    // xml cascade
    return imlib_load_cascade_from_file(cascade, path);
    #else
    return -1;
    #endif
}
#endif // IMLIB_ENABLE_FEATURES
