/* Host stub: the FatFs calls the pad pairings file uses, on one in-memory
 * file.  SPDX-License-Identifier: MIT */
#ifndef STUB_BTPAD_FF_H
#define STUB_BTPAD_FF_H
typedef unsigned char BYTE;
typedef unsigned int  UINT;
typedef char          TCHAR;
typedef int           FRESULT;
typedef struct { int mode; UINT at; } FIL;
#define FR_OK 0
#define FR_NO_FILE 4
#define FA_READ 0x01
#define FA_WRITE 0x02
#define FA_CREATE_ALWAYS 0x08
FRESULT f_open(FIL *fp, const TCHAR *path, BYTE mode);
FRESULT f_read(FIL *fp, void *buff, UINT btr, UINT *br);
FRESULT f_write(FIL *fp, const void *buff, UINT btw, UINT *bw);
FRESULT f_close(FIL *fp);
FRESULT f_mkdir(const TCHAR *path);
#endif
