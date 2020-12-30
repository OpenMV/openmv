/*
 * This file is part of the OpenMV project.
 *
 * Copyright (c) 2013-2019 Ibrahim Abdelkader <iabdalkader@openmv.io>
 * Copyright (c) 2013-2019 Kwabena W. Agyeman <kwagyeman@openmv.io>
 *
 * This work is licensed under the MIT license, see the file LICENSE for details.
 *
 * FIR Python module.
 */
#include <stdbool.h>
#include "py/obj.h"
#include "py/nlr.h"
#include "py/gc.h"
#include "py/mphal.h"

#include "cambus.h"
#include "MLX90640_API.h"
#include "MLX90640_I2C_Driver.h"

#include "MLX90621_API.h"
#include "MLX90621_I2C_Driver.h"

#include "omv_boardconfig.h"
#include "framebuffer.h"
#include "py_helper.h"
#include "py_assert.h"
#include "py_image.h"
#include "py_fir.h"

#define MLX90640_ADDR       0x33
#define AMG8833_ADDR        0xD2

static uint8_t width = 0;
static uint8_t height = 0;
static uint8_t IR_refresh_rate = 0;
static uint8_t ADC_resolution = 0;
static void *mlx_data= NULL;

static cambus_t bus;

static enum {
    FIR_NONE,
    MLX90621,
    FIR_MLX90640,
    FIR_AMG8833
} fir_sensor = FIR_NONE;

// Grayscale to RGB565 conversion
extern const uint16_t rainbow_table[256];

static void test_ack(int ret)
{
    PY_ASSERT_TRUE_MSG(ret == 0, "I2C Bus communication error - missing ACK!");
}

// img->w == data_w && img->h == data_h && img->bpp == IMAGE_BPP_GRAYSCALE
static void fir_fill_image_float_obj(image_t *img, mp_obj_t *data, float min, float max)
{
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
            if (raw < min) raw = min;
            if (raw > max) raw = max;
            int pixel = fast_roundf((raw - min) * diff);
            row_pointer[x] = __USAT(pixel, 8);
        }
    }
}

