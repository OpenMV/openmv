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
 * Hardware Accelerated JPEG Encoder and Decoder
 */
#include "omv_boardconfig.h"
#if (OMV_JPEG_CODEC_ENABLE == 1)
#include "imlib.h"

#include "py/mphal.h"
#include "py/runtime.h"

#include STM32_HAL_H
#include "irq.h"
#include "dma_utils.h"

#define JPEG_CODEC_TIMEOUT          (1000)
#define JPEG_ALLOC_PADDING          ((__SCB_DCACHE_LINE_SIZE) * 4)
#define JPEG_OUTPUT_CHUNK_SIZE      (512) // The minimum output buffer size is 2x this - so 1KB.
#define JPEG_MAX_MDMA_BLOCK_SIZE    (65536UL) // Maximum bytes MDMA can transfer at once.
#define JPEG_INPUT_FIFO_BYTES       (32)
#define JPEG_OUTPUT_FIFO_BYTES      (32)
#define JPEG_MDMA_IN                (0)
#define JPEG_MDMA_OUT               (1)

typedef struct jpeg_state {
    volatile uint32_t in_data_len;
    volatile uint32_t out_data_len_max;
    volatile uint32_t out_data_len;
    volatile bool input_paused;
    volatile bool output_paused;
    JPEG_HandleTypeDef jpeg_descr;
    #if defined(OMV_MDMA_CHANNEL_JPEG_IN)
    MDMA_HandleTypeDef mdma_descr[2];
    #endif
} jpeg_state_t;

static jpeg_state_t JPEG_state = {};

// JIFF-APP0 header designed to be injected at the start of the JPEG byte stream.
// Contains a variable sized COM header at the end for cache alignment.
static const uint8_t JPEG_APP0[] = {
    0xFF, 0xE0, // JIFF-APP0
    0x00, 0x10, // 16
    0x4A, 0x46, 0x49, 0x46, 0x00, // JIFF
    0x01, 0x01, // V1.01
    0x01, // DPI
    0x00, 0x00, // Xdensity 0
    0x00, 0x00, // Ydensity 0
    0x00, // Xthumbnail 0
    0x00, // Ythumbnail 0
    0xFF, 0xFE // COM
};

void JPEG_IRQHandler() {
    IRQ_ENTER(JPEG_IRQn);
    HAL_JPEG_IRQHandler(&JPEG_state.jpeg_descr);
    IRQ_EXIT(JPEG_IRQn);
}

#if defined(OMV_MDMA_CHANNEL_JPEG_IN)
void jpeg_mdma_irq_handler(void) {
    if (MDMA->GISR0 & (1 << OMV_MDMA_CHANNEL_JPEG_IN)) {
        HAL_MDMA_IRQHandler(&JPEG_state.mdma_descr[JPEG_MDMA_IN]);
    }
    if (MDMA->GISR0 & (1 << OMV_MDMA_CHANNEL_JPEG_OUT)) {
        HAL_MDMA_IRQHandler(&JPEG_state.mdma_descr[JPEG_MDMA_OUT]);
    }
}
#endif

static void jpeg_compress_get_data(JPEG_HandleTypeDef *hjpeg, uint32_t NbDecodedData) {
    HAL_JPEG_Pause(hjpeg, JPEG_PAUSE_RESUME_INPUT);
    JPEG_state.input_paused = true;
}

static void jpeg_compress_data_ready(JPEG_HandleTypeDef *hjpeg, uint8_t *pDataOut, uint32_t OutDataLength) {
    #if defined(OMV_MDMA_CHANNEL_JPEG_IN)
    if ((!(((uint32_t) pDataOut) % __SCB_DCACHE_LINE_SIZE)) && (OutDataLength == JPEG_OUTPUT_CHUNK_SIZE)) {
        // Ensure any cached reads are dropped.
        SCB_InvalidateDCache_by_Addr((uint32_t *) pDataOut, JPEG_OUTPUT_CHUNK_SIZE);
    }
    #endif

    // We have received this much data.
    JPEG_state.out_data_len += OutDataLength;

    if ((JPEG_state.out_data_len + JPEG_OUTPUT_CHUNK_SIZE) > JPEG_state.out_data_len_max) {
        // We will overflow if we receive anymore data.
        HAL_JPEG_Pause(hjpeg, JPEG_PAUSE_RESUME_OUTPUT);
        JPEG_state.output_paused = true;
    } else {
        uint8_t *new_pDataOut = pDataOut + OutDataLength;

        #if defined(OMV_MDMA_CHANNEL_JPEG_IN)
        // DMA will write data to the output buffer in __SCB_DCACHE_LINE_SIZE aligned chunks. At the
        // end of JPEG compression the processor will manually transfer the remaining parts of the
        // image in randomly aligned chunks. We only want to invalidate the cache of the output
        // buffer for the initial DMA chunks. So, this code below will do that and then only
        // invalidate aligned regions when the processor is moving the final parts of the image.
        if ((!(((uint32_t) new_pDataOut) % __SCB_DCACHE_LINE_SIZE)) && (OutDataLength == JPEG_OUTPUT_CHUNK_SIZE)) {
            SCB_InvalidateDCache_by_Addr((uint32_t *) new_pDataOut, JPEG_OUTPUT_CHUNK_SIZE);
        }
        #endif

        // We are ok to receive more data.
        HAL_JPEG_ConfigOutputBuffer(hjpeg, new_pDataOut, JPEG_OUTPUT_CHUNK_SIZE);
    }
}

