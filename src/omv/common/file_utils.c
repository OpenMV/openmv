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
 * Filesystem helper functions.
 */
#include "imlib_config.h"
#if defined(IMLIB_ENABLE_IMAGE_FILE_IO)

#include <string.h>
#include "py/runtime.h"
#include "extmod/vfs.h"
#include "extmod/vfs_fat.h"

#include "omv_common.h"
#include "fb_alloc.h"
#include "file_utils.h"
#define FF_MIN(x, y)    (((x) < (y))?(x):(y))

NORETURN static void ff_read_fail(FIL *fp) {
    if (fp) {
        f_close(fp);
    }
    mp_raise_msg(&mp_type_OSError, MP_ERROR_TEXT("Failed to read requested bytes!"));
}

NORETURN static void ff_write_fail(FIL *fp) {
    if (fp) {
        f_close(fp);
    }
    mp_raise_msg(&mp_type_OSError, MP_ERROR_TEXT("Failed to write requested bytes!"));
}

NORETURN static void ff_expect_fail(FIL *fp) {
    if (fp) {
        f_close(fp);
    }
    mp_raise_msg(&mp_type_OSError, MP_ERROR_TEXT("Unexpected value read!"));
}

NORETURN void file_raise_format(FIL *fp) {
    if (fp) {
        f_close(fp);
    }
    mp_raise_msg(&mp_type_OSError, MP_ERROR_TEXT("Unsupported format!"));
}

NORETURN void file_raise_corrupted(FIL *fp) {
    if (fp) {
        f_close(fp);
    }
    mp_raise_msg(&mp_type_OSError, MP_ERROR_TEXT("File corrupted!"));
}

NORETURN void file_raise_error(FIL *fp, FRESULT res) {
    if (fp) {
        f_close(fp);
    }
    mp_raise_msg(&mp_type_OSError, (mp_rom_error_text_t) file_strerror(res));
}

static FATFS *lookup_path(const TCHAR **path) {
    mp_vfs_mount_t *fs = mp_vfs_lookup_path(*path, path);
    if (fs == MP_VFS_NONE || fs == MP_VFS_ROOT) {
        return NULL;
    }
    // here we assume that the mounted device is FATFS
    return &((fs_user_mount_t *) MP_OBJ_TO_PTR(fs->obj))->fatfs;
}

FRESULT file_ll_open(FIL *fp, const TCHAR *path, BYTE mode) {
    FATFS *fs = lookup_path(&path);
    if (fs == NULL) {
        return FR_NO_PATH;
    }
    return f_open(fs, fp, path, mode);
}

FRESULT file_ll_close(FIL *fp) {
    return f_close(fp);
}

FRESULT file_ll_read(FIL *fp, void *buff, UINT btr, UINT *br) {
    return f_read(fp, buff, btr, br);
}

FRESULT file_ll_write(FIL *fp, const void *buff, UINT btw, UINT *bw) {
    return f_write(fp, buff, btw, bw);
}

FRESULT file_ll_opendir(FF_DIR *dp, const TCHAR *path) {
    FATFS *fs = lookup_path(&path);
    if (fs == NULL) {
        return FR_NO_PATH;
    }
    return f_opendir(fs, dp, path);
}

FRESULT file_ll_stat(const TCHAR *path, FILINFO *fno) {
    FATFS *fs = lookup_path(&path);
    if (fs == NULL) {
        return FR_NO_PATH;
    }
    return f_stat(fs, path, fno);
}

FRESULT file_ll_mkdir(const TCHAR *path) {
    FATFS *fs = lookup_path(&path);
    if (fs == NULL) {
        return FR_NO_PATH;
    }
    return f_mkdir(fs, path);
}

FRESULT file_ll_unlink(const TCHAR *path) {
    FATFS *fs = lookup_path(&path);
    if (fs == NULL) {
        return FR_NO_PATH;
    }
    return f_unlink(fs, path);
}

FRESULT file_ll_rename(const TCHAR *path_old, const TCHAR *path_new) {
    FATFS *fs_old = lookup_path(&path_old);
    if (fs_old == NULL) {
        return FR_NO_PATH;
    }
    FATFS *fs_new = lookup_path(&path_new);
    if (fs_new == NULL) {
        return FR_NO_PATH;
    }
    if (fs_old != fs_new) {
        return FR_NO_PATH;
    }
    return f_rename(fs_new, path_old, path_new);
}

FRESULT file_ll_touch(const TCHAR *path) {
    FIL fp;
    FATFS *fs = lookup_path(&path);
    if (fs == NULL) {
        return FR_NO_PATH;
    }

    if (f_stat(fs, path, NULL) != FR_OK) {
        f_open(fs, &fp, path, FA_WRITE | FA_CREATE_ALWAYS);
        f_close(&fp);
    }

    return FR_OK;
}

