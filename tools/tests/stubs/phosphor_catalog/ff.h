/* SPDX-License-Identifier: MIT
 * FatFs directory stand-in for the real Phosphor app's catalog test. */
#ifndef TANG_TEST_CATALOG_FF_H
#define TANG_TEST_CATALOG_FF_H
#include <stdint.h>
typedef unsigned int UINT;
typedef uint64_t FSIZE_t;
typedef enum
{
    FR_OK,
    FR_DISK_ERR,
    FR_NO_PATH
} FRESULT;
typedef struct
{
    int pass;
    unsigned index;
} DIR;
typedef struct
{
    FSIZE_t size;
} FIL;
typedef struct
{
    uint8_t fattrib;
    char fname[256];
} FILINFO;
#define AM_DIR           0x10
#define AM_HID           0x02
#define AM_SYS           0x04
#define FA_READ          0x01
#define f_size(file)     ((file)->size)
#define f_rewinddir(dir) f_readdir((dir), 0)
FRESULT f_opendir(DIR *dir, const char *path);
FRESULT f_readdir(DIR *dir, FILINFO *info);
FRESULT f_closedir(DIR *dir);
FRESULT f_open(FIL *file, const char *path, uint8_t mode);
FRESULT f_read(FIL *file, void *data, UINT length, UINT *read);
FRESULT f_close(FIL *file);
#endif