bool jpeg_compress(image_t *src, image_t *dst, int quality, bool realloc, jpeg_subsampling_t subsampling) {
    OMV_PROFILE_START();
    HAL_JPEG_RegisterGetDataCallback(&JPEG_state.jpeg_descr, jpeg_compress_get_data);
    HAL_JPEG_RegisterDataReadyCallback(&JPEG_state.jpeg_descr, jpeg_compress_data_ready);

    int mcu_size = 0;
    JPEG_ConfTypeDef JPEG_Info;
    JPEG_Info.ImageWidth = src->w;
    JPEG_Info.ImageHeight = src->h;
    JPEG_Info.ImageQuality = quality;

    switch (src->pixfmt) {
        case PIXFORMAT_BINARY:
        case PIXFORMAT_GRAYSCALE:
            mcu_size = JPEG_444_GS_MCU_SIZE;
            JPEG_Info.ColorSpace = JPEG_GRAYSCALE_COLORSPACE;
            JPEG_Info.ChromaSubsampling = JPEG_444_SUBSAMPLING;
            break;
        case PIXFORMAT_RGB565:
        case PIXFORMAT_BAYER_ANY:
        case PIXFORMAT_YUV_ANY:
            mcu_size = JPEG_444_YCBCR_MCU_SIZE;
            JPEG_Info.ColorSpace = JPEG_YCBCR_COLORSPACE;
            JPEG_Info.ChromaSubsampling = JPEG_444_SUBSAMPLING;
            if (subsampling == JPEG_SUBSAMPLING_AUTO) {
                if (quality < 60) {
                    mcu_size = JPEG_422_YCBCR_MCU_SIZE;
                    JPEG_Info.ChromaSubsampling = JPEG_422_SUBSAMPLING;
                }
            } else if (subsampling == JPEG_SUBSAMPLING_422) {
                mcu_size = JPEG_422_YCBCR_MCU_SIZE;
                JPEG_Info.ChromaSubsampling = JPEG_422_SUBSAMPLING;
            } else if (subsampling == JPEG_SUBSAMPLING_420) {
                // not supported
                return true;
            }
            break;
        default:
            break;
    }

    if (memcmp(&JPEG_state.jpeg_descr.Conf, &JPEG_Info, sizeof(JPEG_ConfTypeDef))) {
        HAL_JPEG_ConfigEncoding(&JPEG_state.jpeg_descr, &JPEG_Info);
    }

    int mcu_w = (JPEG_Info.ChromaSubsampling == JPEG_444_SUBSAMPLING) ? JPEG_MCU_W : (JPEG_MCU_W * 2);
    int src_w_mcus = (src->w + mcu_w - 1) / mcu_w;
    int src_w_mcus_bytes = src_w_mcus * mcu_size;
    int src_w_mcus_bytes_2 = src_w_mcus_bytes * 2;

    // If dst->data == NULL then we need to fb_alloc() space for the payload which will be fb_free()'d
    // by the caller. We have to alloc this memory for all cases if we return from the method.
    if (!dst->data) {
        uint32_t avail = fb_avail(FB_ALLOC_NO_HINT);
        uint32_t space = src_w_mcus_bytes_2 + JPEG_ALLOC_PADDING;

        if (avail < space) {
            fb_alloc_fail();
        }

        dst->size = IMLIB_IMAGE_MAX_SIZE(avail - space);
        dst->data = fb_alloc(dst->size, FB_ALLOC_PREFER_SIZE | FB_ALLOC_CACHE_ALIGN);
    }

    if (src->is_compressed) {
        return true;
    }

    // Compute size of the APP0 header with cache alignment padding.
    int app0_size = sizeof(JPEG_APP0);
    int app0_unalign_size = app0_size % __SCB_DCACHE_LINE_SIZE;
    int app0_padding_size = app0_unalign_size ? (__SCB_DCACHE_LINE_SIZE - app0_unalign_size) : 0;
    int app0_total_size = app0_size + app0_padding_size;

    if (dst->size < app0_total_size) {
        return true; // overflow
    }

    // Adjust JPEG size and address by app0 header size.
    dst->size -= app0_total_size;
    uint8_t *dma_buffer = dst->data + app0_total_size;

    // Destination is too small.
    if (dst->size < (JPEG_OUTPUT_CHUNK_SIZE * 2)) {
        return true; // overflow
    }

    bool jpeg_overflow = false;

    JPEG_state.out_data_len_max = dst->size;
    JPEG_state.out_data_len = 0;
    JPEG_state.input_paused = false;
    JPEG_state.output_paused = false;

    uint8_t *mcu_row_buffer = fb_alloc(src_w_mcus_bytes_2, FB_ALLOC_PREFER_SPEED | FB_ALLOC_CACHE_ALIGN);

    for (int y_offset = 0; y_offset < src->h; y_offset += JPEG_MCU_H) {
        uint8_t *mcu_row_buffer_ptr = mcu_row_buffer + (src_w_mcus_bytes * ((y_offset / JPEG_MCU_H) % 2));
        int dy = IM_MIN(JPEG_MCU_H, src->h - y_offset);

        if (JPEG_Info.ChromaSubsampling == JPEG_444_SUBSAMPLING) {
            for (int x_offset = 0; x_offset < src->w; x_offset += JPEG_MCU_W) {
                int8_t *Y0 = (int8_t *) (mcu_row_buffer_ptr + (mcu_size * (x_offset / JPEG_MCU_W)));
                int8_t *CB = Y0 + JPEG_444_GS_MCU_SIZE;
                int8_t *CR = CB + JPEG_444_GS_MCU_SIZE;
                int dx = IM_MIN(JPEG_MCU_W, src->w - x_offset);

                // Copy 8x8 MCUs.
                jpeg_get_mcu(src, x_offset, y_offset, dx, dy, Y0, CB, CR);
            }
        } else if (JPEG_Info.ChromaSubsampling == JPEG_422_SUBSAMPLING) {
            // color only
            int8_t CB[JPEG_444_GS_MCU_SIZE * 2];
            int8_t CR[JPEG_444_GS_MCU_SIZE * 2];

            for (int x_offset = 0; x_offset < src->w; ) {
                int8_t *Y0 = (int8_t *) (mcu_row_buffer_ptr + (mcu_size * (x_offset / (JPEG_MCU_W * 2))));
                int8_t *Y1 = Y0 + JPEG_444_GS_MCU_SIZE;
                int8_t *CB_avg = Y1 + JPEG_444_GS_MCU_SIZE;
                int8_t *CR_avg = CB_avg + JPEG_444_GS_MCU_SIZE;

                for (int i = 0; i < (JPEG_444_GS_MCU_SIZE * 2);
                     i += JPEG_444_GS_MCU_SIZE, x_offset += JPEG_MCU_W) {
                    int dx = IM_MIN(JPEG_MCU_W, src->w - x_offset);

                    if (dx > 0) {
                        // Copy 8x8 MCUs.
                        jpeg_get_mcu(src, x_offset, y_offset, dx, dy, Y0 + i, CB + i, CR + i);
                    } else {
                        memset(Y0 + i, 0, JPEG_444_GS_MCU_SIZE);
                        memset(CB + i, 0, JPEG_444_GS_MCU_SIZE);
                        memset(CR + i, 0, JPEG_444_GS_MCU_SIZE);
                    }
                }

                // horizontal subsampling of U & V
                uint32_t mask = 0x80808080;
                uint32_t *CBp0 = (uint32_t *) CB;
                uint32_t *CRp0 = (uint32_t *) CR;
                uint32_t *CBp1 = (uint32_t *) (CB + JPEG_444_GS_MCU_SIZE);
                uint32_t *CRp1 = (uint32_t *) (CR + JPEG_444_GS_MCU_SIZE);
                for (int j = 0; j < JPEG_444_GS_MCU_SIZE; j += JPEG_MCU_W) {
                    uint32_t CBp0_3210 = *CBp0++ ^ mask;
                    uint32_t CBp0_avg_32_10 = __SHADD8(CBp0_3210, __UXTB16_RORn(CBp0_3210, 8)) ^ mask;
                    CB_avg[j] = CBp0_avg_32_10;
                    CB_avg[j + 1] = CBp0_avg_32_10 >> 16;

                    uint32_t CBp0_7654 = *CBp0++ ^ mask;
                    uint32_t CBp0_avg_76_54 = __SHADD8(CBp0_7654, __UXTB16_RORn(CBp0_7654, 8)) ^ mask;
                    CB_avg[j + 2] = CBp0_avg_76_54;
                    CB_avg[j + 3] = CBp0_avg_76_54 >> 16;

                    uint32_t CBp1_3210 = *CBp1++ ^ mask;
                    uint32_t CBp1_avg_32_10 = __SHADD8(CBp1_3210, __UXTB16_RORn(CBp1_3210, 8)) ^ mask;
                    CB_avg[j + 4] = CBp1_avg_32_10;
                    CB_avg[j + 5] = CBp1_avg_32_10 >> 16;

                    uint32_t CBp1_7654 = *CBp1++ ^ mask;
                    uint32_t CBp1_avg_76_54 = __SHADD8(CBp1_7654, __UXTB16_RORn(CBp1_7654, 8)) ^ mask;
                    CB_avg[j + 6] = CBp1_avg_76_54;
                    CB_avg[j + 7] = CBp1_avg_76_54 >> 16;

                    uint32_t CRp0_3210 = *CRp0++ ^ mask;
                    uint32_t CRp0_avg_32_10 = __SHADD8(CRp0_3210, __UXTB16_RORn(CRp0_3210, 8)) ^ mask;
                    CR_avg[j] = CRp0_avg_32_10;
                    CR_avg[j + 1] = CRp0_avg_32_10 >> 16;

                    uint32_t CRp0_7654 = *CRp0++ ^ mask;
                    uint32_t CRp0_avg_76_54 = __SHADD8(CRp0_7654, __UXTB16_RORn(CRp0_7654, 8)) ^ mask;
                    CR_avg[j + 2] = CRp0_avg_76_54;
                    CR_avg[j + 3] = CRp0_avg_76_54 >> 16;

                    uint32_t CRp1_3210 = *CRp1++ ^ mask;
                    uint32_t CRp1_avg_32_10 = __SHADD8(CRp1_3210, __UXTB16_RORn(CRp1_3210, 8)) ^ mask;
                    CR_avg[j + 4] = CRp1_avg_32_10;
                    CR_avg[j + 5] = CRp1_avg_32_10 >> 16;

                    uint32_t CRp1_7654 = *CRp1++ ^ mask;
                    uint32_t CRp1_avg_76_54 = __SHADD8(CRp1_7654, __UXTB16_RORn(CRp1_7654, 8)) ^ mask;
                    CR_avg[j + 6] = CRp1_avg_76_54;
                    CR_avg[j + 7] = CRp1_avg_76_54 >> 16;
                }
            }
        }

        #if defined(OMV_MDMA_CHANNEL_JPEG_IN)
        // Flush the MCU row for DMA...
        SCB_CleanDCache_by_Addr((uint32_t *) mcu_row_buffer_ptr, src_w_mcus_bytes);
        #endif

        if (!y_offset) {
            #if defined(OMV_MDMA_CHANNEL_JPEG_IN)
            // Invalidate the output buffer.
            SCB_InvalidateDCache_by_Addr(dma_buffer, JPEG_OUTPUT_CHUNK_SIZE);
            // Start the DMA process off on the first row of MCUs.
            HAL_JPEG_Encode_DMA(&JPEG_state.jpeg_descr, mcu_row_buffer_ptr, src_w_mcus_bytes, dma_buffer,
                                JPEG_OUTPUT_CHUNK_SIZE);
            #else
            HAL_JPEG_Encode_IT(&JPEG_state.jpeg_descr, mcu_row_buffer_ptr, src_w_mcus_bytes, dma_buffer,
                               JPEG_OUTPUT_CHUNK_SIZE);
            #endif
        } else {

            // Wait for the last row MCUs to be processed before starting the next row.
            for (mp_uint_t tickstart = mp_hal_ticks_ms(); !JPEG_state.input_paused; ) {
                if (JPEG_state.output_paused || ((mp_hal_ticks_ms() - tickstart) > JPEG_CODEC_TIMEOUT)) {
                    memset(&JPEG_state.jpeg_descr.Conf, 0, sizeof(JPEG_ConfTypeDef));
                    jpeg_overflow = true;
                    goto exit_cleanup;
                }
                MICROPY_EVENT_POLL_HOOK
            }

            // Reset the lock.
            JPEG_state.input_paused = false;
            // Restart the DMA process on the next row of MCUs (that were already prepared).
            HAL_JPEG_ConfigInputBuffer(&JPEG_state.jpeg_descr, mcu_row_buffer_ptr, src_w_mcus_bytes);
            HAL_JPEG_Resume(&JPEG_state.jpeg_descr, JPEG_PAUSE_RESUME_INPUT);
        }
    }

    // After writing the last MCU to the JPEG core it will eventually generate an end-of-conversion
    // interrupt which will finish the JPEG encoding process and clear the busy flag.

    for (mp_uint_t tickstart = mp_hal_ticks_ms();
         HAL_JPEG_GetState(&JPEG_state.jpeg_descr) == HAL_JPEG_STATE_BUSY_ENCODING; ) {
        if (JPEG_state.output_paused || ((mp_hal_ticks_ms() - tickstart) > JPEG_CODEC_TIMEOUT)) {
            memset(&JPEG_state.jpeg_descr.Conf, 0, sizeof(JPEG_ConfTypeDef));
            jpeg_overflow = true;
            goto exit_cleanup;
        }
        MICROPY_EVENT_POLL_HOOK
    }

    // Set output size.
    dst->size = JPEG_state.out_data_len;

    // STM32H7 BUG FIX! The JPEG Encoder will occasionally trigger the EOCF interrupt before writing
    // a final 0x000000D9 long into the output fifo as the end of the JPEG image. When this occurs
    // the output fifo will have a single 0 value in it after the encoding process finishes.
    if (__HAL_JPEG_GET_FLAG(&JPEG_state.jpeg_descr, JPEG_FLAG_OFNEF) && (!JPEG_state.jpeg_descr.Instance->DOR)) {
        // The encoding output process always aborts before writing JPEG_OUTPUT_CHUNK_SIZE bytes
        // to the end of the dma_buffer. So, it is always safe to add one extra byte.
        dma_buffer[dst->size++] = 0xD9;
    }

    // Update the JPEG image size by the new APP0 header and it's padding. However, we have to move
    // the SOI header to the front of the image first...
    dst->size += app0_total_size;
    memcpy(dst->data, dma_buffer, sizeof(uint16_t)); // move SOI
    memcpy(dst->data + sizeof(uint16_t), JPEG_APP0, sizeof(JPEG_APP0)); // inject APP0

    // Add on a comment header with 0 padding to ensure cache alignment after the APP0 header.
    *((uint16_t *) (dst->data + sizeof(uint16_t) + sizeof(JPEG_APP0))) = __REV16(app0_padding_size); // size
    memset(dst->data + sizeof(uint32_t) + sizeof(JPEG_APP0), 0, app0_padding_size - sizeof(uint16_t)); // data

    // Clean trailing data after 0xFFD9 at the end of the jpeg byte stream.
    dst->size = jpeg_clean_trailing_bytes(dst->size, dst->data);

exit_cleanup:
    // Cleanup jpeg state.
    HAL_JPEG_Abort(&JPEG_state.jpeg_descr);
    HAL_JPEG_UnRegisterDataReadyCallback(&JPEG_state.jpeg_descr);
    HAL_JPEG_UnRegisterGetDataCallback(&JPEG_state.jpeg_descr);

    fb_free(); // mcu_row_buffer (after DMA is aborted)

    OMV_PROFILE_PRINT();
    return jpeg_overflow;
}

