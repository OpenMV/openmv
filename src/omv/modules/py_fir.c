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
 * FIR Python module.
 */
#include "py/runtime.h"
#include "py/objlist.h"
#include "omv_boardconfig.h"

#if OMV_FIR_MLX90621_ENABLE || \
    OMV_FIR_MLX90640_ENABLE || \
    OMV_FIR_MLX90641_ENABLE || \
    OMV_FIR_AMG8833_ENABLE ||  \
    OMV_FIR_LEPTON_ENABLE
#include "omv_i2c.h"
#if (OMV_FIR_MLX90621_ENABLE == 1)
#include "MLX90621_API.h"
#include "MLX90621_I2C_Driver.h"
#endif
#if (OMV_FIR_MLX90640_ENABLE == 1)
#include "MLX90640_API.h"
#include "MLX90640_I2C_Driver.h"
#endif
#if (OMV_FIR_MLX90641_ENABLE == 1)
#include "MLX90641_API.h"
#include "MLX90641_I2C_Driver.h"
#endif
#include "framebuffer.h"

#include "py_assert.h"
#include "py_helper.h"
#include "py_image.h"

#if (OMV_FIR_LEPTON_ENABLE == 1)
#include "py_fir_lepton.h"
#endif

#define MLX90621_ADDR                   0x50
#define MLX90621_WIDTH                  16
#define MLX90621_HEIGHT                 4
#define MLX90621_EEPROM_DATA_SIZE       256
#define MLX90621_FRAME_DATA_SIZE        66

#define MLX90640_ADDR                   0x33
#define MLX90640_WIDTH                  32
#define MLX90640_HEIGHT                 24
#define MLX90640_EEPROM_DATA_SIZE       832
#define MLX90640_FRAME_DATA_SIZE        834

#define MLX90641_ADDR                   0x33
#define MLX90641_WIDTH                  16
#define MLX90641_HEIGHT                 12
#define MLX90641_EEPROM_DATA_SIZE       832
#define MLX90641_FRAME_DATA_SIZE        242

#define AMG8833_ADDR                    0xD2
#define AMG8833_WIDTH                   8
#define AMG8833_HEIGHT                  8
#define AMG8833_RESET_REGISTER          0x01
#define AMG8833_THERMISTOR_REGISTER     0x0E
#define AMG8833_TEMPERATURE_REGISTER    0x80
#define AMG8833_INITIAL_RESET_VALUE     0x3F

#define LEPTON_ADDR                     0x54

#define AMG8833_12_TO_16(value)               \
    ({                                        \
        __typeof__ (value) __value = (value); \
        if ((__value >> 11) & 1) {            \
            __value |= 1 << 15;               \
        }                                     \
        __value & 0x87FF;                     \
    })

static omv_i2c_t fir_bus = {};

typedef enum fir_sensor_type {
    FIR_NONE,
    #if (OMV_FIR_MLX90621_ENABLE == 1)
    FIR_MLX90621,
    #endif
    #if (OMV_FIR_MLX90640_ENABLE == 1)
    FIR_MLX90640,
    #endif
    #if (OMV_FIR_MLX90641_ENABLE == 1)
    FIR_MLX90641,
    #endif
    #if (OMV_FIR_AMG8833_ENABLE == 1)
    FIR_AMG8833,
    #endif
    #if (OMV_FIR_LEPTON_ENABLE == 1)
    FIR_LEPTON
    #endif
} fir_sensor_type_t;

static int fir_width = 0;
static int fir_height = 0;
static int fir_ir_fresh_rate = 0;
static int fir_adc_resolution = 0;
static bool fir_transposed = false;
static fir_sensor_type_t fir_sensor = FIR_NONE;

