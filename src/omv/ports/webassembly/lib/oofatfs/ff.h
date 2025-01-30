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
#ifndef FF_DEFINED
#define FF_DEFINED

#include <stdio.h>
#include <stddef.h>
#include <stdbool.h>

typedef char FF_DIR; // hack
typedef char FILINFO; // hack
typedef char FATFS; // hack
typedef char DIR; // hack

typedef char          TCHAR;
typedef unsigned int  UINT;     /* int must be 16-bit or 32-bit */
typedef unsigned char BYTE;     /* char must be 8-bit */
typedef size_t        FSIZE_t;  /* char must be 8-bit */

typedef enum {
    FR_OK = 0,              /* (0) Succeeded */
    FR_DISK_ERR,            /* (1) A hard error occurred in the low level disk I/O layer */
    FR_INT_ERR,             /* (2) Assertion failed */
    FR_NOT_READY,           /* (3) The physical drive cannot work */
    FR_NO_FILE,             /* (4) Could not find the file */
    FR_NO_PATH,             /* (5) Could not find the path */
    FR_INVALID_NAME,        /* (6) The path name format is invalid */
    FR_DENIED,              /* (7) Access denied due to prohibited access or directory full */
    FR_EXIST,               /* (8) Access denied due to prohibited access */
    FR_INVALID_OBJECT,      /* (9) The file/directory object is invalid */
    FR_WRITE_PROTECTED,     /* (10) The physical drive is write protected */
    FR_INVALID_DRIVE,       /* (11) The logical drive number is invalid */
    FR_NOT_ENABLED,         /* (12) The volume has no work area */
    FR_NO_FILESYSTEM,       /* (13) There is no valid FAT volume */
    FR_MKFS_ABORTED,        /* (14) The f_mkfs() aborted due to any problem */
    FR_TIMEOUT,             /* (15) Could not get a grant to access the volume within defined period */
    FR_LOCKED,              /* (16) The operation is rejected according to the file sharing policy */
    FR_NOT_ENOUGH_CORE,     /* (17) LFN working buffer could not be allocated */
    FR_TOO_MANY_OPEN_FILES, /* (18) Number of open files > FF_FS_LOCK */
    FR_INVALID_PARAMETER    /* (19) Given parameter is invalid */
} FRESULT;

// /* File access mode and open method flags (3rd argument of f_open) */
#define FA_READ             0x01
#define FA_WRITE            0x02
#define FA_OPEN_EXISTING    0x00
#define FA_CREATE_NEW       0x04
#define FA_CREATE_ALWAYS    0x08
#define FA_OPEN_ALWAYS      0x10
#define FA_OPEN_APPEND      0x30

typedef struct {
    FILE *file;
    BYTE flag;
} FIL;

size_t f_size(FIL *fp);
size_t f_tell(FIL *fp);
bool f_eof(FIL *fp);
FRESULT f_open(FATFS *fs, FIL *fp, const TCHAR *path, BYTE mode);   /* Open or create a file */
FRESULT f_close(FIL *fp);                                           /* Close an open file object */
FRESULT f_read(FIL *fp, void *buff, UINT btr, UINT *br);            /* Read data from the file */
FRESULT f_write(FIL *fp, const void *buff, UINT btw, UINT *bw);     /* Write data to the file */
FRESULT f_lseek(FIL *fp, FSIZE_t ofs);                              /* Move file pointer of the file object */

FRESULT f_opendir(FATFS *fs, FF_DIR *dp, const TCHAR *path);        /* Open a directory */
FRESULT f_stat(FATFS *fs, const TCHAR *path, FILINFO *fno);         /* Get file status */
FRESULT f_mkdir(FATFS *fs, const TCHAR *path);                      /* Create a sub directory */
FRESULT f_unlink(FATFS *fs, const TCHAR *path);                     /* Delete an existing file or directory */
FRESULT f_rename(FATFS *fs, const TCHAR *path_old, const TCHAR *path_new);  /* Rename/Move a file or directory */
FRESULT f_truncate(FIL *fp);                                        /* Truncate the file */
FRESULT f_sync(FIL *fp);                                            /* Flush cached data of the writing file */
#endif