static void jpeg_decompress_data_ready_abort(JPEG_HandleTypeDef *hjpeg, uint8_t *pDataOut, uint32_t OutDataLength) {
    HAL_JPEG_Abort(hjpeg);
}

static void jpeg_decompress_get_data(JPEG_HandleTypeDef *hjpeg, uint32_t NbDecodedData) {
    JPEG_state.jpeg_descr.pJpegInBuffPtr += NbDecodedData;
    JPEG_state.in_data_len -= NbDecodedData;
    HAL_JPEG_ConfigInputBuffer(&JPEG_state.jpeg_descr, JPEG_state.jpeg_descr.pJpegInBuffPtr,
                               IM_MIN(JPEG_state.in_data_len, JPEG_MAX_MDMA_BLOCK_SIZE));
}

static void jpeg_decompress_data_ready(JPEG_HandleTypeDef *hjpeg, uint8_t *pDataOut, uint32_t OutDataLength) {
    // We have received this much data.
    JPEG_state.out_data_len += OutDataLength;

    int remaining = JPEG_state.out_data_len_max - JPEG_state.out_data_len;

    if (!remaining) {
        HAL_JPEG_Pause(hjpeg, JPEG_PAUSE_RESUME_OUTPUT);
        JPEG_state.out_data_len = 0;
        JPEG_state.output_paused = true;
    } else {
        // We are ok to receive more data.
        HAL_JPEG_ConfigOutputBuffer(hjpeg, pDataOut + OutDataLength, IM_MIN(remaining, JPEG_OUTPUT_CHUNK_SIZE));
    }
}

