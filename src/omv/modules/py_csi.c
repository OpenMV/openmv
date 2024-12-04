/*
 * Copyright (C) 2023-2024 OpenMV, LLC.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 * 3. Any redistribution, use, or modification in source or binary form
 *    is done solely for personal benefit and not for any commercial
 *    purpose or for monetary gain. For commercial licensing options,
 *    please contact openmv@openmv.io
 *
 * THIS SOFTWARE IS PROVIDED BY THE LICENSOR AND COPYRIGHT OWNER "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE LICENSOR OR COPYRIGHT
 * OWNER BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * Sensor Python module.
 */
#include <stdarg.h>
#include <stdio.h>
#include "py/mphal.h"
#include "py/runtime.h"

#if MICROPY_PY_CSI

#include "omv_csi.h"
#include "imlib.h"
#include "xalloc.h"
#include "py_assert.h"
#include "py_image.h"
#if MICROPY_PY_IMU
#include "py_imu.h"
#endif
#include "omv_boardconfig.h"
#include "omv_i2c.h"
#include "py_helper.h"
#include "framebuffer.h"

extern omv_csi_t csi;
static mp_obj_t vsync_callback = mp_const_none;
static mp_obj_t frame_callback = mp_const_none;

#define omv_csi_raise_error(err) mp_raise_msg(&mp_type_RuntimeError, (mp_rom_error_text_t) omv_csi_strerror(err))
#define omv_csi_print_error(op)  printf("\x1B[31mWARNING: %s control is not supported by this image sensor.\x1B[0m\n", op);

#if MICROPY_PY_IMU
static void do_auto_rotation(int pitch_deadzone, int roll_activezone) {
    if (omv_csi_get_auto_rotation()) {
        float pitch = py_imu_pitch_rotation();
        if (((pitch <= (90 - pitch_deadzone)) || ((90 + pitch_deadzone) < pitch))
            && ((pitch <= (270 - pitch_deadzone)) || ((270 + pitch_deadzone) < pitch))) {
            // disable when 90 or 270
            float roll = py_imu_roll_rotation();
            if (((360 - roll_activezone) <= roll) || (roll < (0 + roll_activezone)) ) {
                // center is 0/360, upright
                omv_csi_set_hmirror(false);
                omv_csi_set_vflip(false);
                omv_csi_set_transpose(false);
            } else if (((270 - roll_activezone) <= roll) && (roll < (270 + roll_activezone))) {
                // center is 270, rotated right
                omv_csi_set_hmirror(true);
                omv_csi_set_vflip(false);
                omv_csi_set_transpose(true);
            } else if (((180 - roll_activezone) <= roll) && (roll < (180 + roll_activezone))) {
                // center is 180, upside down
                omv_csi_set_hmirror(true);
                omv_csi_set_vflip(true);
                omv_csi_set_transpose(false);
            } else if (((90 - roll_activezone) <= roll) && (roll < (90 + roll_activezone))) {
                // center is 90, rotated left
                omv_csi_set_hmirror(false);
                omv_csi_set_vflip(true);
                omv_csi_set_transpose(true);
            }
        }
    }
}
#endif // MICROPY_PY_IMU