// img->w == data_w && img->h == data_h && img->pixfmt == PIXFORMAT_GRAYSCALE
static void fir_fill_image_float_obj(image_t *img, mp_obj_t *data, float min, float max) {
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

#if (OMV_FIR_MLX90621_ENABLE == 1)
static void fir_MLX90621_get_frame(float *Ta, float *To) {
    uint16_t *data = fb_alloc(MLX90621_FRAME_DATA_SIZE * sizeof(uint16_t));

    PY_ASSERT_TRUE_MSG(MLX90621_GetFrameData(data) >= 0,
                       "Failed to read the MLX90621 sensor data!");
    *Ta = MLX90621_GetTa(data, MP_STATE_PORT(fir_mlx_data));
    MLX90621_CalculateTo(data, MP_STATE_PORT(fir_mlx_data), 0.95f, *Ta - 8, To);

    fb_free();
}
#endif

#if (OMV_FIR_MLX90640_ENABLE == 1)
static void fir_MLX90640_get_frame(float *Ta, float *To) {
    uint16_t *data = fb_alloc(MLX90640_FRAME_DATA_SIZE * sizeof(uint16_t));

    // Wait for a new data to be available before calling GetFrameData.
    MLX90640_SynchFrame(MLX90640_ADDR);

    // Calculate 1st sub-frame...
    PY_ASSERT_TRUE_MSG(MLX90640_GetFrameData(MLX90640_ADDR, data) >= 0,
                       "Failed to read the MLX90640 sensor data!");
    *Ta = MLX90640_GetTa(data, MP_STATE_PORT(fir_mlx_data));
    MLX90640_CalculateTo(data, MP_STATE_PORT(fir_mlx_data), 0.95f, *Ta - 8, To);

    // Calculate 2nd sub-frame...
    PY_ASSERT_TRUE_MSG(MLX90640_GetFrameData(MLX90640_ADDR, data) >= 0,
                       "Failed to read the MLX90640 sensor data!");
    *Ta = MLX90640_GetTa(data, MP_STATE_PORT(fir_mlx_data));
    MLX90640_CalculateTo(data, MP_STATE_PORT(fir_mlx_data), 0.95f, *Ta - 8, To);

    fb_free();
}
#endif

#if (OMV_FIR_MLX90641_ENABLE == 1)
static void fir_MLX90641_get_frame(float *Ta, float *To) {
    uint16_t *data = fb_alloc(MLX90641_FRAME_DATA_SIZE * sizeof(uint16_t));

    // Wait for a new data to be available before calling GetFrameData.
    MLX90641_SynchFrame(MLX90641_ADDR);

    PY_ASSERT_TRUE_MSG(MLX90641_GetFrameData(MLX90641_ADDR, data) >= 0,
                       "Failed to read the MLX90641 sensor data!");
    *Ta = MLX90641_GetTa(data, MP_STATE_PORT(fir_mlx_data));
    MLX90641_CalculateTo(data, MP_STATE_PORT(fir_mlx_data), 0.95f, *Ta - 8, To);

    fb_free();
}
#endif

#if (OMV_FIR_AMG8833_ENABLE == 1)
static void fir_AMG8833_get_frame(float *Ta, float *To) {
    int16_t temp;
    int error = 0;
    error |= omv_i2c_write_bytes(&fir_bus,
                                 AMG8833_ADDR,
                                 (uint8_t [1]) {AMG8833_THERMISTOR_REGISTER},
                                 1, OMV_I2C_XFER_NO_STOP);
    error |= omv_i2c_read_bytes(&fir_bus, AMG8833_ADDR, (uint8_t *) &temp, sizeof(temp), OMV_I2C_XFER_NO_FLAGS);
    PY_ASSERT_TRUE_MSG((error == 0), "Failed to read the AMG8833 sensor data!");

    *Ta = AMG8833_12_TO_16(temp) * 0.0625f;

    int16_t *data = fb_alloc(AMG8833_WIDTH * AMG8833_HEIGHT * sizeof(int16_t));
    error |= omv_i2c_write_bytes(&fir_bus,
                                 AMG8833_ADDR,
                                 (uint8_t [1]) {AMG8833_TEMPERATURE_REGISTER},
                                 1, OMV_I2C_XFER_NO_STOP);
    error |= omv_i2c_read_bytes(&fir_bus,
                                AMG8833_ADDR,
                                (uint8_t *) data,
                                AMG8833_WIDTH * AMG8833_HEIGHT * 2,
                                OMV_I2C_XFER_NO_FLAGS);
    PY_ASSERT_TRUE_MSG((error == 0), "Failed to read the AMG8833 sensor data!");

    for (int i = 0, ii = AMG8833_WIDTH * AMG8833_HEIGHT; i < ii; i++) {
        To[i] = AMG8833_12_TO_16(data[i]) * 0.25f;
    }

    fb_free();
}
#endif

static mp_obj_t fir_get_ir(int w, int h, float Ta, float *To, bool mirror,
                           bool flip, bool dst_transpose, bool src_transpose) {
    mp_obj_list_t *list = (mp_obj_list_t *) mp_obj_new_list(w * h, NULL);
    float min = FLT_MAX;
    float max = -FLT_MAX;

    if (!src_transpose) {
        for (int y = 0; y < h; y++) {
            int y_dst = flip ? (h - y - 1) : y;
            float *raw_row = To + (y * w);
            mp_obj_t *list_row = list->items + (y_dst * w);
            mp_obj_t *t_list_row = list->items + y_dst;

            for (int x = 0; x < w; x++) {
                int x_dst = mirror ? (w - x - 1) : x;
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
            int x_dst = mirror ? (w - x - 1) : x;
            float *raw_row = To + (x * h);
            mp_obj_t *t_list_row = list->items + (x_dst * h);
            mp_obj_t *list_row = list->items + x_dst;

            for (int y = 0; y < h; y++) {
                int y_dst = flip ? (h - y - 1) : y;
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

    mp_obj_t tuple[4];
    tuple[0] = mp_obj_new_float(Ta);
    tuple[1] = MP_OBJ_FROM_PTR(list);
    tuple[2] = mp_obj_new_float(min);
    tuple[3] = mp_obj_new_float(max);
    return mp_obj_new_tuple(4, tuple);
}

static mp_obj_t py_fir_deinit() {
    #if (OMV_FIR_LEPTON_ENABLE == 1)
    if (fir_sensor == FIR_LEPTON) {
        fir_lepton_deinit();
    }
    #endif

    if (fir_sensor != FIR_NONE) {
        omv_i2c_deinit(&fir_bus);
        fir_sensor = FIR_NONE;
    }

    #if ((OMV_FIR_MLX90621_ENABLE == 1) || (OMV_FIR_MLX90640_ENABLE == 1) || (OMV_FIR_MLX90641_ENABLE == 1))
    if (MP_STATE_PORT(fir_mlx_data) != NULL) {
        MP_STATE_PORT(fir_mlx_data) = NULL;
    }
    #endif

    fir_width = 0;
    fir_height = 0;
    fir_ir_fresh_rate = 0;
    fir_adc_resolution = 0;
    fir_transposed = false;
    return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_0(py_fir_deinit_obj, py_fir_deinit);

mp_obj_t py_fir_init(uint n_args, const mp_obj_t *pos_args, mp_map_t *kw_args) {
    enum { ARG_type, ARG_refresh, ARG_resolution };
    static const mp_arg_t allowed_args[] = {
        { MP_QSTR_type, MP_ARG_INT,  {.u_int = -1 } },
        { MP_QSTR_refresh, MP_ARG_INT | MP_ARG_KW_ONLY,  {.u_int = -1 } },
        { MP_QSTR_resolution, MP_ARG_INT | MP_ARG_KW_ONLY,  {.u_int = -1 } },
    };

    // Parse args.
    mp_arg_val_t args[MP_ARRAY_SIZE(allowed_args)];
    mp_arg_parse_all(n_args, pos_args, kw_args, MP_ARRAY_SIZE(allowed_args), allowed_args, args);

    py_fir_deinit();
    bool first_init = true;
    int type = args[ARG_type].u_int;
    if (type == -1) {
        FIR_SCAN_RETRY:
        omv_i2c_init(&fir_bus, OMV_FIR_I2C_ID, OMV_I2C_SPEED_STANDARD);
        // Scan and detect any supported sensor.
        uint8_t dev_list[10];
        int dev_size = omv_i2c_scan(&fir_bus, dev_list, sizeof(dev_list));
        for (int i = 0; i < dev_size && type == -1; i++) {
            switch (dev_list[i]) {
                #if (OMV_FIR_MLX90621_ENABLE == 1)
                case (MLX90621_ADDR << 1): {
                    type = FIR_MLX90621;
                    break;
                }
                #endif
                #if (OMV_FIR_MLX90640_ENABLE == 1)
                case (MLX90640_ADDR << 1): {
                    type = FIR_MLX90640;
                    break;
                }
                #endif
                #if (OMV_FIR_MLX90640_ENABLE == 0) \
                && (OMV_FIR_MLX90641_ENABLE == 1)
                case (MLX90641_ADDR << 1): {
                    type = FIR_MLX90641;
                    break;
                }
                #endif
                #if (OMV_FIR_AMG8833_ENABLE == 1)
                case AMG8833_ADDR: {
                    type = FIR_AMG8833;
                    break;
                }
                #endif
                #if (OMV_FIR_LEPTON_ENABLE == 1)
                case LEPTON_ADDR: {
                    type = FIR_LEPTON;
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
            omv_i2c_pulse_scl(&fir_bus);
            goto FIR_SCAN_RETRY;
        }

        omv_i2c_deinit(&fir_bus);
    }

    // Initialize the detected sensor.
    first_init = true;
    switch (type) {
        #if (OMV_FIR_MLX90621_ENABLE == 1)
        case FIR_MLX90621: {
            // Set refresh rate and ADC resolution
            uint32_t ir_fresh_rate = args[ARG_refresh].u_int != -1 ? args[ARG_refresh].u_int : 64;
            uint32_t adc_resolution = args[ARG_resolution].u_int != -1 ? args[ARG_resolution].u_int : 18;

            // sanitize values
            ir_fresh_rate = 14 - __CLZ(__RBIT((ir_fresh_rate > 512) ? 512 : ((ir_fresh_rate < 1) ? 1 : ir_fresh_rate)));
            adc_resolution = ((adc_resolution > 18) ? 18 : ((adc_resolution < 15) ? 15 : adc_resolution)) - 15;

            MP_STATE_PORT(fir_mlx_data) = xalloc(sizeof(paramsMLX90621));

            fir_sensor = FIR_MLX90621;
            FIR_MLX90621_RETRY:
            omv_i2c_init(&fir_bus, OMV_FIR_I2C_ID, OMV_I2C_SPEED_FULL); // The EEPROM must be read at <= 400KHz.
            MLX90621_I2CInit(&fir_bus);

            fb_alloc_mark();
            uint8_t *eeprom = fb_alloc(MLX90621_EEPROM_DATA_SIZE * sizeof(uint8_t));
            int error = MLX90621_DumpEE(eeprom);
            error |= MLX90621_Configure(eeprom);
            error |= MLX90621_SetRefreshRate(ir_fresh_rate);
            error |= MLX90621_SetResolution(adc_resolution);
            error |= MLX90621_ExtractParameters(eeprom, MP_STATE_PORT(fir_mlx_data));
            fb_alloc_free_till_mark();

            if (error != 0) {
                if (first_init) {
                    first_init = false;
                    omv_i2c_pulse_scl(&fir_bus);
                    goto FIR_MLX90621_RETRY;
                } else {
                    py_fir_deinit();
                    mp_raise_msg(&mp_type_ValueError, MP_ERROR_TEXT("Failed to init the MLX90621!"));
                }
            }

            // Switch to FAST speed
            omv_i2c_deinit(&fir_bus);
            omv_i2c_init(&fir_bus, OMV_FIR_I2C_ID, OMV_I2C_SPEED_FAST);
            fir_width = MLX90621_WIDTH;
            fir_height = MLX90621_HEIGHT;
            fir_ir_fresh_rate = ir_fresh_rate;
            fir_adc_resolution = adc_resolution;
            return mp_const_none;
        }
        #endif
        #if (OMV_FIR_MLX90640_ENABLE == 1)
        case FIR_MLX90640: {
            // Set refresh rate and ADC resolution
            uint32_t ir_fresh_rate = args[ARG_refresh].u_int != -1 ? args[ARG_refresh].u_int : 32;
            uint32_t adc_resolution = args[ARG_resolution].u_int != -1 ? args[ARG_resolution].u_int : 19;

            // sanitize values
            ir_fresh_rate = __CLZ(__RBIT((ir_fresh_rate > 64) ? 64 : ((ir_fresh_rate < 1) ? 1 : ir_fresh_rate))) + 1;
            adc_resolution = ((adc_resolution > 19) ? 19 : ((adc_resolution < 16) ? 16 : adc_resolution)) - 16;

            MP_STATE_PORT(fir_mlx_data) = xalloc(sizeof(paramsMLX90640));

            fir_sensor = FIR_MLX90640;
            FIR_MLX90640_RETRY:
            omv_i2c_init(&fir_bus, OMV_FIR_I2C_ID, OMV_I2C_SPEED_FULL); // The EEPROM must be read at <= 400KHz.
            MLX90640_I2CInit(&fir_bus);

            fb_alloc_mark();
            uint16_t *eeprom = fb_alloc(MLX90640_EEPROM_DATA_SIZE * sizeof(uint16_t));
            int error = MLX90640_DumpEE(MLX90640_ADDR, eeprom);
            error |= MLX90640_SetRefreshRate(MLX90640_ADDR, ir_fresh_rate);
            error |= MLX90640_SetResolution(MLX90640_ADDR, adc_resolution);
            error |= MLX90640_ExtractParameters(eeprom, MP_STATE_PORT(fir_mlx_data));
            fb_alloc_free_till_mark();

            if (error != 0) {
                if (first_init) {
                    first_init = false;
                    omv_i2c_pulse_scl(&fir_bus);
                    goto FIR_MLX90640_RETRY;
                } else {
                    py_fir_deinit();
                    mp_raise_msg(&mp_type_ValueError, MP_ERROR_TEXT("Failed to init the MLX90640!"));
                }
            }

            // Switch to FAST speed
            omv_i2c_deinit(&fir_bus);
            omv_i2c_init(&fir_bus, OMV_FIR_I2C_ID, OMV_I2C_SPEED_FAST);
            fir_width = MLX90640_WIDTH;
            fir_height = MLX90640_HEIGHT;
            fir_ir_fresh_rate = ir_fresh_rate;
            fir_adc_resolution = adc_resolution;
            return mp_const_none;
        }
        #endif
        #if (OMV_FIR_MLX90641_ENABLE == 1)
        case FIR_MLX90641: {
            // Set refresh rate and ADC resolution
            uint32_t ir_fresh_rate = args[ARG_refresh].u_int != -1 ? args[ARG_refresh].u_int : 32;
            uint32_t adc_resolution = args[ARG_resolution].u_int != -1 ? args[ARG_resolution].u_int : 19;

            // sanitize values
            ir_fresh_rate = __CLZ(__RBIT((ir_fresh_rate > 64) ? 64 : ((ir_fresh_rate < 1) ? 1 : ir_fresh_rate))) + 1;
            adc_resolution = ((adc_resolution > 19) ? 19 : ((adc_resolution < 16) ? 16 : adc_resolution)) - 16;

            MP_STATE_PORT(fir_mlx_data) = xalloc(sizeof(paramsMLX90641));

            fir_sensor = FIR_MLX90641;
            FIR_MLX90641_RETRY:
            omv_i2c_init(&fir_bus, OMV_FIR_I2C_ID, OMV_I2C_SPEED_FULL); // The EEPROM must be read at <= 400KHz.
            MLX90641_I2CInit(&fir_bus);

            fb_alloc_mark();
            uint16_t *eeprom = fb_alloc(MLX90641_EEPROM_DATA_SIZE * sizeof(uint16_t));
            int error = MLX90641_DumpEE(MLX90641_ADDR, eeprom);
            error |= MLX90641_SetRefreshRate(MLX90641_ADDR, ir_fresh_rate);
            error |= MLX90641_SetResolution(MLX90641_ADDR, adc_resolution);
            error |= MLX90641_ExtractParameters(eeprom, MP_STATE_PORT(fir_mlx_data));
            fb_alloc_free_till_mark();

            if (error != 0) {
                if (first_init) {
                    first_init = false;
                    omv_i2c_pulse_scl(&fir_bus);
                    goto FIR_MLX90641_RETRY;
                } else {
                    py_fir_deinit();
                    mp_raise_msg(&mp_type_ValueError, MP_ERROR_TEXT("Failed to init the MLX90641!"));
                }
            }

            // Switch to FAST speed
            omv_i2c_deinit(&fir_bus);
            omv_i2c_init(&fir_bus, OMV_FIR_I2C_ID, OMV_I2C_SPEED_FAST);
            fir_width = MLX90641_WIDTH;
            fir_height = MLX90641_HEIGHT;
            fir_ir_fresh_rate = ir_fresh_rate;
            fir_adc_resolution = adc_resolution;
            return mp_const_none;
        }
        #endif
        #if (OMV_FIR_AMG8833_ENABLE == 1)
        case FIR_AMG8833: {
            fir_sensor = FIR_AMG8833;
            FIR_AMG8833_RETRY:
            omv_i2c_init(&fir_bus, OMV_FIR_I2C_ID, OMV_I2C_SPEED_STANDARD);

            int error = omv_i2c_write_bytes(&fir_bus, AMG8833_ADDR,
                                            (uint8_t [2]) {AMG8833_RESET_REGISTER, AMG8833_INITIAL_RESET_VALUE}, 2, 0);
            if (error != 0) {
                if (first_init) {
                    first_init = false;
                    omv_i2c_pulse_scl(&fir_bus);
                    goto FIR_AMG8833_RETRY;
                } else {
                    py_fir_deinit();
                    mp_raise_msg(&mp_type_ValueError, MP_ERROR_TEXT("Failed to init the AMG8833!"));
                }
            }

            fir_width = AMG8833_WIDTH;
            fir_height = AMG8833_HEIGHT;
            fir_ir_fresh_rate = 10;
            fir_adc_resolution = 12;
            return mp_const_none;
        }
        #endif
        #if (OMV_FIR_LEPTON_ENABLE == 1)
        case FIR_LEPTON: {
            fir_sensor = FIR_LEPTON;
            FIR_LEPTON_RETRY:
            omv_i2c_init(&fir_bus, OMV_FIR_LEPTON_I2C_BUS, OMV_FIR_LEPTON_I2C_BUS_SPEED);

            int error = fir_lepton_init(&fir_bus, &fir_width, &fir_height, &fir_ir_fresh_rate, &fir_adc_resolution);

            if (error != 0) {
                if (first_init) {
                    first_init = false;
                    omv_i2c_pulse_scl(&fir_bus);
                    goto FIR_LEPTON_RETRY;
                } else {
                    py_fir_deinit();
                    mp_raise_msg(&mp_type_ValueError, MP_ERROR_TEXT("Failed to init the Lepton!"));
                }
            }

            return mp_const_none;
        }
        #endif
        default: {
            mp_raise_msg(&mp_type_ValueError, MP_ERROR_TEXT("Failed to detect a supported FIR sensor."));
        }
    }

    return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_KW(py_fir_init_obj, 0, py_fir_init);

static mp_obj_t py_fir_type() {
    if (fir_sensor != FIR_NONE) {
        return mp_obj_new_int(fir_sensor);
    }
    mp_raise_msg(&mp_type_ValueError, MP_ERROR_TEXT("FIR sensor is not initialized"));
}
static MP_DEFINE_CONST_FUN_OBJ_0(py_fir_type_obj, py_fir_type);

static mp_obj_t py_fir_width() {
    if (fir_sensor != FIR_NONE) {
        return mp_obj_new_int(fir_width);
    }
    mp_raise_msg(&mp_type_ValueError, MP_ERROR_TEXT("FIR sensor is not initialized"));
}
static MP_DEFINE_CONST_FUN_OBJ_0(py_fir_width_obj, py_fir_width);

static mp_obj_t py_fir_height() {
    if (fir_sensor != FIR_NONE) {
        return mp_obj_new_int(fir_height);
    }
    mp_raise_msg(&mp_type_ValueError, MP_ERROR_TEXT("FIR sensor is not initialized"));
}
static MP_DEFINE_CONST_FUN_OBJ_0(py_fir_height_obj, py_fir_height);

static mp_obj_t py_fir_refresh() {
    #if (OMV_FIR_MLX90621_ENABLE == 1)
    const int mlx_90621_refresh_rates[16] = {512, 512, 512, 512, 512, 512, 256, 128, 64, 32, 16, 8, 4, 2, 1, 0};
    #endif
    #if (OMV_FIR_MLX90640_ENABLE == 1) || (OMV_FIR_MLX90641_ENABLE == 1)
    const int mlx_90640_1_refresh_rates[8] = {0, 1, 2, 4, 8, 16, 32, 64};
    #endif
    switch (fir_sensor) {
        #if (OMV_FIR_MLX90621_ENABLE == 1)
        case FIR_MLX90621:
            return mp_obj_new_int(mlx_90621_refresh_rates[fir_ir_fresh_rate]);
        #endif
        #if (OMV_FIR_MLX90640_ENABLE == 1)
        case FIR_MLX90640:
            return mp_obj_new_int(mlx_90640_1_refresh_rates[fir_ir_fresh_rate]);
        #endif
        #if (OMV_FIR_MLX90641_ENABLE == 1)
        case FIR_MLX90641:
            return mp_obj_new_int(mlx_90640_1_refresh_rates[fir_ir_fresh_rate]);
        #endif
        #if (OMV_FIR_AMG8833_ENABLE == 1)
        case FIR_AMG8833:
            return mp_obj_new_int(fir_ir_fresh_rate);
        #endif
        #if (OMV_FIR_LEPTON_ENABLE == 1)
        case FIR_LEPTON:
            return mp_obj_new_int(fir_ir_fresh_rate);
        #endif
        default:
            mp_raise_msg(&mp_type_ValueError, MP_ERROR_TEXT("FIR sensor is not initialized"));
    }
}
static MP_DEFINE_CONST_FUN_OBJ_0(py_fir_refresh_obj, py_fir_refresh);

static mp_obj_t py_fir_resolution() {
    switch (fir_sensor) {
        #if (OMV_FIR_MLX90621_ENABLE == 1)
        case FIR_MLX90621:
            return mp_obj_new_int(fir_adc_resolution + 15);
        #endif
        #if (OMV_FIR_MLX90640_ENABLE == 1)
        case FIR_MLX90640:
            return mp_obj_new_int(fir_adc_resolution + 16);
        #endif
        #if (OMV_FIR_MLX90641_ENABLE == 1)
        case FIR_MLX90641:
            return mp_obj_new_int(fir_adc_resolution + 16);
        #endif
        #if (OMV_FIR_AMG8833_ENABLE == 1)
        case FIR_AMG8833:
            return mp_obj_new_int(fir_adc_resolution);
        #endif
        #if (OMV_FIR_LEPTON_ENABLE == 1)
        case FIR_LEPTON:
            return mp_obj_new_int(fir_adc_resolution);
        #endif
        default:
            mp_raise_msg(&mp_type_ValueError, MP_ERROR_TEXT("FIR sensor is not initialized"));
    }
}
static MP_DEFINE_CONST_FUN_OBJ_0(py_fir_resolution_obj, py_fir_resolution);

#if (OMV_FIR_LEPTON_ENABLE == 1)
static mp_obj_t py_fir_radiometric() {
    if (fir_sensor == FIR_LEPTON) {
        return fir_lepton_get_radiometry();
    } else {
        mp_raise_msg(&mp_type_ValueError, MP_ERROR_TEXT("Operation not supported by this FIR sensor"));
    }
}
static MP_DEFINE_CONST_FUN_OBJ_0(py_fir_radiometric_obj, py_fir_radiometric);

#if defined(OMV_FIR_LEPTON_VSYNC_PRESENT)
static mp_obj_t py_fir_register_vsync_cb(mp_obj_t cb) {
    if (fir_sensor == FIR_LEPTON) {
        fir_lepton_register_vsync_cb(cb);
    } else {
        mp_raise_msg(&mp_type_ValueError, MP_ERROR_TEXT("Operation not supported by this FIR sensor"));
    }
    return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_1(py_fir_register_vsync_cb_obj, py_fir_register_vsync_cb);
#endif

static mp_obj_t py_fir_register_frame_cb(mp_obj_t cb) {
    if (fir_sensor == FIR_LEPTON) {
        fir_lepton_register_frame_cb(cb);
    } else {
        mp_raise_msg(&mp_type_ValueError, MP_ERROR_TEXT("Operation not supported by this FIR sensor"));
    }
    return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_1(py_fir_register_frame_cb_obj, py_fir_register_frame_cb);

static mp_obj_t py_fir_get_frame_available() {
    if (fir_sensor == FIR_LEPTON) {
        return fir_lepton_get_frame_available();
    } else {
        mp_raise_msg(&mp_type_ValueError, MP_ERROR_TEXT("Operation not supported by this FIR sensor"));
    }
}
static MP_DEFINE_CONST_FUN_OBJ_0(py_fir_get_frame_available_obj, py_fir_get_frame_available);

static mp_obj_t py_fir_trigger_ffc(uint n_args, const mp_obj_t *pos_args, mp_map_t *kw_args) {
    enum { ARG_timeout };
    static const mp_arg_t allowed_args[] = {
        { MP_QSTR_timeout, MP_ARG_INT, {.u_int = -1 } },
    };

    // Parse args.
    mp_arg_val_t args[MP_ARRAY_SIZE(allowed_args)];
    mp_arg_parse_all(n_args, pos_args, kw_args, MP_ARRAY_SIZE(allowed_args), allowed_args, args);

    if (fir_sensor == FIR_LEPTON) {
        fir_lepton_trigger_ffc(args[ARG_timeout].u_int);
    } else {
        mp_raise_msg(&mp_type_ValueError, MP_ERROR_TEXT("Operation not supported by this FIR sensor"));
    }
    return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_KW(py_fir_trigger_ffc_obj, 0, py_fir_trigger_ffc);
#endif

mp_obj_t py_fir_read_ta() {
    switch (fir_sensor) {
        #if (OMV_FIR_MLX90621_ENABLE == 1)
        case FIR_MLX90621: {
            fb_alloc_mark();
            uint16_t *data = fb_alloc(MLX90621_FRAME_DATA_SIZE * sizeof(uint16_t));
            PY_ASSERT_TRUE_MSG(MLX90621_GetFrameData(data) >= 0,
                               "Failed to read the MLX90640 sensor data!");
            mp_obj_t result = mp_obj_new_float(MLX90621_GetTa(data, MP_STATE_PORT(fir_mlx_data)));
            fb_alloc_free_till_mark();
            return result;
        }
        #endif
        #if (OMV_FIR_MLX90640_ENABLE == 1)
        case FIR_MLX90640: {
            fb_alloc_mark();
            uint16_t *data = fb_alloc(MLX90640_FRAME_DATA_SIZE * sizeof(uint16_t));
            PY_ASSERT_TRUE_MSG(MLX90640_GetFrameData(MLX90640_ADDR, data) >= 0,
                               "Failed to read the MLX90640 sensor data!");
            mp_obj_t result = mp_obj_new_float(MLX90640_GetTa(data, MP_STATE_PORT(fir_mlx_data)));
            fb_alloc_free_till_mark();
            return result;
        }
        #endif
        #if (OMV_FIR_MLX90641_ENABLE == 1)
        case FIR_MLX90641: {
            fb_alloc_mark();
            uint16_t *data = fb_alloc(MLX90641_FRAME_DATA_SIZE * sizeof(uint16_t));
            PY_ASSERT_TRUE_MSG(MLX90641_GetFrameData(MLX90641_ADDR, data) >= 0,
                               "Failed to read the MLX90641 sensor data!");
            mp_obj_t result = mp_obj_new_float(MLX90641_GetTa(data, MP_STATE_PORT(fir_mlx_data)));
            fb_alloc_free_till_mark();
            return result;
        }
        #endif
        #if (OMV_FIR_AMG8833_ENABLE == 1)
        case FIR_AMG8833: {
            int16_t temp;
            int error = 0;
            error |= omv_i2c_write_bytes(&fir_bus,
                                         AMG8833_ADDR,
                                         (uint8_t [1]) {AMG8833_THERMISTOR_REGISTER},
                                         1,
                                         OMV_I2C_XFER_NO_STOP);
            error |= omv_i2c_read_bytes(&fir_bus, AMG8833_ADDR, (uint8_t *) &temp, sizeof(temp), OMV_I2C_XFER_NO_FLAGS);
            PY_ASSERT_TRUE_MSG((error == 0), "Failed to read the AMG8833 sensor data!");
            return mp_obj_new_float(AMG8833_12_TO_16(temp) * 0.0625f);
        }
        #endif
        #if (OMV_FIR_LEPTON_ENABLE == 1)
        case FIR_LEPTON: {
            return fir_lepton_read_ta();
        }
        #endif
        default: {
            mp_raise_msg(&mp_type_ValueError, MP_ERROR_TEXT("FIR sensor is not initialized"));
        }
    }

    return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_0(py_fir_read_ta_obj, py_fir_read_ta);

mp_obj_t py_fir_read_ir(uint n_args, const mp_obj_t *pos_args, mp_map_t *kw_args) {
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

    fir_transposed = args[ARG_transpose].u_bool;

    switch (fir_sensor) {
        #if (OMV_FIR_MLX90621_ENABLE == 1)
        case FIR_MLX90621: {
            fb_alloc_mark();
            float Ta, *To = fb_alloc(MLX90621_WIDTH * MLX90621_HEIGHT * sizeof(float));
            fir_MLX90621_get_frame(&Ta, To);
            mp_obj_t result = fir_get_ir(MLX90621_WIDTH, MLX90621_HEIGHT, Ta, To, !args[ARG_hmirror].u_bool,
                                         args[ARG_vflip].u_bool, args[ARG_transpose].u_bool, true);
            fb_alloc_free_till_mark();
            return result;
        }
        #endif
        #if (OMV_FIR_MLX90640_ENABLE == 1)
        case FIR_MLX90640: {
            fb_alloc_mark();
            float Ta, *To = fb_alloc(MLX90640_WIDTH * MLX90640_HEIGHT * sizeof(float));
            fir_MLX90640_get_frame(&Ta, To);
            mp_obj_t result = fir_get_ir(MLX90640_WIDTH, MLX90640_HEIGHT, Ta, To, !args[ARG_hmirror].u_bool,
                                         args[ARG_vflip].u_bool, args[ARG_transpose].u_bool, false);
            fb_alloc_free_till_mark();
            return result;
        }
        #endif
        #if (OMV_FIR_MLX90641_ENABLE == 1)
        case FIR_MLX90641: {
            fb_alloc_mark();
            float Ta, *To = fb_alloc(MLX90641_WIDTH * MLX90641_HEIGHT * sizeof(float));
            fir_MLX90641_get_frame(&Ta, To);
            mp_obj_t result = fir_get_ir(MLX90641_WIDTH, MLX90641_HEIGHT, Ta, To, !args[ARG_hmirror].u_bool,
                                         args[ARG_vflip].u_bool, args[ARG_transpose].u_bool, false);
            fb_alloc_free_till_mark();
            return result;
        }
        #endif
        #if (OMV_FIR_AMG8833_ENABLE == 1)
        case FIR_AMG8833: {
            fb_alloc_mark();
            float Ta, *To = fb_alloc(AMG8833_WIDTH * AMG8833_HEIGHT * sizeof(float));
            fir_AMG8833_get_frame(&Ta, To);
            mp_obj_t result = fir_get_ir(AMG8833_WIDTH, AMG8833_HEIGHT, Ta, To, !args[ARG_hmirror].u_bool,
                                         args[ARG_vflip].u_bool, args[ARG_transpose].u_bool, true);
            fb_alloc_free_till_mark();
            return result;
        }
        #endif
        #if (OMV_FIR_LEPTON_ENABLE == 1)
        case FIR_LEPTON: {
            return fir_lepton_read_ir(fir_width, fir_height, args[ARG_hmirror].u_bool,
                                      args[ARG_vflip].u_bool, args[ARG_transpose].u_bool, args[ARG_timeout].u_int);
        }
        #endif
        default: {
            mp_raise_msg(&mp_type_ValueError, MP_ERROR_TEXT("FIR sensor is not initialized"));
        }
    }

    return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_KW(py_fir_read_ir_obj, 0, py_fir_read_ir);

mp_obj_t py_fir_draw_ir(uint n_args, const mp_obj_t *pos_args, mp_map_t *kw_args) {
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
        { MP_QSTR_color_palette, MP_ARG_OBJ | MP_ARG_KW_ONLY, {.u_rom_obj = MP_ROM_INT(COLOR_PALETTE_RAINBOW)} },
        { MP_QSTR_alpha_palette, MP_ARG_OBJ | MP_ARG_KW_ONLY, {.u_rom_obj = MP_ROM_NONE} },
        { MP_QSTR_hint, MP_ARG_INT | MP_ARG_KW_ONLY,  {.u_int = 0 } },
        { MP_QSTR_scale, MP_ARG_OBJ | MP_ARG_KW_ONLY, {.u_rom_obj = MP_ROM_NONE} },
    };

    // Parse args.
    mp_arg_val_t args[MP_ARRAY_SIZE(allowed_args)];
    mp_arg_parse_all(n_args - 2, pos_args + 2, kw_args, MP_ARRAY_SIZE(allowed_args), allowed_args, args);

    // Sanity checks
    if (fir_sensor == FIR_NONE) {
        mp_raise_msg(&mp_type_ValueError, MP_ERROR_TEXT("FIR sensor is not initialized"));
    }

    if (args[ARG_channel].u_int < -1 || args[ARG_channel].u_int > 2) {
        mp_raise_msg(&mp_type_ValueError, MP_ERROR_TEXT("RGB channel can be 0, 1, or 2"));
    }

    if (args[ARG_alpha].u_int < 0 || args[ARG_alpha].u_int > 255) {
        mp_raise_msg(&mp_type_ValueError, MP_ERROR_TEXT("Alpha ranges between 0 and 255"));
    }

    image_t src_img = {
        .w = fir_transposed ? fir_height : fir_width,
        .h = fir_transposed ? fir_width : fir_height,
        .pixfmt = PIXFORMAT_GRAYSCALE,
        //.data is allocated later.
    };

    image_t *dst_img = py_helper_arg_to_image(pos_args[0], ARG_IMAGE_MUTABLE);

    mp_obj_t *ir_array;
    mp_obj_get_array_fixed_n(pos_args[1], src_img.w * src_img.h, &ir_array);

    rectangle_t roi = py_helper_arg_to_roi(args[ARG_roi].u_obj, &src_img);

    float x_scale = 1.0f;
    float y_scale = 1.0f;
    py_helper_arg_to_scale(args[ARG_x_scale].u_obj, args[ARG_y_scale].u_obj, &x_scale, &y_scale);

    float min = FLT_MAX;
    float max = -FLT_MAX;
    py_helper_arg_to_minmax(args[ARG_scale].u_obj, &min, &max, ir_array, src_img.w * src_img.h);

    const uint16_t *color_palette = py_helper_arg_to_palette(args[ARG_color_palette].u_obj, PIXFORMAT_RGB565);
    const uint8_t *alpha_palette = py_helper_arg_to_palette(args[ARG_alpha_palette].u_obj, PIXFORMAT_GRAYSCALE);

    fb_alloc_mark();
    src_img.data = fb_alloc(src_img.w * src_img.h * sizeof(uint8_t));
    fir_fill_image_float_obj(&src_img, ir_array, min, max);

    imlib_draw_image(dst_img, &src_img, args[ARG_x].u_int, args[ARG_y].u_int, x_scale, y_scale, &roi,
                     args[ARG_channel].u_int, args[ARG_alpha].u_int, color_palette, alpha_palette,
                     args[ARG_hint].u_int, NULL, NULL, NULL);

    fb_alloc_free_till_mark();
    return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_KW(py_fir_draw_ir_obj, 2, py_fir_draw_ir);

mp_obj_t py_fir_snapshot(uint n_args, const mp_obj_t *pos_args, mp_map_t *kw_args) {
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
        { MP_QSTR_color_palette, MP_ARG_OBJ | MP_ARG_KW_ONLY, {.u_rom_obj = MP_ROM_INT(COLOR_PALETTE_RAINBOW)} },
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
        .w = args[ARG_transpose].u_bool ? fir_height : fir_width,
        .h = args[ARG_transpose].u_bool ? fir_width : fir_height,
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

    switch (fir_sensor) {
        #if (OMV_FIR_MLX90621_ENABLE == 1)
        case FIR_MLX90621: {
            float Ta, *To = fb_alloc(MLX90621_WIDTH * MLX90621_HEIGHT * sizeof(float));
            fir_MLX90621_get_frame(&Ta, To);
            if (args[ARG_scale].u_obj == mp_const_none) {
                fast_get_min_max(To, MLX90621_WIDTH * MLX90621_HEIGHT, &min, &max);
            }
            imlib_fill_image_from_float(&src_img, MLX90621_WIDTH, MLX90621_HEIGHT, To, min, max, !args[ARG_hmirror].u_bool,
                                        args[ARG_vflip].u_bool, args[ARG_transpose].u_bool, true);
            break;
        }
        #endif
        #if (OMV_FIR_MLX90640_ENABLE == 1)
        case FIR_MLX90640: {
            float Ta, *To = fb_alloc(MLX90640_WIDTH * MLX90640_HEIGHT * sizeof(float));
            fir_MLX90640_get_frame(&Ta, To);
            if (args[ARG_scale].u_obj == mp_const_none) {
                fast_get_min_max(To, MLX90640_WIDTH * MLX90640_HEIGHT, &min, &max);
            }
            imlib_fill_image_from_float(&src_img, MLX90640_WIDTH, MLX90640_HEIGHT, To, min, max, !args[ARG_hmirror].u_bool,
                                        args[ARG_vflip].u_bool, args[ARG_transpose].u_bool, false);
            break;
        }
        #endif
        #if (OMV_FIR_MLX90641_ENABLE == 1)
        case FIR_MLX90641: {
            float Ta, *To = fb_alloc(MLX90641_WIDTH * MLX90641_HEIGHT * sizeof(float));
            fir_MLX90641_get_frame(&Ta, To);
            if (args[ARG_scale].u_obj == mp_const_none) {
                fast_get_min_max(To, MLX90641_WIDTH * MLX90641_HEIGHT, &min, &max);
            }
            imlib_fill_image_from_float(&src_img, MLX90641_WIDTH, MLX90641_HEIGHT, To, min, max, !args[ARG_hmirror].u_bool,
                                        args[ARG_vflip].u_bool, args[ARG_transpose].u_bool, false);
            break;
        }
        #endif
        #if (OMV_FIR_AMG8833_ENABLE == 1)
        case FIR_AMG8833: {
            float Ta, *To = fb_alloc(AMG8833_WIDTH * AMG8833_HEIGHT * sizeof(float));
            fir_AMG8833_get_frame(&Ta, To);
            if (args[ARG_scale].u_obj == mp_const_none) {
                fast_get_min_max(To, AMG8833_WIDTH * AMG8833_HEIGHT, &min, &max);
            }
            imlib_fill_image_from_float(&src_img, AMG8833_WIDTH, AMG8833_HEIGHT, To, min, max, !args[ARG_hmirror].u_bool,
                                        args[ARG_vflip].u_bool, args[ARG_transpose].u_bool, true);
            break;
        }
        #endif
        #if (OMV_FIR_LEPTON_ENABLE == 1)
        case FIR_LEPTON: {
            bool auto_range = args[ARG_scale].u_obj == mp_const_none;
            fir_lepton_fill_image(&src_img, fir_width, fir_height, auto_range, min, max,
                                  args[ARG_hmirror].u_bool, args[ARG_vflip].u_bool,
                                  args[ARG_transpose].u_bool, args[ARG_timeout].u_int);
            break;
        }
        #endif
        default: {
            mp_raise_msg(&mp_type_ValueError, MP_ERROR_TEXT("FIR sensor is not initialized"));
        }
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
static MP_DEFINE_CONST_FUN_OBJ_KW(py_fir_snapshot_obj, 0, py_fir_snapshot);

static const mp_rom_map_elem_t globals_dict_table[] = {
    { MP_ROM_QSTR(MP_QSTR___name__),            MP_ROM_QSTR(MP_QSTR_fir)                    },
    #if (OMV_FIR_MLX90621_ENABLE == 1)
    { MP_ROM_QSTR(MP_QSTR_FIR_SHIELD),          MP_ROM_INT(FIR_MLX90621)                    },
    { MP_ROM_QSTR(MP_QSTR_FIR_MLX90621),        MP_ROM_INT(FIR_MLX90621)                    },
    #endif
    #if (OMV_FIR_MLX90640_ENABLE == 1)
    { MP_ROM_QSTR(MP_QSTR_FIR_MLX90640),        MP_ROM_INT(FIR_MLX90640)                    },
    #endif
    #if (OMV_FIR_MLX90641_ENABLE == 1)
    { MP_ROM_QSTR(MP_QSTR_FIR_MLX90641),        MP_ROM_INT(FIR_MLX90641)                    },
    #endif
    #if (OMV_FIR_AMG8833_ENABLE == 1)
    { MP_ROM_QSTR(MP_QSTR_FIR_AMG8833),         MP_ROM_INT(FIR_AMG8833)                     },
    #endif
    #if (OMV_FIR_LEPTON_ENABLE == 1)
    { MP_ROM_QSTR(MP_QSTR_FIR_LEPTON),          MP_ROM_INT(FIR_LEPTON)                      },
    #endif
    { MP_ROM_QSTR(MP_QSTR_init),                MP_ROM_PTR(&py_fir_init_obj)                },
    { MP_ROM_QSTR(MP_QSTR_deinit),              MP_ROM_PTR(&py_fir_deinit_obj)              },
    { MP_ROM_QSTR(MP_QSTR_type),                MP_ROM_PTR(&py_fir_type_obj)                },
    { MP_ROM_QSTR(MP_QSTR_width),               MP_ROM_PTR(&py_fir_width_obj)               },
    { MP_ROM_QSTR(MP_QSTR_height),              MP_ROM_PTR(&py_fir_height_obj)              },
    { MP_ROM_QSTR(MP_QSTR_refresh),             MP_ROM_PTR(&py_fir_refresh_obj)             },
    { MP_ROM_QSTR(MP_QSTR_resolution),          MP_ROM_PTR(&py_fir_resolution_obj)          },
    #if (OMV_FIR_LEPTON_ENABLE == 1)
    { MP_ROM_QSTR(MP_QSTR_radiometric),         MP_ROM_PTR(&py_fir_radiometric_obj)         },
    #if defined(OMV_FIR_LEPTON_VSYNC_PRESENT)
    { MP_ROM_QSTR(MP_QSTR_register_vsync_cb),   MP_ROM_PTR(&py_fir_register_vsync_cb_obj)   },
    #else
    { MP_ROM_QSTR(MP_QSTR_register_vsync_cb),   MP_ROM_PTR(&py_func_unavailable_obj)        },
    #endif
    { MP_ROM_QSTR(MP_QSTR_register_frame_cb),   MP_ROM_PTR(&py_fir_register_frame_cb_obj)   },
    { MP_ROM_QSTR(MP_QSTR_get_frame_available), MP_ROM_PTR(&py_fir_get_frame_available_obj) },
    { MP_ROM_QSTR(MP_QSTR_trigger_ffc),         MP_ROM_PTR(&py_fir_trigger_ffc_obj)         },
    #else
    { MP_ROM_QSTR(MP_QSTR_radiometric),         MP_ROM_PTR(&py_func_unavailable_obj)        },
    { MP_ROM_QSTR(MP_QSTR_register_vsync_cb),   MP_ROM_PTR(&py_func_unavailable_obj)        },
    { MP_ROM_QSTR(MP_QSTR_register_frame_cb),   MP_ROM_PTR(&py_func_unavailable_obj)        },
    { MP_ROM_QSTR(MP_QSTR_get_frame_available), MP_ROM_PTR(&py_func_unavailable_obj)        },
    { MP_ROM_QSTR(MP_QSTR_trigger_ffc),         MP_ROM_PTR(&py_func_unavailable_obj)        },
    #endif
    { MP_ROM_QSTR(MP_QSTR_read_ta),             MP_ROM_PTR(&py_fir_read_ta_obj)             },
    { MP_ROM_QSTR(MP_QSTR_read_ir),             MP_ROM_PTR(&py_fir_read_ir_obj)             },
    { MP_ROM_QSTR(MP_QSTR_draw_ir),             MP_ROM_PTR(&py_fir_draw_ir_obj)             },
    { MP_ROM_QSTR(MP_QSTR_snapshot),            MP_ROM_PTR(&py_fir_snapshot_obj)            }
};

static MP_DEFINE_CONST_DICT(globals_dict, globals_dict_table);

const mp_obj_module_t fir_module = {
    .base = { &mp_type_module },
    .globals = (mp_obj_t) &globals_dict,
};

void py_fir_init0() {
    py_fir_deinit();
}

#if ((OMV_FIR_MLX90621_ENABLE == 1) || (OMV_FIR_MLX90640_ENABLE == 1) || (OMV_FIR_MLX90641_ENABLE == 1))
MP_REGISTER_ROOT_POINTER(void *fir_mlx_data);
#endif
MP_REGISTER_MODULE(MP_QSTR_fir, fir_module);
#endif