void jpeg_decompress(image_t *dst, image_t *src) {
    OMV_PROFILE_START();

    // Verify the jpeg image is not a non-baseline jpeg image and check that is has
    // valid headers up to the start-of-scan header (which cannot be trivially walked).
    if (!jpeg_is_valid(src)) {
        mp_raise_msg(&mp_type_OSError, MP_ERROR_TEXT("Non-Baseline JPEGs are not supported."));
    }

    // Ensure src data is cache algined for MDMA and a multiple of fifo input size in bytes.
    JPEG_state.in_data_len = src->size;
    uint32_t diff = JPEG_state.in_data_len % JPEG_INPUT_FIFO_BYTES;

    if (diff) {
        JPEG_state.in_data_len += JPEG_INPUT_FIFO_BYTES - diff;
    }

    if (((uint32_t) src->data) % __SCB_DCACHE_LINE_SIZE) {
        // Copy to cache aligned buffer.
        JPEG_state.jpeg_descr.pJpegInBuffPtr = fb_alloc(JPEG_state.in_data_len, FB_ALLOC_CACHE_ALIGN);
        memcpy(JPEG_state.jpeg_descr.pJpegInBuffPtr, src->data, src->size);
    } else {
        JPEG_state.jpeg_descr.pJpegInBuffPtr = src->data;
    }

    // Skip zero remaining (__SCB_DCACHE_LINE_SIZE - (src->size % __SCB_DCACHE_LINE_SIZE)) bytes
    // of data as the JPEG bytestream will have already been closed via 0xFF, 0xD9.

    // Set handles for header decoding.
    HAL_JPEG_RegisterDataReadyCallback(&JPEG_state.jpeg_descr, jpeg_decompress_data_ready_abort);

    // Decode the JPEG Header...
    uint8_t temp[JPEG_OUTPUT_CHUNK_SIZE];
    HAL_JPEG_Decode(&JPEG_state.jpeg_descr, JPEG_state.jpeg_descr.pJpegInBuffPtr, JPEG_state.in_data_len,
                    temp, JPEG_OUTPUT_CHUNK_SIZE, JPEG_CODEC_TIMEOUT);

    if ((src->w != JPEG_state.jpeg_descr.Conf.ImageWidth) || (src->h != JPEG_state.jpeg_descr.Conf.ImageHeight)) {
        mp_raise_msg(&mp_type_ValueError, MP_ERROR_TEXT("JPEG Geometry does not match Image Object Geometry!"));
    }

    // Set handles for full decoding.
    HAL_JPEG_RegisterGetDataCallback(&JPEG_state.jpeg_descr, jpeg_decompress_get_data);
    HAL_JPEG_RegisterDataReadyCallback(&JPEG_state.jpeg_descr, jpeg_decompress_data_ready);

    int mcu_w = JPEG_MCU_W;
    int mcu_h = JPEG_MCU_H;
    int mcu_size = JPEG_444_GS_MCU_SIZE;
    DMA2D_HandleTypeDef DMA2D_Handle = {};

    if (JPEG_state.jpeg_descr.Conf.ColorSpace == JPEG_YCBCR_COLORSPACE) {
        switch (JPEG_state.jpeg_descr.Conf.ChromaSubsampling) {
            case JPEG_444_SUBSAMPLING: {
                mcu_w = JPEG_MCU_W;
                mcu_h = JPEG_MCU_H;
                mcu_size = JPEG_444_YCBCR_MCU_SIZE;
                DMA2D_Handle.LayerCfg[1].ChromaSubSampling = DMA2D_NO_CSS;
                break;
            }
            case JPEG_420_SUBSAMPLING: {
                mcu_w = JPEG_MCU_W * 2;
                mcu_h = JPEG_MCU_H * 2;
                mcu_size = JPEG_420_YCBCR_MCU_SIZE;
                DMA2D_Handle.LayerCfg[1].ChromaSubSampling = DMA2D_CSS_420;
                break;
            }
            case JPEG_422_SUBSAMPLING: {
                mcu_w = JPEG_MCU_W * 2;
                mcu_h = JPEG_MCU_H;
                mcu_size = JPEG_422_YCBCR_MCU_SIZE;
                DMA2D_Handle.LayerCfg[1].ChromaSubSampling = DMA2D_CSS_422;
                break;
            }
            default: {
                break;
            }
        }

        if (dst->is_color) {
            DMA2D_Handle.Instance = DMA2D;

            // Configure DMA2D output.
            DMA2D_Handle.Init.Mode = DMA2D_M2M_PFC;
            DMA2D_Handle.Init.ColorMode = DMA2D_OUTPUT_RGB565;
            DMA2D_Handle.Init.OutputOffset = 0;
            DMA2D_Handle.Init.AlphaInverted = DMA2D_REGULAR_ALPHA;
            DMA2D_Handle.Init.RedBlueSwap = DMA2D_RB_REGULAR;
            DMA2D_Handle.Init.BytesSwap = DMA2D_BYTES_REGULAR;
            DMA2D_Handle.Init.LineOffsetMode = DMA2D_LOM_PIXELS;

            // Configure DMA2D input.
            DMA2D_Handle.LayerCfg[1].InputOffset = (((src->w + mcu_w - 1) / mcu_w) * mcu_w) - src->w;
            DMA2D_Handle.LayerCfg[1].InputColorMode = DMA2D_INPUT_YCBCR;
            DMA2D_Handle.LayerCfg[1].AlphaMode = DMA2D_NO_MODIF_ALPHA;
            DMA2D_Handle.LayerCfg[1].InputAlpha = 0xFF;
            DMA2D_Handle.LayerCfg[1].AlphaInverted = DMA2D_REGULAR_ALPHA;
            DMA2D_Handle.LayerCfg[1].RedBlueSwap = DMA2D_RB_REGULAR;

            // DMA2D initialization.
            HAL_DMA2D_Init(&DMA2D_Handle);
            HAL_DMA2D_ConfigLayer(&DMA2D_Handle, 1);

            // Ensure any cached writes are dropped.
            SCB_InvalidateDCache_by_Addr((uint32_t *) dst->data, image_size(dst));
        }
    } else if (JPEG_state.jpeg_descr.Conf.ColorSpace == JPEG_CMYK_COLORSPACE) {
        if (((uint32_t) src->data) % __SCB_DCACHE_LINE_SIZE) {
            fb_free(); // JPEG_state.jpeg_descr.pJpegInBuffPtr
        }
        mp_raise_msg(&mp_type_ValueError, MP_ERROR_TEXT("Unsupported JPEG!"));
    }

    uint32_t dst_w_mcus = (src->w + mcu_w - 1) / mcu_w;
    uint32_t dst_w_mcus_bytes = dst_w_mcus * mcu_size;
    uint32_t dst_w_mcus_bytes_2 = dst_w_mcus_bytes * 2;

    JPEG_state.out_data_len_max = dst_w_mcus_bytes;
    JPEG_state.out_data_len = 0;
    JPEG_state.output_paused = false;

    uint8_t *mcu_row_buffer = fb_alloc(dst_w_mcus_bytes_2, FB_ALLOC_PREFER_SPEED | FB_ALLOC_CACHE_ALIGN);

    #if defined(OMV_MDMA_CHANNEL_JPEG_IN)
    // Flush input.
    SCB_CleanDCache_by_Addr((uint32_t *) JPEG_state.jpeg_descr.pJpegInBuffPtr, JPEG_state.in_data_len);
    // Invalidate the MCU row for DMA.
    SCB_InvalidateDCache_by_Addr((uint32_t *) mcu_row_buffer, dst_w_mcus_bytes);
    // Start the DMA process on the image.
    HAL_JPEG_Decode_DMA(&JPEG_state.jpeg_descr,
                        JPEG_state.jpeg_descr.pJpegInBuffPtr, IM_MIN(JPEG_state.in_data_len, JPEG_MAX_MDMA_BLOCK_SIZE),
                        mcu_row_buffer, IM_MIN(dst_w_mcus_bytes, JPEG_MAX_MDMA_BLOCK_SIZE));
    #else
    HAL_JPEG_Decode_IT(&JPEG_state.jpeg_descr,
                       JPEG_state.jpeg_descr.pJpegInBuffPtr, IM_MIN(JPEG_state.in_data_len, JPEG_MAX_MDMA_BLOCK_SIZE),
                       mcu_row_buffer, IM_MIN(dst_w_mcus_bytes, JPEG_MAX_MDMA_BLOCK_SIZE));
    #endif

    for (int y_offset = 0; y_offset < src->h; y_offset += mcu_h) {
        int h = y_offset / mcu_h;
        uint8_t *this_mcu_row_buffer_ptr = mcu_row_buffer + (dst_w_mcus_bytes * (h % 2));
        uint8_t *next_mcu_row_buffer_ptr = mcu_row_buffer + (dst_w_mcus_bytes * ((h + 1) % 2));
        int dy = IM_MIN(mcu_h, src->h - y_offset);

        #if defined(OMV_MDMA_CHANNEL_JPEG_IN)
        if ((y_offset + mcu_h) < src->h) {
            // not last row
            // Invalidate the MCU row for DMA.
            SCB_InvalidateDCache_by_Addr((uint32_t *) next_mcu_row_buffer_ptr, dst_w_mcus_bytes);
        }
        #endif

        // Wait for the MCUs to be processed.
        for (mp_uint_t tick_start = mp_hal_ticks_ms(); !JPEG_state.output_paused; ) {
            if ((mp_hal_ticks_ms() - tick_start) > JPEG_CODEC_TIMEOUT) {
                goto exit_cleanup;
            }
            MICROPY_EVENT_POLL_HOOK
        }

        if ((y_offset + mcu_h) < src->h) {
            // not last row
            // Reset the lock.
            JPEG_state.output_paused = false;
            // Restart the DMA process on the next row of MCUs.
            HAL_JPEG_ConfigOutputBuffer(&JPEG_state.jpeg_descr,
                                        next_mcu_row_buffer_ptr, IM_MIN(dst_w_mcus_bytes, JPEG_MAX_MDMA_BLOCK_SIZE));
            HAL_JPEG_Resume(&JPEG_state.jpeg_descr, JPEG_PAUSE_RESUME_OUTPUT);
        }

        #if defined(OMV_MDMA_CHANNEL_JPEG_IN)
        // Ensure any cached reads are dropped.
        SCB_InvalidateDCache_by_Addr((uint32_t *) this_mcu_row_buffer_ptr, dst_w_mcus_bytes);
        #endif

        if (JPEG_state.jpeg_descr.Conf.ColorSpace == JPEG_GRAYSCALE_COLORSPACE) {
            for (int x_offset = 0; x_offset < src->w; x_offset += JPEG_MCU_W) {
                uint8_t *Y0 = this_mcu_row_buffer_ptr + (x_offset * JPEG_MCU_H);
                int dx = IM_MIN(JPEG_MCU_W, src->w - x_offset);

                switch (dst->pixfmt) {
                    case PIXFORMAT_BINARY: {
                        for (int y = y_offset; y < (y_offset + dy); y++) {
                            uint32_t *rp = IMAGE_COMPUTE_BINARY_PIXEL_ROW_PTR(dst, y);

                            for (int x = x_offset; x < (x_offset + dx); x++) {
                                int p = *Y0++;
                                int v = p > 128;
                                IMAGE_PUT_BINARY_PIXEL_FAST(rp, x, v);
                            }

                            Y0 += JPEG_MCU_W - dx;
                        }
                        break;
                    }
                    case PIXFORMAT_GRAYSCALE: {
                        for (int y = y_offset; y < (y_offset + dy); y++) {
                            uint8_t *rp = IMAGE_COMPUTE_GRAYSCALE_PIXEL_ROW_PTR(dst, y) + x_offset;

                            if (dx == JPEG_MCU_W) {
                                *((uint32_t *) rp) = *((uint32_t *) Y0);
                                *(((uint32_t *) rp) + 1) = *(((uint32_t *) Y0) + 1);
                            } else if (dx >= 4) {
                                *((uint32_t *) rp) = *((uint32_t *) Y0);

                                if (dx >= 6) {
                                    *(((uint16_t *) rp) + 2) = *(((uint16_t *) Y0) + 2);

                                    if (dx & 1) {
                                        rp[6] = Y0[6];
                                    }
                                } else if (dx & 1) {
                                    rp[4] = Y0[4];
                                }
                            } else if (dx >= 2) {
                                *((uint16_t *) rp) = *((uint16_t *) Y0);

                                if (dx & 1) {
                                    rp[2] = Y0[2];
                                }
                            } else {
                                *rp = *Y0;
                            }

                            Y0 += JPEG_MCU_W;
                        }
                        break;
                    }
                    case PIXFORMAT_RGB565: {
                        for (int y = y_offset; y < (y_offset + dy); y++) {
                            uint16_t *rp = IMAGE_COMPUTE_RGB565_PIXEL_ROW_PTR(dst, y);

                            for (int x = x_offset; x < (x_offset + dx); x++) {
                                int p = *Y0++;
                                int v = COLOR_Y_TO_RGB565(p);
                                IMAGE_PUT_RGB565_PIXEL_FAST(rp, x, v);
                            }

                            Y0 += JPEG_MCU_W - dx;
                        }
                        break;
                    }
                }
            }
        } else if (JPEG_state.jpeg_descr.Conf.ColorSpace == JPEG_YCBCR_COLORSPACE) {
            switch (dst->pixfmt) {
                case PIXFORMAT_BINARY: {
                    for (int x_offset = 0; x_offset < src->w; x_offset += mcu_w) {
                        for (int int_y_offset = 0; int_y_offset < mcu_h; int_y_offset += JPEG_MCU_H) {
                            int dy = IM_MIN(JPEG_MCU_H, src->h - int_y_offset - y_offset);

                            for (int int_x_offset = 0; int_x_offset < mcu_w; int_x_offset += JPEG_MCU_W) {
                                uint8_t *Y0 = this_mcu_row_buffer_ptr + ((x_offset / mcu_w) * mcu_size) +
                                              (int_y_offset * mcu_w) + (int_x_offset * JPEG_MCU_H);
                                int dx = IM_MIN(JPEG_MCU_W, src->w - int_x_offset - x_offset);

                                for (int y = y_offset + int_y_offset; y < (y_offset + int_y_offset + dy); y++) {
                                    uint32_t *rp = IMAGE_COMPUTE_BINARY_PIXEL_ROW_PTR(dst, y);

                                    for (int x = x_offset + int_x_offset; x < (x_offset + int_x_offset + dx); x++) {
                                        int p = *Y0++;
                                        int v = p > 128;
                                        IMAGE_PUT_BINARY_PIXEL_FAST(rp, x, v);
                                    }

                                    Y0 += JPEG_MCU_W - dx;
                                }
                            }
                        }
                    }
                    break;
                }
                case PIXFORMAT_GRAYSCALE: {
                    for (int x_offset = 0; x_offset < src->w; x_offset += mcu_w) {
                        for (int int_y_offset = 0; int_y_offset < mcu_h; int_y_offset += JPEG_MCU_H) {
                            int dy = IM_MIN(JPEG_MCU_H, src->h - int_y_offset - y_offset);

                            for (int int_x_offset = 0; int_x_offset < mcu_w; int_x_offset += JPEG_MCU_W) {
                                uint8_t *Y0 = this_mcu_row_buffer_ptr + ((x_offset / mcu_w) * mcu_size) +
                                              (int_y_offset * mcu_w) + (int_x_offset * JPEG_MCU_H);
                                int dx = IM_MIN(JPEG_MCU_W, src->w - int_x_offset - x_offset);

                                for (int y = y_offset + int_y_offset; y < (y_offset + int_y_offset + dy); y++) {
                                    uint8_t *rp = IMAGE_COMPUTE_GRAYSCALE_PIXEL_ROW_PTR(dst, y) + x_offset + int_x_offset;

                                    if (dx == JPEG_MCU_W) {
                                        *((uint32_t *) rp) = *((uint32_t *) Y0);
                                        *(((uint32_t *) rp) + 1) = *(((uint32_t *) Y0) + 1);
                                    } else if (dx >= 4) {
                                        *((uint32_t *) rp) = *((uint32_t *) Y0);

                                        if (dx >= 6) {
                                            *(((uint16_t *) rp) + 2) = *(((uint16_t *) Y0) + 2);

                                            if (dx & 1) {
                                                rp[6] = Y0[6];
                                            }
                                        } else if (dx & 1) {
                                            rp[4] = Y0[4];
                                        }
                                    } else if (dx >= 2) {
                                        *((uint16_t *) rp) = *((uint16_t *) Y0);

                                        if (dx & 1) {
                                            rp[2] = Y0[2];
                                        }
                                    } else {
                                        *rp = *Y0;
                                    }

                                    Y0 += JPEG_MCU_W;
                                }
                            }
                        }
                    }
                    break;
                }
                case PIXFORMAT_RGB565: {
                    #if !defined(OMV_MDMA_CHANNEL_JPEG_IN)
                    // Ensure any cached writes are written.
                    SCB_CleanDCache_by_Addr((uint32_t *) this_mcu_row_buffer_ptr, dst_w_mcus_bytes);
                    #endif
                    uint16_t *rp = IMAGE_COMPUTE_RGB565_PIXEL_ROW_PTR(dst, y_offset);
                    HAL_DMA2D_Start(&DMA2D_Handle, (uint32_t) this_mcu_row_buffer_ptr, (uint32_t) rp, dst->w, dy);

                    // Invalidate any cached reads for the previous line that was just written.
                    if ((y_offset - mcu_h) >= 0) {
                        uint16_t *previous_rp = IMAGE_COMPUTE_RGB565_PIXEL_ROW_PTR(dst, (y_offset - mcu_h));
                        SCB_InvalidateDCache_by_Addr((uint32_t *) previous_rp, dst->w * mcu_h * sizeof(uint16_t));
                    }

                    HAL_DMA2D_PollForTransfer(&DMA2D_Handle, JPEG_CODEC_TIMEOUT);

                    // For the last row invalidate any cached reads for the line that was just written.
                    if ((y_offset + mcu_h) >= src->h) {
                        SCB_InvalidateDCache_by_Addr((uint32_t *) rp, dst->w * mcu_h * sizeof(uint16_t));
                    }

                    break;
                }
            }
        }

        if ((y_offset + mcu_h) >= src->h) {
            // last row
            for (mp_uint_t tick_start = mp_hal_ticks_ms();
                 HAL_JPEG_GetState(&JPEG_state.jpeg_descr) == HAL_JPEG_STATE_BUSY_DECODING; ) {
                if ((mp_hal_ticks_ms() - tick_start) > JPEG_CODEC_TIMEOUT) {
                    goto exit_cleanup;
                }
                MICROPY_EVENT_POLL_HOOK
            }
        }
    }

exit_cleanup:

    // Cleanup jpeg state.
    HAL_JPEG_Abort(&JPEG_state.jpeg_descr);
    HAL_JPEG_UnRegisterDataReadyCallback(&JPEG_state.jpeg_descr);
    HAL_JPEG_UnRegisterGetDataCallback(&JPEG_state.jpeg_descr);

    fb_free(); // mcu_row_buffer (after DMA is aborted)

    if ((JPEG_state.jpeg_descr.Conf.ColorSpace == JPEG_YCBCR_COLORSPACE) && dst->is_color) {
        HAL_DMA2D_DeInit(&DMA2D_Handle);
    }

    if (((uint32_t) src->data) % __SCB_DCACHE_LINE_SIZE) {
        fb_free(); // JPEG_state.jpeg_descr.pJpegInBuffPtr (after DMA is aborted)
    }

    OMV_PROFILE_PRINT();
}

