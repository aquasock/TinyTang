/* SPDX-License-Identifier: MIT
 * The SDK's FatFs R0.15, built with this repository's fatfs_conf_user.h, on a
 * FAT32 image whose Hungarian file names are stored exactly as Linux wrote
 * them on the card (tools/tests/make_fat_names_image.py).  It does what the
 * Tang's shell and TinyDesk do with a name: list the folder, then look each
 * listed name up again (stat, open and read), then move it to another folder
 * and back, as a drag to the desktop does, and list once more.
 *
 *   tb_fat_names IMAGE
 *
 * One line per step on stdout, names in hex, for check_fat_names.py:
 *   LIST  <name> <stat result> <size> <open result> <hash>
 *   MOVE  <name> <result out> <result back>
 *   AFTER <name> <stat result> <size> <open result> <hash>
 */
#define _XOPEN_SOURCE 700
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "ff.h"
#include "diskio.h"

/* ------------------------------------------------- the disk: an image file */
static int g_fd = -1;

DSTATUS disk_initialize(BYTE pdrv) { return pdrv == 2 && g_fd >= 0 ? 0 : STA_NOINIT; }
DSTATUS disk_status(BYTE pdrv) { return pdrv == 2 && g_fd >= 0 ? 0 : STA_NOINIT; }
DRESULT disk_read(BYTE pdrv, BYTE *buff, LBA_t sector, UINT count)
{
    (void)pdrv;
    return pread(g_fd, buff, count * 512u, (off_t)sector * 512) == (ssize_t)(count * 512u) ? RES_OK : RES_ERROR;
}
DRESULT disk_write(BYTE pdrv, const BYTE *buff, LBA_t sector, UINT count)
{
    (void)pdrv;
    return pwrite(g_fd, buff, count * 512u, (off_t)sector * 512) == (ssize_t)(count * 512u) ? RES_OK : RES_ERROR;
}
DRESULT disk_ioctl(BYTE pdrv, BYTE cmd, void *buff)
{
    (void)pdrv;
    if (cmd == CTRL_SYNC) return RES_OK;
    if (cmd == GET_SECTOR_COUNT) { *(LBA_t *)buff = (LBA_t)(lseek(g_fd, 0, SEEK_END) / 512); return RES_OK; }
    if (cmd == GET_BLOCK_SIZE) { *(DWORD *)buff = 1; return RES_OK; }
    return RES_PARERR;
}
DWORD get_fattime(void) { return ((DWORD)(2026 - 1980) << 25) | (10u << 21) | (8u << 16); }
int ff_mutex_create(int vol) { (void)vol; return 1; }
void ff_mutex_delete(int vol) { (void)vol; }
int ff_mutex_take(int vol) { (void)vol; return 1; }
void ff_mutex_give(int vol) { (void)vol; }
void *ff_memalloc(UINT size) { return malloc(size); }
void ff_memfree(void *p) { free(p); }

/* ------------------------------------------------------------- the steps */
static void hex(const char *s)
{
    for (; *s; s++) printf("%02x", (unsigned char)*s);
}

/* Look a listed name up again, as `ls -l` and TinyDesk do: stat it, then open
 * and hash the contents. */
static void probe(const char *tag, const char *dir, const char *name)
{
    char path[700];
    snprintf(path, sizeof(path), "%s/%s", dir, name);
    FILINFO info;
    FRESULT st = f_stat(path, &info);
    FIL f;
    FRESULT op = f_open(&f, path, FA_READ);
    unsigned long hash = 0x811C9DC5ul;
    if (op == FR_OK) {
        BYTE buf[512];
        UINT got;
        while (f_read(&f, buf, sizeof(buf), &got) == FR_OK && got)
            for (UINT i = 0; i < got; i++) hash = ((hash ^ buf[i]) * 0x01000193ul) & 0xFFFFFFFFul;
        f_close(&f);
    }
    printf("%s\t", tag);
    hex(name);
    printf("\t%d\t%lu\t%d\t%08lx\n", (int)st, st == FR_OK ? (unsigned long)info.fsize : 0ul, (int)op, hash);
}

static int list(const char *dir, char names[][300], int max)
{
    DIR d;
    FILINFO info;
    int n = 0;
    if (f_opendir(&d, dir) != FR_OK) return -1;
    while (n < max && f_readdir(&d, &info) == FR_OK && info.fname[0]) {
        if (info.fattrib & AM_DIR) continue;
        snprintf(names[n++], 300, "%s", info.fname);
    }
    f_closedir(&d);
    return n;
}

int main(int argc, char **argv)
{
    if (argc != 2 || (g_fd = open(argv[1], O_RDWR)) < 0) return 2;
    static FATFS fs;
    if (f_mount(&fs, "/sd", 1) != FR_OK) return 3;
    if (f_mkdir("/sd/Desktop") != FR_OK) return 4;
    static char names[32][300];
    int n = list("/sd", names, 32);
    for (int i = 0; i < n; i++) probe("LIST", "/sd", names[i]);
    for (int i = 0; i < n; i++) {
        char from[700], to[700];
        snprintf(from, sizeof(from), "/sd/%.299s", names[i]);
        snprintf(to, sizeof(to), "/sd/Desktop/%.299s", names[i]);
        FRESULT out = f_rename(from, to);
        FRESULT back = out == FR_OK ? f_rename(to, from) : FR_INVALID_PARAMETER;
        printf("MOVE\t");
        hex(names[i]);
        printf("\t%d\t%d\n", (int)out, (int)back);
    }
    n = list("/sd", names, 32);
    for (int i = 0; i < n; i++) probe("AFTER", "/sd", names[i]);
    f_unmount("/sd");
    close(g_fd);
    return 0;
}
