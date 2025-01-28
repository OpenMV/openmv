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
 * Framebuffer functions.
 */
#include <stdio.h>
#include "mpprint.h"
#include "fmath.h"
#include "framebuffer.h"
#include "omv_boardconfig.h"

#define FB_ALIGN_SIZE_ROUND_DOWN(x) (((x) / FRAMEBUFFER_ALIGNMENT) * FRAMEBUFFER_ALIGNMENT)
#define FB_ALIGN_SIZE_ROUND_UP(x)   FB_ALIGN_SIZE_ROUND_DOWN(((x) + FRAMEBUFFER_ALIGNMENT - 1))
#define OMV_JPEG_BUFFER_SIZE_MAX    ((_jpeg_memory_end_ptr - _jpeg_memory_start_ptr) - sizeof(jpegbuffer_t))

#ifdef OMV_FB_MEMORY
extern char _fb_memory_start;
extern char _fb_memory_end;
framebuffer_t *framebuffer = (framebuffer_t *) &_fb_memory_start;
static char *_fb_memory_end_ptr = &_fb_memory_end;
#else
static const char _fb_memory_start_ptr[OMV_FB_SIZE];
static const char *_fb_memory_end_ptr = &_fb_memory_start_ptr[OMV_FB_SIZE - 1];
framebuffer_t *framebuffer = (framebuffer_t *) _fb_memory_start_ptr;
#endif

#ifdef OMV_FB_MEMORY
extern char _jpeg_memory_start;
extern char _jpeg_memory_end;
static const char *_jpeg_memory_start_ptr = &_jpeg_memory_start;
static const char *_jpeg_memory_end_ptr = &_jpeg_memory_end;
jpegbuffer_t *jpeg_framebuffer = (jpegbuffer_t *) &_jpeg_memory_start;
#else
static const char _jpeg_memory_start_ptr[OMV_JPEG_SIZE];
static const char *_jpeg_memory_end_ptr = &_jpeg_memory_start_ptr[OMV_JPEG_SIZE - 1];
jpegbuffer_t *jpeg_framebuffer = (jpegbuffer_t *) _jpeg_memory_start_ptr;
#endif

void fb_set_streaming_enabled(bool enable) {
    framebuffer->streaming_enabled = enable;
}

bool fb_get_streaming_enabled() {
    return framebuffer->streaming_enabled;
}

int fb_encode_for_ide_new_size(image_t *img) {
    return (((img->size * 8) + 5) / 6) + 2;
}

void fb_encode_for_ide(uint8_t *ptr, image_t *img) {
    *ptr++ = 0xFE;

    for (int i = 0, j = (img->size / 3) * 3; i < j; i += 3) {
        int x = 0;
        x |= img->data[i + 0] << 0;
        x |= img->data[i + 1] << 8;
        x |= img->data[i + 2] << 16;
        *ptr++ = 0x80 | ((x >> 0) & 0x3F);
        *ptr++ = 0x80 | ((x >> 6) & 0x3F);
        *ptr++ = 0x80 | ((x >> 12) & 0x3F);
        *ptr++ = 0x80 | ((x >> 18) & 0x3F);
    }

    if ((img->size % 3) == 2) {
        // 2 bytes -> 16-bits -> 24-bits sent
        int x = 0;
        x |= img->data[img->size - 2] << 0;
        x |= img->data[img->size - 1] << 8;
        *ptr++ = 0x80 | ((x >> 0) & 0x3F);
        *ptr++ = 0x80 | ((x >> 6) & 0x3F);
        *ptr++ = 0x80 | ((x >> 12) & 0x3F);
    }

    if ((img->size % 3) == 1) {
        // 1 byte -> 8-bits -> 16-bits sent
        int x = 0;
        x |= img->data[img->size - 1] << 0;
        *ptr++ = 0x80 | ((x >> 0) & 0x3F);
        *ptr++ = 0x80 | ((x >> 6) & 0x3F);
    }

    *ptr++ = 0xFE;
}

