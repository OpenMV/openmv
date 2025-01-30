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
 * Image I/O Python module.
 */
#include "imlib_config.h"
#if defined(IMLIB_ENABLE_IMAGE_IO)

#include "py/obj.h"
#include "py/nlr.h"
#include "py/mphal.h"
#include "py/runtime.h"

#include "py_assert.h"
#include "py_helper.h"
#include "py_image.h"
#include "py_imageio.h"

#if defined(IMLIB_ENABLE_IMAGE_FILE_IO)
#include "file_utils.h"
#endif
#include "framebuffer.h"
#include "omv_boardconfig.h"

#define OLD_BINARY_BPP          0
#define OLD_GRAYSCALE_BPP       1
#define OLD_RGB565_BPP          2
#define OLD_BAYER_BPP           3
#define OLD_JPG_BPP             4

#define MAGIC_SIZE              16
#define ALIGN_SIZE              16
#define AFTER_SIZE_PADDING      12

#define ORIGINAL_VER            10
#define RGB565_FIXED_VER        11
#define NEW_PIXFORMAT_VER       20

#ifndef __DCACHE_PRESENT
#define IMAGE_ALIGNMENT         32 // Use 32-byte alignment on MCUs with no cache for DMA buffer alignment.
#else
#define IMAGE_ALIGNMENT         __SCB_DCACHE_LINE_SIZE
#endif

#define IMAGE_T_SIZE_ALIGNED    (((sizeof(uint32_t) + sizeof(image_t) + (IMAGE_ALIGNMENT) -1) \
                                  / (IMAGE_ALIGNMENT))                                        \
                                 * (IMAGE_ALIGNMENT))

static size_t image_size_aligned(image_t *image) {
    return ((image_size(image) + (IMAGE_ALIGNMENT) -1) / (IMAGE_ALIGNMENT)) * (IMAGE_ALIGNMENT);
}

typedef enum image_io_stream_type {
    IMAGE_IO_FILE_STREAM,
    IMAGE_IO_MEMORY_STREAM,
} image_io_stream_type_t;

typedef struct py_imageio_obj {
    mp_obj_base_t base;
    image_io_stream_type_t type;
    bool closed;
    uint32_t count;
    uint32_t offset;
    uint32_t ms;
    union {
        #if defined(IMLIB_ENABLE_IMAGE_FILE_IO)
        struct {
            FIL fp;
            int version;
        };
        #endif
        struct {
            uint32_t size;
            uint8_t *buffer;
        };
    };
} py_imageio_obj_t;

static py_imageio_obj_t *py_imageio_obj(mp_obj_t self) {
    py_imageio_obj_t *stream = MP_OBJ_TO_PTR(self);

    if (stream->closed) {
        mp_raise_msg(&mp_type_OSError, MP_ERROR_TEXT("Stream closed"));
    }

    return stream;
}

static void py_imageio_print(const mp_print_t *print, mp_obj_t self, mp_print_kind_t kind) {
    py_imageio_obj_t *stream = MP_OBJ_TO_PTR(self);
    mp_printf(print, "{\"type\":%s, \"closed\":%s, \"count\":%u, \"offset\":%u, "
              "\"version\":%u, \"buffer_size\":%u, \"size\":%u}",
              (stream->type == IMAGE_IO_FILE_STREAM) ? "\"file stream\"" : "\"memory stream\"",
              stream->closed ? "\"true\"" : "\"false\"",
              stream->count,
              stream->offset,
              #if defined(IMLIB_ENABLE_IMAGE_FILE_IO)
              (stream->type == IMAGE_IO_FILE_STREAM)  ? stream->version : 0,
              #else
              0,
              #endif
              (stream->type == IMAGE_IO_FILE_STREAM) ? 0 : stream->size,
              #if defined(IMLIB_ENABLE_IMAGE_FILE_IO)
              (stream->type == IMAGE_IO_FILE_STREAM) ? f_size(&stream->fp) : (stream->count * stream->size));
              #else
              stream->count * stream->size);
              #endif
}

static mp_obj_t py_imageio_get_type(mp_obj_t self) {
    py_imageio_obj_t *stream = MP_OBJ_TO_PTR(self);
    return mp_obj_new_int(stream->type);
}
static MP_DEFINE_CONST_FUN_OBJ_1(py_imageio_get_type_obj, py_imageio_get_type);