void imlib_hardware_jpeg_init() {
    JPEG_state.jpeg_descr.Instance = JPEG;
    HAL_JPEG_Init(&JPEG_state.jpeg_descr);
    NVIC_SetPriority(JPEG_IRQn, IRQ_PRI_JPEG);
    HAL_NVIC_EnableIRQ(JPEG_IRQn);

    #if defined(OMV_MDMA_CHANNEL_JPEG_IN)
    JPEG_state.mdma_descr[JPEG_MDMA_IN].Instance = MDMA_CHAN_TO_INSTANCE(OMV_MDMA_CHANNEL_JPEG_IN);
    JPEG_state.mdma_descr[JPEG_MDMA_IN].Init.Request = MDMA_REQUEST_JPEG_INFIFO_TH;
    JPEG_state.mdma_descr[JPEG_MDMA_IN].Init.TransferTriggerMode = MDMA_BUFFER_TRANSFER;
    JPEG_state.mdma_descr[JPEG_MDMA_IN].Init.Priority = MDMA_PRIORITY_LOW;
    JPEG_state.mdma_descr[JPEG_MDMA_IN].Init.Endianness = MDMA_LITTLE_ENDIANNESS_PRESERVE;
    JPEG_state.mdma_descr[JPEG_MDMA_IN].Init.SourceInc = MDMA_SRC_INC_DOUBLEWORD;
    JPEG_state.mdma_descr[JPEG_MDMA_IN].Init.DestinationInc = MDMA_DEST_INC_DISABLE;
    JPEG_state.mdma_descr[JPEG_MDMA_IN].Init.SourceDataSize = MDMA_SRC_DATASIZE_DOUBLEWORD;
    JPEG_state.mdma_descr[JPEG_MDMA_IN].Init.DestDataSize = MDMA_DEST_DATASIZE_WORD;
    JPEG_state.mdma_descr[JPEG_MDMA_IN].Init.DataAlignment = MDMA_DATAALIGN_PACKENABLE;
    JPEG_state.mdma_descr[JPEG_MDMA_IN].Init.BufferTransferLength = JPEG_INPUT_FIFO_BYTES;
    JPEG_state.mdma_descr[JPEG_MDMA_IN].Init.SourceBurst = MDMA_SOURCE_BURST_4BEATS;
    JPEG_state.mdma_descr[JPEG_MDMA_IN].Init.DestBurst = MDMA_DEST_BURST_8BEATS;
    JPEG_state.mdma_descr[JPEG_MDMA_IN].Init.SourceBlockAddressOffset = 0;
    JPEG_state.mdma_descr[JPEG_MDMA_IN].Init.DestBlockAddressOffset = 0;

    HAL_MDMA_Init(&JPEG_state.mdma_descr[JPEG_MDMA_IN]);
    __HAL_LINKDMA(&JPEG_state.jpeg_descr, hdmain, JPEG_state.mdma_descr[JPEG_MDMA_IN]);

    JPEG_state.mdma_descr[JPEG_MDMA_OUT].Instance = MDMA_CHAN_TO_INSTANCE(OMV_MDMA_CHANNEL_JPEG_OUT);
    JPEG_state.mdma_descr[JPEG_MDMA_OUT].Init.Request = MDMA_REQUEST_JPEG_OUTFIFO_TH;
    JPEG_state.mdma_descr[JPEG_MDMA_OUT].Init.TransferTriggerMode = MDMA_BUFFER_TRANSFER;
    JPEG_state.mdma_descr[JPEG_MDMA_OUT].Init.Priority = MDMA_PRIORITY_LOW;
    JPEG_state.mdma_descr[JPEG_MDMA_OUT].Init.Endianness = MDMA_LITTLE_ENDIANNESS_PRESERVE;
    JPEG_state.mdma_descr[JPEG_MDMA_OUT].Init.SourceInc = MDMA_SRC_INC_DISABLE;
    JPEG_state.mdma_descr[JPEG_MDMA_OUT].Init.DestinationInc = MDMA_DEST_INC_DOUBLEWORD;
    JPEG_state.mdma_descr[JPEG_MDMA_OUT].Init.SourceDataSize = MDMA_SRC_DATASIZE_WORD;
    JPEG_state.mdma_descr[JPEG_MDMA_OUT].Init.DestDataSize = MDMA_DEST_DATASIZE_DOUBLEWORD;
    JPEG_state.mdma_descr[JPEG_MDMA_OUT].Init.DataAlignment = MDMA_DATAALIGN_PACKENABLE;
    JPEG_state.mdma_descr[JPEG_MDMA_OUT].Init.BufferTransferLength = JPEG_OUTPUT_FIFO_BYTES;
    JPEG_state.mdma_descr[JPEG_MDMA_OUT].Init.SourceBurst = MDMA_SOURCE_BURST_8BEATS;
    JPEG_state.mdma_descr[JPEG_MDMA_OUT].Init.DestBurst = MDMA_DEST_BURST_4BEATS;
    JPEG_state.mdma_descr[JPEG_MDMA_OUT].Init.SourceBlockAddressOffset = 0;
    JPEG_state.mdma_descr[JPEG_MDMA_OUT].Init.DestBlockAddressOffset = 0;

    HAL_MDMA_Init(&JPEG_state.mdma_descr[JPEG_MDMA_OUT]);
    __HAL_LINKDMA(&JPEG_state.jpeg_descr, hdmaout, JPEG_state.mdma_descr[JPEG_MDMA_OUT]);
    #endif
}

void imlib_hardware_jpeg_deinit() {
    memset(&JPEG_state.jpeg_descr.Conf, 0, sizeof(JPEG_ConfTypeDef));
    HAL_JPEG_Abort(&JPEG_state.jpeg_descr);
    #if defined(OMV_MDMA_CHANNEL_JPEG_IN)
    HAL_MDMA_DeInit(&JPEG_state.mdma_descr[JPEG_MDMA_OUT]);
    HAL_MDMA_DeInit(&JPEG_state.mdma_descr[JPEG_MDMA_IN]);
    #endif
    HAL_NVIC_DisableIRQ(JPEG_IRQn);
    HAL_JPEG_DeInit(&JPEG_state.jpeg_descr);
}
#endif
