/* Host stub: the FatFS surface tang_fpga_uart.c uses. SPDX-License-Identifier: MIT
 *
 * Only the calls it makes, with FatFs's own result values, so a refused open
 * and a short read reach the module as the kinds of failure they really are. */
#ifndef STUB_FU_FF_H
#define STUB_FU_FF_H
#include <stdint.h>
typedef unsigned int UINT;
typedef uint32_t FSIZE_t;
typedef struct { int fd; } FIL;
typedef enum {
    FR_OK = 0, FR_DISK_ERR = 1, FR_INT_ERR = 2, FR_NOT_READY = 3,
    FR_NO_FILE = 4, FR_NO_PATH = 5, FR_INVALID_NAME = 6, FR_DENIED = 7,
    FR_EXIST = 8, FR_INVALID_OBJECT = 9, FR_WRITE_PROTECTED = 10,
    FR_INVALID_DRIVE = 11, FR_NOT_ENABLED = 12, FR_NO_FILESYSTEM = 13,
    FR_MKFS_ABORTED = 14, FR_TIMEOUT = 15, FR_LOCKED = 16,
    FR_NOT_ENOUGH_CORE = 17, FR_TOO_MANY_OPEN_FILES = 18,
    FR_INVALID_PARAMETER = 19
} FRESULT;

#define FA_READ          0x01
#define FA_WRITE         0x02
#define FA_OPEN_EXISTING 0x00
#define FA_CREATE_NEW    0x04
#define FA_CREATE_ALWAYS 0x08
#define FA_OPEN_ALWAYS   0x10
#define FA_OPEN_APPEND   0x30

FRESULT f_open(FIL *fp, const char *path, uint8_t mode);
FRESULT f_read(FIL *fp, void *buf, UINT n, UINT *got);
FRESULT f_write(FIL *fp, const void *buf, UINT n, UINT *wrote);
FRESULT f_close(FIL *fp);
FRESULT f_unlink(const char *path);
FSIZE_t f_size(FIL *fp);
FRESULT f_lseek(FIL *fp, FSIZE_t ofs);
FRESULT f_mkdir(const char *path);
FRESULT f_sync(FIL *fp);
FRESULT f_rename(const char *from, const char *to);
#endif
