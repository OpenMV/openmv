/*
 * Copyright (C) 2024 OpenMV, LLC.
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
 * Python Machine Learning Module.
 */
#include <stdio.h>
#include "py/runtime.h"
#include "py/obj.h"
#include "py/objlist.h"
#include "py/objtuple.h"
#include "py/binary.h"

#if MICROPY_VFS
#include "py/stream.h"
#include "extmod/vfs.h"
#endif

#include "py_helper.h"
#include "imlib_config.h"

#if MICROPY_PY_ML
#include "py_image.h"
#include "file_utils.h"
#include "py_ml.h"
#include "ulab/code/ndarray.h"

#ifndef IMLIB_ML_MODEL_ALIGN
#ifndef __DCACHE_PRESENT
#define IMLIB_ML_MODEL_ALIGN    (32 - 1)
#else
#define IMLIB_ML_MODEL_ALIGN    (__SCB_DCACHE_LINE_SIZE - 1)
#endif
#endif

static size_t py_ml_tuple_sum(mp_obj_tuple_t *o) {
    if (o->len < 1) {
        mp_raise_msg(&mp_type_ValueError, MP_ERROR_TEXT("Unexpected tensor shape"));
    }

    size_t size = mp_obj_get_int(o->items[0]);
    for (size_t i = 1; i < o->len; i++) {
        size *= mp_obj_get_int(o->items[i]);
    }
    return size;
}

static size_t pl_ml_dtype_size(char dtype) {
    switch (dtype) {
        case 'f':
            return 4;
        case 'H':
        case 'h':
            return 2;
        default:
            return 1;
    }
}

static void py_ml_process_input(py_ml_model_obj_t *model, mp_obj_t arg) {
    mp_obj_list_t *input_list = MP_OBJ_TO_PTR(arg);

    for (size_t i = 0; i < model->inputs_size; i++) {
        void *input_buffer = ml_backend_get_input(model, i);
        size_t input_size = py_ml_tuple_sum(MP_OBJ_TO_PTR(model->input_shape->items[i]));
        mp_obj_tuple_t *input_shape = MP_OBJ_TO_PTR(model->input_shape->items[i]);
        float input_scale = 1.0f / mp_obj_get_float(model->input_scale->items[i]);
        int input_zero_point = mp_obj_get_int(model->input_zero_point->items[i]);
        int input_dtype = mp_obj_get_int(model->input_dtype->items[i]);
        mp_obj_t input_arg = input_list->items[i];

        if (mp_obj_is_callable(input_arg)) {
            // Input is a callable. Call the object and pass the tensor buffer and dtype.
            mp_obj_t fargs[3] = {
                mp_obj_new_bytearray_by_ref(input_size * pl_ml_dtype_size(input_dtype), input_buffer),
                MP_OBJ_FROM_PTR(input_shape),
                mp_obj_new_int(input_dtype)
            };
            mp_call_function_n_kw(input_arg, 3, 0, fargs);
        } else if (MP_OBJ_IS_TYPE(input_arg, &ulab_ndarray_type)) {
            // Input is an ndarry. The input is converted and copied to the tensor buffer.
            ndarray_obj_t *input_array = MP_OBJ_TO_PTR(input_arg);

            if (input_array->ndim != input_shape->len) {
                mp_raise_msg(&mp_type_ValueError,
                             MP_ERROR_TEXT("Input shape does not match the model input shape"));
            }

            for (size_t i = 0; i < input_array->ndim; i++) {
                size_t ulab_offset = ULAB_MAX_DIMS - input_array->ndim;
                if (input_array->shape[ulab_offset + i] != mp_obj_get_int(input_shape->items[i])) {
                    mp_raise_msg(&mp_type_ValueError,
                                 MP_ERROR_TEXT("Input shape does not match the model input shape"));
                }
            }

            if (input_dtype == 'f') {
                float *model_input_float = (float *) input_buffer;
                for (size_t i = 0; i < input_array->len; i++) {
                    float value = ndarray_get_float_index(input_array->array, input_array->dtype, i);
                    model_input_float[i] = value;
                }
            } else if (input_dtype == 'b') {
                int8_t *model_input_8 = (int8_t *) input_buffer;
                for (size_t i = 0; i < input_array->len; i++) {
                    float value = ndarray_get_float_index(input_array->array, input_array->dtype, i);
                    model_input_8[i] = (int8_t) ((value * input_scale) + input_zero_point);
                }
            } else if (input_dtype == 'B') {
                uint8_t *model_input_8 = (uint8_t *) input_buffer;
                for (size_t i = 0; i < input_array->len; i++) {
                    float value = ndarray_get_float_index(input_array->array, input_array->dtype, i);
                    model_input_8[i] = (uint8_t) ((value * input_scale) + input_zero_point);
                }
            } else if (input_dtype == 'h') {
                int16_t *model_input_16 = (int16_t *) input_buffer;
                for (size_t i = 0; i < input_array->len; i++) {
                    float value = ndarray_get_float_index(input_array->array, input_array->dtype, i);
                    model_input_16[i] = (int16_t) ((value * input_scale) + input_zero_point);
                }
            } else if (input_dtype == 'H') {
                uint16_t *model_input_16 = (uint16_t *) input_buffer;
                for (size_t i = 0; i < input_array->len; i++) {
                    float value = ndarray_get_float_index(input_array->array, input_array->dtype, i);
                    model_input_16[i] = (uint16_t) ((value * input_scale) + input_zero_point);
                }
            }
        } else {
            mp_raise_msg(&mp_type_ValueError, MP_ERROR_TEXT("Unsupported input type"));
        }
    }
}

