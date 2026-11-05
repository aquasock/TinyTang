/* Host stub: SPDX-License-Identifier: MIT */
#ifndef STUB_CR_FF_H
#define STUB_CR_FF_H
#include <stdint.h>
typedef unsigned int UINT;
typedef uint32_t FSIZE_t;
typedef struct { int fd; } FIL;
typedef enum { FR_OK = 0, FR_NO_FILE = 4, FR_DENIED = 7, FR_DISK_ERR = 1 } FRESULT;
#define FA_WRITE 0x02
#define FA_OPEN_APPEND 0x30
FRESULT f_open(FIL *f, const char *path, uint8_t mode);
FRESULT f_write(FIL *f, const void *buf, unsigned n, unsigned *put);
FRESULT f_close(FIL *f);
#endif
