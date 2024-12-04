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
 * Python module for time of flight sensors.
 */
#include "py/runtime.h"
#include "py/objlist.h"
#include "py/mphal.h"

#include "omv_boardconfig.h"

#if (MICROPY_PY_TOF == 1)
#include "omv_i2c.h"
#include "py_assert.h"
#include "py_helper.h"
#include "py_image.h"
#include "framebuffer.h"

#if (OMV_TOF_VL53L5CX_ENABLE == 1)
#include "vl53l5cx_api.h"
#endif

#define VL53L5CX_ADDR               0x52
#define VL53L5CX_WIDTH              8
#define VL53L5CX_HEIGHT             8
#define VL53L5CX_FRAME_DATA_SIZE    64

typedef enum tof_type {
    TOF_NONE,
    #if (OMV_TOF_VL53L5CX_ENABLE == 1)
    TOF_VL53L5CX,
    #endif
} tof_type_t;

static int tof_width = 0;
static int tof_height = 0;
static bool tof_transposed = false;
static tof_type_t tof_sensor = TOF_NONE;
static omv_i2c_t tof_bus = { 0 };

#if (OMV_TOF_VL53L5CX_ENABLE == 1)
static VL53L5CX_Configuration vl53l5cx_dev = {
    .platform = {
        .bus = &tof_bus,
        .address = VL53L5CX_ADDR,
    }
};
#endif

// img->w == data_w && img->h == data_h && img->pixfmt == PIXFORMAT_GRAYSCALE
static void tof_fill_image_float_obj(image_t *img, mp_obj_t *data, float min, float max) {
    float tmp = min;
    min = (min < max) ? min : max;
    max = (max > tmp) ? max : tmp;

    float diff = 255.f / (max - min);

    for (int y = 0; y < img->h; y++) {
        int row_offset = y * img->w;
        mp_obj_t *raw_row = data + row_offset;
        uint8_t *row_pointer = ((uint8_t *) img->data) + row_offset;

        for (int x = 0; x < img->w; x++) {
            float raw = mp_obj_get_float(raw_row[x]);

            if (raw < min) {
                raw = min;
            }

            if (raw > max) {
                raw = max;
            }

            int pixel = fast_roundf((raw - min) * diff);
            row_pointer[x] = __USAT(pixel, 8);
        }
    }
}

#if (OMV_TOF_VL53L5CX_ENABLE == 1)
static void tof_vl53l5cx_get_depth(VL53L5CX_Configuration *vl53l5cx_dev, float *frame, uint32_t timeout) {
    uint8_t frame_ready = 0;
    // Note depending on the config in platform.h, this struct can be too big to alloc on the stack.
    VL53L5CX_ResultsData ranging_data;

    for (mp_uint_t start = mp_hal_ticks_ms(); !frame_ready; mp_hal_delay_ms(1)) {
        if (vl53l5cx_check_data_ready(vl53l5cx_dev, &frame_ready) != 0) {
            mp_raise_msg(&mp_type_RuntimeError, MP_ERROR_TEXT("VL53L5CX ranging failed"));
        }

        if ((mp_hal_ticks_ms() - start) >= timeout) {
            mp_raise_msg(&mp_type_RuntimeError, MP_ERROR_TEXT("VL53L5CX ranging timeout"));
        }
    }

    if (vl53l5cx_get_ranging_data(vl53l5cx_dev, &ranging_data) != 0) {
        mp_raise_msg(&mp_type_RuntimeError, MP_ERROR_TEXT("VL53L5CX ranging failed"));
    }

    for (int i = 0, ii = VL53L5CX_WIDTH * VL53L5CX_HEIGHT; i < ii; i++) {
        frame[i] = (float) ranging_data.distance_mm[i];
    }
}