static mp_obj_t py_ml_process_output(py_ml_model_obj_t *model) {
    mp_obj_list_t *output_list = MP_OBJ_TO_PTR(mp_obj_new_list(model->outputs_size, NULL));
    for (size_t i = 0; i < model->outputs_size; i++) {
        void *model_output = ml_backend_get_output(model, i);
        size_t size = py_ml_tuple_sum(MP_OBJ_TO_PTR(model->output_shape->items[i]));
        mp_obj_tuple_t *output_shape = MP_OBJ_TO_PTR(model->output_shape->items[i]);
        float output_scale = mp_obj_get_float(model->output_scale->items[i]);
        int output_zero_point = mp_obj_get_int(model->output_zero_point->items[i]);
        int output_dtype = mp_obj_get_int(model->output_dtype->items[i]);

        size_t shape[ULAB_MAX_DIMS] = {};

        if (ULAB_MAX_DIMS < output_shape->len) {
            mp_raise_msg(&mp_type_ValueError, MP_ERROR_TEXT("Output shape has too many dimensions"));
        }

        for (size_t j = 0; j < output_shape->len; j++) {
            size_t ulab_offset = ULAB_MAX_DIMS - output_shape->len;
            shape[ulab_offset + j] = mp_obj_get_int(output_shape->items[j]);
        }

        ndarray_obj_t *ndarray = ndarray_new_dense_ndarray(output_shape->len, shape, NDARRAY_FLOAT);

        if (output_dtype == 'f') {
            memcpy(ndarray->array, model_output, size * sizeof(float));
        } else if (output_dtype == 'b') {
            for (size_t j = 0; j < size; j++) {
                float v = (((int8_t *) model_output)[j] - output_zero_point);
                ((float *) ndarray->array)[j] = v * output_scale;
            }
        } else if (output_dtype == 'B') {
            for (size_t j = 0; j < size; j++) {
                float v = (((uint8_t *) model_output)[j] - output_zero_point);
                ((float *) ndarray->array)[j] = v * output_scale;
            }
        } else if (output_dtype == 'h') {
            for (size_t j = 0; j < size; j++) {
                float v = (((int16_t *) model_output)[j] - output_zero_point);
                ((float *) ndarray->array)[j] = v * output_scale;
            }
        } else if (output_dtype == 'H') {
            for (size_t j = 0; j < size; j++) {
                float v = (((uint16_t *) model_output)[j] - output_zero_point);
                ((float *) ndarray->array)[j] = v * output_scale;
            }
        }
        output_list->items[i] = MP_OBJ_FROM_PTR(ndarray);
    }

    return MP_OBJ_FROM_PTR(output_list);
}

// TF Model Object.
static const mp_obj_type_t py_ml_model_type;

static mp_obj_t py_ml_dtype_char_tuple(const mp_obj_tuple_t *dtype) {
    mp_obj_tuple_t *r = (mp_obj_tuple_t *) MP_OBJ_TO_PTR(mp_obj_new_tuple(dtype->len, NULL));
    for (size_t i = 0; i < dtype->len; i++) {
        char d = mp_obj_get_int(dtype->items[i]);
        r->items[i] = mp_obj_new_str(&d, 1);
    }
    return r;
}

static void py_ml_model_print(const mp_print_t *print, mp_obj_t self_in, mp_print_kind_t kind) {
    py_ml_model_obj_t *self = MP_OBJ_TO_PTR(self_in);
    mp_printf(print, "{ model_size: %d, model_addr: 0x%x, ram_size: %d, ram_addr: 0x%x",
              self->size, (uint32_t) self->data, self->memory_size, self->memory_addr);
    mp_printf(print, ", input_shape: ");
    mp_obj_print_helper(print, self->input_shape, kind);
    mp_printf(print, ", input_scale: ");
    mp_obj_print_helper(print, self->input_scale, kind);
    mp_printf(print, ", input_zero_point: ");
    mp_obj_print_helper(print, self->input_zero_point, kind);
    mp_printf(print, ", input_dtype: ");
    mp_obj_print_helper(print, py_ml_dtype_char_tuple(self->input_dtype), kind);
    mp_printf(print, ", output_shape: ");
    mp_obj_print_helper(print, self->output_shape, kind);
    mp_printf(print, ", output_scale: ");
    mp_obj_print_helper(print, self->output_scale, kind);
    mp_printf(print, ", output_zero_point: ");
    mp_obj_print_helper(print, self->output_zero_point, kind);
    mp_printf(print, ", output_dtype: ");
    mp_obj_print_helper(print, py_ml_dtype_char_tuple(self->output_dtype), kind);
    mp_printf(print, " }");
}