void framebuffer_init0() {
    // Save fb_enabled flag state
    int fb_enabled = JPEG_FB()->enabled;

    // Clear framebuffers
    memset(MAIN_FB(), 0, sizeof(*MAIN_FB()));
    memset(JPEG_FB(), 0, sizeof(*JPEG_FB()));

    mutex_init0(&JPEG_FB()->lock);

    // Enable streaming.
    MAIN_FB()->streaming_enabled = true; // controlled by the OpenMV Cam.

    // Set default quality
    JPEG_FB()->quality = ((OMV_JPEG_QUALITY_HIGH - OMV_JPEG_QUALITY_LOW) / 2) + OMV_JPEG_QUALITY_LOW;

    // Set fb_enabled
    JPEG_FB()->enabled = fb_enabled; // controlled by the IDE.

    // Setup buffering.
    framebuffer_set_buffers(1);
}

void framebuffer_init_image(image_t *img) {
    if (img != NULL) {
        img->w = framebuffer->w;
        img->h = framebuffer->h;
        img->size = framebuffer->size;
        img->pixfmt = framebuffer->pixfmt;
        img->pixels = framebuffer_get_buffer(framebuffer->head)->data;
    }
}

void framebuffer_init_from_image(image_t *img) {
    framebuffer->w = img->w;
    framebuffer->h = img->h;
    framebuffer->size = img->size;
    framebuffer->pixfmt = img->pixfmt;
}

static void jpegbuffer_init_from_image(image_t *img) {
    if (img == NULL) {
        jpeg_framebuffer->w = 0;
        jpeg_framebuffer->h = 0;
        jpeg_framebuffer->size = 0;
    } else {
        jpeg_framebuffer->w = img->w;
        jpeg_framebuffer->h = img->h;
        jpeg_framebuffer->size = img->size;
    }
}

