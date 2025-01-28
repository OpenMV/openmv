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
 * Python helper functions.
 */
#include "py/obj.h"
#include "py/runtime.h"
#include "framebuffer.h"
#include "py_helper.h"
#include "py_assert.h"
#if MICROPY_PY_CSI
#include "omv_csi.h"
#endif

extern void *py_image_cobj(mp_obj_t img_obj);

mp_obj_t py_func_unavailable(size_t n_args, const mp_obj_t *args, mp_map_t *kw_args) {
    PY_ASSERT_TRUE_MSG(false, "This function is unavailable on your OpenMV Cam.");
    return mp_const_none;
}
MP_DEFINE_CONST_FUN_OBJ_KW(py_func_unavailable_obj, 0, py_func_unavailable);

image_t *py_helper_arg_to_image(const mp_obj_t arg, uint32_t flags) {
    image_t *image = NULL;
    if ((flags & ARG_IMAGE_ALLOC) && MP_OBJ_IS_STR(arg)) {
        #if defined(IMLIB_ENABLE_IMAGE_FILE_IO)
        const char *path = mp_obj_str_get_str(arg);
        FIL fp;
        image = xalloc(sizeof(image_t));
        img_read_settings_t rs;
        imlib_read_geometry(&fp, image, path, &rs);
        file_close(&fp);
        image->data = fb_alloc(image_size(image), FB_ALLOC_CACHE_ALIGN);
        imlib_load_image(image, path);
        #else
        mp_raise_msg(&mp_type_OSError, MP_ERROR_TEXT("Image I/O is not supported"));
        #endif // IMLIB_ENABLE_IMAGE_FILE_IO
    } else {
        image = py_image_cobj(arg);
    }
    if (flags) {
        if ((flags & ARG_IMAGE_MUTABLE) && !image->is_mutable) {
            mp_raise_msg(&mp_type_ValueError, MP_ERROR_TEXT("Expected a mutable image"));
        } else if ((flags & ARG_IMAGE_UNCOMPRESSED) && image->is_compressed) {
            mp_raise_msg(&mp_type_ValueError, MP_ERROR_TEXT("Expected an uncompressed image"));
        } else if ((flags & ARG_IMAGE_GRAYSCALE) && image->pixfmt != PIXFORMAT_GRAYSCALE) {
            mp_raise_msg(&mp_type_ValueError, MP_ERROR_TEXT("Expected an uncompressed image"));
        }
    }
    return image;
}

const void *py_helper_arg_to_palette(const mp_obj_t arg, uint32_t pixfmt) {
    const void *palette = NULL;
    if (mp_obj_is_int(arg)) {
        uint32_t type = mp_obj_get_int(arg);
        if (type == COLOR_PALETTE_RAINBOW) {
            palette = rainbow_table;
        } else if (type == COLOR_PALETTE_IRONBOW) {
            palette = ironbow_table;
        #if (MICROPY_PY_TOF == 1)
        } else if (type == COLOR_PALETTE_DEPTH) {
            palette = depth_table;
        #endif // MICROPY_PY_TOF == 1
        #if (OMV_GENX320_ENABLE == 1)
        } else if (type == COLOR_PALETTE_EVT_DARK) {
            palette = evt_dark_table;
        } else if (type == COLOR_PALETTE_EVT_LIGHT) {
            palette = evt_light_table;
        #endif // OMV_GENX320_ENABLE == 1
        } else {
            mp_raise_msg(&mp_type_ValueError, MP_ERROR_TEXT("Invalid color palette"));
        }
    } else if (arg != mp_const_none) {
        image_t *img = py_helper_arg_to_image(arg, ARG_IMAGE_MUTABLE);
        if (img->pixfmt != pixfmt) {
            mp_raise_msg(&mp_type_ValueError, MP_ERROR_TEXT("Unexpcted color palette format"));
        }
        if ((img->w * img->h) != 256) {
            mp_raise_msg(&mp_type_ValueError, MP_ERROR_TEXT("Color palette must be 256 pixels"));
        }
        palette = img->data;
    }
    return palette;
}