static mp_obj_t py_imageio_is_closed(mp_obj_t self) {
    py_imageio_obj_t *stream = MP_OBJ_TO_PTR(self);
    return mp_obj_new_int(stream->closed);
}
static MP_DEFINE_CONST_FUN_OBJ_1(py_imageio_is_closed_obj, py_imageio_is_closed);

static mp_obj_t py_imageio_count(mp_obj_t self) {
    py_imageio_obj_t *stream = MP_OBJ_TO_PTR(self);
    return mp_obj_new_int(stream->count);
}
static MP_DEFINE_CONST_FUN_OBJ_1(py_imageio_count_obj, py_imageio_count);

static mp_obj_t py_imageio_offset(mp_obj_t self) {
    py_imageio_obj_t *stream = MP_OBJ_TO_PTR(self);
    return mp_obj_new_int(stream->offset);
}
static MP_DEFINE_CONST_FUN_OBJ_1(py_imageio_offset_obj, py_imageio_offset);

#if defined(IMLIB_ENABLE_IMAGE_FILE_IO)
static mp_obj_t py_imageio_version(mp_obj_t self) {
    py_imageio_obj_t *stream = MP_OBJ_TO_PTR(self);
    return (stream->type == IMAGE_IO_FILE_STREAM) ?  mp_obj_new_int(stream->version) : mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_1(py_imageio_version_obj, py_imageio_version);
#endif

static mp_obj_t py_imageio_buffer_size(mp_obj_t self) {
    py_imageio_obj_t *stream = MP_OBJ_TO_PTR(self);

    #if defined(IMLIB_ENABLE_IMAGE_FILE_IO)
    if (stream->type == IMAGE_IO_FILE_STREAM) {
        return mp_const_none;
    }
    #endif

    return mp_obj_new_int(stream->size - IMAGE_T_SIZE_ALIGNED);
}
static MP_DEFINE_CONST_FUN_OBJ_1(py_imageio_buffer_size_obj, py_imageio_buffer_size);

static mp_obj_t py_imageio_size(mp_obj_t self) {
    py_imageio_obj_t *stream = MP_OBJ_TO_PTR(self);

    #if defined(IMLIB_ENABLE_IMAGE_FILE_IO)
    if (stream->type == IMAGE_IO_FILE_STREAM) {
        return mp_obj_new_int(f_size(&stream->fp));
    }
    #endif

    return mp_obj_new_int(stream->count * stream->size);
}
static MP_DEFINE_CONST_FUN_OBJ_1(py_imageio_size_obj, py_imageio_size);

static mp_obj_t py_imageio_write(mp_obj_t self, mp_obj_t img_obj) {
    py_imageio_obj_t *stream = py_imageio_obj(self);
    image_t *image = py_image_cobj(img_obj);

    uint32_t ms = mp_hal_ticks_ms(), elapsed_ms = ms - stream->ms;
    stream->ms = ms;

    if (0) {
    #if defined(IMLIB_ENABLE_IMAGE_FILE_IO)
    } else if (stream->type == IMAGE_IO_FILE_STREAM) {
        FIL *fp = &stream->fp;

        file_write_long(fp, elapsed_ms);
        file_write_long(fp, image->w);
        file_write_long(fp, image->h);

        char padding[ALIGN_SIZE] = {};

        if (stream->version < NEW_PIXFORMAT_VER) {
            if (image->pixfmt == PIXFORMAT_BINARY) {
                file_write_long(fp, OLD_BINARY_BPP);
            } else if (image->pixfmt == PIXFORMAT_GRAYSCALE) {
                file_write_long(fp, OLD_GRAYSCALE_BPP);
            } else if (image->pixfmt == PIXFORMAT_RGB565) {
                file_write_long(fp, OLD_RGB565_BPP);
            } else if (image->pixfmt == PIXFORMAT_BAYER) {
                file_write_long(fp, OLD_BAYER_BPP);
            } else if (image->pixfmt == PIXFORMAT_JPEG) {
                file_write_long(fp, image->size);
            } else {
                mp_raise_msg(&mp_type_ValueError, MP_ERROR_TEXT("Invalid image stream bpp"));
            }
        } else {
            file_write_long(fp, image->pixfmt);
            file_write_long(fp, image->size);
            file_write(fp, padding, AFTER_SIZE_PADDING);
        }

        uint32_t size = image_size(image);
        file_write(fp, image->data, size);

        if (size % ALIGN_SIZE) {
            file_write(fp, padding, ALIGN_SIZE - (size % ALIGN_SIZE));
        }

        // Seeking to the middle of a file and writing data corrupts the remainder of the file. So,
        // truncate the rest of the file when this happens to prevent crashing because of this.
        if (!f_eof(fp)) {
            file_truncate(fp);
        }

        stream->count = stream->offset + 1;
    #endif
    } else if (stream->type == IMAGE_IO_MEMORY_STREAM) {
        if (stream->offset == stream->count) {
            mp_raise_msg(&mp_type_EOFError, MP_ERROR_TEXT("End of stream"));
        }

        uint32_t size = image_size(image);

        if (stream->size < size) {
            mp_raise_msg(&mp_type_ValueError, MP_ERROR_TEXT("Invalid frame size"));
        }

        *((uint32_t *) (stream->buffer + (stream->offset * stream->size))) = elapsed_ms;
        memcpy(stream->buffer + (stream->offset * stream->size) + sizeof(uint32_t), image, sizeof(image_t));
        memcpy(stream->buffer + (stream->offset * stream->size) + IMAGE_T_SIZE_ALIGNED, image->data, size);
    }

    stream->offset += 1;

    return self;
}
static MP_DEFINE_CONST_FUN_OBJ_2(py_imageio_write_obj, py_imageio_write);

static void int_py_imageio_pause(py_imageio_obj_t *stream, bool pause) {
    uint32_t elapsed_ms;

    if (0) {
    #if defined(IMLIB_ENABLE_IMAGE_FILE_IO)
    } else if (stream->type == IMAGE_IO_FILE_STREAM) {
        file_read(&stream->fp, &elapsed_ms, 4);
    #endif
    } else if (stream->type == IMAGE_IO_MEMORY_STREAM) {
        elapsed_ms = *((uint32_t *) (stream->buffer + (stream->offset * stream->size)));
    }

    while (pause && ((mp_hal_ticks_ms() - stream->ms) < elapsed_ms)) {
        __WFI();
    }

    stream->ms += elapsed_ms;
}

#if defined(IMLIB_ENABLE_IMAGE_FILE_IO)
static void int_py_imageio_read_chunk(py_imageio_obj_t *stream, image_t *image, bool pause) {
    FIL *fp = &stream->fp;

    if (f_eof(fp)) {
        mp_raise_msg(&mp_type_EOFError, MP_ERROR_TEXT("End of stream"));
    }

    int_py_imageio_pause(stream, pause);

    file_read(fp, &image->w, 4);
    file_read(fp, &image->h, 4);

    uint32_t bpp;
    file_read(fp, &bpp, 4);

    if (stream->version < NEW_PIXFORMAT_VER) {
        if (bpp < 0) {
            mp_raise_msg(&mp_type_OSError, MP_ERROR_TEXT("Invalid image stream bpp"));
        } else if (bpp == OLD_BINARY_BPP) {
            image->pixfmt = PIXFORMAT_BINARY;
        } else if (bpp == OLD_GRAYSCALE_BPP) {
            image->pixfmt = PIXFORMAT_GRAYSCALE;
        } else if (bpp == OLD_RGB565_BPP) {
            image->pixfmt = PIXFORMAT_RGB565;
        } else if (bpp == OLD_BAYER_BPP) {
            image->pixfmt = PIXFORMAT_BAYER;
        } else if (bpp >= OLD_JPG_BPP) {
            image->pixfmt = PIXFORMAT_JPEG;
        }
    } else {
        if (!IMLIB_PIXFORMAT_IS_VALID(bpp)) {
            mp_raise_msg(&mp_type_OSError, MP_ERROR_TEXT("Invalid image stream pixformat"));
        }

        image->pixfmt = bpp;
        file_read(fp, &image->size, 4);

        char ignore[AFTER_SIZE_PADDING];
        file_read(fp, ignore, AFTER_SIZE_PADDING);
    }
}
#endif

static mp_obj_t py_imageio_read(size_t n_args, const mp_obj_t *pos_args, mp_map_t *kw_args) {
    enum { ARG_copy_to_fb, ARG_loop, ARG_pause };
    static const mp_arg_t allowed_args[] = {
        { MP_QSTR_copy_to_fb, MP_ARG_INT,  {.u_bool = true } },
        { MP_QSTR_loop, MP_ARG_INT | MP_ARG_KW_ONLY,  {.u_bool = true } },
        { MP_QSTR_pause, MP_ARG_INT | MP_ARG_KW_ONLY,  {.u_bool = true } },
    };

    // Parse args.
    py_imageio_obj_t *stream = py_imageio_obj(pos_args[0]);
    mp_arg_val_t args[MP_ARRAY_SIZE(allowed_args)];
    mp_arg_parse_all(n_args - 1, pos_args + 1, kw_args, MP_ARRAY_SIZE(allowed_args), allowed_args, args);

    image_t image = { 0 };

    if (0) {
    #if defined(IMLIB_ENABLE_IMAGE_FILE_IO)
    } else if (stream->type == IMAGE_IO_FILE_STREAM) {
        FIL *fp = &stream->fp;

        if (f_eof(fp)) {
            if (args[ARG_loop].u_bool == false) {
                return mp_const_none;
            }
            // Skip the header
            file_seek(fp, MAGIC_SIZE);

            stream->offset = 0;

            if (f_eof(fp)) {
                // Empty file
                return mp_const_none;
            }
        }

        int_py_imageio_read_chunk(stream, &image, args[ARG_pause].u_bool);
    #endif
    } else if (stream->type == IMAGE_IO_MEMORY_STREAM) {
        if (stream->offset == stream->count) {
            mp_raise_msg(&mp_type_EOFError, MP_ERROR_TEXT("End of stream"));
        }

        int_py_imageio_pause(stream, args[ARG_pause].u_bool);
        memcpy(&image, stream->buffer + (stream->offset * stream->size) + sizeof(uint32_t), sizeof(image_t));
    }

    uint32_t size = image_size(&image);

    if (args[ARG_copy_to_fb].u_bool) {
        py_helper_set_to_framebuffer(&image);
    } else {
        image_xalloc(&image, size);
    }

    if (0) {
    #if defined(IMLIB_ENABLE_IMAGE_FILE_IO)
    } else if (stream->type == IMAGE_IO_FILE_STREAM) {
        FIL *fp = &stream->fp;
        file_read(fp, image.data, size);

        // Check if original byte reversed data.
        if ((image.pixfmt == PIXFORMAT_RGB565) && (stream->version == ORIGINAL_VER)) {
            uint32_t *data_ptr = (uint32_t *) image.data;
            size_t data_len = image.w * image.h;

            for (; data_len >= 2; data_len -= 2, data_ptr += 1) {
                *data_ptr = __REV16(*data_ptr); // long aligned
            }

            if (data_len) {
                *((uint16_t *) data_ptr) = __REV16(*((uint16_t *) data_ptr)); // word aligned
            }
        }

        if (size % ALIGN_SIZE) {
            char ignore[ALIGN_SIZE];
            file_read(fp, ignore, ALIGN_SIZE - (size % ALIGN_SIZE));
        }

        if (stream->offset >= stream->count) {
            stream->count = stream->offset + 1;
        }
    #endif
    } else if (stream->type == IMAGE_IO_MEMORY_STREAM) {
        memcpy(image.data, stream->buffer + (stream->offset * stream->size) + IMAGE_T_SIZE_ALIGNED, size);
    }

    stream->offset += 1;

    py_helper_update_framebuffer(&image);

    if (args[ARG_copy_to_fb].u_bool) {
        framebuffer_update_jpeg_buffer();
    }
    return py_image_from_struct(&image);
}
static MP_DEFINE_CONST_FUN_OBJ_KW(py_imageio_read_obj, 1, py_imageio_read);

static mp_obj_t py_imageio_seek(mp_obj_t self, mp_obj_t offs) {
    py_imageio_obj_t *stream = py_imageio_obj(self);
    int offset = mp_obj_get_int(offs);

    if ((offset < 0) || ((stream->type == IMAGE_IO_MEMORY_STREAM) && (stream->count <= offset))) {
        mp_raise_msg(&mp_type_ValueError, MP_ERROR_TEXT("Invalid stream offset"));
    }

    #if defined(IMLIB_ENABLE_IMAGE_FILE_IO)
    if (stream->type == IMAGE_IO_FILE_STREAM) {
        FIL *fp = &stream->fp;
        file_seek(fp, MAGIC_SIZE); // skip past the file header

        for (int i = 0; i < offset; i++) {
            image_t image = {};
            int_py_imageio_read_chunk(stream, &image, false);
            uint32_t size = image_size(&image);

            if (size % ALIGN_SIZE) {
                size += ALIGN_SIZE - (size % ALIGN_SIZE);
            }

            file_seek(fp, f_tell(fp) + size);
        }

        if (stream->offset >= stream->count) {
            stream->count = offset + 1;
        }
    }
    #endif

    stream->offset = offset;

    return self;
}
static MP_DEFINE_CONST_FUN_OBJ_2(py_imageio_seek_obj, py_imageio_seek);

static mp_obj_t py_imageio_sync(mp_obj_t self) {
    #if defined(IMLIB_ENABLE_IMAGE_FILE_IO)
    py_imageio_obj_t *stream = py_imageio_obj(self);

    if (stream->type == IMAGE_IO_FILE_STREAM) {
        file_sync(&stream->fp);
    }
    #endif

    return self;
}
static MP_DEFINE_CONST_FUN_OBJ_1(py_imageio_sync_obj, py_imageio_sync);

static mp_obj_t py_imageio_close(mp_obj_t self) {
    py_imageio_obj_t *stream = py_imageio_obj(self);

    if (0) {
    #if defined(IMLIB_ENABLE_IMAGE_FILE_IO)
    } else if (stream->type == IMAGE_IO_FILE_STREAM) {
        file_close(&stream->fp);
    #endif
    } else if (stream->type == IMAGE_IO_MEMORY_STREAM) {
        fb_alloc_free_till_mark_past_mark_permanent();
    }

    stream->closed = true;

    return self;
}
static MP_DEFINE_CONST_FUN_OBJ_1(py_imageio_close_obj, py_imageio_close);

static mp_obj_t py_imageio_make_new(const mp_obj_type_t *type, size_t n_args, size_t n_kw, const mp_obj_t *args) {
    mp_arg_check_num(n_args, n_kw, 2, 2, false);
    py_imageio_obj_t *stream = mp_obj_malloc_with_finaliser(py_imageio_obj_t, &py_imageio_type);
    stream->closed = false;

    if (0) {
    #if defined(IMLIB_ENABLE_IMAGE_FILE_IO)
    } else if (mp_obj_is_str(args[0])) {
        // File Stream I/O
        FIL *fp = &stream->fp;
        stream->type = IMAGE_IO_FILE_STREAM;
        stream->count = 0;

        char mode = mp_obj_str_get_str(args[1])[0];

        if ((mode == 'W') || (mode == 'w')) {
            file_open(fp, mp_obj_str_get_str(args[0]), false, FA_READ | FA_WRITE | FA_OPEN_ALWAYS);
            const char string[] = "OMV IMG STR V2.0";
            stream->version = NEW_PIXFORMAT_VER;

            // Overwrite if file is too small.
            if (f_size(fp) < MAGIC_SIZE) {
                file_write(fp, string, sizeof(string) - 1); // exclude null terminator
            } else {
                uint8_t version_hi, period, version_lo;
                char temp[sizeof(string) - 3] = {};
                file_read(fp, temp, sizeof(temp) - 1);
                file_read(fp, &version_hi, 1);
                file_read(fp, &period, 1);
                file_read(fp, &version_lo, 1);
                int version = ((version_hi - '0') * 10) + (version_lo - '0');

                // Overwrite if file magic does not match.
                if (strcmp(string, temp)
                    || (period != ((uint8_t) '.'))
                    || (version != ORIGINAL_VER)
                    || (version != RGB565_FIXED_VER)
                    || (version != NEW_PIXFORMAT_VER)) {
                    file_seek(fp, 0);
                    file_write(fp, string, sizeof(string) - 1); // exclude null terminator
                } else {
                    file_close(fp);
                    mode = 'R';
                }
            }
        }

        if ((mode == 'R') || (mode == 'r')) {
            uint8_t version_hi, version_lo;
            file_open(fp, mp_obj_str_get_str(args[0]), false, FA_READ | FA_WRITE | FA_OPEN_EXISTING);
            file_read_check(fp, "OMV IMG STR ", 12); // Magic
            file_read_check(fp, "V", 1);
            file_read(fp, &version_hi, 1);
            file_read_check(fp, ".", 1);
            file_read(fp, &version_lo, 1);

            stream->version = ((version_hi - '0') * 10) + (version_lo - '0');

            if ((stream->version != ORIGINAL_VER)
                && (stream->version != RGB565_FIXED_VER)
                && (stream->version != NEW_PIXFORMAT_VER)) {
                mp_raise_msg(&mp_type_ValueError, MP_ERROR_TEXT("Expected version V1.0, V1.1, or V2.0"));
            }
        } else if ((mode != 'W') && (mode != 'w')) {
            mp_raise_msg(&mp_type_ValueError, MP_ERROR_TEXT("Invalid stream mode, expected 'R/r' or 'W/w'"));
        }
    #endif
    } else if (mp_obj_is_type(args[0], &mp_type_tuple)) {
        // Memory Stream I/O
        stream->type = IMAGE_IO_MEMORY_STREAM;

        mp_obj_t *image_info;
        mp_obj_get_array_fixed_n(args[0], 3, &image_info);
        int w = mp_obj_get_int(image_info[0]);
        int h = mp_obj_get_int(image_info[1]);
        int pixfmt = mp_obj_get_int(image_info[2]);

        if (!IMLIB_PIXFORMAT_IS_VALID(pixfmt)) {
            mp_raise_msg(&mp_type_OSError, MP_ERROR_TEXT("Invalid image stream pixformat"));
        }

        image_t image = {.w = w, .h = h, .pixfmt = pixfmt};

        // Estimate that the compressed image will fit in less than 2 bits per pixel.
        if (image.is_compressed) {
            image.h *= 2; // double calculated image size
            image.pixfmt = PIXFORMAT_BINARY;
        }

        stream->count = mp_obj_get_int(args[1]);
        stream->size = IMAGE_T_SIZE_ALIGNED + image_size_aligned(&image);

        fb_alloc_mark();
        stream->buffer = fb_alloc(stream->count * stream->size, FB_ALLOC_PREFER_SIZE | FB_ALLOC_CACHE_ALIGN);
        fb_alloc_mark_permanent();
    } else {
        mp_raise_msg(&mp_type_ValueError, MP_ERROR_TEXT("Invalid stream type"));
    }

    stream->offset = 0;
    stream->ms = mp_hal_ticks_ms();

    return MP_OBJ_FROM_PTR(stream);
}

static const mp_rom_map_elem_t py_imageio_locals_dict_table[] = {
    { MP_ROM_QSTR(MP_QSTR___name__),        MP_ROM_QSTR(MP_QSTR_imageio)            },
    { MP_ROM_QSTR(MP_QSTR___del__),         MP_ROM_PTR(&py_imageio_close_obj)       },
    { MP_ROM_QSTR(MP_QSTR_FILE_STREAM),     MP_ROM_INT(IMAGE_IO_FILE_STREAM)        },
    { MP_ROM_QSTR(MP_QSTR_MEMORY_STREAM),   MP_ROM_INT(IMAGE_IO_MEMORY_STREAM)      },
    { MP_ROM_QSTR(MP_QSTR_type),            MP_ROM_PTR(&py_imageio_get_type_obj)    },
    { MP_ROM_QSTR(MP_QSTR_is_closed),       MP_ROM_PTR(&py_imageio_is_closed_obj)   },
    { MP_ROM_QSTR(MP_QSTR_count),           MP_ROM_PTR(&py_imageio_count_obj)       },
    { MP_ROM_QSTR(MP_QSTR_offset),          MP_ROM_PTR(&py_imageio_offset_obj)      },
    #if defined(IMLIB_ENABLE_IMAGE_FILE_IO)
    { MP_ROM_QSTR(MP_QSTR_version),         MP_ROM_PTR(&py_imageio_version_obj)     },
    #else
    { MP_ROM_QSTR(MP_QSTR_version),         MP_ROM_PTR(&py_func_unavailable_obj)    },
    #endif
    { MP_ROM_QSTR(MP_QSTR_buffer_size),     MP_ROM_PTR(&py_imageio_buffer_size_obj) },
    { MP_ROM_QSTR(MP_QSTR_size),            MP_ROM_PTR(&py_imageio_size_obj)        },
    { MP_ROM_QSTR(MP_QSTR_write),           MP_ROM_PTR(&py_imageio_write_obj)       },
    { MP_ROM_QSTR(MP_QSTR_read),            MP_ROM_PTR(&py_imageio_read_obj)        },
    { MP_ROM_QSTR(MP_QSTR_seek),            MP_ROM_PTR(&py_imageio_seek_obj)        },
    { MP_ROM_QSTR(MP_QSTR_sync),            MP_ROM_PTR(&py_imageio_sync_obj)        },
    { MP_ROM_QSTR(MP_QSTR_close),           MP_ROM_PTR(&py_imageio_close_obj)       }
};

static MP_DEFINE_CONST_DICT(py_imageio_locals_dict, py_imageio_locals_dict_table);

MP_DEFINE_CONST_OBJ_TYPE(
    py_imageio_type,
    MP_QSTR_ImageIO,
    MP_TYPE_FLAG_NONE,
    print, py_imageio_print,
    make_new, py_imageio_make_new,
    locals_dict, &py_imageio_locals_dict
    );
#endif // IMLIB_ENABLE_IMAGE_IO