// When a sector boundary is encountered while writing a file and there are
// more than 512 bytes left to write FatFs will detect that it can bypass
// its internal write buffer and pass the data buffer passed to it directly
// to the disk write function. However, the disk write function needs the
// buffer to be aligned to a 4-byte boundary. FatFs doesn't know this and
// will pass an unaligned buffer if we don't fix the issue. To fix this problem
// we use a temporary buffer to fix the alignment and to speed everything up.
// We use this temporary buffer for both reads and writes. The buffer allows us
// to do multi-block reads and writes which significantly speed things up.

static uint32_t file_buffer_offset = 0;
static uint8_t *file_buffer_pointer = 0;
static uint32_t file_buffer_size = 0;
static uint32_t file_buffer_index = 0;

void file_buffer_init0() {
    file_buffer_offset = 0;
    file_buffer_pointer = 0;
    file_buffer_size = 0;
    file_buffer_index = 0;
}

OMV_ATTR_ALWAYS_INLINE static void file_fill(FIL *fp) {
    if (file_buffer_index == file_buffer_size) {
        file_buffer_pointer -= file_buffer_offset;
        file_buffer_size += file_buffer_offset;
        file_buffer_offset = 0;
        file_buffer_index = 0;
        uint32_t file_remaining = f_size(fp) - f_tell(fp);
        uint32_t can_do = FF_MIN(file_buffer_size, file_remaining);
        UINT bytes;
        FRESULT res = f_read(fp, file_buffer_pointer, can_do, &bytes);
        if (res != FR_OK) {
            file_raise_error(fp, res);
        }
        if (bytes != can_do) {
            ff_read_fail(fp);
        }
    }
}

OMV_ATTR_ALWAYS_INLINE static void file_flush(FIL *fp) {
    if (file_buffer_index == file_buffer_size) {
        UINT bytes;
        FRESULT res = f_write(fp, file_buffer_pointer, file_buffer_index, &bytes);
        if (res != FR_OK) {
            file_raise_error(fp, res);
        }
        if (bytes != file_buffer_index) {
            ff_write_fail(fp);
        }
        file_buffer_pointer -= file_buffer_offset;
        file_buffer_size += file_buffer_offset;
        file_buffer_offset = 0;
        file_buffer_index = 0;
    }
}

void file_buffer_on(FIL *fp) {
    file_buffer_offset = f_tell(fp) % 4;
    file_buffer_pointer = fb_alloc_all(&file_buffer_size, FB_ALLOC_NO_HINT) + file_buffer_offset;
    if (!file_buffer_size) {
        mp_raise_msg(&mp_type_MemoryError, MP_ERROR_TEXT("No memory!"));
    }
    file_buffer_size -= file_buffer_offset;
    file_buffer_index = 0;
    if (fp->flag & FA_READ) {
        uint32_t file_remaining = f_size(fp) - f_tell(fp);
        uint32_t can_do = FF_MIN(file_buffer_size, file_remaining);
        UINT bytes;
        FRESULT res = f_read(fp, file_buffer_pointer, can_do, &bytes);
        if (res != FR_OK) {
            file_raise_error(fp, res);
        }
        if (bytes != can_do) {
            ff_read_fail(fp);
        }
    }
}

void file_buffer_off(FIL *fp) {
    if ((fp->flag & FA_WRITE) && file_buffer_index) {
        UINT bytes;
        FRESULT res = f_write(fp, file_buffer_pointer, file_buffer_index, &bytes);
        if (res != FR_OK) {
            file_raise_error(fp, res);
        }
        if (bytes != file_buffer_index) {
            ff_write_fail(fp);
        }
    }
    file_buffer_pointer = 0;
    fb_free();
}

void file_open(FIL *fp, const char *path, bool buffered, uint32_t flags) {
    FRESULT res = file_ll_open(fp, path, flags);
    if (res != FR_OK) {
        file_raise_error(fp, res);
    }
    if (buffered) {
        file_buffer_on(fp);
    }
}

void file_close(FIL *fp) {
    if (file_buffer_pointer) {
        file_buffer_off(fp);
    }

    FRESULT res = f_close(fp);
    if (res != FR_OK) {
        file_raise_error(fp, res);
    }
}

void file_seek(FIL *fp, UINT offset) {
    FRESULT res = f_lseek(fp, offset);
    if (res != FR_OK) {
        file_raise_error(fp, res);
    }
}

void file_truncate(FIL *fp) {
    FRESULT res = f_truncate(fp);
    if (res != FR_OK) {
        file_raise_error(fp, res);
    }
}

void file_sync(FIL *fp) {
    FRESULT res = f_sync(fp);
    if (res != FR_OK) {
        file_raise_error(fp, res);
    }
}

