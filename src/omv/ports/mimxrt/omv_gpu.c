/*
 * This file is part of the OpenMV project.
 *
 * Copyright (c) 2013-2024 Ibrahim Abdelkader <iabdalkader@openmv.io>
 * Copyright (c) 2013-2024 Kwabena W. Agyeman <kwagyeman@openmv.io>
 *
 * This work is licensed under the MIT license, see the file LICENSE for details.
 *
 * GPU driver for IMXRT port.
 */
#include "omv_boardconfig.h"
#if (OMV_GPU_ENABLE == 1)
#include "imlib.h"
#include "fsl_pxp.h"

int omv_gpu_init() {
    return 0;
}

void omv_gpu_deinit() {
    PXP_Deinit(PXP);
}

int omv_gpu_draw_image(image_t *src_img,
                       rectangle_t *src_rect,
                       image_t *dst_img,
                       rectangle_t *dst_rect,
                       int alpha,
                       const uint16_t *color_palette,
                       const uint8_t *alpha_palette,
                       image_hint_t hint) {
    OMV_PROFILE_START();

    // PXP input image must be GRAYSCALE, RGB565, YUV422 or YVU422.
    if ((src_img->pixfmt != PIXFORMAT_GRAYSCALE) && (src_img->pixfmt != PIXFORMAT_RGB565) &&
        (src_img->pixfmt != PIXFORMAT_YUV422) && (src_img->pixfmt != PIXFORMAT_YVU422)) {
        return -1;
    }

    // PXP output image must be GRAYSCALE or RGB565.
    if ((dst_img->pixfmt != PIXFORMAT_GRAYSCALE) && (dst_img->pixfmt != PIXFORMAT_RGB565)) {
        return -1;
    }

    // If the output image is GRAYSCALE, the input image must be GRAYSCALE or YUV422.
    if ((dst_img->pixfmt == PIXFORMAT_GRAYSCALE) && (src_img->pixfmt != PIXFORMAT_GRAYSCALE) &&
        (src_img->pixfmt != PIXFORMAT_YUV422) && (src_img->pixfmt != PIXFORMAT_YVU422)) {
        return -1;
    }

    // PXP can only operate on 8x8 or 16x16 pixel blocks.
    if ((src_rect->w % 8) || (src_rect->h % 8) || (dst_rect->w % 8) || (dst_rect->h % 8)) {
        return -1;
    }

    // PXP cannot alpha blend on the background in-place and doesn't have a LUT for alpha or color palettes.
    if ((alpha != 256) || (color_palette || alpha_palette)) {
        return -1;
    }

    // PXP cannot reduce the image size by more than 16x.
    if ((dst_rect->w > (src_rect->w * 16)) || (dst_rect->h > (src_rect->h * 16))) {
        return -1;
    }

    // PXP cannot scale the image size by more than 4096x.
    if ((dst_rect->w < (src_rect->w / 4096)) || (dst_rect->h < (src_rect->h / 4096))) {
        return -1;
    }

    // PXP cannot hmirror or vflip when scaling.
    if ((hint & (IMAGE_HINT_HMIRROR | IMAGE_HINT_VFLIP)) &&
        ((src_rect->w != dst_rect->w) || (src_rect->h != dst_rect->h))) {
        return -1;
    }

    // PXP always applies bilinear scaling, which is always better than nearest-neighbor so ignore the hint.

    PXP_Init(PXP);

    // Try to operate in 16x16 blocks.
    if ((src_rect->w % 16) || (src_rect->h % 16) || (dst_rect->w % 16) || (dst_rect->h % 16)) {
        PXP_SetProcessBlockSize(PXP, kPXP_BlockSize8);
    } else {
        PXP_SetProcessBlockSize(PXP, kPXP_BlockSize16);
    }

    pxp_ps_buffer_config_t input_buffer_config = {};

    if (src_img->pixfmt == PIXFORMAT_GRAYSCALE) {
        input_buffer_config.pixelFormat = kPXP_PsPixelFormatY8;
    } else if (src_img->pixfmt == PIXFORMAT_RGB565) {
        input_buffer_config.pixelFormat = kPXP_PsPixelFormatRGB565;
    } else if (src_img->pixfmt == PIXFORMAT_YUV422) {
        input_buffer_config.pixelFormat = kPXP_PsPixelFormatUYVY1P422;
        input_buffer_config.swapByte = true;
    } else if (src_img->pixfmt == PIXFORMAT_YVU422) {
        input_buffer_config.pixelFormat = kPXP_PsPixelFormatVYUY1P422;
        input_buffer_config.swapByte = true;
    }

    if (src_img->pixfmt == PIXFORMAT_GRAYSCALE) {
        uint8_t *src8 = IMAGE_COMPUTE_GRAYSCALE_PIXEL_ROW_PTR(src_img, src_rect->y) + src_rect->x;

        input_buffer_config.bufferAddr = (uint32_t) src8;
        input_buffer_config.pitchBytes = src_img->w;

        #if __DCACHE_PRESENT
        if (src_img->w == src_rect->w) {
            SCB_CleanDCache_by_Addr(src8, src_rect->w * src_rect->h);
        } else {
            for (int i = 0; i < src_rect->h; i++) {
                SCB_CleanDCache_by_Addr(src8, src_rect->w);
                src8 += src_img->w;
            }
        }
        #endif
    } else {
        uint16_t *src16 = IMAGE_COMPUTE_RGB565_PIXEL_ROW_PTR(src_img, src_rect->y) + src_rect->x;

        input_buffer_config.bufferAddr = (uint32_t) src16;
        input_buffer_config.pitchBytes = src_img->w * sizeof(uint16_t);

        #if __DCACHE_PRESENT
        if (src_img->w == src_rect->w) {
            SCB_CleanDCache_by_Addr(src16, src_rect->w * src_rect->h * sizeof(uint16_t));
        } else {
            for (int i = 0; i < src_rect->h; i++) {
                SCB_CleanDCache_by_Addr(src16, src_rect->w * sizeof(uint16_t));
                src16 += src_img->w;
            }
        }
        #endif
    }

    PXP_SetProcessSurfaceBufferConfig(PXP, &input_buffer_config);
    PXP_SetProcessSurfaceScaler(PXP, src_rect->w, src_rect->h, dst_rect->w, dst_rect->h);
    PXP_SetProcessSurfacePosition(PXP, 0, 0, dst_rect->w - 1, dst_rect->h - 1);

    pxp_output_buffer_config_t output_buffer_config = {};

    if (dst_img->pixfmt == PIXFORMAT_GRAYSCALE) {
        uint8_t *dst8 = IMAGE_COMPUTE_GRAYSCALE_PIXEL_ROW_PTR(dst_img, dst_rect->y) + dst_rect->x;

        output_buffer_config.pixelFormat = kPXP_OutputPixelFormatY8;
        output_buffer_config.buffer0Addr = (uint32_t) dst8;
        output_buffer_config.pitchBytes = dst_img->w;

        #if __DCACHE_PRESENT
        // Ensures any cached writes to dst8 are flushed.
        if (dst_img->w == dst_rect->w) {
            SCB_CleanInvalidateDCache_by_Addr(dst8, dst_rect->w * dst_rect->h);
        } else {
            for (int i = 0; i < dst_rect->h; i++) {
                SCB_CleanInvalidateDCache_by_Addr(dst8, dst_rect->w);
                dst8 += dst_img->w;
            }
        }
        #endif
    } else {
        uint16_t *dst16 = IMAGE_COMPUTE_RGB565_PIXEL_ROW_PTR(dst_img, dst_rect->y) + dst_rect->x;

        output_buffer_config.pixelFormat = kPXP_OutputPixelFormatRGB565;
        output_buffer_config.buffer0Addr = (uint32_t) dst16;
        output_buffer_config.pitchBytes = dst_img->w * sizeof(uint16_t);

        PXP_EnableCsc1(PXP, src_img->pixfmt != PIXFORMAT_RGB565);

        #if __DCACHE_PRESENT
        // Ensures any cached writes to dst16 are flushed.
        if (dst_img->w == dst_rect->w) {
            SCB_CleanInvalidateDCache_by_Addr(dst16, dst_rect->w * dst_rect->h * sizeof(uint16_t));
        } else {
            for (int i = 0; i < dst_rect->h; i++) {
                SCB_CleanInvalidateDCache_by_Addr(dst16, dst_rect->w * sizeof(uint16_t));
                dst16 += dst_img->w;
            }
        }
        #endif
    }

    output_buffer_config.width = dst_rect->w;
    output_buffer_config.height = dst_rect->h;

    PXP_SetOutputBufferConfig(PXP, &output_buffer_config);

    pxp_flip_mode_t flip_mode = kPXP_FlipDisable;
    flip_mode |= (hint & IMAGE_HINT_HMIRROR) ? kPXP_FlipHorizontal : 0;
    flip_mode |= (hint & IMAGE_HINT_VFLIP) ? kPXP_FlipVertical : 0;
    PXP_SetRotateConfig(PXP, kPXP_RotateProcessSurface, kPXP_Rotate0, flip_mode);

    PXP_ClearStatusFlags(PXP, kPXP_CompleteFlag);
    PXP_Start(PXP);

    while (!(kPXP_CompleteFlag & PXP_GetStatusFlags(PXP))) {
        MICROPY_EVENT_POLL_HOOK
    }

    PXP_Reset(PXP);
    PXP_Deinit(PXP);

    if (dst_img->pixfmt == PIXFORMAT_GRAYSCALE) {
        uint8_t *dst8 = IMAGE_COMPUTE_GRAYSCALE_PIXEL_ROW_PTR(dst_img, dst_rect->y) + dst_rect->x;

        #if __DCACHE_PRESENT
        // Ensures any cached reads to dst8 are dropped.
        if (dst_img->w == dst_rect->w) {
            SCB_InvalidateDCache_by_Addr(dst8, dst_rect->w * dst_rect->h);
        } else {
            for (int i = 0; i < dst_rect->h; i++) {
                SCB_InvalidateDCache_by_Addr(dst8, dst_rect->w);
                dst8 += dst_img->w;
            }
        }
        #endif
    } else {
        uint16_t *dst16 = IMAGE_COMPUTE_RGB565_PIXEL_ROW_PTR(dst_img, dst_rect->y) + dst_rect->x;

        #if __DCACHE_PRESENT
        // Ensures any cached reads to dst16 are dropped.
        if (dst_img->w == dst_rect->w) {
            SCB_InvalidateDCache_by_Addr(dst16, dst_rect->w * dst_rect->h * sizeof(uint16_t));
        } else {
            for (int i = 0; i < dst_rect->h; i++) {
                SCB_InvalidateDCache_by_Addr(dst16, dst_rect->w * sizeof(uint16_t));
                dst16 += dst_img->w;
            }
        }
        #endif
    }

    OMV_PROFILE_PRINT();
    return 0;
}
#endif // (OMV_GPU_ENABLE == 1)
