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
 * PNG CODEC
 */
#include "imlib.h"
#include "py/runtime.h"
#include "file_utils.h"
#if defined(IMLIB_ENABLE_PNG_ENCODER) || defined(IMLIB_ENABLE_PNG_DECODER)
#include "lodepng.h"
#include "umm_malloc.h"

void *lodepng_malloc(size_t size) {
    return umm_malloc(size);
}

void *lodepng_realloc(void *ptr, size_t new_size) {
    return umm_realloc(ptr, new_size);
}

void lodepng_free(void *ptr) {
    return umm_free(ptr);
}

unsigned lodepng_convert_cb(unsigned char *out, const unsigned char *in,
                            const LodePNGColorMode *mode_out, const LodePNGColorMode *mode_in, unsigned w, unsigned h) {
    unsigned error = 0;
    unsigned numpixels = w * h;

    if (mode_in->colortype == LCT_CUSTOM) {
        // Compression.
        // Note: we're always encoding to 8 bits.
        switch (mode_in->customfmt) {
            case PIXFORMAT_RGB565: {
                uint16_t *pixels = (uint16_t *) in;
                if (mode_out->colortype == LCT_RGB) {
                    // RGB565 -> RGB888
                    for (int i = 0; i < numpixels; i++, out += 3) {
                        out[0] = COLOR_RGB565_TO_R8(pixels[i]);
                        out[1] = COLOR_RGB565_TO_G8(pixels[i]);
                        out[2] = COLOR_RGB565_TO_B8(pixels[i]);
                    }
                } else if (mode_out->colortype == LCT_RGBA) {
                    // RGB565 -> RGBA888
                    for (int i = 0; i < numpixels; i++, out += 4) {
                        out[0] = COLOR_RGB565_TO_R8(pixels[i]);
                        out[1] = COLOR_RGB565_TO_G8(pixels[i]);
                        out[2] = COLOR_RGB565_TO_B8(pixels[i]);
                        out[3] = 255;
                    }
                } else {
                    error = 56; // unsupported color mode conversion.
                }
                break;
            }
            case PIXFORMAT_YUV_ANY:
            // YUV   -> RGB888
            case PIXFORMAT_BAYER_ANY:
            // BAYER -> RGB888
            default:
                error = 56; // unsupported color mode conversion.
                break;
        }
    } else if (mode_out->colortype == LCT_CUSTOM) {
        // Decompression.
        // NOTE: decode from 16 bits needs to be implemented.
        switch (mode_out->customfmt) {
            case PIXFORMAT_RGB565: {
                uint16_t *pixels = (uint16_t *) out;
                if (mode_in->colortype == LCT_RGB) {
                    // RGB888 -> RGB565
                    for (int i = 0; i < numpixels; i++, in += 3) {
                        pixels[i] = COLOR_R8_G8_B8_TO_RGB565(in[0], in[1], in[2]);
                    }
                } else if (mode_in->colortype == LCT_RGBA) {
                    // RGBA888 -> RGB565
                    for (int i = 0; i < numpixels; i++, in += 4) {
                        pixels[i] = COLOR_R8_G8_B8_TO_RGB565(in[0], in[1], in[2]);
                    }
                } else if (mode_in->colortype == LCT_GREY && mode_in->bitdepth == 8) {
                    // GRAYSCALE -> RGB565
                    for (int i = 0; i < numpixels; i++, in++) {
                        pixels[i] = COLOR_R8_G8_B8_TO_RGB565(in[0], in[0], in[0]);
                    }
                } else {
                    error = 56; // unsupported color mode conversion.
                }
                break;
            }
            default:
                error = 56; // unsupported color mode conversion.
                break;
        }

    } else {
        error = 56; // unsupported color mode conversion.
    }
    return error;
}

#if defined(IMLIB_ENABLE_PNG_ENCODER)
bool png_compress(image_t *src, image_t *dst) {
    OMV_PROFILE_START();

    if (src->is_compressed) {
        return true;
    }

    umm_init_x(fb_avail(FB_ALLOC_NO_HINT));

    LodePNGState state;
    lodepng_state_init(&state);
    // Invoked on custom formats.
    state.lodepng_convert = &lodepng_convert_cb;
    // Faster compression.
    state.encoder.zlibsettings.windowsize = 1024;

    switch (src->pixfmt) {
        case PIXFORMAT_BINARY:
            state.info_raw.bitdepth = 1;
            state.info_raw.colortype = LCT_GREY;

            state.encoder.auto_convert = false;
            state.info_png.color.bitdepth = 8;
            state.info_png.color.colortype = LCT_GREY;
            break;
        case PIXFORMAT_GRAYSCALE:
            state.info_raw.bitdepth = 8;
            state.info_raw.colortype = LCT_GREY;

            state.encoder.auto_convert = false;
            state.info_png.color.bitdepth = 8;
            state.info_png.color.colortype = LCT_GREY;
            break;
        case PIXFORMAT_RGB565:
            state.info_raw.bitdepth = 16;
            state.info_raw.colortype = LCT_CUSTOM;
            state.info_raw.customfmt = PIXFORMAT_RGB565;

            state.encoder.auto_convert = false;
            state.info_png.color.bitdepth = 8;
            state.info_png.color.colortype = LCT_RGB;
            break;
        case PIXFORMAT_YUV_ANY:
            mp_raise_msg_varg(&mp_type_RuntimeError, MP_ERROR_TEXT("Input format is not supported"));
            break;
        case PIXFORMAT_BAYER_ANY:
            mp_raise_msg_varg(&mp_type_RuntimeError, MP_ERROR_TEXT("Input format is not supported"));
            break;
    }

    size_t png_size = 0;
    uint8_t *png_data = NULL;
    unsigned error = lodepng_encode(&png_data, &png_size, src->data, src->w, src->h, &state);
    lodepng_state_cleanup(&state);
    if (error) {
        mp_raise_msg(&mp_type_RuntimeError, (mp_rom_error_text_t) lodepng_error_text(error));
    }

    if (dst->data == NULL) {
        dst->data = png_data;
        dst->size = png_size;
        // fb_alloc() memory ill be free'd by called.
    } else {
        if (image_size(dst) <= png_size) {
            dst->size = png_size;
            memcpy(dst->data, png_data, png_size);
        } else {
            mp_raise_msg_varg(&mp_type_RuntimeError,
                              MP_ERROR_TEXT("Failed to compress image in place"));
        }
        // free fb_alloc() memory used for umm_init_x().
        fb_free(); // umm_init_x();
    }
    OMV_PROFILE_PRINT();
    return false;
}
#endif // IMLIB_ENABLE_PNG_ENCODER