static mp_obj_t py_omv_csi__init__() {
    // This is the module init function, not the sensor init function.
    // This gets called when the module is imported, so it's a good
    // place to check if the sensor was detected or not.
    if (omv_csi_is_detected() == false) {
        omv_csi_raise_error(OMV_CSI_ERROR_ISC_UNDETECTED);
    }
    return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_0(py_omv_csi__init__obj, py_omv_csi__init__);

static mp_obj_t py_omv_csi_reset() {
    int error = omv_csi_reset();
    if (error != 0) {
        omv_csi_raise_error(error);
    }
    #if MICROPY_PY_IMU
    // +-10 degree dead-zone around pitch 90/270.
    // +-45 degree active-zone around roll 0/90/180/270/360.
    do_auto_rotation(10, 45);
    // We're setting the dead-zone on pitch because roll readings are invalid there.
    // We're setting the full range on roll to set the initial state.
    #endif // MICROPY_PY_IMU
    return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_0(py_omv_csi_reset_obj, py_omv_csi_reset);

static mp_obj_t py_omv_csi_sleep(mp_obj_t enable) {
    PY_ASSERT_FALSE_MSG(omv_csi_sleep(mp_obj_is_true(enable)) != 0, "Sleep Failed");
    return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_1(py_omv_csi_sleep_obj, py_omv_csi_sleep);

static mp_obj_t py_omv_csi_shutdown(mp_obj_t enable) {
    PY_ASSERT_FALSE_MSG(omv_csi_shutdown(mp_obj_is_true(enable)) != 0, "Shutdown Failed");
    return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_1(py_omv_csi_shutdown_obj, py_omv_csi_shutdown);

static mp_obj_t py_omv_csi_flush() {
    framebuffer_update_jpeg_buffer();
    return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_0(py_omv_csi_flush_obj, py_omv_csi_flush);

static mp_obj_t py_omv_csi_snapshot(uint n_args, const mp_obj_t *args, mp_map_t *kw_args) {
    #if MICROPY_PY_IMU
    // +-10 degree dead-zone around pitch 90/270.
    // +-35 degree active-zone around roll 0/90/180/270/360.
    do_auto_rotation(10, 35);
    // We're setting the dead-zone on pitch because roll readings are invalid there.
    // We're not setting the full range on roll to prevent oscillation.
    #endif // MICROPY_PY_IMU

    mp_obj_t image = py_image(0, 0, 0, 0, 0);
    int error = csi.snapshot(&csi, (image_t *) py_image_cobj(image), 0);
    if (error != 0) {
        omv_csi_raise_error(error);
    }
    return image;
}
static MP_DEFINE_CONST_FUN_OBJ_KW(py_omv_csi_snapshot_obj, 0, py_omv_csi_snapshot);

static mp_obj_t py_omv_csi_skip_frames(uint n_args, const mp_obj_t *args, mp_map_t *kw_args) {
    mp_map_elem_t *kw_arg = mp_map_lookup(kw_args, MP_ROM_QSTR(MP_QSTR_time), MP_MAP_LOOKUP);
    mp_int_t time = 300; // OV Recommended.

    if (kw_arg != NULL) {
        time = mp_obj_get_int(kw_arg->value);
    }

    uint32_t millis = mp_hal_ticks_ms();

    if (!n_args) {
        while ((mp_hal_ticks_ms() - millis) < time) {
            // 32-bit math handles wrap around...
            py_omv_csi_snapshot(0, NULL, NULL);
        }
    } else {
        for (int i = 0, j = mp_obj_get_int(args[0]); i < j; i++) {
            if ((kw_arg != NULL) && ((mp_hal_ticks_ms() - millis) >= time)) {
                break;
            }

            py_omv_csi_snapshot(0, NULL, NULL);
        }
    }

    return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_KW(py_omv_csi_skip_frames_obj, 0, py_omv_csi_skip_frames);

static mp_obj_t py_omv_csi_width() {
    return mp_obj_new_int(resolution[csi.framesize][0]);
}
static MP_DEFINE_CONST_FUN_OBJ_0(py_omv_csi_width_obj, py_omv_csi_width);

static mp_obj_t py_omv_csi_height() {
    return mp_obj_new_int(resolution[csi.framesize][1]);
}
static MP_DEFINE_CONST_FUN_OBJ_0(py_omv_csi_height_obj, py_omv_csi_height);

static mp_obj_t py_omv_csi_get_fb() {
    if (framebuffer_get_depth() < 0) {
        return mp_const_none;
    }

    image_t image;
    framebuffer_init_image(&image);
    return py_image_from_struct(&image);
}
static MP_DEFINE_CONST_FUN_OBJ_0(py_omv_csi_get_fb_obj, py_omv_csi_get_fb);

static mp_obj_t py_omv_csi_get_id() {
    return mp_obj_new_int(omv_csi_get_id());
}
static MP_DEFINE_CONST_FUN_OBJ_0(py_omv_csi_get_id_obj, py_omv_csi_get_id);

static mp_obj_t py_omv_csi_get_frame_available() {
    return mp_obj_new_bool(framebuffer->tail != framebuffer->head);
}
static MP_DEFINE_CONST_FUN_OBJ_0(py_omv_csi_get_frame_available_obj, py_omv_csi_get_frame_available);

static mp_obj_t py_omv_csi_alloc_extra_fb(mp_obj_t w_obj, mp_obj_t h_obj, mp_obj_t pixfmt_obj) {
    int w = mp_obj_get_int(w_obj);
    PY_ASSERT_TRUE_MSG(w > 0, "Width must be > 0");

    int h = mp_obj_get_int(h_obj);
    PY_ASSERT_TRUE_MSG(h > 0, "Height must be > 0");

    pixformat_t pixfmt = mp_obj_get_int(pixfmt_obj);
    PY_ASSERT_TRUE_MSG(IMLIB_PIXFORMAT_IS_VALID(pixfmt), "Invalid Pixel Format");

    image_t img = {.w = w, .h = h, .pixfmt = pixfmt, .size = 0, .pixels = 0};

    // Alloc image first (could fail) then alloc RAM so that there's no leak on failure.
    mp_obj_t r = py_image_from_struct(&img);

    fb_alloc_mark();
    ((image_t *) py_image_cobj(r))->pixels = fb_alloc0(image_size(&img));
    fb_alloc_mark_permanent(); // pixels will not be popped on exception
    return r;
}
static MP_DEFINE_CONST_FUN_OBJ_3(py_omv_csi_alloc_extra_fb_obj, py_omv_csi_alloc_extra_fb);

static mp_obj_t py_omv_csi_dealloc_extra_fb() {
    fb_alloc_free_till_mark_past_mark_permanent();
    return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_0(py_omv_csi_dealloc_extra_fb_obj, py_omv_csi_dealloc_extra_fb);

static mp_obj_t py_omv_csi_set_pixformat(mp_obj_t pixformat) {
    int error = omv_csi_set_pixformat(mp_obj_get_int(pixformat));
    if (error != 0) {
        omv_csi_raise_error(error);
    }
    return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_1(py_omv_csi_set_pixformat_obj, py_omv_csi_set_pixformat);

static mp_obj_t py_omv_csi_get_pixformat() {
    if (csi.pixformat == PIXFORMAT_INVALID) {
        omv_csi_raise_error(OMV_CSI_ERROR_INVALID_PIXFORMAT);
    }
    return mp_obj_new_int(csi.pixformat);
}
static MP_DEFINE_CONST_FUN_OBJ_0(py_omv_csi_get_pixformat_obj, py_omv_csi_get_pixformat);

static mp_obj_t py_omv_csi_set_framesize(mp_obj_t framesize) {
    int error = omv_csi_set_framesize(mp_obj_get_int(framesize));
    if (error != 0) {
        omv_csi_raise_error(error);
    }
    return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_1(py_omv_csi_set_framesize_obj, py_omv_csi_set_framesize);

static mp_obj_t py_omv_csi_get_framesize() {
    if (csi.framesize == OMV_CSI_FRAMESIZE_INVALID) {
        omv_csi_raise_error(OMV_CSI_ERROR_INVALID_FRAMESIZE);
    }
    return mp_obj_new_int(csi.framesize);
}
static MP_DEFINE_CONST_FUN_OBJ_0(py_omv_csi_get_framesize_obj, py_omv_csi_get_framesize);

static mp_obj_t py_omv_csi_set_framerate(mp_obj_t framerate) {
    int error = omv_csi_set_framerate(mp_obj_get_int(framerate));
    if (error != 0) {
        omv_csi_raise_error(error);
    }
    return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_1(py_omv_csi_set_framerate_obj, py_omv_csi_set_framerate);

static mp_obj_t py_omv_csi_get_framerate() {
    if (csi.framerate == 0) {
        omv_csi_raise_error(OMV_CSI_ERROR_INVALID_FRAMERATE);
    }
    return mp_obj_new_int(csi.framerate);
}
static MP_DEFINE_CONST_FUN_OBJ_0(py_omv_csi_get_framerate_obj, py_omv_csi_get_framerate);

static mp_obj_t py_omv_csi_set_windowing(uint n_args, const mp_obj_t *args) {
    if (csi.framesize == OMV_CSI_FRAMESIZE_INVALID) {
        omv_csi_raise_error(OMV_CSI_ERROR_INVALID_FRAMESIZE);
    }

    rectangle_t temp;
    temp.x = 0;
    temp.y = 0;
    temp.w = resolution[csi.framesize][0];
    temp.h = resolution[csi.framesize][1];

    mp_obj_t *array = (mp_obj_t *) args;
    mp_uint_t array_len = n_args;

    if (n_args == 1) {
        mp_obj_get_array(args[0], &array_len, &array);
    }

    rectangle_t r;

    if (array_len == 2) {
        r.w = mp_obj_get_int(array[0]);
        r.h = mp_obj_get_int(array[1]);
        r.x = (temp.w / 2) - (r.w / 2);
        r.y = (temp.h / 2) - (r.h / 2);
    } else if (array_len == 4) {
        r.x = mp_obj_get_int(array[0]);
        r.y = mp_obj_get_int(array[1]);
        r.w = mp_obj_get_int(array[2]);
        r.h = mp_obj_get_int(array[3]);
    } else {
        mp_raise_msg(&mp_type_ValueError, MP_ERROR_TEXT("The tuple/list must either be (x, y, w, h) or (w, h)"));
    }

    if ((r.w < 1) || (r.h < 1)) {
        mp_raise_msg(&mp_type_ValueError, MP_ERROR_TEXT("Invalid ROI dimensions!"));
    }

    if (!rectangle_overlap(&r, &temp)) {
        mp_raise_msg(&mp_type_ValueError, MP_ERROR_TEXT("ROI does not overlap on the image!"));
    }

    rectangle_intersected(&r, &temp);

    int error = omv_csi_set_windowing(r.x, r.y, r.w, r.h);
    if (error != 0) {
        omv_csi_raise_error(error);
    }

    return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(py_omv_csi_set_windowing_obj, 1, 4, py_omv_csi_set_windowing);

static mp_obj_t py_omv_csi_get_windowing() {
    if (csi.framesize == OMV_CSI_FRAMESIZE_INVALID) {
        omv_csi_raise_error(OMV_CSI_ERROR_INVALID_FRAMESIZE);
    }

    return mp_obj_new_tuple(4, (mp_obj_t []) {mp_obj_new_int(framebuffer_get_x()),
                                              mp_obj_new_int(framebuffer_get_y()),
                                              mp_obj_new_int(framebuffer_get_u()),
                                              mp_obj_new_int(framebuffer_get_v())});
}
static MP_DEFINE_CONST_FUN_OBJ_0(py_omv_csi_get_windowing_obj, py_omv_csi_get_windowing);

static mp_obj_t py_omv_csi_set_gainceiling(mp_obj_t gainceiling) {
    omv_csi_gainceiling_t gain;
    switch (mp_obj_get_int(gainceiling)) {
        case 2:
            gain = OMV_CSI_GAINCEILING_2X;
            break;
        case 4:
            gain = OMV_CSI_GAINCEILING_4X;
            break;
        case 8:
            gain = OMV_CSI_GAINCEILING_8X;
            break;
        case 16:
            gain = OMV_CSI_GAINCEILING_16X;
            break;
        case 32:
            gain = OMV_CSI_GAINCEILING_32X;
            break;
        case 64:
            gain = OMV_CSI_GAINCEILING_64X;
            break;
        case 128:
            gain = OMV_CSI_GAINCEILING_128X;
            break;
        default:
            omv_csi_raise_error(OMV_CSI_ERROR_INVALID_ARGUMENT);
            break;
    }

    if (omv_csi_set_gainceiling(gain) != 0) {
        return mp_const_false;
    }
    return mp_const_true;
}
static MP_DEFINE_CONST_FUN_OBJ_1(py_omv_csi_set_gainceiling_obj, py_omv_csi_set_gainceiling);

static mp_obj_t py_omv_csi_set_brightness(mp_obj_t brightness) {
    if (omv_csi_set_brightness(mp_obj_get_int(brightness)) != 0) {
        return mp_const_false;
    }
    return mp_const_true;
}
static MP_DEFINE_CONST_FUN_OBJ_1(py_omv_csi_set_brightness_obj, py_omv_csi_set_brightness);

static mp_obj_t py_omv_csi_set_contrast(mp_obj_t contrast) {
    if (omv_csi_set_contrast(mp_obj_get_int(contrast)) != 0) {
        return mp_const_false;
    }
    return mp_const_true;
}
static MP_DEFINE_CONST_FUN_OBJ_1(py_omv_csi_set_contrast_obj, py_omv_csi_set_contrast);

static mp_obj_t py_omv_csi_set_saturation(mp_obj_t saturation) {
    if (omv_csi_set_saturation(mp_obj_get_int(saturation)) != 0) {
        return mp_const_false;
    }
    return mp_const_true;
}
static MP_DEFINE_CONST_FUN_OBJ_1(py_omv_csi_set_saturation_obj, py_omv_csi_set_saturation);

static mp_obj_t py_omv_csi_set_quality(mp_obj_t qs) {
    int q = mp_obj_get_int(qs);
    PY_ASSERT_TRUE((q >= 0 && q <= 100));

    q = 100 - q; //invert quality
    q = 255 * q / 100; //map to 0->255
    if (omv_csi_set_quality(q) != 0) {
        return mp_const_false;
    }
    return mp_const_true;
}
static MP_DEFINE_CONST_FUN_OBJ_1(py_omv_csi_set_quality_obj, py_omv_csi_set_quality);

static mp_obj_t py_omv_csi_set_colorbar(mp_obj_t enable) {
    if (omv_csi_set_colorbar(mp_obj_is_true(enable)) != 0) {
        return mp_const_false;
    }
    return mp_const_true;
}
static MP_DEFINE_CONST_FUN_OBJ_1(py_omv_csi_set_colorbar_obj, py_omv_csi_set_colorbar);

static mp_obj_t py_omv_csi_set_auto_gain(uint n_args, const mp_obj_t *pos_args, mp_map_t *kw_args) {
    enum { ARG_gain_db, ARG_gain_db_ceiling };
    static const mp_arg_t allowed_args[] = {
        { MP_QSTR_gain_db, MP_ARG_OBJ | MP_ARG_KW_ONLY, {.u_rom_obj = MP_ROM_NONE} },
        { MP_QSTR_gain_db_ceiling, MP_ARG_OBJ | MP_ARG_KW_ONLY, {.u_rom_obj = MP_ROM_NONE} },
    };

    // Parse args.
    int enable = mp_obj_get_int(pos_args[0]);
    mp_arg_val_t args[MP_ARRAY_SIZE(allowed_args)];
    mp_arg_parse_all(n_args - 1, pos_args + 1, kw_args, MP_ARRAY_SIZE(allowed_args), allowed_args, args);

    float gain_db = py_helper_arg_to_float(args[ARG_gain_db].u_obj, NAN);
    float gain_db_ceiling = py_helper_arg_to_float(args[ARG_gain_db_ceiling].u_obj, NAN);

    int error = omv_csi_set_auto_gain(enable, gain_db, gain_db_ceiling);
    if (error != 0) {
        if (error != OMV_CSI_ERROR_CTL_UNSUPPORTED) {
            omv_csi_raise_error(error);
        }
        omv_csi_print_error("Auto Gain");
    }
    return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_KW(py_omv_csi_set_auto_gain_obj, 1, py_omv_csi_set_auto_gain);

static mp_obj_t py_omv_csi_get_gain_db() {
    float gain_db;
    int error = omv_csi_get_gain_db(&gain_db);
    if (error != 0) {
        omv_csi_raise_error(error);
    }
    return mp_obj_new_float(gain_db);
}
static MP_DEFINE_CONST_FUN_OBJ_0(py_omv_csi_get_gain_db_obj, py_omv_csi_get_gain_db);

static mp_obj_t py_omv_csi_set_auto_exposure(uint n_args, const mp_obj_t *pos_args, mp_map_t *kw_args) {
    enum { ARG_exposure_us };
    static const mp_arg_t allowed_args[] = {
        { MP_QSTR_exposure_us, MP_ARG_INT | MP_ARG_KW_ONLY, {.u_int = -1} },
    };

    // Parse args.
    int enable = mp_obj_get_int(pos_args[0]);
    mp_arg_val_t args[MP_ARRAY_SIZE(allowed_args)];
    mp_arg_parse_all(n_args - 1, pos_args + 1, kw_args, MP_ARRAY_SIZE(allowed_args), allowed_args, args);

    int error = omv_csi_set_auto_exposure(enable, args[ARG_exposure_us].u_int);
    if (error != 0) {
        if (error != OMV_CSI_ERROR_CTL_UNSUPPORTED) {
            omv_csi_raise_error(error);
        }
        omv_csi_print_error("Auto Exposure");
    }
    return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_KW(py_omv_csi_set_auto_exposure_obj, 1, py_omv_csi_set_auto_exposure);

static mp_obj_t py_omv_csi_get_exposure_us() {
    int exposure_us;
    int error = omv_csi_get_exposure_us(&exposure_us);
    if (error != 0) {
        omv_csi_raise_error(error);
    }
    return mp_obj_new_int(exposure_us);
}
static MP_DEFINE_CONST_FUN_OBJ_0(py_omv_csi_get_exposure_us_obj, py_omv_csi_get_exposure_us);

static mp_obj_t py_omv_csi_set_auto_whitebal(uint n_args, const mp_obj_t *pos_args, mp_map_t *kw_args) {
    enum { ARG_rgb_gain_db };
    static const mp_arg_t allowed_args[] = {
        { MP_QSTR_rgb_gain_db, MP_ARG_OBJ | MP_ARG_KW_ONLY, {.u_rom_obj = MP_ROM_NONE} },
    };

    // Parse args.
    int enable = mp_obj_get_int(pos_args[0]);
    mp_arg_val_t args[MP_ARRAY_SIZE(allowed_args)];
    mp_arg_parse_all(n_args - 1, pos_args + 1, kw_args, MP_ARRAY_SIZE(allowed_args), allowed_args, args);

    float rgb_gain_db[3] = {NAN, NAN, NAN};
    py_helper_arg_to_float_array(args[ARG_rgb_gain_db].u_obj, rgb_gain_db, 3);

    int error = omv_csi_set_auto_whitebal(enable, rgb_gain_db[0], rgb_gain_db[1], rgb_gain_db[2]);
    if (error != 0) {
        if (error != OMV_CSI_ERROR_CTL_UNSUPPORTED) {
            omv_csi_raise_error(error);
        }
        omv_csi_print_error("Auto White Balance");
    }
    return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_KW(py_omv_csi_set_auto_whitebal_obj, 1, py_omv_csi_set_auto_whitebal);

static mp_obj_t py_omv_csi_get_rgb_gain_db() {
    float r_gain_db = 0.0, g_gain_db = 0.0, b_gain_db = 0.0;
    int error = omv_csi_get_rgb_gain_db(&r_gain_db, &g_gain_db, &b_gain_db);
    if (error != 0) {
        omv_csi_raise_error(error);
    }
    return mp_obj_new_tuple(3, (mp_obj_t []) {
        mp_obj_new_float(r_gain_db),
        mp_obj_new_float(g_gain_db),
        mp_obj_new_float(b_gain_db)
    });
}
static MP_DEFINE_CONST_FUN_OBJ_0(py_omv_csi_get_rgb_gain_db_obj, py_omv_csi_get_rgb_gain_db);

static mp_obj_t py_omv_csi_set_auto_blc(uint n_args, const mp_obj_t *pos_args, mp_map_t *kw_args) {
    enum { ARG_enable, ARG_regs };
    static const mp_arg_t allowed_args[] = {
        { MP_QSTR_enable, MP_ARG_REQUIRED | MP_ARG_INT },
        { MP_QSTR_regs,   MP_ARG_OBJ, {.u_rom_obj = MP_ROM_NONE} },
    };

    mp_arg_val_t args[MP_ARRAY_SIZE(allowed_args)];
    mp_arg_parse_all(n_args, pos_args, kw_args, MP_ARRAY_SIZE(allowed_args), allowed_args, args);

    int enable = args[ARG_enable].u_int;
    int regs[csi.blc_size];
    bool regs_present = args[ARG_regs].u_obj != mp_const_none;
    if (regs_present) {
        mp_obj_t *arg_array;
        mp_obj_get_array_fixed_n(args[ARG_regs].u_obj, csi.blc_size, &arg_array);
        for (uint32_t i = 0; i < csi.blc_size; i++) {
            regs[i] = mp_obj_get_int(arg_array[i]);
        }
    }

    int error = omv_csi_set_auto_blc(enable, regs_present ? regs : NULL);
    if (error != 0) {
        if (error != OMV_CSI_ERROR_CTL_UNSUPPORTED) {
            omv_csi_raise_error(error);
        }
        omv_csi_print_error("Auto BLC");
    }
    return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_KW(py_omv_csi_set_auto_blc_obj, 1,  py_omv_csi_set_auto_blc);

static mp_obj_t py_omv_csi_get_blc_regs() {
    int regs[csi.blc_size];
    int error = omv_csi_get_blc_regs(regs);
    if (error != 0) {
        omv_csi_raise_error(error);
    }

    mp_obj_list_t *l = mp_obj_new_list(csi.blc_size, NULL);
    for (uint32_t i = 0; i < csi.blc_size; i++) {
        l->items[i] = mp_obj_new_int(regs[i]);
    }
    return l;
}
static MP_DEFINE_CONST_FUN_OBJ_0(py_omv_csi_get_blc_regs_obj, py_omv_csi_get_blc_regs);

static mp_obj_t py_omv_csi_set_hmirror(mp_obj_t enable) {
    int error = omv_csi_set_hmirror(mp_obj_is_true(enable));
    if (error != 0) {
        omv_csi_raise_error(error);
    }
    return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_1(py_omv_csi_set_hmirror_obj, py_omv_csi_set_hmirror);

static mp_obj_t py_omv_csi_get_hmirror() {
    return mp_obj_new_bool(omv_csi_get_hmirror());
}
static MP_DEFINE_CONST_FUN_OBJ_0(py_omv_csi_get_hmirror_obj, py_omv_csi_get_hmirror);

static mp_obj_t py_omv_csi_set_vflip(mp_obj_t enable) {
    int error = omv_csi_set_vflip(mp_obj_is_true(enable));
    if (error != 0) {
        omv_csi_raise_error(error);
    }
    return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_1(py_omv_csi_set_vflip_obj, py_omv_csi_set_vflip);

static mp_obj_t py_omv_csi_get_vflip() {
    return mp_obj_new_bool(omv_csi_get_vflip());
}
static MP_DEFINE_CONST_FUN_OBJ_0(py_omv_csi_get_vflip_obj, py_omv_csi_get_vflip);

static mp_obj_t py_omv_csi_set_transpose(mp_obj_t enable) {
    int error = omv_csi_set_transpose(mp_obj_is_true(enable));
    if (error != 0) {
        omv_csi_raise_error(error);
    }
    return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_1(py_omv_csi_set_transpose_obj, py_omv_csi_set_transpose);

static mp_obj_t py_omv_csi_get_transpose() {
    return mp_obj_new_bool(omv_csi_get_transpose());
}
static MP_DEFINE_CONST_FUN_OBJ_0(py_omv_csi_get_transpose_obj, py_omv_csi_get_transpose);

static mp_obj_t py_omv_csi_set_auto_rotation(mp_obj_t enable) {
    int error = omv_csi_set_auto_rotation(mp_obj_is_true(enable));
    if (error != 0) {
        omv_csi_raise_error(error);
    }
    return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_1(py_omv_csi_set_auto_rotation_obj, py_omv_csi_set_auto_rotation);

static mp_obj_t py_omv_csi_get_auto_rotation() {
    return mp_obj_new_bool(omv_csi_get_auto_rotation());
}
static MP_DEFINE_CONST_FUN_OBJ_0(py_omv_csi_get_auto_rotation_obj, py_omv_csi_get_auto_rotation);

static mp_obj_t py_omv_csi_set_framebuffers(mp_obj_t count) {
    mp_int_t c = mp_obj_get_int(count);

    if (framebuffer->n_buffers == c) {
        return mp_const_none;
    }

    if (c < 1) {
        omv_csi_raise_error(OMV_CSI_ERROR_INVALID_ARGUMENT);
    }

    int error = omv_csi_set_framebuffers(c);
    if (error != 0) {
        omv_csi_raise_error(error);
    }

    return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_1(py_omv_csi_set_framebuffers_obj, py_omv_csi_set_framebuffers);

static mp_obj_t py_omv_csi_get_framebuffers() {
    return mp_obj_new_int(framebuffer->n_buffers);
}
static MP_DEFINE_CONST_FUN_OBJ_0(py_omv_csi_get_framebuffers_obj, py_omv_csi_get_framebuffers);

static mp_obj_t py_omv_csi_disable_delays(uint n_args, const mp_obj_t *args) {
    if (!n_args) {
        return mp_obj_new_bool(csi.disable_delays);
    }

    csi.disable_delays = mp_obj_get_int(args[0]);
    return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(py_omv_csi_disable_delays_obj, 0, 1, py_omv_csi_disable_delays);

static mp_obj_t py_omv_csi_disable_full_flush(uint n_args, const mp_obj_t *args) {
    if (!n_args) {
        return mp_obj_new_bool(csi.disable_full_flush);
    }

    csi.disable_full_flush = mp_obj_get_int(args[0]);
    return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(py_omv_csi_disable_full_flush_obj, 0, 1, py_omv_csi_disable_full_flush);

static mp_obj_t py_omv_csi_set_special_effect(mp_obj_t sde) {
    if (omv_csi_set_special_effect(mp_obj_get_int(sde)) != 0) {
        return mp_const_false;
    }
    return mp_const_true;
}
static MP_DEFINE_CONST_FUN_OBJ_1(py_omv_csi_set_special_effect_obj, py_omv_csi_set_special_effect);

static mp_obj_t py_omv_csi_set_lens_correction(mp_obj_t enable, mp_obj_t radi, mp_obj_t coef) {
    if (omv_csi_set_lens_correction(mp_obj_is_true(enable),
                                    mp_obj_get_int(radi), mp_obj_get_int(coef)) != 0) {
        return mp_const_false;
    }
    return mp_const_true;
}
static MP_DEFINE_CONST_FUN_OBJ_3(py_omv_csi_set_lens_correction_obj, py_omv_csi_set_lens_correction);

static void omv_csi_vsync_callback(uint32_t vsync) {
    if (mp_obj_is_callable(vsync_callback)) {
        mp_call_function_1(vsync_callback, mp_obj_new_int(vsync));
    }
}

static mp_obj_t py_omv_csi_set_vsync_callback(mp_obj_t vsync_callback_obj) {
    if (!mp_obj_is_callable(vsync_callback_obj)) {
        vsync_callback = mp_const_none;
        omv_csi_set_vsync_callback(NULL);
    } else {
        vsync_callback = vsync_callback_obj;
        omv_csi_set_vsync_callback(omv_csi_vsync_callback);
    }

    return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_1(py_omv_csi_set_vsync_callback_obj, py_omv_csi_set_vsync_callback);

static void omv_csi_frame_callback() {
    if (mp_obj_is_callable(frame_callback)) {
        mp_call_function_0(frame_callback);
    }
}

static mp_obj_t py_omv_csi_set_frame_callback(mp_obj_t frame_callback_obj) {
    if (!mp_obj_is_callable(frame_callback_obj)) {
        frame_callback = mp_const_none;
        omv_csi_set_frame_callback(NULL);
    } else {
        frame_callback = frame_callback_obj;
        omv_csi_set_frame_callback(omv_csi_frame_callback);
    }

    return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_1(py_omv_csi_set_frame_callback_obj, py_omv_csi_set_frame_callback);

static mp_obj_t py_omv_csi_ioctl(uint n_args, const mp_obj_t *args) {
    mp_obj_t ret_obj = mp_const_none;
    int request = mp_obj_get_int(args[0]);
    int error = OMV_CSI_ERROR_INVALID_ARGUMENT;

    switch (request) {
        case OMV_CSI_IOCTL_SET_READOUT_WINDOW: {
            if (n_args >= 2) {
                int x, y, w, h;
                mp_obj_t *array;
                mp_uint_t array_len;
                mp_obj_get_array(args[1], &array_len, &array);

                if (array_len == 4) {
                    x = mp_obj_get_int(array[0]);
                    y = mp_obj_get_int(array[1]);
                    w = mp_obj_get_int(array[2]);
                    h = mp_obj_get_int(array[3]);
                } else if (array_len == 2) {
                    w = mp_obj_get_int(array[0]);
                    h = mp_obj_get_int(array[1]);
                    x = 0;
                    y = 0;
                } else {
                    mp_raise_msg(&mp_type_ValueError,
                                 MP_ERROR_TEXT("The tuple/list must either be (x, y, w, h) or (w, h)"));
                }

                error = omv_csi_ioctl(request, x, y, w, h);
            }
            break;
        }

        case OMV_CSI_IOCTL_GET_READOUT_WINDOW: {
            int x, y, w, h;
            error = omv_csi_ioctl(request, &x, &y, &w, &h);
            if (error == 0) {
                ret_obj = mp_obj_new_tuple(4, (mp_obj_t []) {mp_obj_new_int(x),
                                                             mp_obj_new_int(y),
                                                             mp_obj_new_int(w),
                                                             mp_obj_new_int(h)});
            }
            break;
        }

        case OMV_CSI_IOCTL_SET_TRIGGERED_MODE:
        case OMV_CSI_IOCTL_SET_FOV_WIDE:
        case OMV_CSI_IOCTL_SET_NIGHT_MODE: {
            if (n_args >= 2) {
                error = omv_csi_ioctl(request, mp_obj_get_int(args[1]));
            }
            break;
        }

        case OMV_CSI_IOCTL_GET_TRIGGERED_MODE:
        case OMV_CSI_IOCTL_GET_FOV_WIDE:
        case OMV_CSI_IOCTL_GET_NIGHT_MODE: {
            int enabled;
            error = omv_csi_ioctl(request, &enabled);
            if (error == 0) {
                ret_obj = mp_obj_new_bool(enabled);
            }
            break;
        }

        #if (OMV_OV5640_AF_ENABLE == 1)
        case OMV_CSI_IOCTL_TRIGGER_AUTO_FOCUS:
        case OMV_CSI_IOCTL_PAUSE_AUTO_FOCUS:
        case OMV_CSI_IOCTL_RESET_AUTO_FOCUS: {
            error = omv_csi_ioctl(request);
            break;
        }
        case OMV_CSI_IOCTL_WAIT_ON_AUTO_FOCUS: {
            error = omv_csi_ioctl(request, (n_args < 2) ? 5000 : mp_obj_get_int(args[1]));
            break;
        }
        #endif

        case OMV_CSI_IOCTL_LEPTON_GET_WIDTH: {
            int width;
            error = omv_csi_ioctl(request, &width);
            if (error == 0) {
                ret_obj = mp_obj_new_int(width);
            }
            break;
        }

        case OMV_CSI_IOCTL_LEPTON_GET_HEIGHT: {
            int height;
            error = omv_csi_ioctl(request, &height);
            if (error == 0) {
                ret_obj = mp_obj_new_int(height);
            }
            break;
        }

        case OMV_CSI_IOCTL_LEPTON_GET_RADIOMETRY: {
            int radiometry;
            error = omv_csi_ioctl(request, &radiometry);
            if (error == 0) {
                ret_obj = mp_obj_new_int(radiometry);
            }
            break;
        }

        case OMV_CSI_IOCTL_LEPTON_GET_REFRESH: {
            int refresh;
            error = omv_csi_ioctl(request, &refresh);
            if (error == 0) {
                ret_obj = mp_obj_new_int(refresh);
            }
            break;
        }

        case OMV_CSI_IOCTL_LEPTON_GET_RESOLUTION: {
            int resolution;
            error = omv_csi_ioctl(request, &resolution);
            if (error == 0) {
                ret_obj = mp_obj_new_int(resolution);
            }
            break;
        }

        case OMV_CSI_IOCTL_LEPTON_RUN_COMMAND: {
            if (n_args >= 2) {
                error = omv_csi_ioctl(request, mp_obj_get_int(args[1]));
            }
            break;
        }

        case OMV_CSI_IOCTL_LEPTON_SET_ATTRIBUTE: {
            if (n_args >= 3) {
                size_t data_len;
                int command = mp_obj_get_int(args[1]);
                uint16_t *data = (uint16_t *) mp_obj_str_get_data(args[2], &data_len);
                PY_ASSERT_TRUE_MSG(data_len > 0, "0 bytes transferred!");
                error = omv_csi_ioctl(request, command, data, data_len / sizeof(uint16_t));
            }
            break;
        }

        case OMV_CSI_IOCTL_LEPTON_GET_ATTRIBUTE: {
            if (n_args >= 3) {
                int command = mp_obj_get_int(args[1]);
                size_t data_len = mp_obj_get_int(args[2]);
                PY_ASSERT_TRUE_MSG(data_len > 0, "0 bytes transferred!");
                uint16_t *data = xalloc(data_len * sizeof(uint16_t));
                error = omv_csi_ioctl(request, command, data, data_len);
                if (error == 0) {
                    ret_obj = mp_obj_new_bytearray_by_ref(data_len * sizeof(uint16_t), data);
                }
            }
            break;
        }

        case OMV_CSI_IOCTL_LEPTON_GET_FPA_TEMP:
        case OMV_CSI_IOCTL_LEPTON_GET_AUX_TEMP: {
            int temp;
            error = omv_csi_ioctl(request, &temp);
            if (error == 0) {
                ret_obj = mp_obj_new_float((((float) temp) / 100) - 273.15f);
            }
            break;
        }

        case OMV_CSI_IOCTL_LEPTON_SET_MODE:
            if (n_args >= 2) {
                int high_temp = (n_args == 2) ? false : mp_obj_get_int(args[2]);
                error = omv_csi_ioctl(request, mp_obj_get_int(args[1]), high_temp);
            }
            break;

        case OMV_CSI_IOCTL_LEPTON_GET_MODE: {
            int enabled, high_temp;
            error = omv_csi_ioctl(request, &enabled, &high_temp);
            if (error == 0) {
                ret_obj = mp_obj_new_tuple(2, (mp_obj_t []) {mp_obj_new_bool(enabled), mp_obj_new_bool(high_temp)});
            }
            break;
        }

        case OMV_CSI_IOCTL_LEPTON_SET_RANGE:
            if (n_args >= 3) {
                // GCC will not let us pass floats to ... so we have to pass float pointers instead.
                float min = mp_obj_get_float(args[1]);
                float max = mp_obj_get_float(args[2]);
                error = omv_csi_ioctl(request, &min, &max);
            }
            break;

        case OMV_CSI_IOCTL_LEPTON_GET_RANGE: {
            float min, max;
            error = omv_csi_ioctl(request, &min, &max);
            if (error == 0) {
                ret_obj = mp_obj_new_tuple(2, (mp_obj_t []) {mp_obj_new_float(min), mp_obj_new_float(max)});
            }
            break;
        }

        #if (OMV_HM01B0_ENABLE == 1)
        case OMV_CSI_IOCTL_HIMAX_MD_ENABLE: {
            if (n_args >= 2) {
                error = omv_csi_ioctl(request, mp_obj_get_int(args[1]));
            }
            break;
        }

        case OMV_CSI_IOCTL_HIMAX_MD_WINDOW: {
            if (n_args >= 2) {
                int x, y, w, h;
                mp_obj_t *array;
                mp_uint_t array_len;
                mp_obj_get_array(args[1], &array_len, &array);

                if (array_len == 4) {
                    x = mp_obj_get_int(array[0]);
                    y = mp_obj_get_int(array[1]);
                    w = mp_obj_get_int(array[2]);
                    h = mp_obj_get_int(array[3]);
                } else if (array_len == 2) {
                    w = mp_obj_get_int(array[0]);
                    h = mp_obj_get_int(array[1]);
                    x = 0;
                    y = 0;
                } else {
                    mp_raise_msg(&mp_type_ValueError,
                                 MP_ERROR_TEXT("The tuple/list must either be (x, y, w, h) or (w, h)"));
                }

                error = omv_csi_ioctl(request, x, y, w, h);
            }
            break;
        }

        case OMV_CSI_IOCTL_HIMAX_MD_THRESHOLD: {
            if (n_args >= 2) {
                error = omv_csi_ioctl(request, mp_obj_get_int(args[1]));
            }
            break;
        }

        case OMV_CSI_IOCTL_HIMAX_MD_CLEAR: {
            error = omv_csi_ioctl(request);
            break;
        }

        case OMV_CSI_IOCTL_HIMAX_OSC_ENABLE: {
            if (n_args >= 2) {
                error = omv_csi_ioctl(request, mp_obj_get_int(args[1]));
            }
            break;
        }
        #endif // (OMV_HM01B0_ENABLE == 1)

        case OMV_CSI_IOCTL_GET_RGB_STATS: {
            uint32_t r, gb, gr, b;
            error = omv_csi_ioctl(request, &r, &gb, &gr, &b);
            if (error == 0) {
                ret_obj = mp_obj_new_tuple(4, (mp_obj_t []) {mp_obj_new_int(r),
                                                             mp_obj_new_int(gb),
                                                             mp_obj_new_int(gr),
                                                             mp_obj_new_int(b)});
            }
            break;
        }

        default: {
            omv_csi_raise_error(OMV_CSI_ERROR_CTL_UNSUPPORTED);
            break;
        }
    }

    if (error != 0) {
        omv_csi_raise_error(error);
    }

    return ret_obj;
}
static MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(py_omv_csi_ioctl_obj, 1, 5, py_omv_csi_ioctl);

static mp_obj_t py_omv_csi_set_color_palette(mp_obj_t palette_obj) {
    int palette = mp_obj_get_int(palette_obj);
    switch (palette) {
        case COLOR_PALETTE_RAINBOW:
            omv_csi_set_color_palette(rainbow_table);
            break;
        case COLOR_PALETTE_IRONBOW:
            omv_csi_set_color_palette(ironbow_table);
            break;
        #if (MICROPY_PY_TOF == 1)
        case COLOR_PALETTE_DEPTH:
            omv_csi_set_color_palette(depth_table);
            break;
        #endif // MICROPY_PY_TOF == 1
        #if (OMV_GENX320_ENABLE == 1)
        case COLOR_PALETTE_EVT_DARK:
            omv_csi_set_color_palette(evt_dark_table);
            break;
        case COLOR_PALETTE_EVT_LIGHT:
            omv_csi_set_color_palette(evt_light_table);
            break;
        #endif // OMV_GENX320_ENABLE == 1
        default:
            omv_csi_raise_error(OMV_CSI_ERROR_INVALID_ARGUMENT);
            break;
    }
    return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_1(py_omv_csi_set_color_palette_obj, py_omv_csi_set_color_palette);

static mp_obj_t py_omv_csi_get_color_palette() {
    const uint16_t *palette = omv_csi_get_color_palette();
    if (palette == rainbow_table) {
        return mp_obj_new_int(COLOR_PALETTE_RAINBOW);
    } else if (palette == ironbow_table) {
        return mp_obj_new_int(COLOR_PALETTE_IRONBOW);
    #if (MICROPY_PY_TOF == 1)
    } else if (palette == depth_table) {
        return mp_obj_new_int(COLOR_PALETTE_DEPTH);
    #endif // MICROPY_PY_TOF == 1
    #if (OMV_GENX320_ENABLE == 1)
    } else if (palette == evt_dark_table) {
        return mp_obj_new_int(COLOR_PALETTE_EVT_DARK);
    } else if (palette == evt_light_table) {
        return mp_obj_new_int(COLOR_PALETTE_EVT_LIGHT);
    #endif // OMV_GENX320_ENABLE == 1
    }
    return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_0(py_omv_csi_get_color_palette_obj, py_omv_csi_get_color_palette);

static mp_obj_t py_omv_csi_write_reg(mp_obj_t addr, mp_obj_t val) {
    omv_csi_write_reg(mp_obj_get_int(addr), mp_obj_get_int(val));
    return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_2(py_omv_csi_write_reg_obj, py_omv_csi_write_reg);

static mp_obj_t py_omv_csi_read_reg(mp_obj_t addr) {
    return mp_obj_new_int(omv_csi_read_reg(mp_obj_get_int(addr)));
}
static MP_DEFINE_CONST_FUN_OBJ_1(py_omv_csi_read_reg_obj, py_omv_csi_read_reg);

static const mp_rom_map_elem_t globals_dict_table[] = {
    { MP_ROM_QSTR(MP_QSTR___name__),            MP_ROM_QSTR(MP_QSTR_csi)},

    // Pixel Formats
    { MP_ROM_QSTR(MP_QSTR_BINARY),              MP_ROM_INT(PIXFORMAT_BINARY)},      /* 1BPP/BINARY*/
    { MP_ROM_QSTR(MP_QSTR_GRAYSCALE),           MP_ROM_INT(PIXFORMAT_GRAYSCALE)},   /* 1BPP/GRAYSCALE*/
    { MP_ROM_QSTR(MP_QSTR_RGB565),              MP_ROM_INT(PIXFORMAT_RGB565)},      /* 2BPP/RGB565*/
    { MP_ROM_QSTR(MP_QSTR_BAYER),               MP_ROM_INT(PIXFORMAT_BAYER)},       /* 1BPP/RAW*/
    { MP_ROM_QSTR(MP_QSTR_YUV422),              MP_ROM_INT(PIXFORMAT_YUV422)},      /* 2BPP/YUV422*/
    { MP_ROM_QSTR(MP_QSTR_JPEG),                MP_ROM_INT(PIXFORMAT_JPEG)},        /* JPEG/COMPRESSED*/

    // Image Sensors
    { MP_ROM_QSTR(MP_QSTR_OV2640),              MP_ROM_INT(OV2640_ID)},
    { MP_ROM_QSTR(MP_QSTR_OV5640),              MP_ROM_INT(OV5640_ID)},
    { MP_ROM_QSTR(MP_QSTR_OV7670),              MP_ROM_INT(OV7670_ID)},
    { MP_ROM_QSTR(MP_QSTR_OV7690),              MP_ROM_INT(OV7690_ID)},
    { MP_ROM_QSTR(MP_QSTR_OV7725),              MP_ROM_INT(OV7725_ID)},
    { MP_ROM_QSTR(MP_QSTR_OV9650),              MP_ROM_INT(OV9650_ID)},
    { MP_ROM_QSTR(MP_QSTR_MT9V022),             MP_ROM_INT(MT9V0X2_ID)},
    { MP_ROM_QSTR(MP_QSTR_MT9V024),             MP_ROM_INT(MT9V0X4_ID)},
    { MP_ROM_QSTR(MP_QSTR_MT9V032),             MP_ROM_INT(MT9V0X2_ID)},
    { MP_ROM_QSTR(MP_QSTR_MT9V034),             MP_ROM_INT(MT9V0X4_ID)},
    { MP_ROM_QSTR(MP_QSTR_MT9M114),             MP_ROM_INT(MT9M114_ID)},
    { MP_ROM_QSTR(MP_QSTR_LEPTON),              MP_ROM_INT(LEPTON_ID)},
    { MP_ROM_QSTR(MP_QSTR_HM01B0),              MP_ROM_INT(HM01B0_ID)},
    { MP_ROM_QSTR(MP_QSTR_GC2145),              MP_ROM_INT(GC2145_ID)},
    { MP_ROM_QSTR(MP_QSTR_PAG7920),             MP_ROM_INT(PAG7920_ID)},
    { MP_ROM_QSTR(MP_QSTR_PAJ6100),             MP_ROM_INT(PAJ6100_ID)},
    { MP_ROM_QSTR(MP_QSTR_FROGEYE2020),         MP_ROM_INT(FROGEYE2020_ID)},

    // Special effects
    { MP_ROM_QSTR(MP_QSTR_NORMAL),              MP_ROM_INT(OMV_CSI_SDE_NORMAL)},    /* Normal/No SDE */
    { MP_ROM_QSTR(MP_QSTR_NEGATIVE),            MP_ROM_INT(OMV_CSI_SDE_NEGATIVE)},  /* Negative image */

    // C/SIF Resolutions
    { MP_ROM_QSTR(MP_QSTR_QQCIF),               MP_ROM_INT(OMV_CSI_FRAMESIZE_QQCIF)},    /* 88x72     */
    { MP_ROM_QSTR(MP_QSTR_QCIF),                MP_ROM_INT(OMV_CSI_FRAMESIZE_QCIF)},     /* 176x144   */
    { MP_ROM_QSTR(MP_QSTR_CIF),                 MP_ROM_INT(OMV_CSI_FRAMESIZE_CIF)},      /* 352x288   */
    { MP_ROM_QSTR(MP_QSTR_QQSIF),               MP_ROM_INT(OMV_CSI_FRAMESIZE_QQSIF)},    /* 88x60     */
    { MP_ROM_QSTR(MP_QSTR_QSIF),                MP_ROM_INT(OMV_CSI_FRAMESIZE_QSIF)},     /* 176x120   */
    { MP_ROM_QSTR(MP_QSTR_SIF),                 MP_ROM_INT(OMV_CSI_FRAMESIZE_SIF)},      /* 352x240   */
    // VGA Resolutions
    { MP_ROM_QSTR(MP_QSTR_QQQQVGA),             MP_ROM_INT(OMV_CSI_FRAMESIZE_QQQQVGA)},  /* 40x30     */
    { MP_ROM_QSTR(MP_QSTR_QQQVGA),              MP_ROM_INT(OMV_CSI_FRAMESIZE_QQQVGA)},   /* 80x60     */
    { MP_ROM_QSTR(MP_QSTR_QQVGA),               MP_ROM_INT(OMV_CSI_FRAMESIZE_QQVGA)},    /* 160x120   */
    { MP_ROM_QSTR(MP_QSTR_QVGA),                MP_ROM_INT(OMV_CSI_FRAMESIZE_QVGA)},     /* 320x240   */
    { MP_ROM_QSTR(MP_QSTR_VGA),                 MP_ROM_INT(OMV_CSI_FRAMESIZE_VGA)},      /* 640x480   */
    { MP_ROM_QSTR(MP_QSTR_HQQQQVGA),            MP_ROM_INT(OMV_CSI_FRAMESIZE_HQQQQVGA)}, /* 40x20     */
    { MP_ROM_QSTR(MP_QSTR_HQQQVGA),             MP_ROM_INT(OMV_CSI_FRAMESIZE_HQQQVGA)},  /* 80x40     */
    { MP_ROM_QSTR(MP_QSTR_HQQVGA),              MP_ROM_INT(OMV_CSI_FRAMESIZE_HQQVGA)},   /* 160x80    */
    { MP_ROM_QSTR(MP_QSTR_HQVGA),               MP_ROM_INT(OMV_CSI_FRAMESIZE_HQVGA)},    /* 240x160   */
    { MP_ROM_QSTR(MP_QSTR_HVGA),                MP_ROM_INT(OMV_CSI_FRAMESIZE_HVGA)},     /* 480x320   */
    // FFT Resolutions
    { MP_ROM_QSTR(MP_QSTR_B64X32),              MP_ROM_INT(OMV_CSI_FRAMESIZE_64X32)},    /* 64x32     */
    { MP_ROM_QSTR(MP_QSTR_B64X64),              MP_ROM_INT(OMV_CSI_FRAMESIZE_64X64)},    /* 64x64     */
    { MP_ROM_QSTR(MP_QSTR_B128X64),             MP_ROM_INT(OMV_CSI_FRAMESIZE_128X64)},   /* 128x64    */
    { MP_ROM_QSTR(MP_QSTR_B128X128),            MP_ROM_INT(OMV_CSI_FRAMESIZE_128X128)},  /* 128x128   */
    // Himax Resolutions
    { MP_ROM_QSTR(MP_QSTR_B160X160),            MP_ROM_INT(OMV_CSI_FRAMESIZE_160X160)},  /* 160x160   */
    { MP_ROM_QSTR(MP_QSTR_B320X320),            MP_ROM_INT(OMV_CSI_FRAMESIZE_320X320)},  /* 320x320   */
    // Other Resolutions
    { MP_ROM_QSTR(MP_QSTR_LCD),                 MP_ROM_INT(OMV_CSI_FRAMESIZE_LCD)},      /* 128x160   */
    { MP_ROM_QSTR(MP_QSTR_QQVGA2),              MP_ROM_INT(OMV_CSI_FRAMESIZE_QQVGA2)},   /* 128x160   */
    { MP_ROM_QSTR(MP_QSTR_WVGA),                MP_ROM_INT(OMV_CSI_FRAMESIZE_WVGA)},     /* 720x480   */
    { MP_ROM_QSTR(MP_QSTR_WVGA2),               MP_ROM_INT(OMV_CSI_FRAMESIZE_WVGA2)},    /* 752x480   */
    { MP_ROM_QSTR(MP_QSTR_SVGA),                MP_ROM_INT(OMV_CSI_FRAMESIZE_SVGA)},     /* 800x600   */
    { MP_ROM_QSTR(MP_QSTR_XGA),                 MP_ROM_INT(OMV_CSI_FRAMESIZE_XGA)},      /* 1024x768  */
    { MP_ROM_QSTR(MP_QSTR_WXGA),                MP_ROM_INT(OMV_CSI_FRAMESIZE_WXGA)},     /* 1280x768  */
    { MP_ROM_QSTR(MP_QSTR_SXGA),                MP_ROM_INT(OMV_CSI_FRAMESIZE_SXGA)},     /* 1280x1024 */
    { MP_ROM_QSTR(MP_QSTR_SXGAM),               MP_ROM_INT(OMV_CSI_FRAMESIZE_SXGAM)},    /* 1280x960  */
    { MP_ROM_QSTR(MP_QSTR_UXGA),                MP_ROM_INT(OMV_CSI_FRAMESIZE_UXGA)},     /* 1600x1200 */
    { MP_ROM_QSTR(MP_QSTR_HD),                  MP_ROM_INT(OMV_CSI_FRAMESIZE_HD)},       /* 1280x720  */
    { MP_ROM_QSTR(MP_QSTR_FHD),                 MP_ROM_INT(OMV_CSI_FRAMESIZE_FHD)},      /* 1920x1080 */
    { MP_ROM_QSTR(MP_QSTR_QHD),                 MP_ROM_INT(OMV_CSI_FRAMESIZE_QHD)},      /* 2560x1440 */
    { MP_ROM_QSTR(MP_QSTR_QXGA),                MP_ROM_INT(OMV_CSI_FRAMESIZE_QXGA)},     /* 2048x1536 */
    { MP_ROM_QSTR(MP_QSTR_WQXGA),               MP_ROM_INT(OMV_CSI_FRAMESIZE_WQXGA)},    /* 2560x1600 */
    { MP_ROM_QSTR(MP_QSTR_WQXGA2),              MP_ROM_INT(OMV_CSI_FRAMESIZE_WQXGA2)},   /* 2592x1944 */

    // OMV_CSI_IOCTLs
    { MP_ROM_QSTR(MP_QSTR_IOCTL_SET_READOUT_WINDOW),    MP_ROM_INT(OMV_CSI_IOCTL_SET_READOUT_WINDOW)},
    { MP_ROM_QSTR(MP_QSTR_IOCTL_GET_READOUT_WINDOW),    MP_ROM_INT(OMV_CSI_IOCTL_GET_READOUT_WINDOW)},
    { MP_ROM_QSTR(MP_QSTR_IOCTL_SET_TRIGGERED_MODE),    MP_ROM_INT(OMV_CSI_IOCTL_SET_TRIGGERED_MODE)},
    { MP_ROM_QSTR(MP_QSTR_IOCTL_GET_TRIGGERED_MODE),    MP_ROM_INT(OMV_CSI_IOCTL_GET_TRIGGERED_MODE)},
    { MP_ROM_QSTR(MP_QSTR_IOCTL_SET_FOV_WIDE),          MP_ROM_INT(OMV_CSI_IOCTL_SET_FOV_WIDE)},
    { MP_ROM_QSTR(MP_QSTR_IOCTL_GET_FOV_WIDE),          MP_ROM_INT(OMV_CSI_IOCTL_GET_FOV_WIDE)},
    #if (OMV_OV5640_AF_ENABLE == 1)
    { MP_ROM_QSTR(MP_QSTR_IOCTL_TRIGGER_AUTO_FOCUS),    MP_ROM_INT(OMV_CSI_IOCTL_TRIGGER_AUTO_FOCUS)},
    { MP_ROM_QSTR(MP_QSTR_IOCTL_PAUSE_AUTO_FOCUS),      MP_ROM_INT(OMV_CSI_IOCTL_PAUSE_AUTO_FOCUS)},
    { MP_ROM_QSTR(MP_QSTR_IOCTL_RESET_AUTO_FOCUS),      MP_ROM_INT(OMV_CSI_IOCTL_RESET_AUTO_FOCUS)},
    { MP_ROM_QSTR(MP_QSTR_IOCTL_WAIT_ON_AUTO_FOCUS),    MP_ROM_INT(OMV_CSI_IOCTL_WAIT_ON_AUTO_FOCUS)},
    #endif
    { MP_ROM_QSTR(MP_QSTR_IOCTL_SET_NIGHT_MODE),        MP_ROM_INT(OMV_CSI_IOCTL_SET_NIGHT_MODE)},
    { MP_ROM_QSTR(MP_QSTR_IOCTL_GET_NIGHT_MODE),        MP_ROM_INT(OMV_CSI_IOCTL_GET_NIGHT_MODE)},
    { MP_ROM_QSTR(MP_QSTR_IOCTL_LEPTON_GET_WIDTH),      MP_ROM_INT(OMV_CSI_IOCTL_LEPTON_GET_WIDTH)},
    { MP_ROM_QSTR(MP_QSTR_IOCTL_LEPTON_GET_HEIGHT),     MP_ROM_INT(OMV_CSI_IOCTL_LEPTON_GET_HEIGHT)},
    { MP_ROM_QSTR(MP_QSTR_IOCTL_LEPTON_GET_RADIOMETRY), MP_ROM_INT(OMV_CSI_IOCTL_LEPTON_GET_RADIOMETRY)},
    { MP_ROM_QSTR(MP_QSTR_IOCTL_LEPTON_GET_REFRESH),    MP_ROM_INT(OMV_CSI_IOCTL_LEPTON_GET_REFRESH)},
    { MP_ROM_QSTR(MP_QSTR_IOCTL_LEPTON_GET_RESOLUTION), MP_ROM_INT(OMV_CSI_IOCTL_LEPTON_GET_RESOLUTION)},
    { MP_ROM_QSTR(MP_QSTR_IOCTL_LEPTON_RUN_COMMAND),    MP_ROM_INT(OMV_CSI_IOCTL_LEPTON_RUN_COMMAND)},
    { MP_ROM_QSTR(MP_QSTR_IOCTL_LEPTON_SET_ATTRIBUTE),  MP_ROM_INT(OMV_CSI_IOCTL_LEPTON_SET_ATTRIBUTE)},
    { MP_ROM_QSTR(MP_QSTR_IOCTL_LEPTON_GET_ATTRIBUTE),  MP_ROM_INT(OMV_CSI_IOCTL_LEPTON_GET_ATTRIBUTE)},
    { MP_ROM_QSTR(MP_QSTR_IOCTL_LEPTON_GET_FPA_TEMP),   MP_ROM_INT(OMV_CSI_IOCTL_LEPTON_GET_FPA_TEMP)},
    { MP_ROM_QSTR(MP_QSTR_IOCTL_LEPTON_GET_AUX_TEMP),   MP_ROM_INT(OMV_CSI_IOCTL_LEPTON_GET_AUX_TEMP)},
    { MP_ROM_QSTR(MP_QSTR_IOCTL_LEPTON_SET_MODE),       MP_ROM_INT(OMV_CSI_IOCTL_LEPTON_SET_MODE)},
    { MP_ROM_QSTR(MP_QSTR_IOCTL_LEPTON_GET_MODE),       MP_ROM_INT(OMV_CSI_IOCTL_LEPTON_GET_MODE)},
    { MP_ROM_QSTR(MP_QSTR_IOCTL_LEPTON_SET_RANGE),      MP_ROM_INT(OMV_CSI_IOCTL_LEPTON_SET_RANGE)},
    { MP_ROM_QSTR(MP_QSTR_IOCTL_LEPTON_GET_RANGE),      MP_ROM_INT(OMV_CSI_IOCTL_LEPTON_GET_RANGE)},
    #if (OMV_HM01B0_ENABLE == 1)
    { MP_ROM_QSTR(MP_QSTR_IOCTL_HIMAX_MD_ENABLE),       MP_ROM_INT(OMV_CSI_IOCTL_HIMAX_MD_ENABLE)},
    { MP_ROM_QSTR(MP_QSTR_IOCTL_HIMAX_MD_WINDOW),       MP_ROM_INT(OMV_CSI_IOCTL_HIMAX_MD_WINDOW)},
    { MP_ROM_QSTR(MP_QSTR_IOCTL_HIMAX_MD_THRESHOLD),    MP_ROM_INT(OMV_CSI_IOCTL_HIMAX_MD_THRESHOLD)},
    { MP_ROM_QSTR(MP_QSTR_IOCTL_HIMAX_MD_CLEAR),        MP_ROM_INT(OMV_CSI_IOCTL_HIMAX_MD_CLEAR)},
    { MP_ROM_QSTR(MP_QSTR_IOCTL_HIMAX_OSC_ENABLE),      MP_ROM_INT(OMV_CSI_IOCTL_HIMAX_OSC_ENABLE)},
    #endif
    { MP_ROM_QSTR(MP_QSTR_IOCTL_GET_RGB_STATS),         MP_ROM_INT(OMV_CSI_IOCTL_GET_RGB_STATS)},

    // Sensor functions
    { MP_ROM_QSTR(MP_QSTR___init__),                    MP_ROM_PTR(&py_omv_csi__init__obj) },
    { MP_ROM_QSTR(MP_QSTR_reset),                       MP_ROM_PTR(&py_omv_csi_reset_obj) },
    { MP_ROM_QSTR(MP_QSTR_sleep),                       MP_ROM_PTR(&py_omv_csi_sleep_obj) },
    { MP_ROM_QSTR(MP_QSTR_shutdown),                    MP_ROM_PTR(&py_omv_csi_shutdown_obj) },
    { MP_ROM_QSTR(MP_QSTR_flush),                       MP_ROM_PTR(&py_omv_csi_flush_obj) },
    { MP_ROM_QSTR(MP_QSTR_snapshot),                    MP_ROM_PTR(&py_omv_csi_snapshot_obj) },
    { MP_ROM_QSTR(MP_QSTR_skip_frames),                 MP_ROM_PTR(&py_omv_csi_skip_frames_obj) },
    { MP_ROM_QSTR(MP_QSTR_width),                       MP_ROM_PTR(&py_omv_csi_width_obj) },
    { MP_ROM_QSTR(MP_QSTR_height),                      MP_ROM_PTR(&py_omv_csi_height_obj) },
    { MP_ROM_QSTR(MP_QSTR_get_fb),                      MP_ROM_PTR(&py_omv_csi_get_fb_obj) },
    { MP_ROM_QSTR(MP_QSTR_get_id),                      MP_ROM_PTR(&py_omv_csi_get_id_obj) },
    { MP_ROM_QSTR(MP_QSTR_get_frame_available),         MP_ROM_PTR(&py_omv_csi_get_frame_available_obj) },
    { MP_ROM_QSTR(MP_QSTR_alloc_extra_fb),              MP_ROM_PTR(&py_omv_csi_alloc_extra_fb_obj) },
    { MP_ROM_QSTR(MP_QSTR_dealloc_extra_fb),            MP_ROM_PTR(&py_omv_csi_dealloc_extra_fb_obj) },
    { MP_ROM_QSTR(MP_QSTR_set_pixformat),               MP_ROM_PTR(&py_omv_csi_set_pixformat_obj) },
    { MP_ROM_QSTR(MP_QSTR_get_pixformat),               MP_ROM_PTR(&py_omv_csi_get_pixformat_obj) },
    { MP_ROM_QSTR(MP_QSTR_set_framesize),               MP_ROM_PTR(&py_omv_csi_set_framesize_obj) },
    { MP_ROM_QSTR(MP_QSTR_get_framesize),               MP_ROM_PTR(&py_omv_csi_get_framesize_obj) },
    { MP_ROM_QSTR(MP_QSTR_set_framerate),               MP_ROM_PTR(&py_omv_csi_set_framerate_obj) },
    { MP_ROM_QSTR(MP_QSTR_get_framerate),               MP_ROM_PTR(&py_omv_csi_get_framerate_obj) },
    { MP_ROM_QSTR(MP_QSTR_set_windowing),               MP_ROM_PTR(&py_omv_csi_set_windowing_obj) },
    { MP_ROM_QSTR(MP_QSTR_get_windowing),               MP_ROM_PTR(&py_omv_csi_get_windowing_obj) },
    { MP_ROM_QSTR(MP_QSTR_set_gainceiling),             MP_ROM_PTR(&py_omv_csi_set_gainceiling_obj) },
    { MP_ROM_QSTR(MP_QSTR_set_contrast),                MP_ROM_PTR(&py_omv_csi_set_contrast_obj) },
    { MP_ROM_QSTR(MP_QSTR_set_brightness),              MP_ROM_PTR(&py_omv_csi_set_brightness_obj) },
    { MP_ROM_QSTR(MP_QSTR_set_saturation),              MP_ROM_PTR(&py_omv_csi_set_saturation_obj) },
    { MP_ROM_QSTR(MP_QSTR_set_quality),                 MP_ROM_PTR(&py_omv_csi_set_quality_obj) },
    { MP_ROM_QSTR(MP_QSTR_set_colorbar),                MP_ROM_PTR(&py_omv_csi_set_colorbar_obj) },
    { MP_ROM_QSTR(MP_QSTR_set_auto_gain),               MP_ROM_PTR(&py_omv_csi_set_auto_gain_obj) },
    { MP_ROM_QSTR(MP_QSTR_get_gain_db),                 MP_ROM_PTR(&py_omv_csi_get_gain_db_obj) },
    { MP_ROM_QSTR(MP_QSTR_set_auto_exposure),           MP_ROM_PTR(&py_omv_csi_set_auto_exposure_obj) },
    { MP_ROM_QSTR(MP_QSTR_get_exposure_us),             MP_ROM_PTR(&py_omv_csi_get_exposure_us_obj) },
    { MP_ROM_QSTR(MP_QSTR_set_auto_whitebal),           MP_ROM_PTR(&py_omv_csi_set_auto_whitebal_obj) },
    { MP_ROM_QSTR(MP_QSTR_get_rgb_gain_db),             MP_ROM_PTR(&py_omv_csi_get_rgb_gain_db_obj) },
    { MP_ROM_QSTR(MP_QSTR_set_auto_blc),                MP_ROM_PTR(&py_omv_csi_set_auto_blc_obj) },
    { MP_ROM_QSTR(MP_QSTR_get_blc_regs),                MP_ROM_PTR(&py_omv_csi_get_blc_regs_obj) },
    { MP_ROM_QSTR(MP_QSTR_set_hmirror),                 MP_ROM_PTR(&py_omv_csi_set_hmirror_obj) },
    { MP_ROM_QSTR(MP_QSTR_get_hmirror),                 MP_ROM_PTR(&py_omv_csi_get_hmirror_obj) },
    { MP_ROM_QSTR(MP_QSTR_set_vflip),                   MP_ROM_PTR(&py_omv_csi_set_vflip_obj) },
    { MP_ROM_QSTR(MP_QSTR_get_vflip),                   MP_ROM_PTR(&py_omv_csi_get_vflip_obj) },
    { MP_ROM_QSTR(MP_QSTR_set_transpose),               MP_ROM_PTR(&py_omv_csi_set_transpose_obj) },
    { MP_ROM_QSTR(MP_QSTR_get_transpose),               MP_ROM_PTR(&py_omv_csi_get_transpose_obj) },
    { MP_ROM_QSTR(MP_QSTR_set_auto_rotation),           MP_ROM_PTR(&py_omv_csi_set_auto_rotation_obj) },
    { MP_ROM_QSTR(MP_QSTR_get_auto_rotation),           MP_ROM_PTR(&py_omv_csi_get_auto_rotation_obj) },
    { MP_ROM_QSTR(MP_QSTR_set_framebuffers),            MP_ROM_PTR(&py_omv_csi_set_framebuffers_obj) },
    { MP_ROM_QSTR(MP_QSTR_get_framebuffers),            MP_ROM_PTR(&py_omv_csi_get_framebuffers_obj) },
    { MP_ROM_QSTR(MP_QSTR_disable_delays),              MP_ROM_PTR(&py_omv_csi_disable_delays_obj) },
    { MP_ROM_QSTR(MP_QSTR_disable_full_flush),          MP_ROM_PTR(&py_omv_csi_disable_full_flush_obj) },
    { MP_ROM_QSTR(MP_QSTR_set_special_effect),          MP_ROM_PTR(&py_omv_csi_set_special_effect_obj) },
    { MP_ROM_QSTR(MP_QSTR_set_lens_correction),         MP_ROM_PTR(&py_omv_csi_set_lens_correction_obj) },
    { MP_ROM_QSTR(MP_QSTR_set_vsync_callback),          MP_ROM_PTR(&py_omv_csi_set_vsync_callback_obj) },
    { MP_ROM_QSTR(MP_QSTR_set_frame_callback),          MP_ROM_PTR(&py_omv_csi_set_frame_callback_obj) },
    { MP_ROM_QSTR(MP_QSTR_ioctl),                       MP_ROM_PTR(&py_omv_csi_ioctl_obj) },
    { MP_ROM_QSTR(MP_QSTR_set_color_palette),           MP_ROM_PTR(&py_omv_csi_set_color_palette_obj) },
    { MP_ROM_QSTR(MP_QSTR_get_color_palette),           MP_ROM_PTR(&py_omv_csi_get_color_palette_obj) },
    { MP_ROM_QSTR(MP_QSTR___write_reg),                 MP_ROM_PTR(&py_omv_csi_write_reg_obj) },
    { MP_ROM_QSTR(MP_QSTR___read_reg),                  MP_ROM_PTR(&py_omv_csi_read_reg_obj) },
};
static MP_DEFINE_CONST_DICT(globals_dict, globals_dict_table);

const mp_obj_module_t omv_csi_module = {
    .base = { &mp_type_module },
    .globals = (mp_obj_t) &globals_dict,
};

MP_REGISTER_MODULE(MP_QSTR_csi, omv_csi_module);
MP_REGISTER_MODULE(MP_QSTR_sensor, omv_csi_module);
#endif // MICROPY_PY_CSI
