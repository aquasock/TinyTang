/* Host stub: the FatFS surface tdsh_fs_bl616.c uses, with FatFs's own
 * result values so the errno mapping is exercised against the real ones.
 * SPDX-License-Identifier: MIT */
#ifndef STUB_FS_FF_H
#define STUB_FS_FF_H
#include <stdint.h>

typedef unsigned int UINT;
typedef unsigned char BYTE;
typedef uint32_t FSIZE_t;
typedef uint8_t TCHAR;

typedef struct { int fd; } FIL;
typedef struct { int handle; } FF_DIR;
typedef struct { int mounted; } FATFS;

typedef struct {
    FSIZE_t fsize;
    uint32_t fattrib;
    char     fname[256];
} FILINFO;

typedef enum {
    FR_OK = 0, FR_DISK_ERR = 1, FR_INT_ERR = 2, FR_NOT_READY = 3,
    FR_NO_FILE = 4, FR_NO_PATH = 5, FR_INVALID_NAME = 6, FR_DENIED = 7,
    FR_EXIST = 8, FR_INVALID_OBJECT = 9, FR_WRITE_PROTECTED = 10,
    FR_INVALID_DRIVE = 11, FR_NOT_ENABLED = 12, FR_NO_FILESYSTEM = 13,
    FR_MKFS_ABORTED = 14, FR_TIMEOUT = 15, FR_LOCKED = 16,
    FR_NOT_ENOUGH_CORE = 17, FR_TOO_MANY_OPEN_FILES = 18,
    FR_INVALID_PARAMETER = 19
} FRESULT;

#define AM_DIR 0x10
#define AM_HID 0x02
#define AM_SYS 0x04

#define FA_READ          0x01
#define FA_WRITE         0x02
#define FA_OPEN_EXISTING 0x00
#define FA_CREATE_NEW    0x04
#define FA_CREATE_ALWAYS 0x08
#define FA_OPEN_ALWAYS   0x10
#define FA_OPEN_APPEND   0x30

FRESULT f_mount(FATFS *fs, const char *path, BYTE opt);
FRESULT f_open(FIL *fp, const char *path, BYTE mode);
FRESULT f_close(FIL *fp);
FRESULT f_read(FIL *fp, void *buf, UINT n, UINT *br);
FRESULT f_write(FIL *fp, const void *buf, UINT n, UINT *bw);
FRESULT f_lseek(FIL *fp, FSIZE_t ofs);
FSIZE_t f_tell(FIL *fp);
FSIZE_t f_size(FIL *fp);
FRESULT f_stat(const char *path, FILINFO *info);
FRESULT f_unlink(const char *path);
FRESULT f_mkdir(const char *path);
FRESULT f_rmdir(const char *path);
FRESULT f_rename(const char *from, const char *to);
FRESULT f_opendir(FF_DIR *dp, const char *path);
FRESULT f_readdir(FF_DIR *dp, FILINFO *info);
FRESULT f_closedir(FF_DIR *dp);
#endif
