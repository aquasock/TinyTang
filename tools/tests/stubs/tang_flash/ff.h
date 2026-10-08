/* FatFs stand-in for tools/tests/tb_tang_flash.c: only what tdsh_tang_flash.c
 * and tang_fw_update.h use. */
#ifndef STUB_TANG_FLASH_FF_H
#define STUB_TANG_FLASH_FF_H
typedef struct { int open; } FIL;
typedef unsigned UINT;
typedef int FRESULT;
#define FR_OK 0
#define FA_READ 1
#define FA_WRITE 2
#define FA_CREATE_ALWAYS 8
FRESULT f_open(FIL *fp, const char *path, int mode);
FRESULT f_read(FIL *fp, void *buf, UINT n, UINT *got);
FRESULT f_write(FIL *fp, const void *buf, UINT n, UINT *wrote);
FRESULT f_close(FIL *fp);
#endif
