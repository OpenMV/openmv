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
 * FatFs Posix Wraper.
 */
#include "ff.h"
#include <unistd.h>
#include <fcntl.h>
#include <stdio.h>

size_t f_size(FIL *fp) {
    long current_pos = ftell(fp->file);
    fseek(fp->file, 0, SEEK_END);
    long size = ftell(fp->file);
    fseek(fp->file, current_pos, SEEK_SET);
    return size;
}

size_t f_tell(FIL *fp) {
    return ftell(fp->file);
}

bool f_eof(FIL *fp) {
    return feof(fp->file) != 0;
}

FRESULT f_lseek(FIL *fp, FSIZE_t ofs) {
    fseek(fp->file, ofs, SEEK_SET);
    return FR_OK;
}

FRESULT f_opendir(FATFS *fs, FF_DIR *dp, const TCHAR *path) {
    return FR_DISK_ERR;
}

FRESULT f_truncate(FIL *fp) {
    return FR_DISK_ERR;
}

FRESULT f_sync(FIL *fp) {
    fflush(fp->file);
    return FR_OK;
}

FRESULT f_open(FATFS *fs, FIL *fp, const TCHAR *path, BYTE mode) {
    fp->file = fopen(path, mode == FA_READ ? "rb" : "wb");
    if (fp->file == NULL) {
        return FR_NO_FILE;
    }
    fp->flag = mode;
    fseek(fp->file, 0, SEEK_SET);
    return FR_OK;
}

FRESULT f_close(FIL *fp) {
    if (fp->file) {
        fclose(fp->file);
    }
    return FR_OK;
}

FRESULT f_read(FIL *fp, void *buff, UINT btr, UINT *br) {
    *br = fread(buff, 1, btr, fp->file);
    return FR_OK;
}

FRESULT f_write(FIL *fp, const void *buff, UINT btw, UINT *bw) {
    *bw = fwrite(buff, 1, btw, fp->file);
    return FR_OK;
}
