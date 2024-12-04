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
 * GPU driver for STM32 port.
 */
#include "omv_boardconfig.h"
#if (OMV_GPU_ENABLE == 1)
#include STM32_HAL_H
#include "imlib.h"
#include "dma.h"

int omv_gpu_init() {
    return 0;
}

void omv_gpu_deinit() {
    DMA2D_HandleTypeDef dma2d = {};
    dma2d.Instance = DMA2D;
    HAL_DMA2D_DeInit(&dma2d);
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

    // DMA2D can only draw on RGB565 buffers and the destination/source buffers must be accessible by DMA.
    if ((dst_img->pixfmt != PIXFORMAT_RGB565) || (!DMA_BUFFER(dst_img->data)) || (!DMA_BUFFER(src_img->data))) {
        return -1;
    }

    // DMA2D can only read from RGB565 or GRAYSCALE buffers.
    // If the source image is RGB565, it must not have a color or alpha palette.
    if ((src_img->pixfmt != PIXFORMAT_GRAYSCALE) &&
        ((src_img->pixfmt != PIXFORMAT_RGB565) || color_palette || alpha_palette)) {
        return -1;
    }

    // DMA2D cannot scale so ensure that the source and destination rectangles are the same size.
    // The bilinear/nearest-neighbor flag doesn't matter since we can't scale.
    if ((dst_rect->w != src_rect->w) || (dst_rect->h != src_rect->h)) {
        return -1;
    }

    // DMA2D cannot hmirror or vflip.
    if (hint & (IMAGE_HINT_HMIRROR | IMAGE_HINT_VFLIP)) {
        return -1;
    }

    // DMA2D must always fetch the background on the F4 and F7 series so do this in software.
    #if defined(STM32F4) || defined(STM32F7)
    if (hint & IMAGE_HINT_BLACK_BACKGROUND) {
        return -1;
    }
    #endif

    DMA2D_HandleTypeDef dma2d = {};

    dma2d.Instance = DMA2D;

    if (dst_img->pixfmt != src_img->pixfmt) {
        dma2d.Init.Mode = DMA2D_M2M_PFC;
    }

    if ((alpha != 255) || alpha_palette) {
        dma2d.Init.Mode = DMA2D_M2M_BLEND;
    }

    #if defined(STM32H7)
    if (hint & IMAGE_HINT_BLACK_BACKGROUND) {
        dma2d.Init.Mode = DMA2D_M2M_BLEND_BG;
    }
    #endif

    dma2d.Init.ColorMode = DMA2D_OUTPUT_RGB565;
    dma2d.Init.OutputOffset = dst_img->w - dst_rect->w;

    HAL_DMA2D_Init(&dma2d);

    dma2d.LayerCfg[0].InputOffset = dst_img->w - dst_rect->w;
    dma2d.LayerCfg[0].InputColorMode = DMA2D_INPUT_RGB565;
    dma2d.LayerCfg[0].AlphaMode = DMA2D_REPLACE_ALPHA;
    dma2d.LayerCfg[0].InputAlpha = 0xff;

    HAL_DMA2D_ConfigLayer(&dma2d, 0);

    if (src_img->pixfmt == PIXFORMAT_GRAYSCALE) {
        dma2d.LayerCfg[1].InputColorMode = DMA2D_INPUT_L8;
        dma2d.LayerCfg[1].AlphaMode = DMA2D_COMBINE_ALPHA;
        uint32_t *clut = fb_alloc(256 * sizeof(uint32_t));

        if (!alpha_palette) {
            if (!color_palette) {
                for (int i = 0; i < 256; i++) {
                    clut[i] = (0xff << 24) | COLOR_Y_TO_RGB888(i);
                }
            } else {
                for (int i = 0; i < 256; i++) {
                    int pixel = color_palette[i];
                    clut[i] = (0xff << 24) |
                              (COLOR_RGB565_TO_R8(pixel) << 16) |
                              (COLOR_RGB565_TO_G8(pixel) << 8) |
                              COLOR_RGB565_TO_B8(pixel);
                }
            }
        } else {
            if (!color_palette) {
                for (int i = 0; i < 256; i++) {
                    clut[i] = (alpha_palette[i] << 24) | COLOR_Y_TO_RGB888(i);
                }
            } else {
                for (int i = 0; i < 256; i++) {
                    int pixel = color_palette[i];
                    clut[i] = (alpha_palette[i] << 24) |
                              (COLOR_RGB565_TO_R8(pixel) << 16) |
                              (COLOR_RGB565_TO_G8(pixel) << 8) |
                              COLOR_RGB565_TO_B8(pixel);
                }
            }
        }

        DMA2D_CLUTCfgTypeDef cfg;
        cfg.pCLUT = clut;
        cfg.CLUTColorMode = DMA2D_CCM_ARGB8888;
        cfg.Size = 255;
        #if __DCACHE_PRESENT
        SCB_CleanDCache_by_Addr(clut, 256 * sizeof(uint32_t));
        #endif
        HAL_DMA2D_CLUTLoad(&dma2d, cfg, 1);
        HAL_DMA2D_PollForTransfer(&dma2d, 1000);
    } else {
        dma2d.LayerCfg[1].InputColorMode = DMA2D_INPUT_RGB565;
        dma2d.LayerCfg[1].AlphaMode = DMA2D_REPLACE_ALPHA;
    }

    dma2d.LayerCfg[1].InputOffset = src_img->w - src_rect->w;
    dma2d.LayerCfg[1].InputAlpha = alpha;

    HAL_DMA2D_ConfigLayer(&dma2d, 1);

    uint16_t *dst16 = IMAGE_COMPUTE_RGB565_PIXEL_ROW_PTR(dst_img, dst_rect->y) + dst_rect->x;

    #if __DCACHE_PRESENT
    // Ensures any cached writes to dst16 are flushed.
    uint16_t *dst16_tmp = dst16;
    for (int i = 0; i < dst_rect->h; i++) {
        SCB_CleanInvalidateDCache_by_Addr(dst16_tmp, dst_rect->w * sizeof(uint16_t));
        dst16_tmp += dst_img->w;
    }
    #endif

    uint32_t src;

    if (src_img->pixfmt == PIXFORMAT_GRAYSCALE) {
        uint8_t *src8 = IMAGE_COMPUTE_GRAYSCALE_PIXEL_ROW_PTR(src_img, src_rect->y) + src_rect->x;
        src = (uint32_t) src8;

        #if __DCACHE_PRESENT
        uint8_t *src8_tmp = src8;
        for (int i = 0; i < src_rect->h; i++) {
            SCB_CleanDCache_by_Addr(src8_tmp, src_rect->w);
            src8_tmp += src_img->w;
        }
        #endif
    } else {
        uint16_t *src16 = IMAGE_COMPUTE_RGB565_PIXEL_ROW_PTR(src_img, src_rect->y) + src_rect->x;
        src = (uint32_t) src16;

        #if __DCACHE_PRESENT
        uint16_t *src16_tmp = src16;
        for (int i = 0; i < src_rect->h; i++) {
            SCB_CleanDCache_by_Addr(src16_tmp, src_rect->w * sizeof(uint16_t));
            src16_tmp += src_img->w;
        }
        #endif
    }

    uint32_t dst = (uint32_t) dst16;

    #if defined(STM32H7)
    if (hint & IMAGE_HINT_BLACK_BACKGROUND) {
        dst = 0;
    }
    #endif

    HAL_DMA2D_BlendingStart(&dma2d, src, dst, (uint32_t) dst16, dst_rect->w, dst_rect->h);
    HAL_DMA2D_PollForTransfer(&dma2d, 1000);

    #if __DCACHE_PRESENT
    // Ensures any cached reads to dst16 are dropped.
    dst16_tmp = dst16;
    for (int i = 0; i < dst_rect->h; i++) {
        SCB_InvalidateDCache_by_Addr(dst16_tmp, dst_rect->w * sizeof(uint16_t));
        dst16_tmp += dst_img->w;
    }
    #endif

    HAL_DMA2D_DeInit(&dma2d);

    if (src_img->pixfmt == PIXFORMAT_GRAYSCALE) {
        fb_free(); // clut
    }

    OMV_PROFILE_PRINT();
    return 0;
}
#endif // (OMV_GPU_ENABLE == 1)