rectangle_t py_helper_arg_to_roi(const mp_obj_t arg, const image_t *img) {
    rectangle_t roi = {0, 0, img->w, img->h};
    if (arg != mp_const_none) {
        mp_obj_t *arg_roi;
        mp_obj_get_array_fixed_n(arg, 4, &arg_roi);
        roi.x = mp_obj_get_int(arg_roi[0]);
        roi.y = mp_obj_get_int(arg_roi[1]);
        roi.w = mp_obj_get_int(arg_roi[2]);
        roi.h = mp_obj_get_int(arg_roi[3]);

        PY_ASSERT_TRUE_MSG((roi.w >= 1) && (roi.h >= 1), "Invalid ROI dimensions!");
        rectangle_t bounds = {0, 0, img->w, img->h};
        PY_ASSERT_TRUE_MSG(rectangle_overlap(&roi, &bounds), "ROI does not overlap on the image!");
        rectangle_intersected(&roi, &bounds);
    }
    return roi;
}

void py_helper_arg_to_scale(const mp_obj_t arg_x_scale, const mp_obj_t arg_y_scale,
                            float *x_scale, float *y_scale) {
    if (arg_x_scale != mp_const_none) {
        *x_scale = mp_obj_get_float(arg_x_scale);
    }
    if (arg_y_scale != mp_const_none) {
        *y_scale = mp_obj_get_float(arg_y_scale);
    }

    if (arg_x_scale == mp_const_none && arg_y_scale != mp_const_none) {
        *x_scale = *y_scale;
    } else if (arg_y_scale == mp_const_none && arg_x_scale != mp_const_none) {
        *y_scale = *x_scale;
    }
}

void py_helper_arg_to_minmax(const mp_obj_t minmax, float *min, float *max,
                             const mp_obj_t *array, size_t array_size) {
    float min_out = FLT_MAX;
    float max_out = -FLT_MAX;

    if (minmax != mp_const_none) {
        mp_obj_t *arg_scale;
        mp_obj_get_array_fixed_n(minmax, 2, &arg_scale);
        min_out = mp_obj_get_float(arg_scale[0]);
        max_out = mp_obj_get_float(arg_scale[1]);
    } else if (array && array_size) {
        for (int i = 0; i < array_size; i++) {
            float t = mp_obj_get_float(array[i]);
            if (t < min_out) {
                min_out = t;
            }
            if (t > max_out) {
                max_out = t;
            }
        }
    }

    *min = min_out;
    *max = max_out;
}

float py_helper_arg_to_float(const mp_obj_t arg, float default_value) {
    if (arg != mp_const_none) {
        return mp_obj_get_float(arg);
    }
    return default_value;
}

void py_helper_arg_to_float_array(const mp_obj_t arg, float *array, size_t size) {
    if (arg != mp_const_none) {
        mp_obj_t *arg_array;
        mp_obj_get_array_fixed_n(arg, size, &arg_array);
        for (int i = 0; i < size; i++) {
            array[i] = mp_obj_get_float(arg_array[i]);
        }
    }
}

image_t *py_helper_keyword_to_image(uint n_args, const mp_obj_t *args, uint arg_index,
                                    mp_map_t *kw_args, mp_obj_t kw, image_t *default_val) {
    mp_map_elem_t *kw_arg = mp_map_lookup(kw_args, kw, MP_MAP_LOOKUP);

    if (kw_arg) {
        default_val = py_helper_arg_to_image(kw_arg->value, ARG_IMAGE_MUTABLE);
    } else if (n_args > arg_index) {
        default_val = py_helper_arg_to_image(args[arg_index], ARG_IMAGE_MUTABLE);
    }

    return default_val;
}

