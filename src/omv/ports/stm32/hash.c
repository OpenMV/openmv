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
 * Hash functions.
 */
#if (OMV_ENABLE_HASH == 1)
#include STM32_HAL_H
#include <stdbool.h>
#include <stdio.h>
#include "hash.h"
#include "fb_alloc.h"
#include "file_utils.h"

#define BLOCK_SIZE    (512)
static HASH_HandleTypeDef hhash;

int hash_start() {
    if (HAL_HASH_DeInit(&hhash) != HAL_OK) {
        return -1;
    }
    hhash.Init.DataType = HASH_DATATYPE_8B;
    HAL_HASH_Init(&hhash);
}

int hash_update(uint8_t *buffer, uint32_t size) {
    if ((size % 4) != 0) {
        return -1;
    }

    if (HAL_HASH_MD5_Accumulate(&hhash, buffer, size) != HAL_OK) {
        return -1;
    }

    return 0;
}

int hash_digest(uint8_t *buffer, uint32_t size, uint8_t *digest) {
    if (HAL_HASH_MD5_Start(&hhash, buffer, size, digest, 0xFF) != HAL_OK) {
        return -1;
    }

    return 0;
}

int hash_from_file(const char *path, uint8_t *digest) {
    FIL fp;
    uint32_t offset = 0;
    UINT bytes = 0;
    UINT bytes_out = 0;

    int ret = -1;
    uint8_t *buf = fb_alloc(BLOCK_SIZE);

    if (file_ll_open(&fp, path, FA_READ | FA_OPEN_EXISTING) != FR_OK) {
        goto error;
    }

    // File size
    uint32_t size = f_size(&fp);

    while (size) {
        // Read a block.
        bytes = MIN(size, BLOCK_SIZE);
        if (file_ll_read(&fp, buf, bytes, &bytes_out) != FR_OK || bytes != bytes_out) {
            printf("hash_from_file: file read error!\n");
            goto error;
        }

        // Accumulate buffer.
        if ((size - bytes) > 0) {
            ret = hash_update(buf, bytes);
        } else {
            // last block, get digest.
            ret = hash_digest(buf, bytes, digest);
        }

        if (ret != 0) {
            printf("hash_from_file: hash_update/digest failed!\n");
            goto error;
        }

        size -= bytes;
        offset += bytes;
    }

    ret = 0;

error:
    fb_free();
    file_ll_close(&fp);
    return ret;
}
#endif //OMV_ENABLE_HASH == 1
