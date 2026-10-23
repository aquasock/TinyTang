/* SPDX-License-Identifier: MIT
 *
 * Enough of FatFS for tang_settings_ini.c to compile on the host.  The calls are
 * defined in tools/tests/tb_nes_ini.c, over a string rather than a card, so the
 * parser's real load path is what runs.  Only what that file uses is here.
 */
#ifndef TANG_TESTS_FF_H
#define TANG_TESTS_FF_H

typedef unsigned char BYTE;
typedef unsigned int  UINT;
typedef char          TCHAR;
typedef int           FRESULT;

typedef struct { const char *text; unsigned long at; } FIL;

#define FR_OK   0
#define FA_READ 1

FRESULT f_open(FIL *fp, const TCHAR *path, BYTE mode);
FRESULT f_read(FIL *fp, void *buff, UINT btr, UINT *br);
FRESULT f_close(FIL *fp);

#endif /* TANG_TESTS_FF_H */
