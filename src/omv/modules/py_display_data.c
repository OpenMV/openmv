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
 * Display data channel module (such as CEC/DDC).
 */
#include "omv_boardconfig.h"

#if OMV_DISPLAY_CEC_ENABLE || OMV_DISPLAY_DDC_ENABLE
#include "py/obj.h"
#include "py/objarray.h"
#include "py/runtime.h"
#include "mphal.h"
#include "extmod/modmachine.h"

#include "omv_gpio.h"
#include "fb_alloc.h"
#include "py_display.h"
#include "cec.h"

typedef struct _py_display_data_obj_t {
    mp_obj_base_t base;
    #if OMV_DISPLAY_CEC_ENABLE
    bool cec_enabled;
    uint8_t dst_addr;
    mp_obj_t cec_frame;
    mp_obj_t cec_callback;
    #endif
    #if OMV_DISPLAY_DDC_ENABLE
    bool ddc_enabled;
    uint8_t ddc_addr;
    mp_obj_t ddc_bus;
    #endif
} py_display_data_obj_t;

#if OMV_DISPLAY_CEC_ENABLE
static void cec_extint_callback(mp_obj_t self_in) {
    py_display_data_obj_t *self = MP_OBJ_TO_PTR(self_in);
    if (self->cec_callback != mp_const_none) {
        uint8_t src_addr = 0;
        mp_obj_array_t *frame = MP_OBJ_TO_PTR(self->cec_frame);
        if (cec_receive_frame(frame->items, &frame->len, self->dst_addr, &src_addr) != -1) {
            mp_call_function_2(self->cec_callback, MP_OBJ_NEW_SMALL_INT(src_addr), self->cec_frame);
        }
    }
}