static mp_obj_t py_ml_model_predict(uint n_args, const mp_obj_t *pos_args, mp_map_t *kw_args) {
    enum { ARG_callback };
    static const mp_arg_t allowed_args[] = {
        { MP_QSTR_callback, MP_ARG_OBJ | MP_ARG_KW_ONLY, {.u_rom_obj = MP_ROM_NONE} },
    };

    // Parse args.
    mp_arg_val_t args[MP_ARRAY_SIZE(allowed_args)];
    mp_arg_parse_all(n_args - 2, pos_args + 2, kw_args, MP_ARRAY_SIZE(allowed_args), allowed_args, args);

    py_ml_model_obj_t *model = MP_OBJ_TO_PTR(pos_args[0]);

    if (!MP_OBJ_IS_TYPE(pos_args[1], &mp_type_list)) {
        mp_raise_msg(&mp_type_ValueError, MP_ERROR_TEXT("Unsupported input type. Expected a list"));
    }

    OMV_PROFILE_START(preprocess);
    py_ml_process_input(model, pos_args[1]);
    OMV_PROFILE_PRINT(preprocess);

    OMV_PROFILE_START(inference);
    ml_backend_run_inference(model);
    OMV_PROFILE_PRINT(inference);

    mp_obj_t output = py_ml_process_output(model);

    if (args[ARG_callback].u_obj != mp_const_none) {
        // Pass model, inputs, outputs to the post-processing callback.
        mp_obj_t fargs[3] = { MP_OBJ_FROM_PTR(model), pos_args[1], output };
        output = mp_call_function_n_kw(args[ARG_callback].u_obj, 3, 0, fargs);
    }

    return output;
}
static MP_DEFINE_CONST_FUN_OBJ_KW(py_ml_model_predict_obj, 2, py_ml_model_predict);

static void py_ml_model_attr(mp_obj_t self_in, qstr attr, mp_obj_t *dest) {
    py_ml_model_obj_t *self = MP_OBJ_TO_PTR(self_in);
    if (dest[0] == MP_OBJ_NULL) {
        // Load attribute.
        switch (attr) {
            case MP_QSTR_len:
                dest[0] = mp_obj_new_int(self->size);
                break;
            case MP_QSTR_ram:
                dest[0] = mp_obj_new_int(self->memory_size);
                break;
            case MP_QSTR_input_shape:
                dest[0] = MP_OBJ_FROM_PTR(self->input_shape);
                break;
            case MP_QSTR_input_dtype:
                dest[0] = py_ml_dtype_char_tuple(self->input_dtype);
                break;
            case MP_QSTR_input_scale:
                dest[0] = MP_OBJ_FROM_PTR(self->input_scale);
                break;
            case MP_QSTR_input_zero_point:
                dest[0] = MP_OBJ_FROM_PTR(self->input_zero_point);
                break;
            case MP_QSTR_output_shape:
                dest[0] = MP_OBJ_FROM_PTR(self->output_shape);
                break;
            case MP_QSTR_output_dtype:
                dest[0] = py_ml_dtype_char_tuple(self->output_dtype);
                break;
            case MP_QSTR_output_scale:
                dest[0] = MP_OBJ_FROM_PTR(self->output_scale);
                break;
            case MP_QSTR_output_zero_point:
                dest[0] = MP_OBJ_FROM_PTR(self->output_zero_point);
                break;
            default:
                // Continue lookup in locals_dict.
                dest[1] = MP_OBJ_SENTINEL;
                break;
        }
    }
}