void framebuffer_update_jpeg_buffer() {
    static int overflow_count = 0;

    image_t main_fb_src;
    framebuffer_init_image(&main_fb_src);
    image_t *src = &main_fb_src;

    if (src->pixfmt != PIXFORMAT_INVALID &&
        framebuffer->streaming_enabled && jpeg_framebuffer->enabled) {
        if (src->is_compressed) {
            bool does_not_fit = false;

            if (mutex_try_lock_alternate(&jpeg_framebuffer->lock, MUTEX_TID_OMV)) {
                if (OMV_JPEG_BUFFER_SIZE_MAX < src->size) {
                    jpegbuffer_init_from_image(NULL);
                    does_not_fit = true;
                } else {
                    jpegbuffer_init_from_image(src);
                    memcpy(jpeg_framebuffer->pixels, src->pixels, src->size);
                }

                mutex_unlock(&jpeg_framebuffer->lock, MUTEX_TID_OMV);
            }

            if (does_not_fit) {
                printf("Warning: JPEG/PNG too big! Trying framebuffer transfer using fallback method!\n");
                int new_size = fb_encode_for_ide_new_size(src);
                fb_alloc_mark();
                uint8_t *temp = fb_alloc(new_size, FB_ALLOC_NO_HINT);
                fb_encode_for_ide(temp, src);
                (MP_PYTHON_PRINTER)->print_strn((MP_PYTHON_PRINTER)->data, (const char *) temp, new_size);
                fb_alloc_free_till_mark();
            }
        } else if (src->pixfmt != PIXFORMAT_INVALID) {
            if (mutex_try_lock_alternate(&jpeg_framebuffer->lock, MUTEX_TID_OMV)) {
                image_t dst = {
                    .w = src->w,
                    .h = src->h,
                    .pixfmt = PIXFORMAT_JPEG,
                    .size = OMV_JPEG_BUFFER_SIZE_MAX,
                    .pixels = jpeg_framebuffer->pixels
                };

                bool compress = true;
                bool overflow = false;

                #if OMV_RAW_PREVIEW_ENABLE
                if (src->is_mutable) {
                    // Down-scale the frame (if necessary) and send the raw frame.
                    dst.size = src->bpp;
                    dst.pixfmt = src->pixfmt;
                    if (src->w <= OMV_RAW_PREVIEW_WIDTH && src->h <= OMV_RAW_PREVIEW_HEIGHT) {
                        if (image_size(&dst) <= OMV_JPEG_BUFFER_SIZE_MAX) {
                            memcpy(dst.pixels, src->pixels, image_size(src));
                            compress = false;
                        }
                    } else {
                        float x_scale = OMV_RAW_PREVIEW_WIDTH / (float) src->w;
                        float y_scale = OMV_RAW_PREVIEW_HEIGHT / (float) src->h;
                        float scale = IM_MIN(x_scale, y_scale);
                        dst.w = fast_floorf(src->w * scale);
                        dst.h = fast_floorf(src->h * scale);
                        if (image_size(&dst) <= OMV_JPEG_BUFFER_SIZE_MAX) {
                            imlib_draw_image(&dst, src, 0, 0, scale, scale, NULL, -1, 255, NULL, NULL,
                                             IMAGE_HINT_BILINEAR | IMAGE_HINT_BLACK_BACKGROUND, NULL, NULL, NULL);
                            compress = false;
                        }
                    }
                }
                #endif

                if (compress) {
                    // For all other formats, send a compressed frame.
                    overflow = jpeg_compress(src, &dst, jpeg_framebuffer->quality, false, JPEG_SUBSAMPLING_AUTO);
                }

                if (overflow) {
                    // JPEG buffer overflowed, reduce JPEG quality for the next frame
                    // and skip the current frame. The IDE doesn't receive this frame.
                    if (jpeg_framebuffer->quality > 1) {
                        // Keep this quality for the next n frames
                        overflow_count = 60;
                        jpeg_framebuffer->quality = IM_MAX(1, (jpeg_framebuffer->quality / 2));
                    }

                    jpegbuffer_init_from_image(NULL);
                } else {
                    if (overflow_count) {
                        overflow_count--;
                    }

                    // Dynamically adjust our quality if the image is huge.
                    bool big_frame_buffer = image_size(src) > OMV_JPEG_QUALITY_THRESHOLD;
                    int jpeg_quality_max = big_frame_buffer ? OMV_JPEG_QUALITY_LOW : OMV_JPEG_QUALITY_HIGH;

                    // No buffer overflow, increase quality up to max quality based on frame size...
                    if ((!overflow_count) && (jpeg_framebuffer->quality < jpeg_quality_max)) {
                        jpeg_framebuffer->quality++;
                    }

                    jpegbuffer_init_from_image(&dst);
                }

                mutex_unlock(&jpeg_framebuffer->lock, MUTEX_TID_OMV);
            }
        }
    }
}

int32_t framebuffer_get_x() {
    return framebuffer->x;
}

int32_t framebuffer_get_y() {
    return framebuffer->y;
}

int32_t framebuffer_get_u() {
    return framebuffer->u;
}

int32_t framebuffer_get_v() {
    return framebuffer->v;
}

int32_t framebuffer_get_width() {
    return framebuffer->w;
}

int32_t framebuffer_get_height() {
    return framebuffer->h;
}

int32_t framebuffer_get_depth() {
    return framebuffer->bpp;
}

// Returns the current frame buffer size, factoring in the space taken by fb_alloc.
static uint32_t framebuffer_max_buffer_size() {
    uint32_t fb_total_size = FB_ALIGN_SIZE_ROUND_DOWN(_fb_memory_end_ptr - (char *) framebuffer->data);
    uint32_t fb_avail_size = FB_ALIGN_SIZE_ROUND_DOWN(fb_alloc_stack_pointer() - (char *) framebuffer->data);
    return IM_MIN(fb_total_size, fb_avail_size);
}