static mp_obj_t py_cec_send_frame(uint n_args, const mp_obj_t *pos_args, mp_map_t *kw_args) {
    enum { ARG_dst_addr, ARG_src_addr, ARG_data };
    static const mp_arg_t allowed_args[] = {
        { MP_QSTR_dst_addr, MP_ARG_REQUIRED | MP_ARG_INT },
        { MP_QSTR_src_addr, MP_ARG_REQUIRED | MP_ARG_INT },
        { MP_QSTR_data,     MP_ARG_REQUIRED | MP_ARG_OBJ },
    };

    // Parse args.
    py_display_data_obj_t *self = MP_OBJ_TO_PTR(pos_args[0]);
    mp_arg_val_t args[MP_ARRAY_SIZE(allowed_args)];
    mp_arg_parse_all(n_args - 1, pos_args + 1, kw_args, MP_ARRAY_SIZE(allowed_args), allowed_args, args);

    mp_buffer_info_t bufinfo;
    mp_get_buffer_raise(args[ARG_data].u_obj, &bufinfo, MP_BUFFER_READ);

    // Disable interrupt while transmitting a frame.
    if (self->cec_callback) {
        omv_gpio_irq_enable(OMV_CEC_PIN, false);
    }
    if (!cec_send_frame(args[ARG_dst_addr].u_int, args[ARG_src_addr].u_int, bufinfo.len, bufinfo.buf)) {
        mp_raise_msg(&mp_type_OSError, MP_ERROR_TEXT("Failed to send frame."));
    }
    if (self->cec_callback) {
        omv_gpio_irq_enable(OMV_CEC_PIN, true);
    }
    return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_KW(py_cec_send_frame_obj, 4, py_cec_send_frame);

static mp_obj_t py_cec_receive_frame(uint n_args, const mp_obj_t *pos_args, mp_map_t *kw_args) {
    enum { ARG_dst_addr, ARG_timeout };
    static const mp_arg_t allowed_args[] = {
        { MP_QSTR_dst_addr, MP_ARG_REQUIRED | MP_ARG_INT },
        { MP_QSTR_timeout,  MP_ARG_KW_ONLY | MP_ARG_INT,  {.u_int = 1000 } },
    };

    // Parse args.
    mp_arg_val_t args[MP_ARRAY_SIZE(allowed_args)];
    mp_arg_parse_all(n_args - 1, pos_args + 1, kw_args, MP_ARRAY_SIZE(allowed_args), allowed_args, args);

    mp_uint_t start = mp_hal_ticks_ms();
    while (omv_gpio_read(OMV_CEC_PIN)) {
        if ((mp_hal_ticks_ms() - start) > args[ARG_timeout].u_int) {
            mp_raise_msg(&mp_type_OSError, MP_ERROR_TEXT("Receive timeout!"));
        }
    }
    uint8_t src_addr = 0;
    mp_obj_array_t *frame = MP_OBJ_TO_PTR(mp_obj_new_bytearray_by_ref(16, m_new(byte, 16)));
    if (cec_receive_frame(frame->items, &frame->len, args[ARG_dst_addr].u_int, &src_addr) != 0) {
        mp_raise_msg(&mp_type_OSError, MP_ERROR_TEXT("Failed to receive frame."));
    }
    return mp_obj_new_tuple(2, (mp_obj_t []) { MP_OBJ_NEW_SMALL_INT(src_addr), frame });
}
static MP_DEFINE_CONST_FUN_OBJ_KW(py_cec_receive_frame_obj, 2, py_cec_receive_frame);

static mp_obj_t py_cec_frame_callback(mp_obj_t self_in, mp_obj_t cb, mp_obj_t dst_addr) {
    py_display_data_obj_t *self = MP_OBJ_TO_PTR(self_in);

    self->cec_callback = cb;
    if (cb == mp_const_none) {
        omv_gpio_irq_enable(OMV_CEC_PIN, false);
        self->cec_frame = mp_const_none;
    } else {
        self->dst_addr = mp_obj_get_int(dst_addr);
        self->cec_frame = mp_obj_new_bytearray_by_ref(0, m_new(byte, 16));
        omv_gpio_config(OMV_CEC_PIN, OMV_GPIO_MODE_IT_FALL, OMV_GPIO_PULL_UP, OMV_GPIO_SPEED_LOW, -1);
        omv_gpio_irq_register(OMV_CEC_PIN, cec_extint_callback, self_in);
        omv_gpio_irq_enable(OMV_CEC_PIN, true);
    }
    return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_3(py_cec_frame_callback_obj, py_cec_frame_callback);
#endif // OMV_DISPLAY_CEC_ENABLE

#if OMV_DISPLAY_DDC_ENABLE
static bool ddc_checksum(uint8_t *data, int long_count) {
    uint32_t *data32 = (uint32_t *) data;
    uint32_t sum = 0;

    for (int i = 0; i < long_count; i++) {
        sum = __USADA8(data32[i], 0, sum);
    }

    return !(sum & 0xFF);
}

static mp_obj_t py_ddc_display_id(mp_obj_t self_in) {
    py_display_data_obj_t *self = MP_OBJ_TO_PTR(self_in);

    if (mp_machine_soft_i2c_transfer(self->ddc_bus, self->ddc_addr, 1, &((mp_machine_i2c_buf_t) {
        .len = 1, .buf = (uint8_t []) {0x00} // addr
    }), MP_MACHINE_I2C_FLAG_STOP) == 1) {
        fb_alloc_mark();
        uint8_t *data = fb_alloc(128);

        if (mp_machine_soft_i2c_transfer(self->ddc_bus, self->ddc_addr, 1, &((mp_machine_i2c_buf_t) {
            .len = 128, .buf = data
        }), MP_MACHINE_I2C_FLAG_READ | MP_MACHINE_I2C_FLAG_STOP) == 0) {
            uint32_t *data32 = (uint32_t *) data;

            if ((data32[0] == 0xFFFFFF00) && (data32[1] == 0x00FFFFFF) && ddc_checksum(data, 32)
                && (mp_machine_soft_i2c_transfer(self->ddc_bus, self->ddc_addr, 1, &((mp_machine_i2c_buf_t) {
                .len = 1, .buf = (uint8_t []) {0x80} // addr
            }), MP_MACHINE_I2C_FLAG_STOP) == 1)) {
                int extensions = data[126];
                int extensions_byte_size = extensions * 128;
                int total_data_byte_size = extensions_byte_size + 128;
                uint8_t *data2 = fb_alloc(total_data_byte_size), *data2_ext = data2 + 128;
                memcpy(data2, data, 128);

                if ((mp_machine_soft_i2c_transfer(self->ddc_bus, self->ddc_addr, 1, &((mp_machine_i2c_buf_t) {
                    .len = extensions_byte_size, .buf = data2_ext
                }), MP_MACHINE_I2C_FLAG_READ | MP_MACHINE_I2C_FLAG_STOP) == 0)
                    && ddc_checksum(data2_ext, extensions_byte_size / 4)) {
                    mp_obj_t result = mp_obj_new_bytes(data2, total_data_byte_size);
                    fb_alloc_free_till_mark();
                    return result;
                }
            }
        }
    }

    mp_raise_msg(&mp_type_OSError, MP_ERROR_TEXT("Failed to get display id data!"));
}
static MP_DEFINE_CONST_FUN_OBJ_1(py_ddc_display_id_obj, py_ddc_display_id);
#endif // OMV_DISPLAY_DDC_ENABLE

mp_obj_t py_display_data_make_new(const mp_obj_type_t *type, size_t n_args, size_t n_kw, const mp_obj_t *all_args) {
    enum { ARG_cec, ARG_ddc, ARG_ddc_addr };
    static const mp_arg_t allowed_args[] = {
        { MP_QSTR_cec,      MP_ARG_KW_ONLY | MP_ARG_BOOL, {.u_bool = false} },
        { MP_QSTR_ddc,      MP_ARG_KW_ONLY | MP_ARG_BOOL, {.u_bool = false} },
        { MP_QSTR_ddc_addr, MP_ARG_KW_ONLY | MP_ARG_INT,  {.u_int = 0x50 } },
    };

    // Parse args.
    mp_arg_val_t args[MP_ARRAY_SIZE(allowed_args)];
    mp_arg_parse_all_kw_array(n_args, n_kw, all_args, MP_ARRAY_SIZE(allowed_args), allowed_args, args);

    py_display_data_obj_t *self = mp_obj_malloc_with_finaliser(py_display_data_obj_t, &py_display_data_type);

    #if OMV_DISPLAY_CEC_ENABLE
    self->cec_enabled = args[ARG_cec].u_bool;
    self->dst_addr = 0;
    self->cec_frame = mp_const_none;
    self->cec_callback = mp_const_none;
    if (self->cec_enabled) {
        cec_init();
    }
    #endif // OMV_DISPLAY_CEC_ENABLE

    #if OMV_DISPLAY_DDC_ENABLE
    self->ddc_enabled = args[ARG_ddc].u_bool;
    self->ddc_addr = args[ARG_ddc_addr].u_int;
    self->ddc_bus = mp_const_none;
    if (self->ddc_enabled) {
        self->ddc_bus = MP_OBJ_TYPE_GET_SLOT(
            &mp_machine_soft_i2c_type, make_new) (&mp_machine_soft_i2c_type, 2, 1, (const mp_obj_t []) {
            (mp_obj_t) OMV_DDC_SCL_PIN, (mp_obj_t) OMV_DDC_SDA_PIN,
            MP_OBJ_NEW_QSTR(MP_QSTR_freq),
            MP_OBJ_NEW_SMALL_INT(100000)
        });
    }
    #endif // OMV_DISPLAY_DDC_ENABLE

    return MP_OBJ_FROM_PTR(self);
}

static mp_obj_t py_display_data_deinit(mp_obj_t self_in) {
    py_display_data_obj_t *self = MP_OBJ_TO_PTR(self_in);

    #if OMV_DISPLAY_DDC_ENABLE
    if (self->ddc_enabled) {
        // Deinit bus.
    }
    #endif // OMV_DISPLAY_DDC_ENABLE

    #if OMV_DISPLAY_CEC_ENABLE
    if (self->cec_enabled) {
        cec_deinit();
    }
    #endif // OMV_DISPLAY_CEC_ENABLE

    return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_1(py_display_data_deinit_obj, py_display_data_deinit);

static const mp_rom_map_elem_t py_display_data_locals_dict_table[] = {
    { MP_ROM_QSTR(MP_QSTR___name__),                MP_ROM_QSTR(MP_QSTR_display_data)       },
    { MP_ROM_QSTR(MP_QSTR___del__),                 MP_ROM_PTR(&py_display_data_deinit_obj) },
    #if OMV_DISPLAY_DDC_ENABLE
    { MP_ROM_QSTR(MP_QSTR_display_id),              MP_ROM_PTR(&py_ddc_display_id_obj)      },
    #endif
    #if OMV_DISPLAY_CEC_ENABLE
    { MP_ROM_QSTR(MP_QSTR_send_frame),              MP_ROM_PTR(&py_cec_send_frame_obj)      },
    { MP_ROM_QSTR(MP_QSTR_receive_frame),           MP_ROM_PTR(&py_cec_receive_frame_obj)   },
    { MP_ROM_QSTR(MP_QSTR_frame_callback),          MP_ROM_PTR(&py_cec_frame_callback_obj)  },
    #endif
};
MP_DEFINE_CONST_DICT(py_display_data_locals_dict, py_display_data_locals_dict_table);

MP_DEFINE_CONST_OBJ_TYPE(
    py_display_data_type,
    MP_QSTR_DisplayData,
    MP_TYPE_FLAG_NONE,
    make_new, py_display_data_make_new,
    locals_dict, &py_display_data_locals_dict
    );

#endif // OMV_DISPLAY_DATA