static mp_obj_t py_fir_deinit()
{
    width = 0;
    height = 0;
    ADC_resolution = 0;
    IR_refresh_rate = 0;
    if (mlx_data != NULL) {
        mlx_data = NULL;
    }
    if (fir_sensor != FIR_NONE) {
        fir_sensor = FIR_NONE;
        cambus_deinit(&bus);
    }
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_0(py_fir_deinit_obj, py_fir_deinit);

/*
Allows the refresh rate to be set in the range 1Hz and 512Hz, in powers of 2. (64Hz default)
The MLX90621 sensor is capable of a larger range but these extreme values are probably not useful with OpenMV.

Allows the ADC precision to be set in the range of 15 to 18 bits. (18 bit default).
Lower ADC precision allows a large maximum temperature within the scene without sensor overflow.
ADC 18-bits: max scene temperature ~450C, 15-bits: max scene temperature ~950C

calling:
 fir.init()
 fir.init(fir_sensor=1, refresh=64, resolution=18)
*/
mp_obj_t py_fir_init(uint n_args, const mp_obj_t *args, mp_map_t *kw_args)
{
    py_fir_deinit();
    bool first_init = true;
    switch (py_helper_keyword_int(n_args, args, 0, kw_args, MP_OBJ_NEW_QSTR(MP_QSTR_type), MLX90621)) {
        case FIR_NONE: {
            return mp_const_none;
        }

        case MLX90621: {
            FIR_MLX90621:
            width = 16;
            height = 4;
            fir_sensor = MLX90621;
            MLX90621_I2CInit(&bus);
            // The EEPROM must be read at <= 400KHz.
            cambus_init(&bus, FIR_I2C_ID, CAMBUS_SPEED_FULL);

            // parse refresh rate and ADC resolution
            IR_refresh_rate = py_helper_keyword_int(n_args, args, 1, kw_args, MP_OBJ_NEW_QSTR(MP_QSTR_refresh), 64);     // 64Hz
            ADC_resolution  = py_helper_keyword_int(n_args, args, 2, kw_args, MP_OBJ_NEW_QSTR(MP_QSTR_resolution), 18);  // 18-bits

            // sanitize values
            ADC_resolution  = ((ADC_resolution > 18) ? 18 : (ADC_resolution < 15) ? 15 : ADC_resolution) - 15;
            IR_refresh_rate = 14 - __CLZ(__RBIT((IR_refresh_rate > 512) ? 512 : (IR_refresh_rate < 1) ? 1 : IR_refresh_rate));

            mlx_data = xalloc(sizeof(paramsMLX90621));

            fb_alloc_mark();
            uint8_t *eeprom = fb_alloc0(256 * sizeof(uint8_t), FB_ALLOC_NO_HINT);
            int error = 0;
            error |= MLX90621_DumpEE(eeprom);
            error |= MLX90621_Configure(eeprom);
            error |= MLX90621_SetResolution(ADC_resolution);
            error |= MLX90621_SetRefreshRate(IR_refresh_rate);
            error |= MLX90621_ExtractParameters(eeprom, mlx_data);
            fb_alloc_free_till_mark();

            if (error != 0 && first_init == true) {
                first_init = false;
                cambus_pulse_scl(&bus);
                xfree(mlx_data);
                mlx_data = NULL;
                goto FIR_MLX90621;
            }

            // Switch to FAST speed
            cambus_deinit(&bus);
            cambus_init(&bus, FIR_I2C_ID, CAMBUS_SPEED_FAST);

            PY_ASSERT_TRUE_MSG(error == 0, "Failed to init the MLX90621!");
            return mp_const_none;
        }

        case FIR_MLX90640: {
            FIR_MLX90640:
            width = 32;
            height = 24;
            fir_sensor = FIR_MLX90640;
            MLX90640_I2CInit(&bus);
            // The EEPROM must be read at <= 400KHz.
            cambus_init(&bus, FIR_I2C_ID, CAMBUS_SPEED_FULL);

            // parse refresh rate and ADC resolution
            IR_refresh_rate = py_helper_keyword_int(n_args, args, 1, kw_args, MP_OBJ_NEW_QSTR(MP_QSTR_refresh), 32);     // 32Hz
            ADC_resolution  = py_helper_keyword_int(n_args, args, 2, kw_args, MP_OBJ_NEW_QSTR(MP_QSTR_resolution), 19);  // 19-bits

            // sanitize values
            ADC_resolution  = ((ADC_resolution > 19) ? 19 : (ADC_resolution < 16) ? 16 : ADC_resolution) - 16;
            IR_refresh_rate = __CLZ(__RBIT((IR_refresh_rate > 64) ? 64 : (IR_refresh_rate < 1) ? 1 : IR_refresh_rate)) + 1;

            mlx_data = xalloc(sizeof(paramsMLX90640));

            int error = 0;
            error |= MLX90640_SetResolution(MLX90640_ADDR, ADC_resolution);
            error |= MLX90640_SetRefreshRate(MLX90640_ADDR, IR_refresh_rate);

            fb_alloc_mark();
            uint16_t *eeprom = fb_alloc(832 * sizeof(uint16_t), FB_ALLOC_NO_HINT);
            error |= MLX90640_DumpEE(MLX90640_ADDR, eeprom);
            error |= MLX90640_ExtractParameters(eeprom, mlx_data);
            fb_alloc_free_till_mark();

            if (error != 0 && first_init == true) {
                first_init = false;
                cambus_pulse_scl(&bus);
                xfree(mlx_data);
                mlx_data = NULL;
                goto FIR_MLX90640;
            }

            // Switch to FAST speed
            cambus_deinit(&bus);
            cambus_init(&bus, FIR_I2C_ID, CAMBUS_SPEED_FAST);

            PY_ASSERT_TRUE_MSG(error == 0, "Failed to init the MLX90640!");
            return mp_const_none;
        }

        case FIR_AMG8833: {
            FIR_AMG8833:
            width = 8;
            height = 8;
            fir_sensor = FIR_AMG8833;
            cambus_init(&bus, FIR_I2C_ID, CAMBUS_SPEED_STANDARD);

            IR_refresh_rate = 10;
            ADC_resolution  = 12;

            int error = cambus_write_bytes(&bus, AMG8833_ADDR, 0x01, (uint8_t [1]){0x3F}, 1);
            if (error != 0 && first_init == true) {
                first_init = false;
                cambus_pulse_scl(&bus);
                goto FIR_AMG8833;
            }

            PY_ASSERT_TRUE_MSG(error == 0, "Failed to init the AMG8833!");
            return mp_const_none;
        }
    }
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_KW(py_fir_init_obj, 0, py_fir_init);

static mp_obj_t py_fir_width()
{
    if (fir_sensor == FIR_NONE) return mp_const_none;
    return mp_obj_new_int(width);
}
STATIC MP_DEFINE_CONST_FUN_OBJ_0(py_fir_width_obj, py_fir_width);

static mp_obj_t py_fir_height()
{
    if (fir_sensor == FIR_NONE) return mp_const_none;
    return mp_obj_new_int(height);
}
STATIC MP_DEFINE_CONST_FUN_OBJ_0(py_fir_height_obj, py_fir_height);

static mp_obj_t py_fir_type()
{
    if (fir_sensor == FIR_NONE) return mp_const_none;
    return mp_obj_new_int(fir_sensor);
}
STATIC MP_DEFINE_CONST_FUN_OBJ_0(py_fir_type_obj, py_fir_type);

static mp_obj_t py_fir_refresh()
{
    const int mlx_90621_refresh_rates[16] = {512, 512, 512, 512, 512, 512, 256, 128, 64, 32, 16, 8, 4, 2, 1, 0};
    const int mlx_90640_refresh_rates[8] = {0, 1, 2, 4, 8, 16, 32, 64};
    if (fir_sensor == FIR_NONE) return mp_const_none;
    if (fir_sensor == MLX90621) return mp_obj_new_int(mlx_90621_refresh_rates[IR_refresh_rate]);
    if (fir_sensor == FIR_MLX90640) return mp_obj_new_int(mlx_90640_refresh_rates[IR_refresh_rate]);
    if (fir_sensor == FIR_AMG8833) return mp_obj_new_int(IR_refresh_rate);
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_0(py_fir_refresh_obj, py_fir_refresh);

static mp_obj_t py_fir_resolution()
{
    if (fir_sensor == FIR_NONE) return mp_const_none;
    if (fir_sensor == MLX90621) return mp_obj_new_int(ADC_resolution + 15);
    if (fir_sensor == FIR_MLX90640) return mp_obj_new_int(ADC_resolution + 16);
    if (fir_sensor == FIR_AMG8833) return mp_obj_new_int(ADC_resolution);
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_0(py_fir_resolution_obj, py_fir_resolution);

mp_obj_t py_fir_read_ta()
{
    switch(fir_sensor) {
        case FIR_NONE:
            return mp_const_none;

        case MLX90621: {
            fb_alloc_mark();
            uint16_t *data = fb_alloc0(66 * sizeof(uint16_t), FB_ALLOC_NO_HINT);
            PY_ASSERT_TRUE_MSG(MLX90621_GetFrameData(data) >= 0,
                               "Failed to read the MLX90640 sensor data!");
            mp_obj_t result = mp_obj_new_float(MLX90621_GetTa(data, mlx_data));
            fb_alloc_free_till_mark();
            return result;
        }
        case FIR_MLX90640: {
            fb_alloc_mark();
            uint16_t *data = fb_alloc(834 * sizeof(uint16_t), FB_ALLOC_NO_HINT);
            PY_ASSERT_TRUE_MSG(MLX90640_GetFrameData(MLX90640_ADDR, data) >= 0,
                               "Failed to read the MLX90640 sensor data!");
            mp_obj_t result = mp_obj_new_float(MLX90640_GetTa(data, mlx_data));
            fb_alloc_free_till_mark();
            return result;
        }

        case FIR_AMG8833: {
            int16_t temp=0;
            test_ack(cambus_read_bytes(&bus, AMG8833_ADDR, 0x0E, (uint8_t *) &temp, 2));
            if ((temp >> 11) & 1) temp |= 1 << 15;
            temp &= 0x87FF;
            return mp_obj_new_float(temp * 0.0625);
        }
    }
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_0(py_fir_read_ta_obj, py_fir_read_ta);

mp_obj_t py_fir_read_ir()
{
    switch(fir_sensor) {
        case FIR_NONE: {
            return mp_const_none;
        }

        case MLX90621: {
            fb_alloc_mark();
            uint16_t *data = fb_alloc0(66 * sizeof(uint16_t), FB_ALLOC_NO_HINT);
            PY_ASSERT_TRUE_MSG(MLX90621_GetFrameData(data) >= 0,
                               "Failed to read the MLX90621 sensor data!");
            float Ta = MLX90621_GetTa(data, mlx_data);
            float *To = fb_alloc0(64 * sizeof(float), FB_ALLOC_NO_HINT);
            MLX90621_CalculateTo(data, mlx_data, 0.95, Ta - 8, To);
            float min = FLT_MAX, max = FLT_MIN;

            for (int i=0; i<64; i++) {
                min = IM_MIN(min, To[i]);
                max = IM_MAX(max, To[i]);
            }

            mp_obj_t tuple[4];
            tuple[0] = mp_obj_new_float(Ta);
            tuple[1] = mp_obj_new_list(0, NULL);
            tuple[2] = mp_obj_new_float(min);
            tuple[3] = mp_obj_new_float(max);

            for (int i=0; i<4; i++) {
                for (int j=0; j<16; j++) {
                    mp_obj_list_append(tuple[1], mp_obj_new_float(To[((15-j)*4)+i]));
                }
            }

            fb_alloc_free_till_mark();
            return mp_obj_new_tuple(4, tuple);
        }

        case FIR_MLX90640: {
            fb_alloc_mark();
            uint16_t *data = fb_alloc(834 * sizeof(uint16_t), FB_ALLOC_NO_HINT);
            // Calculate 1st sub-frame...
            PY_ASSERT_TRUE_MSG(MLX90640_GetFrameData(MLX90640_ADDR, data) >= 0,
                               "Failed to read the MLX90640 sensor data!");
            float Ta = MLX90640_GetTa(data, mlx_data);
            float *To = fb_alloc0(768 * sizeof(float), FB_ALLOC_NO_HINT);
            MLX90640_CalculateTo(data, mlx_data, 0.95, Ta - 8, To);
            // Calculate 2nd sub-frame...
            PY_ASSERT_TRUE_MSG(MLX90640_GetFrameData(MLX90640_ADDR, data) >= 0,
                               "Failed to read the MLX90640 sensor data!");
            Ta = MLX90640_GetTa(data, mlx_data);
            MLX90640_CalculateTo(data, mlx_data, 0.95, Ta - 8, To);
            float min = FLT_MAX, max = FLT_MIN;

            for (int i=0; i<768; i++) {
                min = IM_MIN(min, To[i]);
                max = IM_MAX(max, To[i]);
            }

            mp_obj_t tuple[4];
            tuple[0] = mp_obj_new_float(Ta);
            tuple[1] = mp_obj_new_list(0, NULL);
            tuple[2] = mp_obj_new_float(min);
            tuple[3] = mp_obj_new_float(max);

            for (int i=0; i<24; i++) {
                for (int j=0; j<32; j++) {
                    mp_obj_list_append(tuple[1], mp_obj_new_float(To[(i*32)+(31-j)]));
                }
            }

            fb_alloc_free_till_mark();
            return mp_obj_new_tuple(4, tuple);
        }

        case FIR_AMG8833: {
            int16_t temp;
            test_ack(cambus_read_bytes(&bus, AMG8833_ADDR, 0x0E, (uint8_t *) &temp, 2));
            if ((temp >> 11) & 1) temp |= 1 << 15;
            temp &= 0x87FF;
            float Ta = temp * 0.0625;

            fb_alloc_mark();
            int16_t *data = fb_alloc(64 * sizeof(int16_t), FB_ALLOC_NO_HINT);
            test_ack(cambus_read_bytes(&bus, AMG8833_ADDR, 0x80, (uint8_t *) data, 128));
            float To[64], min = FLT_MAX, max = FLT_MIN;
            for (int i = 0; i < 64; i++) {
                if ((data[i] >> 11) & 1) data[i] |= 1 << 15;
                data[i] &= 0x87FF;
                To[i] = data[i] * 0.25;
                min = IM_MIN(min, To[i]);
                max = IM_MAX(max, To[i]);
            }

            mp_obj_t tuple[4];
            tuple[0] = mp_obj_new_float(Ta);
            tuple[1] = mp_obj_new_list(0, NULL);
            tuple[2] = mp_obj_new_float(min);
            tuple[3] = mp_obj_new_float(max);

            for (int i=0; i<8; i++) {
                for (int j=0; j<8; j++) {
                    mp_obj_list_append(tuple[1], mp_obj_new_float(To[((7-j)*8)+i]));
                }
            }

            fb_alloc_free_till_mark();
            return mp_obj_new_tuple(4, tuple);
        }
    }
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_0(py_fir_read_ir_obj,  py_fir_read_ir);

mp_obj_t py_fir_draw_ir(uint n_args, const mp_obj_t *args, mp_map_t *kw_args)
{
    image_t *dst_img = py_helper_arg_to_image_mutable(args[0]);

    image_t src_img;
    src_img.bpp = IMAGE_BPP_GRAYSCALE;

    size_t len;
    mp_obj_t *items, *arg_to;
    mp_obj_get_array(args[1], &len, &items);

    if (len == 3) {
        src_img.w = mp_obj_get_int(items[0]);
        src_img.h = mp_obj_get_int(items[1]);
        mp_obj_get_array_fixed_n(items[2], src_img.w * src_img.h, &arg_to);
    } else if (fir_sensor != FIR_NONE) {
        src_img.w = width;
        src_img.h = height;
        // Handle if the user passed an array of the array.
        if (len == 1) mp_obj_get_array_fixed_n(*items, src_img.w * src_img.h, &arg_to);
        else mp_obj_get_array_fixed_n(args[1], src_img.w * src_img.h, &arg_to);
    } else {
        nlr_raise(mp_obj_new_exception_msg(&mp_type_TypeError, "Invalid IR array!"));
    }

    int arg_x_off = 0;
    int arg_y_off = 0;
    uint offset = 2;
    if (n_args > 2) {
        if (MP_OBJ_IS_TYPE(args[2], &mp_type_tuple) || MP_OBJ_IS_TYPE(args[2], &mp_type_list)) {
            mp_obj_t *arg_vec;
            mp_obj_get_array_fixed_n(args[2], 2, &arg_vec);
            arg_x_off = mp_obj_get_int(arg_vec[0]);
            arg_y_off = mp_obj_get_int(arg_vec[1]);
            offset = 3;
        } else if (n_args > 3) {
            arg_x_off = mp_obj_get_int(args[2]);
            arg_y_off = mp_obj_get_int(args[3]);
            offset = 4;
        } else if (n_args > 2) {
            nlr_raise(mp_obj_new_exception_msg(&mp_type_TypeError, "Expected x and y offset!"));
        }
    }

    float arg_x_scale = 1.f;
    bool got_x_scale = py_helper_keyword_float_maybe(n_args, args, offset + 0, kw_args, MP_OBJ_NEW_QSTR(MP_QSTR_x_scale), &arg_x_scale);

    float arg_y_scale = 1.f;
    bool got_y_scale = py_helper_keyword_float_maybe(n_args, args, offset + 1, kw_args, MP_OBJ_NEW_QSTR(MP_QSTR_y_scale), &arg_y_scale);

    rectangle_t arg_roi;
    py_helper_keyword_rectangle_roi(&src_img, n_args, args, offset + 2, kw_args, &arg_roi);

    float tmp_x_scale = dst_img->w / ((float) arg_roi.w);
    float tmp_y_scale = dst_img->h / ((float) arg_roi.h);
    float tmp_scale = IM_MIN(tmp_x_scale, tmp_y_scale);

    if (n_args == 2) {
        arg_x_off = fast_floorf((dst_img->w - (arg_roi.w * tmp_scale)) / 2.f);
        arg_y_off = fast_floorf((dst_img->h - (arg_roi.h * tmp_scale)) / 2.f);
    }

    if (!got_x_scale) arg_x_scale = tmp_scale;
    if (!got_y_scale) arg_y_scale = tmp_scale;

    int arg_rgb_channel = py_helper_keyword_int(n_args, args, offset + 3, kw_args, MP_OBJ_NEW_QSTR(MP_QSTR_rgb_channel), -1);
    if ((arg_rgb_channel < -1) || (2 < arg_rgb_channel)) nlr_raise(mp_obj_new_exception_msg(&mp_type_ValueError, "-1 <= rgb_channel <= 2!"));

    int arg_alpha = py_helper_keyword_int(n_args, args, offset + 4, kw_args, MP_OBJ_NEW_QSTR(MP_QSTR_alpha), 128);
    if ((arg_alpha < 0) || (256 < arg_alpha)) nlr_raise(mp_obj_new_exception_msg(&mp_type_ValueError, "0 <= alpha <= 256!"));

    const uint16_t *color_palette = rainbow_table;
    {
        int palette;

        uint arg_index = offset + 5;
        mp_map_elem_t *kw_arg = mp_map_lookup(kw_args, MP_OBJ_NEW_QSTR(MP_QSTR_color_palette), MP_MAP_LOOKUP);

        if (kw_arg && MP_OBJ_IS_TYPE(kw_arg->value, mp_const_none)) {
            color_palette = NULL;
        } else if ((n_args > arg_index) && MP_OBJ_IS_TYPE(args[arg_index], mp_const_none)) {
            color_palette = NULL;
        } else if (py_helper_keyword_int_maybe(n_args, args, arg_index, kw_args, MP_OBJ_NEW_QSTR(MP_QSTR_color_palette), &palette)) {
            if (palette == COLOR_PALETTE_RAINBOW) color_palette = rainbow_table;
            else if (palette == COLOR_PALETTE_IRONBOW) color_palette = ironbow_table;
            else nlr_raise(mp_obj_new_exception_msg(&mp_type_ValueError, "Invalid pre-defined color palette!"));
        } else {
            image_t *arg_color_palette = py_helper_keyword_to_image_mutable_color_palette(n_args, args, arg_index, kw_args);

            if (arg_color_palette) {
                if (arg_color_palette->bpp != IMAGE_BPP_RGB565) nlr_raise(mp_obj_new_exception_msg(&mp_type_ValueError, "Color palette must be RGB565!"));
                if ((arg_color_palette->w * arg_color_palette->h) != 256) nlr_raise(mp_obj_new_exception_msg(&mp_type_ValueError, "Color palette must be 256 pixels!"));
                color_palette = (uint16_t *) arg_color_palette->data;
            }
        }
    }

    const uint8_t *alpha_palette = NULL;
    {
        image_t *arg_alpha_palette = py_helper_keyword_to_image_mutable_alpha_palette(n_args, args, offset + 6, kw_args);

        if (arg_alpha_palette) {
            if (arg_alpha_palette->bpp != IMAGE_BPP_GRAYSCALE) nlr_raise(mp_obj_new_exception_msg(&mp_type_ValueError, "Alpha palette must be GRAYSCALE!"));
            if ((arg_alpha_palette->w * arg_alpha_palette->h) != 256) nlr_raise(mp_obj_new_exception_msg(&mp_type_ValueError, "Alpha palette must be 256 pixels!"));
            alpha_palette = (uint8_t *) arg_alpha_palette->data;
        }
    }

    image_hint_t hint = py_helper_keyword_int(n_args, args, offset + 7, kw_args, MP_OBJ_NEW_QSTR(MP_QSTR_hint), 0);

    int arg_x_size;
    bool got_x_size = py_helper_keyword_int_maybe(n_args, args, offset + 8, kw_args, MP_OBJ_NEW_QSTR(MP_QSTR_x_size), &arg_x_size);

    int arg_y_size;
    bool got_y_size = py_helper_keyword_int_maybe(n_args, args, offset + 9, kw_args, MP_OBJ_NEW_QSTR(MP_QSTR_y_size), &arg_y_size);

    if (got_x_scale && got_x_size) nlr_raise(mp_obj_new_exception_msg(&mp_type_ValueError, "Choose either x_scale or x_size not both!"));
    if (got_y_scale && got_y_size) nlr_raise(mp_obj_new_exception_msg(&mp_type_ValueError, "Choose either y_scale or y_size not both!"));

    if (got_x_size) arg_x_scale = arg_x_size / ((float) arg_roi.w);
    if (got_y_size) arg_y_scale = arg_y_size / ((float) arg_roi.h);

    if ((!got_x_scale) && (!got_x_size) && got_y_size) arg_x_scale = arg_y_scale;
    if ((!got_y_scale) && (!got_y_size) && got_x_size) arg_y_scale = arg_x_scale;

    mp_obj_t scale_obj = py_helper_keyword_object(n_args, args, offset + 10, kw_args, MP_OBJ_NEW_QSTR(MP_QSTR_scale), NULL);
    float min = FLT_MAX, max = FLT_MIN;

    if (scale_obj) {
        mp_obj_t *arg_scale;
        mp_obj_get_array_fixed_n(scale_obj, 2, &arg_scale);
        min = mp_obj_get_float(arg_scale[0]);
        max = mp_obj_get_float(arg_scale[1]);
    } else {
        for (int i = 0, ii = src_img.w * src_img.h; i < ii; i++) {
            float temp = mp_obj_get_float(arg_to[i]);
            if (temp < min) min = temp;
            if (temp > max) max = temp;
        }
    }

    fb_alloc_mark();

    src_img.data = fb_alloc(src_img.w * src_img.h * sizeof(uint8_t), FB_ALLOC_NO_HINT);
    fir_fill_image_float_obj(&src_img, arg_to, min, max);

    imlib_draw_image(dst_img, &src_img, arg_x_off, arg_y_off, arg_x_scale, arg_y_scale, &arg_roi,
                     arg_rgb_channel, arg_alpha, color_palette, alpha_palette, hint, NULL, NULL);

    fb_alloc_free_till_mark();

    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_KW(py_fir_draw_ir_obj, 2, py_fir_draw_ir);

mp_obj_t py_fir_snapshot(uint n_args, const mp_obj_t *args, mp_map_t *kw_args)
{
    if (fir_sensor == FIR_NONE) return mp_const_none;
    mp_obj_t ir = py_fir_read_ir();
    size_t len;
    mp_obj_t *items;
    mp_obj_tuple_get(ir, &len, &items);

    int pixformat = py_helper_keyword_int(n_args, args, 2, kw_args, MP_OBJ_NEW_QSTR(MP_QSTR_pixformat), PIXFORMAT_RGB565);
    PY_ASSERT_TRUE_MSG((pixformat == PIXFORMAT_GRAYSCALE) || (pixformat == PIXFORMAT_RGB565), "Invalid Pixformat!");

    mp_obj_t copy_to_fb_obj = py_helper_keyword_object(n_args, args, 3, kw_args, MP_OBJ_NEW_QSTR(MP_QSTR_copy_to_fb), NULL);
    bool copy_to_fb = false;
    image_t *arg_other = NULL;

    if (copy_to_fb_obj) {
        if (mp_obj_is_integer(copy_to_fb_obj)) {
            copy_to_fb = mp_obj_get_int(copy_to_fb_obj);
        } else {
            arg_other = py_helper_arg_to_image_mutable(copy_to_fb_obj);
        }
    }

    if (copy_to_fb) {
        fb_update_jpeg_buffer();
    }

    image_t image;
    image.w = width;
    image.h = height;
    image.bpp = (pixformat == PIXFORMAT_RGB565) ? IMAGE_BPP_RGB565 : IMAGE_BPP_GRAYSCALE;
    image.data = NULL;

    if (copy_to_fb) {
        py_helper_set_to_framebuffer(&image);
    } else if (arg_other) {
        PY_ASSERT_TRUE_MSG((image_size(&image) <= image_size(arg_other)), "The new image won't fit in the target frame buffer!");
        image.data = arg_other->data;
    } else {
        image.data = xalloc(image_size(&image));
    }

    // Zero the image we are about to draw on.
    memset(image.data, 0, image_size(&image));

    py_helper_update_framebuffer(&image);

    if (arg_other) {
        arg_other->w = image.w;
        arg_other->h = image.h;
        arg_other->bpp = image.bpp;
    }

    mp_obj_t snapshot = py_image_from_struct(&image);

    mp_obj_t *new_args = xalloc((2 + n_args) * sizeof(mp_obj_t));
    new_args[0] = snapshot;
    new_args[1] = items[1]; // ir array

    for (uint i = 0; i < n_args; i++) {
        new_args[2+i] = args[i];
    }

    py_fir_draw_ir(2 + n_args, new_args, kw_args);
    gc_collect();

    return snapshot;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_KW(py_fir_snapshot_obj, 0, py_fir_snapshot);

STATIC const mp_rom_map_elem_t globals_dict_table[] = {
    { MP_ROM_QSTR(MP_QSTR___name__),        MP_OBJ_NEW_QSTR(MP_QSTR_fir) },
    { MP_ROM_QSTR(MP_QSTR_FIR_NONE),        MP_ROM_INT(FIR_NONE) },
    { MP_ROM_QSTR(MP_QSTR_FIR_SHIELD),      MP_ROM_INT(MLX90621) },
    { MP_ROM_QSTR(MP_QSTR_FIR_MLX90620),    MP_ROM_INT(MLX90621) },
    { MP_ROM_QSTR(MP_QSTR_FIR_MLX90621),    MP_ROM_INT(MLX90621) },
    { MP_ROM_QSTR(MP_QSTR_FIR_MLX90640),    MP_ROM_INT(FIR_MLX90640) },
    { MP_ROM_QSTR(MP_QSTR_FIR_AMG8833),     MP_ROM_INT(FIR_AMG8833) },
    { MP_ROM_QSTR(MP_QSTR_init),            MP_ROM_PTR(&py_fir_init_obj) },
    { MP_ROM_QSTR(MP_QSTR_deinit),          MP_ROM_PTR(&py_fir_deinit_obj) },
    { MP_ROM_QSTR(MP_QSTR_width),           MP_ROM_PTR(&py_fir_width_obj) },
    { MP_ROM_QSTR(MP_QSTR_height),          MP_ROM_PTR(&py_fir_height_obj) },
    { MP_ROM_QSTR(MP_QSTR_type),            MP_ROM_PTR(&py_fir_type_obj) },
    { MP_ROM_QSTR(MP_QSTR_refresh),         MP_ROM_PTR(&py_fir_refresh_obj) },
    { MP_ROM_QSTR(MP_QSTR_resolution),      MP_ROM_PTR(&py_fir_resolution_obj) },
    { MP_ROM_QSTR(MP_QSTR_read_ta),         MP_ROM_PTR(&py_fir_read_ta_obj) },
    { MP_ROM_QSTR(MP_QSTR_read_ir),         MP_ROM_PTR(&py_fir_read_ir_obj) },
    { MP_ROM_QSTR(MP_QSTR_draw_ir),         MP_ROM_PTR(&py_fir_draw_ir_obj) },
    { MP_ROM_QSTR(MP_QSTR_snapshot),        MP_ROM_PTR(&py_fir_snapshot_obj) }
};

STATIC MP_DEFINE_CONST_DICT(globals_dict, globals_dict_table);

const mp_obj_module_t fir_module = {
    .base = { &mp_type_module },
    .globals = (mp_obj_t) &globals_dict,
};

void py_fir_init0()
{
    py_fir_deinit();
}