uint32_t framebuffer_get_buffer_size() {
    uint32_t size;

    if (framebuffer->n_buffers == 1) {
        // With only 1 vbuffer the frame buffer size can change given fb_alloc().
        size = framebuffer_max_buffer_size();
    } else {
        // Whatever the raw size was when the number of buffers were set is locked in.
        size = framebuffer->buff_size;
    }

    // Remove the size of the state header plus alignment padding.
    size -= sizeof(vbuffer_t);

    // Needs to be a multiple of FRAMEBUFFER_ALIGNMENT for DMA transfers.
    return FB_ALIGN_SIZE_ROUND_DOWN(size);
}

// Each raw frame buffer is split into two parts. The vbuffer_t struct followed by
// padding and then the pixel array starting at the next 32-byte offset.
vbuffer_t *framebuffer_get_buffer(int32_t index) {
    uint32_t offset = (sizeof(vbuffer_t) + framebuffer_get_buffer_size()) * index;
    return (vbuffer_t *) (framebuffer->data + offset);
}

void framebuffer_flush_buffers(bool fifo_flush) {
    if (fifo_flush) {
        // Drop all frame buffers.
        for (uint32_t i = 0; i < framebuffer->n_buffers; i++) {
            memset(framebuffer_get_buffer(i), 0, sizeof(vbuffer_t));
        }
    }
    // Move the tail pointer to the head which empties the virtual fifo while keeping the same
    // position of the current frame for the rest of the code.
    framebuffer->tail = framebuffer->head;
    framebuffer->check_head = true;
    framebuffer->sampled_head = 0;
}

int framebuffer_set_buffers(int32_t n_buffers) {
    uint32_t avail_size = FB_ALIGN_SIZE_ROUND_DOWN(framebuffer_max_buffer_size());
    uint32_t frame_size = FB_ALIGN_SIZE_ROUND_UP(framebuffer->frame_size + sizeof(vbuffer_t));
    uint32_t vbuff_size = (n_buffers == 1) ? avail_size : frame_size;
    uint32_t vbuff_count = IM_MIN((avail_size / vbuff_size), (n_buffers == -1) ? 3 : (uint32_t) n_buffers);

    if (vbuff_count == 0 || vbuff_size < sizeof(vbuffer_t)) {
        return -1;
    }

    framebuffer->head = 0;
    framebuffer->buff_size = vbuff_size;
    framebuffer->n_buffers = vbuff_count;
    framebuffer->pixfmt = PIXFORMAT_INVALID;

    framebuffer_flush_buffers(true);
    return 0;
}

// Returns the real size of bytes in the frame buffer.
static uint32_t framebuffer_total_buffer_size() {
    if (framebuffer->n_buffers == 1) {
        // Allow fb_alloc to use frame buffer space up until the image size.
        image_t img;
        framebuffer_init_image(&img);
        return sizeof(vbuffer_t) + FB_ALIGN_SIZE_ROUND_UP(image_size(&img));
    } else {
        // fb_alloc may only use up to the size of all the virtual buffers...
        return (sizeof(vbuffer_t) + framebuffer_get_buffer_size()) * framebuffer->n_buffers;
    }
}

void framebuffer_free_current_buffer() {
    vbuffer_t *buffer = framebuffer_get_buffer(framebuffer->head);

    #ifdef __DCACHE_PRESENT
    // Make sure all cached CPU writes are discarded before returning the buffer.
    SCB_InvalidateDCache_by_Addr(buffer->data, framebuffer_get_buffer_size());
    #endif

    // Invalidate frame.
    framebuffer->pixfmt = PIXFORMAT_INVALID;

    // Allow frame to be updated in single buffer mode...
    if (framebuffer->n_buffers == 1) {
        buffer->waiting_for_data = true;
    }
}