static mp_obj_t tof_get_depth_obj(int w, int h, float *frame, bool mirror,
                                  bool flip, bool dst_transpose, bool src_transpose) {
    mp_obj_list_t *list = (mp_obj_list_t *) mp_obj_new_list(w * h, NULL);
    float min = FLT_MAX;
    float max = -FLT_MAX;
    int w_1 = w - 1;
    int h_1 = h - 1;

    if (!src_transpose) {
        for (int y = 0; y < h; y++) {
            int y_dst = flip ? (h_1 - y) : y;
            float *raw_row = frame + (y * w);
            mp_obj_t *list_row = list->items + (y_dst * w);
            mp_obj_t *t_list_row = list->items + y_dst;

            for (int x = 0; x < w; x++) {
                int x_dst = mirror ? (w_1 - x) : x;
                float raw = raw_row[x];

                if (raw < min) {
                    min = raw;
                }

                if (raw > max) {
                    max = raw;
                }

                mp_obj_t f = mp_obj_new_float(raw);

                if (!dst_transpose) {
                    list_row[x_dst] = f;
                } else {
                    t_list_row[x_dst * h] = f;
                }
            }
        }
    } else {
        for (int x = 0; x < w; x++) {
            int x_dst = mirror ? (w_1 - x) : x;
            float *raw_row = frame + (x * h);
            mp_obj_t *t_list_row = list->items + (x_dst * h);
            mp_obj_t *list_row = list->items + x_dst;

            for (int y = 0; y < h; y++) {
                int y_dst = flip ? (h_1 - y) : y;
                float raw = raw_row[y];

                if (raw < min) {
                    min = raw;
                }

                if (raw > max) {
                    max = raw;
                }

                mp_obj_t f = mp_obj_new_float(raw);

                if (!dst_transpose) {
                    list_row[y_dst * w] = f;
                } else {
                    t_list_row[y_dst] = f;
                }
            }
        }
    }

    mp_obj_t tuple[3] = {
        MP_OBJ_FROM_PTR(list),
        mp_obj_new_float(min),
        mp_obj_new_float(max)
    };
    return mp_obj_new_tuple(3, tuple);
}
#endif