void py_helper_keyword_rectangle(image_t *img, uint n_args, const mp_obj_t *args, uint arg_index,
                                 mp_map_t *kw_args, mp_obj_t kw, rectangle_t *r) {
    mp_map_elem_t *kw_arg = mp_map_lookup(kw_args, kw, MP_MAP_LOOKUP);

    if (kw_arg) {
        mp_obj_t *arg_rectangle;
        mp_obj_get_array_fixed_n(kw_arg->value, 4, &arg_rectangle);
        r->x = mp_obj_get_int(arg_rectangle[0]);
        r->y = mp_obj_get_int(arg_rectangle[1]);
        r->w = mp_obj_get_int(arg_rectangle[2]);
        r->h = mp_obj_get_int(arg_rectangle[3]);
    } else if (n_args > arg_index) {
        mp_obj_t *arg_rectangle;
        mp_obj_get_array_fixed_n(args[arg_index], 4, &arg_rectangle);
        r->x = mp_obj_get_int(arg_rectangle[0]);
        r->y = mp_obj_get_int(arg_rectangle[1]);
        r->w = mp_obj_get_int(arg_rectangle[2]);
        r->h = mp_obj_get_int(arg_rectangle[3]);
    } else {
        r->x = 0;
        r->y = 0;
        r->w = img->w;
        r->h = img->h;
    }

    PY_ASSERT_TRUE_MSG((r->w >= 1) && (r->h >= 1), "Invalid ROI dimensions!");
    rectangle_t temp;
    temp.x = 0;
    temp.y = 0;
    temp.w = img->w;
    temp.h = img->h;

    PY_ASSERT_TRUE_MSG(rectangle_overlap(r, &temp), "ROI does not overlap on the image!");
    rectangle_intersected(r, &temp);
}

void py_helper_keyword_rectangle_roi(image_t *img, uint n_args, const mp_obj_t *args, uint arg_index,
                                     mp_map_t *kw_args, rectangle_t *r) {
    py_helper_keyword_rectangle(img, n_args, args, arg_index, kw_args, MP_OBJ_NEW_QSTR(MP_QSTR_roi), r);
}

int py_helper_keyword_int(uint n_args, const mp_obj_t *args, uint arg_index,
                          mp_map_t *kw_args, mp_obj_t kw, int default_val) {
    mp_map_elem_t *kw_arg = mp_map_lookup(kw_args, kw, MP_MAP_LOOKUP);

    if (kw_arg) {
        default_val = mp_obj_get_int(kw_arg->value);
    } else if (n_args > arg_index) {
        default_val = mp_obj_get_int(args[arg_index]);
    }

    return default_val;
}

bool py_helper_keyword_int_maybe(uint n_args, const mp_obj_t *args, uint arg_index,
                                 mp_map_t *kw_args, mp_obj_t kw, int *value) {
    mp_map_elem_t *kw_arg = mp_map_lookup(kw_args, kw, MP_MAP_LOOKUP);

    if (kw_arg) {
        return mp_obj_get_int_maybe(kw_arg->value, value);
    } else if (n_args > arg_index) {
        return mp_obj_get_int_maybe(args[arg_index], value);
    }

    return false;
}

float py_helper_keyword_float(uint n_args, const mp_obj_t *args, uint arg_index,
                              mp_map_t *kw_args, mp_obj_t kw, float default_val) {
    mp_map_elem_t *kw_arg = mp_map_lookup(kw_args, kw, MP_MAP_LOOKUP);

    if (kw_arg) {
        default_val = mp_obj_get_float(kw_arg->value);
    } else if (n_args > arg_index) {
        default_val = mp_obj_get_float(args[arg_index]);
    }

    return default_val;
}

bool py_helper_keyword_float_maybe(uint n_args, const mp_obj_t *args, uint arg_index,
                                   mp_map_t *kw_args, mp_obj_t kw, float *value) {
    mp_map_elem_t *kw_arg = mp_map_lookup(kw_args, kw, MP_MAP_LOOKUP);

    if (kw_arg) {
        return mp_obj_get_float_maybe(kw_arg->value, value);
    } else if (n_args > arg_index) {
        return mp_obj_get_float_maybe(args[arg_index], value);
    }

    return false;
}

void py_helper_keyword_int_array(uint n_args, const mp_obj_t *args, uint arg_index,
                                 mp_map_t *kw_args, mp_obj_t kw, int *x, int size) {
    mp_map_elem_t *kw_arg = mp_map_lookup(kw_args, kw, MP_MAP_LOOKUP);

    if (kw_arg) {
        mp_obj_t *arg_array;
        mp_obj_get_array_fixed_n(kw_arg->value, size, &arg_array);
        for (int i = 0; i < size; i++) {
            x[i] = mp_obj_get_int(arg_array[i]);
        }
    } else if (n_args > arg_index) {
        mp_obj_t *arg_array;
        mp_obj_get_array_fixed_n(args[arg_index], size, &arg_array);
        for (int i = 0; i < size; i++) {
            x[i] = mp_obj_get_int(arg_array[i]);
        }
    }
}