uint32_t file_tell(FIL *fp) {
    if (file_buffer_pointer) {
        if (fp->flag & FA_READ) {
            return f_tell(fp) - file_buffer_size + file_buffer_index;
        } else {
            return f_tell(fp) + file_buffer_index;
        }
    }
    return f_tell(fp);
}

uint32_t file_size(FIL *fp) {
    if (file_buffer_pointer) {
        if (fp->flag & FA_READ) {
            return f_size(fp);
        } else {
            return f_size(fp) + file_buffer_index;
        }
    }
    return f_size(fp);
}

void file_read(FIL *fp, void *data, size_t size) {
    if (data == NULL) {
        uint8_t byte;
        if (file_buffer_pointer) {
            for (size_t i = 0; i < size; i++) {
                file_fill(fp);
                byte = file_buffer_pointer[file_buffer_index++];
            }
        } else {
            for (size_t i = 0; i < size; i++) {
                UINT bytes;
                FRESULT res = f_read(fp, &byte, 1, &bytes);
                if (res != FR_OK) {
                    file_raise_error(fp, res);
                }
                if (bytes != 1) {
                    ff_read_fail(fp);
                }
            }
        }
        return;
    }

    if (file_buffer_pointer) {
        if (size <= 4) {
            for (size_t i = 0; i < size; i++) {
                file_fill(fp);
                ((uint8_t *) data)[i] = file_buffer_pointer[file_buffer_index++];
            }
        } else {
            while (size) {
                file_fill(fp);
                uint32_t file_buffer_space_left = file_buffer_size - file_buffer_index;
                uint32_t can_do = FF_MIN(size, file_buffer_space_left);
                memcpy(data, file_buffer_pointer + file_buffer_index, can_do);
                file_buffer_index += can_do;
                data += can_do;
                size -= can_do;
            }
        }
    } else {
        UINT bytes;
        FRESULT res = f_read(fp, data, size, &bytes);
        if (res != FR_OK) {
            file_raise_error(fp, res);
        }
        if (bytes != size) {
            ff_read_fail(fp);
        }
    }
}

void file_write(FIL *fp, const void *data, size_t size) {
    if (file_buffer_pointer) {
        // We get a massive speed boost by buffering up as much data as possible
        // before a write to the SD card. So much so that the time wasted by
        // all these operations does not cost us.
        while (size) {
            uint32_t file_buffer_space_left = file_buffer_size - file_buffer_index;
            uint32_t can_do = FF_MIN(size, file_buffer_space_left);
            memcpy(file_buffer_pointer + file_buffer_index, data, can_do);
            file_buffer_index += can_do;
            data += can_do;
            size -= can_do;
            file_flush(fp);
        }
    } else {
        UINT bytes;
        FRESULT res = f_write(fp, data, size, &bytes);
        if (res != FR_OK) {
            file_raise_error(fp, res);
        }
        if (bytes != size) {
            ff_write_fail(fp);
        }
    }
}

void file_write_byte(FIL *fp, uint8_t value) {
    file_write(fp, &value, 1);
}

void file_write_short(FIL *fp, uint16_t value) {
    file_write(fp, &value, 2);
}

void file_write_long(FIL *fp, uint32_t value) {
    file_write(fp, &value, 4);
}

void file_read_check(FIL *fp, const void *data, size_t size) {
    uint8_t buf[16];
    while (size) {
        size_t len = OMV_MIN(sizeof(buf), size);
        file_read(fp, buf, len);
        if (memcmp(data, buf, len)) {
            ff_expect_fail(fp);
        }
        size -= len;
        data = ((uint8_t *) data) + len;
    }
}

const char *file_strerror(FRESULT res) {
    static const char *ffs_errors[] = {
        "Succeeded",
        "A hard error occurred in the low level disk I/O layer",
        "Assertion failed",
        "The physical drive cannot work",
        "Could not find the file",
        "Could not find the path",
        "The path name format is invalid",
        "Access denied due to prohibited access or directory full",
        "Access denied due to prohibited access",
        "The file/directory object is invalid",
        "The physical drive is write protected",
        "The logical drive number is invalid",
        "The volume has no work area",
        "There is no valid FAT volume",
        "The f_mkfs() aborted due to any parameter error",
        "Could not get a grant to access the volume within defined period",
        "The operation is rejected according to the file sharing policy",
        "LFN working buffer could not be allocated",
        "Number of open files > _FS_SHARE",
        "Given parameter is invalid",
    };

    if (res > (sizeof(ffs_errors) / sizeof(ffs_errors[0]))) {
        return "unknown error";
    } else {
        return ffs_errors[res];
    }
}
#endif //IMLIB_ENABLE_IMAGE_FILE_IO
