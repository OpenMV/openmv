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
 * MicroPython board config.
 */
#ifndef __OMV_BOARDCONFIG_H__
#define __OMV_BOARDCONFIG_H__


// Architecture info
// #define OMV_BOARD_ARCH                  "WASM"    // 33 chars max
// #define OMV_BOARD_TYPE                  "webassembly"
// #define OMV_BOARD_UID_ADDR              0x401f4410   // Unique ID address.
// #define OMV_BOARD_UID_SIZE              3            // Unique ID size in words.
// #define OMV_BOARD_UID_OFFSET            12           // Bytes offset for multi-word UIDs.

// // JPEG configuration.
#define OMV_JPEG_CODEC_ENABLE           (0)
#define OMV_JPEG_QUALITY_LOW            (50)
#define OMV_JPEG_QUALITY_HIGH           (90)
#define OMV_JPEG_QUALITY_THRESHOLD      (320 * 240 * 2)

// // UMM heap block size
#define OMV_UMM_BLOCK_SIZE              256

#define OMV_FB_SIZE                     (64 * 1024 * 1024)        // FB memory: header + VGA/GS image
#define OMV_FB_ALLOC_SIZE               (64 * 1024 * 1024)         // minimum fb alloc size
// #define OMV_FB_OVERLAY_SIZE             (1024 * 1024)
// #define OMV_STACK_SIZE                  (64 * 1024)
// #define OMV_HEAP_SIZE                   (512 * 1024)
// #define OMV_SDRAM_SIZE                  (64 * 1024 * 1024)  // This needs to be here for UVC firmware.
// #define OMV_LINE_BUF_SIZE               (22 * 1024)  // Image line buffer.
#define OMV_JPEG_SIZE                   (2 * 1024 * 1024)  // IDE JPEG buffer (header + data).

#endif //__OMV_BOARDCONFIG_H__