float *py_helper_keyword_corner_array(uint n_args, const mp_obj_t *args, uint arg_index,
                                      mp_map_t *kw_args, mp_obj_t kw) {
    mp_map_elem_t *kw_arg = mp_map_lookup(kw_args, kw, MP_MAP_LOOKUP);

    if (kw_arg) {
        mp_obj_t *arg_array;
        mp_obj_get_array_fixed_n(kw_arg->value, 4, &arg_array);
        float *corners = xalloc(sizeof(float) * 8);
        for (int i = 0; i < 4; i++) {
            mp_obj_t *arg_point;
            mp_obj_get_array_fixed_n(arg_array[i], 2, &arg_point);
            corners[(i * 2) + 0] = mp_obj_get_float(arg_point[0]);
            corners[(i * 2) + 1] = mp_obj_get_float(arg_point[1]);
        }
        return corners;
    } else if (n_args > arg_index) {
        mp_obj_t *arg_array;
        mp_obj_get_array_fixed_n(args[arg_index], 4, &arg_array);
        float *corners = xalloc(sizeof(float) * 8);
        for (int i = 0; i < 4; i++) {
            mp_obj_t *arg_point;
            mp_obj_get_array_fixed_n(arg_array[i], 2, &arg_point);
            corners[(i * 2) + 0] = mp_obj_get_float(arg_point[0]);
            corners[(i * 2) + 1] = mp_obj_get_float(arg_point[1]);
        }
        return corners;
    }

    return NULL;
}

uint py_helper_consume_array(uint n_args, const mp_obj_t *args, uint arg_index, size_t len, const mp_obj_t **items) {
    if (MP_OBJ_IS_TYPE(args[arg_index], &mp_type_tuple) || MP_OBJ_IS_TYPE(args[arg_index], &mp_type_list)) {
        mp_obj_get_array_fixed_n(args[arg_index], len, (mp_obj_t **) items);
        return arg_index + 1;
    } else {
        PY_ASSERT_TRUE_MSG((n_args - arg_index) >= len, "Not enough positional arguments!");
        *items = args + arg_index;
        return arg_index + len;
    }
}

int py_helper_keyword_color(image_t *img, uint n_args, const mp_obj_t *args, uint arg_index,
                            mp_map_t *kw_args, int default_val) {
    mp_map_elem_t *kw_arg = kw_args ? mp_map_lookup(kw_args, MP_OBJ_NEW_QSTR(MP_QSTR_color), MP_MAP_LOOKUP) : NULL;

    if (kw_arg) {
        if (mp_obj_is_integer(kw_arg->value)) {
            default_val = mp_obj_get_int(kw_arg->value);
        } else {
            mp_obj_t *arg_color;
            mp_obj_get_array_fixed_n(kw_arg->value, 3, &arg_color);
            default_val = COLOR_R8_G8_B8_TO_RGB565(__USAT(mp_obj_get_int(arg_color[0]), 8),
                                                   __USAT(mp_obj_get_int(arg_color[1]), 8),
                                                   __USAT(mp_obj_get_int(arg_color[2]), 8));
            switch (img->pixfmt) {
                case PIXFORMAT_BINARY: {
                    default_val = COLOR_RGB565_TO_BINARY(default_val);
                    break;
                }
                case PIXFORMAT_GRAYSCALE: {
                    default_val = COLOR_RGB565_TO_GRAYSCALE(default_val);
                    break;
                }
                default: {
                    break;
                }
            }
        }
    } else if (n_args > arg_index) {
        if (mp_obj_is_integer(args[arg_index])) {
            default_val = mp_obj_get_int(args[arg_index]);
        } else {
            mp_obj_t *arg_color;
            mp_obj_get_array_fixed_n(args[arg_index], 3, &arg_color);
            default_val = COLOR_R8_G8_B8_TO_RGB565(__USAT(mp_obj_get_int(arg_color[0]), 8),
                                                   __USAT(mp_obj_get_int(arg_color[1]), 8),
                                                   __USAT(mp_obj_get_int(arg_color[2]), 8));
            switch (img->pixfmt) {
                case PIXFORMAT_BINARY: {
                    default_val = COLOR_RGB565_TO_BINARY(default_val);
                    break;
                }
                case PIXFORMAT_GRAYSCALE: {
                    default_val = COLOR_RGB565_TO_GRAYSCALE(default_val);
                    break;
                }
                default: {
                    break;
                }
            }
        }
    }

    return default_val;
}