static mp_obj_t py_tof_deinit() {
    tof_width = 0;
    tof_height = 0;
    tof_transposed = false;

    if (tof_sensor != TOF_NONE) {
        #if (OMV_TOF_VL53L5CX_ENABLE == 1)
        if (tof_sensor == TOF_VL53L5CX) {
            vl53l5cx_stop_ranging(&vl53l5cx_dev);
        }
        #endif
        omv_i2c_deinit(&tof_bus);
        tof_sensor = TOF_NONE;
    }
    #if (OMV_TOF_VL53L5CX_ENABLE == 1)
    vl53l5cx_reset(&vl53l5cx_dev.platform);
    #endif
    return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_0(py_tof_deinit_obj, py_tof_deinit);

mp_obj_t py_tof_init(uint n_args, const mp_obj_t *pos_args, mp_map_t *kw_args) {
    enum { ARG_type };
    static const mp_arg_t allowed_args[] = {
        { MP_QSTR_type, MP_ARG_INT,  {.u_int = -1 } },
    };

    // Parse args.
    mp_arg_val_t args[MP_ARRAY_SIZE(allowed_args)];
    mp_arg_parse_all(n_args, pos_args, kw_args, MP_ARRAY_SIZE(allowed_args), allowed_args, args);

    py_tof_deinit();
    bool first_init = true;
    int type = args[ARG_type].u_int;

    if (type == -1) {
        TOF_SCAN_RETRY:
        omv_i2c_init(&tof_bus, OMV_TOF_I2C_ID, OMV_TOF_I2C_SPEED);
        // Scan and detect any supported sensor.
        uint8_t dev_list[10];
        int dev_size = omv_i2c_scan(&tof_bus, dev_list, sizeof(dev_list));
        for (int i = 0; i < dev_size && type == -1; i++) {
            switch (dev_list[i]) {
                #if (OMV_TOF_VL53L5CX_ENABLE == 1)
                case (VL53L5CX_ADDR): {
                    type = TOF_VL53L5CX;
                    break;
                }
                #endif
                default:
                    continue;
            }
        }

        if (type == -1 && first_init) {
            first_init = false;
            // Recover bus and scan one more time.
            omv_i2c_pulse_scl(&tof_bus);
            goto TOF_SCAN_RETRY;
        }

        omv_i2c_deinit(&tof_bus);
    }

    // Initialize the detected sensor.
    first_init = true;
    switch (type) {
        #if (OMV_TOF_VL53L5CX_ENABLE == 1)
        case TOF_VL53L5CX: {
            int error = 0;
            uint8_t isAlive = 0;
            TOF_VL53L5CX_RETRY:
            // Initialize I2C bus.
            omv_i2c_init(&tof_bus, OMV_TOF_I2C_ID, OMV_TOF_I2C_SPEED);

            // Check sensor and initialize.
            error |= vl53l5cx_is_alive(&vl53l5cx_dev, &isAlive);
            error |= vl53l5cx_init(&vl53l5cx_dev);

            // Set resolution (number of zones).
            // NOTE: This function must be called before updating the ranging frequency.
            error |= vl53l5cx_set_resolution(&vl53l5cx_dev, VL53L5CX_RESOLUTION_8X8);

            // Set ranging frequency (FPS).
            // For 4x4 the allowed ranging frequency range is 1 -> 60.
            // For 8x8 the allowed ranging frequency range is 1 -> 15.
            error |= vl53l5cx_set_ranging_frequency_hz(&vl53l5cx_dev, 15);

            // Set ranging mode to continuous:
            // The device continuously grabs frames with the set ranging frequency.
            // Maximum ranging depth and ambient immunity are better.
            // This mode is advised for fast ranging measurements or high performances.
            error |= vl53l5cx_set_ranging_mode(&vl53l5cx_dev, VL53L5CX_RANGING_MODE_CONTINUOUS);

            error |= vl53l5cx_set_sharpener_percent(&vl53l5cx_dev, 50);

            // Start ranging.
            error |= vl53l5cx_start_ranging(&vl53l5cx_dev);

            if (error != 0 && first_init) {
                first_init = false;
                // Recover bus and scan one more time.
                omv_i2c_pulse_scl(&tof_bus);
                goto TOF_VL53L5CX_RETRY;
            } else if (error != 0) {
                py_tof_deinit();
                mp_raise_msg(&mp_type_RuntimeError, MP_ERROR_TEXT("Failed to init the VL53L5CX"));
            }
            tof_sensor = TOF_VL53L5CX;
            tof_width = VL53L5CX_WIDTH;
            tof_height = VL53L5CX_HEIGHT;
            break;
        }
        #endif
        default: {
            mp_raise_msg(&mp_type_RuntimeError, MP_ERROR_TEXT("Failed to detect a supported TOF sensor."));
        }
    }

    return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_KW(py_tof_init_obj, 0, py_tof_init);

static mp_obj_t py_tof_type() {
    if (tof_sensor != TOF_NONE) {
        return mp_obj_new_int(tof_sensor);
    }
    mp_raise_msg(&mp_type_RuntimeError, MP_ERROR_TEXT("TOF sensor is not initialized"));
}
static MP_DEFINE_CONST_FUN_OBJ_0(py_tof_type_obj, py_tof_type);

static mp_obj_t py_tof_width() {
    if (tof_sensor != TOF_NONE) {
        return mp_obj_new_int(tof_width);
    }
    mp_raise_msg(&mp_type_RuntimeError, MP_ERROR_TEXT("TOF sensor is not initialized"));
}
static MP_DEFINE_CONST_FUN_OBJ_0(py_tof_width_obj, py_tof_width);

static mp_obj_t py_tof_height() {
    if (tof_sensor != TOF_NONE) {
        return mp_obj_new_int(tof_height);
    }
    mp_raise_msg(&mp_type_RuntimeError, MP_ERROR_TEXT("TOF sensor is not initialized"));
}
static MP_DEFINE_CONST_FUN_OBJ_0(py_tof_height_obj, py_tof_height);

static mp_obj_t py_tof_refresh() {
    switch (tof_sensor) {
        #if (OMV_TOF_VL53L5CX_ENABLE == 1)
        case TOF_VL53L5CX:
            return mp_obj_new_int(15);
        #endif
        default:
            mp_raise_msg(&mp_type_RuntimeError, MP_ERROR_TEXT("TOF sensor is not initialized"));
    }
}
static MP_DEFINE_CONST_FUN_OBJ_0(py_tof_refresh_obj, py_tof_refresh);

mp_obj_t py_tof_read_depth(uint n_args, const mp_obj_t *pos_args, mp_map_t *kw_args) {
    enum { ARG_hmirror, ARG_vflip, ARG_transpose, ARG_timeout };
    static const mp_arg_t allowed_args[] = {
        { MP_QSTR_hmirror, MP_ARG_INT | MP_ARG_KW_ONLY,  {.u_bool = false } },
        { MP_QSTR_vflip, MP_ARG_INT | MP_ARG_KW_ONLY,  {.u_bool = false } },
        { MP_QSTR_transpose, MP_ARG_INT | MP_ARG_KW_ONLY,  {.u_bool = false } },
        { MP_QSTR_timeout, MP_ARG_INT | MP_ARG_KW_ONLY,  {.u_int = -1 } },
    };

    // Parse args.
    mp_arg_val_t args[MP_ARRAY_SIZE(allowed_args)];
    mp_arg_parse_all(n_args, pos_args, kw_args, MP_ARRAY_SIZE(allowed_args), allowed_args, args);

    tof_transposed = args[ARG_transpose].u_bool;

    switch (tof_sensor) {
        #if (OMV_TOF_VL53L5CX_ENABLE == 1)
        case TOF_VL53L5CX: {
            fb_alloc_mark();
            float *frame = fb_alloc(VL53L5CX_WIDTH * VL53L5CX_HEIGHT * sizeof(float), FB_ALLOC_FAST_HINT);
            tof_vl53l5cx_get_depth(&vl53l5cx_dev, frame, args[ARG_timeout].u_int);
            mp_obj_t result = tof_get_depth_obj(VL53L5CX_WIDTH, VL53L5CX_HEIGHT, frame, !args[ARG_hmirror].u_bool,
                                                args[ARG_vflip].u_bool, args[ARG_transpose].u_bool, true);
            fb_alloc_free_till_mark();
            return result;
        }
        #endif
        default:
            mp_raise_msg(&mp_type_RuntimeError, MP_ERROR_TEXT("TOF sensor is not initialized"));
    }

    return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_KW(py_tof_read_depth_obj, 0, py_tof_read_depth);

mp_obj_t py_tof_draw_depth(uint n_args, const mp_obj_t *pos_args, mp_map_t *kw_args) {
    enum {
        ARG_x, ARG_y, ARG_x_scale, ARG_y_scale, ARG_roi, ARG_channel, ARG_alpha,
        ARG_color_palette, ARG_alpha_palette, ARG_hint, ARG_scale
    };
    static const mp_arg_t allowed_args[] = {
        { MP_QSTR_x, MP_ARG_INT | MP_ARG_KW_ONLY,  {.u_int = 0 } },
        { MP_QSTR_y, MP_ARG_INT | MP_ARG_KW_ONLY,  {.u_int = 0 } },
        { MP_QSTR_x_scale, MP_ARG_OBJ | MP_ARG_KW_ONLY, {.u_rom_obj = MP_ROM_NONE} },
        { MP_QSTR_y_scale, MP_ARG_OBJ | MP_ARG_KW_ONLY, {.u_rom_obj = MP_ROM_NONE} },
        { MP_QSTR_roi, MP_ARG_OBJ | MP_ARG_KW_ONLY, {.u_rom_obj = MP_ROM_NONE} },
        { MP_QSTR_rgb_channel, MP_ARG_INT | MP_ARG_KW_ONLY,  {.u_int = -1 } },
        { MP_QSTR_alpha, MP_ARG_INT | MP_ARG_KW_ONLY,  {.u_int = 255 } },
        { MP_QSTR_color_palette, MP_ARG_OBJ | MP_ARG_KW_ONLY, {.u_rom_obj = MP_ROM_INT(COLOR_PALETTE_DEPTH)} },
        { MP_QSTR_alpha_palette, MP_ARG_OBJ | MP_ARG_KW_ONLY, {.u_rom_obj = MP_ROM_NONE} },
        { MP_QSTR_hint, MP_ARG_INT | MP_ARG_KW_ONLY,  {.u_int = 0 } },
        { MP_QSTR_scale, MP_ARG_OBJ | MP_ARG_KW_ONLY, {.u_rom_obj = MP_ROM_NONE} },
    };

    // Parse args.
    mp_arg_val_t args[MP_ARRAY_SIZE(allowed_args)];
    mp_arg_parse_all(n_args - 2, pos_args + 2, kw_args, MP_ARRAY_SIZE(allowed_args), allowed_args, args);

    // Sanity checks
    if (tof_sensor == TOF_NONE) {
        mp_raise_msg(&mp_type_RuntimeError, MP_ERROR_TEXT("TOF sensor is not initialized"));
    }

    if (args[ARG_channel].u_int < -1 || args[ARG_channel].u_int > 2) {
        mp_raise_msg(&mp_type_RuntimeError, MP_ERROR_TEXT("RGB channel can be 0, 1, or 2"));
    }

    if (args[ARG_alpha].u_int < 0 || args[ARG_alpha].u_int > 255) {
        mp_raise_msg(&mp_type_RuntimeError, MP_ERROR_TEXT("Alpha ranges between 0 and 255"));
    }

    image_t src_img = {
        .w = tof_transposed ? tof_height : tof_width,
        .h = tof_transposed ? tof_width : tof_height,
        .pixfmt = PIXFORMAT_GRAYSCALE,
        //.data is allocated later.
    };

    image_t *dst_img = py_helper_arg_to_image(pos_args[0], ARG_IMAGE_MUTABLE);

    mp_obj_t *depth_array;
    mp_obj_get_array_fixed_n(pos_args[1], src_img.w * src_img.h, &depth_array);

    rectangle_t roi = py_helper_arg_to_roi(args[ARG_roi].u_obj, &src_img);

    float x_scale = 1.0f;
    float y_scale = 1.0f;
    py_helper_arg_to_scale(args[ARG_x_scale].u_obj, args[ARG_y_scale].u_obj, &x_scale, &y_scale);

    float min = FLT_MAX;
    float max = -FLT_MAX;
    py_helper_arg_to_minmax(args[ARG_scale].u_obj, &min, &max, depth_array, src_img.w * src_img.h);

    const uint16_t *color_palette = py_helper_arg_to_palette(args[ARG_color_palette].u_obj, PIXFORMAT_RGB565);
    const uint8_t *alpha_palette = py_helper_arg_to_palette(args[ARG_alpha_palette].u_obj, PIXFORMAT_GRAYSCALE);

    fb_alloc_mark();
    src_img.data = fb_alloc(src_img.w * src_img.h * sizeof(uint8_t));
    tof_fill_image_float_obj(&src_img, depth_array, min, max);

    imlib_draw_image(dst_img, &src_img, args[ARG_x].u_int, args[ARG_y].u_int, x_scale, y_scale, &roi,
                     args[ARG_channel].u_int, args[ARG_alpha].u_int, color_palette, alpha_palette,
                     args[ARG_hint].u_int, NULL, NULL, NULL);

    fb_alloc_free_till_mark();
    return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_KW(py_tof_draw_depth_obj, 2, py_tof_draw_depth);

mp_obj_t py_tof_snapshot(uint n_args, const mp_obj_t *pos_args, mp_map_t *kw_args) {
    enum {
        ARG_hmirror, ARG_vflip, ARG_transpose, ARG_x_scale, ARG_y_scale, ARG_roi, ARG_channel,
        ARG_alpha, ARG_color_palette, ARG_alpha_palette, ARG_hint, ARG_scale, ARG_pixformat,
        ARG_copy_to_fb, ARG_timeout
    };
    static const mp_arg_t allowed_args[] = {
        { MP_QSTR_hmirror, MP_ARG_INT | MP_ARG_KW_ONLY,  {.u_bool = false } },
        { MP_QSTR_vflip, MP_ARG_INT | MP_ARG_KW_ONLY,  {.u_bool = false } },
        { MP_QSTR_transpose, MP_ARG_INT | MP_ARG_KW_ONLY,  {.u_bool = false } },
        { MP_QSTR_x_scale, MP_ARG_OBJ | MP_ARG_KW_ONLY, {.u_rom_obj = MP_ROM_NONE} },
        { MP_QSTR_y_scale, MP_ARG_OBJ | MP_ARG_KW_ONLY, {.u_rom_obj = MP_ROM_NONE} },
        { MP_QSTR_roi, MP_ARG_OBJ | MP_ARG_KW_ONLY, {.u_rom_obj = MP_ROM_NONE} },
        { MP_QSTR_rgb_channel, MP_ARG_INT | MP_ARG_KW_ONLY,  {.u_int = -1 } },
        { MP_QSTR_alpha, MP_ARG_INT | MP_ARG_KW_ONLY,  {.u_int = 255 } },
        { MP_QSTR_color_palette, MP_ARG_OBJ | MP_ARG_KW_ONLY, {.u_rom_obj = MP_ROM_INT(COLOR_PALETTE_DEPTH)} },
        { MP_QSTR_alpha_palette, MP_ARG_OBJ | MP_ARG_KW_ONLY, {.u_rom_obj = MP_ROM_NONE} },
        { MP_QSTR_hint, MP_ARG_INT | MP_ARG_KW_ONLY,  {.u_int = 0 } },
        { MP_QSTR_scale, MP_ARG_OBJ | MP_ARG_KW_ONLY, {.u_rom_obj = MP_ROM_NONE} },
        { MP_QSTR_pixformat, MP_ARG_INT | MP_ARG_KW_ONLY,  {.u_int = PIXFORMAT_RGB565 } },
        { MP_QSTR_copy_to_fb, MP_ARG_INT | MP_ARG_KW_ONLY,  {.u_bool = false } },
        { MP_QSTR_timeout, MP_ARG_INT | MP_ARG_KW_ONLY,  {.u_int = -1 } },
    };

    // Parse args.
    mp_arg_val_t args[MP_ARRAY_SIZE(allowed_args)];
    mp_arg_parse_all(n_args, pos_args, kw_args, MP_ARRAY_SIZE(allowed_args), allowed_args, args);

    // Sanity checks
    if (args[ARG_channel].u_int < -1 || args[ARG_channel].u_int > 2) {
        mp_raise_msg(&mp_type_ValueError, MP_ERROR_TEXT("RGB channel can be 0, 1, or 2"));
    }

    if (args[ARG_alpha].u_int < 0 || args[ARG_alpha].u_int > 255) {
        mp_raise_msg(&mp_type_ValueError, MP_ERROR_TEXT("Alpha ranges between 0 and 255"));
    }

    if ((args[ARG_pixformat].u_int != PIXFORMAT_GRAYSCALE) && (args[ARG_pixformat].u_int != PIXFORMAT_RGB565)) {
        mp_raise_msg(&mp_type_ValueError, MP_ERROR_TEXT("Invalid pixformat"));
    }

    image_t src_img = {
        .w = args[ARG_transpose].u_bool ? tof_height : tof_width,
        .h = args[ARG_transpose].u_bool ? tof_width : tof_height,
        .pixfmt = PIXFORMAT_GRAYSCALE,
        //.data is allocated later.
    };

    rectangle_t roi = py_helper_arg_to_roi(args[ARG_roi].u_obj, &src_img);

    float x_scale = 1.0f;
    float y_scale = 1.0f;
    py_helper_arg_to_scale(args[ARG_x_scale].u_obj, args[ARG_y_scale].u_obj, &x_scale, &y_scale);

    image_t dst_img = {
        .w = fast_floorf(roi.w * x_scale),
        .h = fast_floorf(roi.h * y_scale),
        .pixfmt = args[ARG_pixformat].u_int,
    };
    if (args[ARG_copy_to_fb].u_bool) {
        py_helper_set_to_framebuffer(&dst_img);
    } else {
        dst_img.data = xalloc(image_size(&dst_img));
    }

    float min = FLT_MAX;
    float max = -FLT_MAX;
    py_helper_arg_to_minmax(args[ARG_scale].u_obj, &min, &max, NULL, 0);

    const uint16_t *color_palette = py_helper_arg_to_palette(args[ARG_color_palette].u_obj, PIXFORMAT_RGB565);
    const uint8_t *alpha_palette = py_helper_arg_to_palette(args[ARG_alpha_palette].u_obj, PIXFORMAT_GRAYSCALE);

    fb_alloc_mark();
    // Allocate source image data.
    src_img.data = fb_alloc(src_img.w * src_img.h * sizeof(uint8_t));

    switch (tof_sensor) {
        #if (OMV_TOF_VL53L5CX_ENABLE == 1)
        case TOF_VL53L5CX: {
            float *frame = fb_alloc(VL53L5CX_WIDTH * VL53L5CX_HEIGHT * sizeof(float), FB_ALLOC_FAST_HINT);
            tof_vl53l5cx_get_depth(&vl53l5cx_dev, frame, args[ARG_timeout].u_int);
            if (args[ARG_scale].u_obj == mp_const_none) {
                fast_get_min_max(frame, VL53L5CX_WIDTH * VL53L5CX_HEIGHT, &min, &max);
            }
            imlib_fill_image_from_float(&src_img, VL53L5CX_WIDTH, VL53L5CX_HEIGHT, frame, min, max,
                                        !args[ARG_hmirror].u_bool, args[ARG_vflip].u_bool,
                                        args[ARG_transpose].u_bool, true);
            break;
        }
        #endif
        default:
            mp_raise_msg(&mp_type_RuntimeError, MP_ERROR_TEXT("TOF sensor is not initialized"));
    }

    imlib_draw_image(&dst_img, &src_img, 0, 0, x_scale, y_scale, &roi,
                     args[ARG_channel].u_int, args[ARG_alpha].u_int, color_palette, alpha_palette,
                     (args[ARG_hint].u_int & (~IMAGE_HINT_CENTER)) | IMAGE_HINT_BLACK_BACKGROUND, NULL, NULL, NULL);

    fb_alloc_free_till_mark();

    if (args[ARG_copy_to_fb].u_bool) {
        framebuffer_update_jpeg_buffer();
    }
    return py_image_from_struct(&dst_img);
}
static MP_DEFINE_CONST_FUN_OBJ_KW(py_tof_snapshot_obj, 0, py_tof_snapshot);

static const mp_rom_map_elem_t globals_dict_table[] = {
    { MP_ROM_QSTR(MP_QSTR___name__),            MP_ROM_QSTR(MP_QSTR_tof)                },
    { MP_ROM_QSTR(MP_QSTR_TOF_NONE),            MP_ROM_INT(TOF_NONE)                    },
    #if (OMV_TOF_VL53L5CX_ENABLE == 1)
    { MP_ROM_QSTR(MP_QSTR_TOF_VL53L5CX),        MP_ROM_INT(TOF_VL53L5CX)                },
    #endif
    { MP_ROM_QSTR(MP_QSTR_init),                MP_ROM_PTR(&py_tof_init_obj)            },
    { MP_ROM_QSTR(MP_QSTR_reset),               MP_ROM_PTR(&py_tof_init_obj)            },
    { MP_ROM_QSTR(MP_QSTR_deinit),              MP_ROM_PTR(&py_tof_deinit_obj)          },
    { MP_ROM_QSTR(MP_QSTR_type),                MP_ROM_PTR(&py_tof_type_obj)            },
    { MP_ROM_QSTR(MP_QSTR_width),               MP_ROM_PTR(&py_tof_width_obj)           },
    { MP_ROM_QSTR(MP_QSTR_height),              MP_ROM_PTR(&py_tof_height_obj)          },
    { MP_ROM_QSTR(MP_QSTR_refresh),             MP_ROM_PTR(&py_tof_refresh_obj)         },
    { MP_ROM_QSTR(MP_QSTR_read_depth),          MP_ROM_PTR(&py_tof_read_depth_obj)      },
    { MP_ROM_QSTR(MP_QSTR_draw_depth),          MP_ROM_PTR(&py_tof_draw_depth_obj)      },
    { MP_ROM_QSTR(MP_QSTR_snapshot),            MP_ROM_PTR(&py_tof_snapshot_obj)        }
};

static MP_DEFINE_CONST_DICT(globals_dict, globals_dict_table);

const mp_obj_module_t tof_module = {
    .base = { &mp_type_module },
    .globals = (mp_obj_t) &globals_dict,
};

MP_REGISTER_MODULE(MP_QSTR_tof, tof_module);
#endif