#if defined(IMLIB_ENABLE_PNG_DECODER)
void png_decompress(image_t *dst, image_t *src) {
    OMV_PROFILE_START();
    umm_init_x(fb_avail(FB_ALLOC_NO_HINT));

    LodePNGState state;
    lodepng_state_init(&state);
    // Invoked on custom formats.
    state.lodepng_convert = &lodepng_convert_cb;

    switch (dst->pixfmt) {
        case PIXFORMAT_BINARY:
            state.info_raw.bitdepth = 1;
            state.info_raw.colortype = LCT_GREY;
            break;
        case PIXFORMAT_GRAYSCALE:
            state.info_raw.bitdepth = 8;
            state.info_raw.colortype = LCT_GREY;
            break;
        case PIXFORMAT_RGB565:
            state.info_raw.bitdepth = 16;
            state.info_raw.colortype = LCT_CUSTOM;
            state.info_raw.customfmt = PIXFORMAT_RGB565;
            break;
    }

    uint8_t *png_data = NULL;
    uint32_t img_size = image_size(dst);
    unsigned error = lodepng_decode(&png_data, (unsigned *) &dst->w, (unsigned *) &dst->h, &state, src->data, src->size);
    lodepng_state_cleanup(&state);
    if (error) {
        mp_raise_msg(&mp_type_RuntimeError, (mp_rom_error_text_t) lodepng_error_text(error));
    }

    uint32_t new_img_size = image_size(dst);
    if (new_img_size <= img_size) {
        memcpy(dst->data, png_data, new_img_size);
    } else {
        mp_raise_msg_varg(&mp_type_RuntimeError,
                          MP_ERROR_TEXT("Failed to compress image in place"));
    }

    // free fb_alloc() memory used for umm_init_x().
    fb_free(); // umm_init_x();
    OMV_PROFILE_PRINT();
}
#endif // IMLIB_ENABLE_PNG_DECODER
#endif // IMLIB_ENABLE_PNG_ENCODER || IMLIB_ENABLE_PNG_DECODER


#if !defined(IMLIB_ENABLE_PNG_ENCODER)
bool png_compress(image_t *src, image_t *dst) {
    mp_raise_msg_varg(&mp_type_RuntimeError, MP_ERROR_TEXT("PNG encoder is not enabled"));
}
#endif

#if !defined(IMLIB_ENABLE_PNG_DECODER)
void png_decompress(image_t *dst, image_t *src) {
    mp_raise_msg_varg(&mp_type_RuntimeError, MP_ERROR_TEXT("PNG decoder is not enabled"));
}
#endif

#if defined(IMLIB_ENABLE_IMAGE_FILE_IO)
// This function inits the geometry values of an image.
void png_read_geometry(FIL *fp, image_t *img, const char *path, png_read_settings_t *rs) {
    uint32_t header;
    file_seek(fp, 12); // start of IHDR
    file_read(fp, &header, 4);
    if (header == 0x52444849) {
        // IHDR
        uint32_t width, height;
        file_read(fp, &width, 4);
        file_read(fp, &height, 4);
        width = __builtin_bswap32(width);
        height = __builtin_bswap32(height);

        rs->png_w = width;
        rs->png_h = height;
        rs->png_size = IMLIB_IMAGE_MAX_SIZE(f_size(fp));

        img->w = rs->png_w;
        img->h = rs->png_h;
        img->size = rs->png_size;
        img->pixfmt = PIXFORMAT_PNG;
    } else {
        file_raise_corrupted(fp);
    }
}

// This function reads the pixel values of an image.
void png_read_pixels(FIL *fp, image_t *img) {
    file_seek(fp, 0);
    file_read(fp, img->pixels, img->size);
}

void png_read(image_t *img, const char *path) {
    FIL fp;
    png_read_settings_t rs;

    // Do not use file buferring here.
    file_open(&fp, path, false, FA_READ | FA_OPEN_EXISTING);

    png_read_geometry(&fp, img, path, &rs);

    if (!img->pixels) {
        image_xalloc(img, img->size);
    }

    png_read_pixels(&fp, img);
    file_close(&fp);
}

void png_write(image_t *img, const char *path) {
    FIL fp;
    file_open(&fp, path, false, FA_WRITE | FA_CREATE_ALWAYS);
    if (img->pixfmt == PIXFORMAT_PNG) {
        file_write(&fp, img->pixels, img->size);
    } else {
        image_t out = { .w = img->w, .h = img->h, .pixfmt = PIXFORMAT_PNG, .size = 0, .pixels = NULL }; // alloc in png compress
        png_compress(img, &out);
        file_write(&fp, out.pixels, out.size);
        fb_free(); // frees alloc in png_compress()
    }
    file_close(&fp);
}
#endif //IMLIB_ENABLE_IMAGE_FILE_IO)