void py_helper_arg_to_thresholds(const mp_obj_t arg, list_t *thresholds) {
    mp_uint_t arg_thresholds_len;
    mp_obj_t *arg_thresholds;
    mp_obj_get_array(arg, &arg_thresholds_len, &arg_thresholds);
    if (!arg_thresholds_len) {
        return;
    }
    for (mp_uint_t i = 0; i < arg_thresholds_len; i++) {
        mp_uint_t arg_threshold_len;
        mp_obj_t *arg_threshold;
        mp_obj_get_array(arg_thresholds[i], &arg_threshold_len, &arg_threshold);
        if (arg_threshold_len) {
            color_thresholds_list_lnk_data_t lnk_data;
            lnk_data.LMin = (arg_threshold_len > 0) ? __USAT(mp_obj_get_int(arg_threshold[0]), 8) :
                            IM_MIN(COLOR_L_MIN, COLOR_GRAYSCALE_MIN);
            lnk_data.LMax = (arg_threshold_len > 1) ? __USAT(mp_obj_get_int(arg_threshold[1]), 8) :
                            IM_MAX(COLOR_L_MAX, COLOR_GRAYSCALE_MAX);
            lnk_data.AMin =
                (arg_threshold_len > 2) ? __SSAT(mp_obj_get_int(arg_threshold[2]), 8) : COLOR_A_MIN;
            lnk_data.AMax =
                (arg_threshold_len > 3) ? __SSAT(mp_obj_get_int(arg_threshold[3]), 8) : COLOR_A_MAX;
            lnk_data.BMin =
                (arg_threshold_len > 4) ? __SSAT(mp_obj_get_int(arg_threshold[4]), 8) : COLOR_B_MIN;
            lnk_data.BMax =
                (arg_threshold_len > 5) ? __SSAT(mp_obj_get_int(arg_threshold[5]), 8) : COLOR_B_MAX;
            color_thresholds_list_lnk_data_t lnk_data_tmp;
            memcpy(&lnk_data_tmp, &lnk_data, sizeof(color_thresholds_list_lnk_data_t));
            lnk_data.LMin = IM_MIN(lnk_data_tmp.LMin, lnk_data_tmp.LMax);
            lnk_data.LMax = IM_MAX(lnk_data_tmp.LMin, lnk_data_tmp.LMax);
            lnk_data.AMin = IM_MIN(lnk_data_tmp.AMin, lnk_data_tmp.AMax);
            lnk_data.AMax = IM_MAX(lnk_data_tmp.AMin, lnk_data_tmp.AMax);
            lnk_data.BMin = IM_MIN(lnk_data_tmp.BMin, lnk_data_tmp.BMax);
            lnk_data.BMax = IM_MAX(lnk_data_tmp.BMin, lnk_data_tmp.BMax);
            list_push_back(thresholds, &lnk_data);
        }
    }
}

void py_helper_keyword_thresholds(uint n_args, const mp_obj_t *args, uint arg_index,
                                  mp_map_t *kw_args, list_t *thresholds) {
    mp_map_elem_t *kw_arg = mp_map_lookup(kw_args, MP_OBJ_NEW_QSTR(MP_QSTR_thresholds), MP_MAP_LOOKUP);

    if (kw_arg) {
        py_helper_arg_to_thresholds(kw_arg->value, thresholds);
    } else if (n_args > arg_index) {
        py_helper_arg_to_thresholds(args[arg_index], thresholds);
    }
}

int py_helper_arg_to_ksize(const mp_obj_t arg) {
    int ksize = mp_obj_get_int(arg);
    PY_ASSERT_TRUE_MSG(ksize >= 0, "KernelSize must be >= 0!");
    return ksize;
}

mp_obj_t py_helper_keyword_object(uint n_args, const mp_obj_t *args,
                                  uint arg_index, mp_map_t *kw_args, mp_obj_t kw, mp_obj_t default_val) {
    mp_map_elem_t *kw_arg = mp_map_lookup(kw_args, kw, MP_MAP_LOOKUP);

    if (kw_arg) {
        return kw_arg->value;
    } else if (n_args > arg_index) {
        return args[arg_index];
    } else {
        return default_val;
    }
}