void framebuffer_setup_buffers() {
    #ifdef __DCACHE_PRESENT
    for (int32_t i = 0; i < framebuffer->n_buffers; i++) {
        if (i != framebuffer->head) {
            vbuffer_t *buffer = framebuffer_get_buffer(i);
            // Make sure all cached CPU writes are discarded before returning the buffer.
            SCB_InvalidateDCache_by_Addr(buffer->data, framebuffer_get_buffer_size());
        }
    }
    #endif
}

vbuffer_t *framebuffer_get_head(framebuffer_flags_t flags) {
    int32_t new_head = (framebuffer->head + 1) % framebuffer->n_buffers;

    // Single Buffer Mode.
    if (framebuffer->n_buffers == 1) {
        if (framebuffer_get_buffer(framebuffer->head)->waiting_for_data) {
            return NULL;
        }
        // Double Buffer Mode.
    } else if (framebuffer->n_buffers == 2) {
        if (framebuffer->head == framebuffer->tail) {
            return NULL;
        }
        // Triple Buffer Mode.
    } else if (framebuffer->n_buffers == 3) {
        int32_t sampled_tail = framebuffer->tail;
        if (framebuffer->head == sampled_tail) {
            return NULL;
        } else {
            new_head = sampled_tail;
        }
        // Video FIFO Mode.
    } else {
        if (framebuffer->head == framebuffer->tail) {
            return NULL;
        }
    }

    if (!(flags & FB_PEEK)) {
        framebuffer->head = new_head;
    }

    vbuffer_t *buffer = framebuffer_get_buffer(new_head);

    #ifdef __DCACHE_PRESENT
    if (flags & FB_INVALIDATE) {
        // Make sure any cached CPU reads are dropped before returning the buffer.
        SCB_InvalidateDCache_by_Addr(buffer->data, framebuffer_get_buffer_size());
    }
    #endif

    return buffer;
}

vbuffer_t *framebuffer_get_tail(framebuffer_flags_t flags) {
    // Sample head on the first line of a new frame.
    if (framebuffer->check_head) {
        framebuffer->check_head = false;
        framebuffer->sampled_head = framebuffer->head;
    }

    int32_t new_tail = (framebuffer->tail + 1) % framebuffer->n_buffers;

    // Single Buffer Mode.
    if (framebuffer->n_buffers == 1) {
        if (!framebuffer_get_buffer(new_tail)->waiting_for_data) {
            // Setup to check head again.
            framebuffer->check_head = true;
            return NULL;
        }
        // Double Buffer Mode.
    } else if (framebuffer->n_buffers == 2) {
        if (new_tail == framebuffer->sampled_head) {
            // Setup to check head again.
            framebuffer->check_head = true;
            return NULL;
        }
        // Triple Buffer Mode.
    } else if (framebuffer->n_buffers == 3) {
        // For triple buffering we are never writing where tail or head
        // (which may instantly update to be equal to tail) is.
        if (new_tail == framebuffer->sampled_head) {
            new_tail = (new_tail + 1) % framebuffer->n_buffers;
        }
        // Video FIFO Mode.
    } else {
        if (new_tail == framebuffer->sampled_head) {
            // Setup to check head again.
            framebuffer->check_head = true;
            return NULL;
        }
    }

    vbuffer_t *buffer = framebuffer_get_buffer(new_tail);

    // Reset on start versus the end so offset and jpeg_buffer_overflow are valid after FB_COMMIT.
    if (buffer->reset_state) {
        buffer->reset_state = false;
        buffer->offset = 0;
        buffer->jpeg_buffer_overflow = false;
    }

    if (!(flags & FB_PEEK)) {
        // Trigger reset on the frame buffer the next time it is used.
        buffer->reset_state = true;

        // Mark the frame buffer ready in single buffer mode.
        if (framebuffer->n_buffers == 1) {
            buffer->waiting_for_data = false;
        }

        framebuffer->tail = new_tail;

        // Setup to check head again.
        framebuffer->check_head = true;
    }
    return buffer;
}

char *framebuffer_get_buffers_end() {
    return (char *) (framebuffer->data + framebuffer_total_buffer_size());
}