mp_obj_t py_ml_model_make_new(const mp_obj_type_t *type, size_t n_args, size_t n_kw, const mp_obj_t *all_args) {
    enum { ARG_path, ARG_load_to_fb };
    static const mp_arg_t allowed_args[] = {
        { MP_QSTR_path, MP_ARG_REQUIRED | MP_ARG_OBJ },
        { MP_QSTR_load_to_fb, MP_ARG_REQUIRED | MP_ARG_BOOL },
    };

    // Parse args.
    mp_arg_val_t args[MP_ARRAY_SIZE(allowed_args)];
    mp_arg_parse_all_kw_array(n_args, n_kw, all_args, MP_ARRAY_SIZE(allowed_args), allowed_args, args);

    //const char *path = mp_obj_str_get_str(args[ARG_path].u_obj);
    py_ml_model_obj_t *model = mp_obj_malloc_with_finaliser(py_ml_model_obj_t, &py_ml_model_type);

    #if MICROPY_VFS
    mp_obj_t file_args[2] = {
        args[ARG_path].u_obj,
        MP_OBJ_NEW_QSTR(MP_QSTR_rb),
    };

    mp_buffer_info_t bufinfo;
    mp_obj_t file = mp_vfs_open(MP_ARRAY_SIZE(file_args), file_args, (mp_map_t *) &mp_const_empty_map);

    if (mp_get_buffer(file, &bufinfo, MP_BUFFER_READ)) {
        model->size = bufinfo.len;
        model->data = bufinfo.buf;
        model->fb_alloc = false;
    } else {
        int error;
        // Get file size
        mp_off_t res = mp_stream_seek(file, 0, MP_SEEK_END, &error);
        if (res == (mp_off_t) -1) {
            mp_raise_OSError(error);
        }
        if (mp_stream_seek(file, 0, MP_SEEK_SET, &error) == (mp_off_t) -1) {
            mp_raise_OSError(error);
        }

        model->size = res;
        model->fb_alloc = args[ARG_load_to_fb].u_bool;

        // Allocate model data buffer.
        if (model->fb_alloc) {
            // The model's data will Not be free'd on exceptions.
            fb_alloc_mark();
            model->data = fb_alloc(model->size, FB_ALLOC_PREFER_SPEED | FB_ALLOC_CACHE_ALIGN);
            fb_alloc_mark_permanent();
        } else {
            // Align size and memory and keep a reference to the GC block.
            size_t size = (model->size + IMLIB_ML_MODEL_ALIGN) & ~IMLIB_ML_MODEL_ALIGN;
            model->_raw = xalloc(size + IMLIB_ML_MODEL_ALIGN);
            model->data = (void *) (((uintptr_t) model->_raw + IMLIB_ML_MODEL_ALIGN) & ~IMLIB_ML_MODEL_ALIGN);
        }

        // Read file data.
        mp_stream_read_exactly(file, model->data, model->size, &error);
        if (error != 0) {
            mp_raise_OSError(error);
        }
    }
    mp_stream_close(file);
    #else
    mp_raise_msg(&mp_type_RuntimeError, MP_ERROR_TEXT("File I/O is not supported"));
    #endif


    ml_backend_init_model(model);
    return MP_OBJ_FROM_PTR(model);
}

static mp_obj_t py_ml_model_deinit(mp_obj_t self_in) {
    py_ml_model_obj_t *model = MP_OBJ_TO_PTR(self_in);
    if (model->fb_alloc) {
        fb_alloc_free_till_mark_past_mark_permanent();
    }
    return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_1(py_ml_model_deinit_obj, py_ml_model_deinit);

static const mp_rom_map_elem_t py_ml_model_locals_dict_table[] = {
    { MP_ROM_QSTR(MP_QSTR___del__),             MP_ROM_PTR(&py_ml_model_deinit_obj) },
    { MP_ROM_QSTR(MP_QSTR_predict),             MP_ROM_PTR(&py_ml_model_predict_obj) },
};

static MP_DEFINE_CONST_DICT(py_ml_model_locals_dict, py_ml_model_locals_dict_table);

static MP_DEFINE_CONST_OBJ_TYPE(
    py_ml_model_type,
    MP_QSTR_ml_model,
    MP_TYPE_FLAG_NONE,
    attr, py_ml_model_attr,
    print, py_ml_model_print,
    make_new, py_ml_model_make_new,
    locals_dict, &py_ml_model_locals_dict
    );

extern const mp_obj_type_t py_ml_nms_type;

static const mp_rom_map_elem_t py_ml_globals_dict_table[] = {
    { MP_ROM_QSTR(MP_QSTR___name__),            MP_OBJ_NEW_QSTR(MP_QSTR_ml) },
    { MP_ROM_QSTR(MP_QSTR_Model),               MP_ROM_PTR(&py_ml_model_type) },
};

static MP_DEFINE_CONST_DICT(py_ml_globals_dict, py_ml_globals_dict_table);

const mp_obj_module_t ml_module = {
    .base = { &mp_type_module },
    .globals = (mp_obj_t) &py_ml_globals_dict
};

// Alias for backwards compatibility
MP_REGISTER_EXTENSIBLE_MODULE(MP_QSTR_tf, ml_module);
MP_REGISTER_EXTENSIBLE_MODULE(MP_QSTR_ml, ml_module);
#endif // MICROPY_PY_ML