const uint16_t *py_helper_keyword_color_palette(uint n_args, const mp_obj_t *args,
                                                uint arg_index, mp_map_t *kw_args, const uint16_t *default_color_palette) {
    int palette;

    mp_map_elem_t *kw_arg =
        mp_map_lookup(kw_args, MP_OBJ_NEW_QSTR(MP_QSTR_color_palette), MP_MAP_LOOKUP);

    if (kw_arg && (kw_arg->value == mp_const_none)) {
        default_color_palette = NULL;
    } else if ((n_args > arg_index) && (args[arg_index] == mp_const_none)) {
        default_color_palette = NULL;
    } else if (py_helper_keyword_int_maybe(n_args, args, arg_index, kw_args,
                                           MP_OBJ_NEW_QSTR(MP_QSTR_color_palette), &palette)) {
        if (palette == COLOR_PALETTE_RAINBOW) {
            default_color_palette = rainbow_table;
        } else if (palette == COLOR_PALETTE_IRONBOW) {
            default_color_palette = ironbow_table;
        #if (MICROPY_PY_TOF == 1)
        } else if (palette == COLOR_PALETTE_DEPTH) {
            default_color_palette = depth_table;
        #endif // MICROPY_PY_TOF == 1
        #if (OMV_GENX320_ENABLE == 1)
        } else if (palette == COLOR_PALETTE_EVT_DARK) {
            default_color_palette = evt_dark_table;
        } else if (palette == COLOR_PALETTE_EVT_LIGHT) {
            default_color_palette = evt_light_table;
        #endif // OMV_GENX320_ENABLE == 1
        } else {
            mp_raise_msg(&mp_type_ValueError,
                         MP_ERROR_TEXT("Invalid pre-defined color palette!"));
        }
    } else {
        image_t *arg_color_palette =
            py_helper_keyword_to_image(n_args, args, arg_index, kw_args, MP_OBJ_NEW_QSTR(MP_QSTR_color_palette), NULL);

        if (arg_color_palette) {
            if (arg_color_palette->pixfmt != PIXFORMAT_RGB565) {
                mp_raise_msg(&mp_type_ValueError,
                             MP_ERROR_TEXT("Color palette must be RGB565!"));
            }

            if ((arg_color_palette->w * arg_color_palette->h) != 256) {
                mp_raise_msg(&mp_type_ValueError,
                             MP_ERROR_TEXT("Color palette must be 256 pixels!"));
            }

            default_color_palette = (uint16_t *) arg_color_palette->data;
        }
    }

    return default_color_palette;
}

const uint8_t *py_helper_keyword_alpha_palette(uint n_args, const mp_obj_t *args,
                                               uint arg_index, mp_map_t *kw_args, const uint8_t *default_alpha_palette) {
    image_t *arg_alpha_palette =
        py_helper_keyword_to_image(n_args, args, 9, kw_args, MP_OBJ_NEW_QSTR(MP_QSTR_alpha_palette), NULL);

    if (arg_alpha_palette) {
        if (arg_alpha_palette->pixfmt != PIXFORMAT_GRAYSCALE) {
            mp_raise_msg(&mp_type_ValueError,
                         MP_ERROR_TEXT("Alpha palette must be GRAYSCALE!"));
        }

        if ((arg_alpha_palette->w * arg_alpha_palette->h) != 256) {
            mp_raise_msg(&mp_type_ValueError,
                         MP_ERROR_TEXT("Alpha palette must be 256 pixels!"));
        }

        default_alpha_palette = (uint8_t *) arg_alpha_palette->data;
    }

    return default_alpha_palette;
}

bool py_helper_is_equal_to_framebuffer(image_t *img) {
    return framebuffer_get_buffer(framebuffer->head)->data == img->data;
}

void py_helper_update_framebuffer(image_t *img) {
    if (py_helper_is_equal_to_framebuffer(img)) {
        framebuffer_init_from_image(img);
    }
}

void py_helper_set_to_framebuffer(image_t *img) {
    #if MICROPY_PY_CSI
    omv_csi_set_framebuffers(1);
    #else
    framebuffer_set_buffers(1);
    #endif

    PY_ASSERT_TRUE_MSG((image_size(img) <= framebuffer_get_buffer_size()),
                       "The image doesn't fit in the frame buffer!");
    framebuffer_init_from_image(img);
    img->data = framebuffer_get_buffer(framebuffer->head)->data;
}
