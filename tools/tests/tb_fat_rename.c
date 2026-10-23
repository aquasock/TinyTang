/* SPDX-License-Identifier: MIT
 *
 * Renaming over a name that is already in use, through the SDK's real FatFs
 * R0.15 and this repository's fatfs_conf_user.h, on a FAT32 image that
 * tools/tests/test_fat_rename.sh makes with mkfs.fat.  The names are the ones
 * the app uses, two dots and all, so it is the long-name path that is
 * exercised.
 *
 * The TinyNES Control Deck app saved its settings by building /tinynes.ini.tmp and
 * renaming it over /tinynes.ini.  That cannot work: f_rename() checks the new name
 * and returns FR_EXIST when an object holds it, because FAT has no atomic
 * replace, so the save failed with "cannot rename over the settings file"
 * while /tinynes.ini.tmp vanished and /tinynes.ini never changed.
 *
 * The first case below is therefore a negative one, kept as a guard: if the
 * save is ever simplified back to a single rename, this says why it cannot be.
 * The second is the sequence the app uses now -- real to a backup name, the
 * temporary into place, the backup dropped -- which never asks FatFs to
 * overwrite, and leaves the old file whole under a name until the new one has
 * landed.
 *
 *   tb_fat_rename IMAGE
 */
#define _XOPEN_SOURCE 700
#include <fcntl.h>
#include <stdbool.h>
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

/* --------------------------------------------------------------- reporting */
static int s_checks;
static int s_failed;

static void check(bool held, const char *what)
{
    printf("%s  %s\n", held ? "ok  " : "FAIL", what);
    s_checks++;
    if (!held) s_failed++;
}

/* Short names for what the app calls them. */
static const char REAL[] = "/sd/tinynes.ini";
static const char TMP[] = "/sd/tinynes.ini.tmp";
static const char BAK[] = "/sd/tinynes.ini.bak";

static FRESULT write_file(const char *path, const char *text)
{
    FIL f;
    UINT wrote = 0;
    FRESULT r = f_open(&f, path, FA_WRITE | FA_CREATE_ALWAYS);
    if (r != FR_OK) return r;
    r = f_write(&f, text, (UINT)strlen(text), &wrote);
    if (r == FR_OK && wrote != (UINT)strlen(text)) r = FR_INT_ERR;
    FRESULT c = f_close(&f);
    return r != FR_OK ? r : c;
}

/* The contents, or NULL when it could not be read. */
static const char *read_file(const char *path, char *out, size_t cap)
{
    FIL f;
    UINT got = 0;
    if (f_open(&f, path, FA_READ) != FR_OK) return NULL;
    FRESULT r = f_read(&f, out, (UINT)(cap - 1), &got);
    (void)f_close(&f);
    if (r != FR_OK) return NULL;
    out[got] = '\0';
    return out;
}

static bool is_file(const char *path)
{
    FILINFO info;
    return f_stat(path, &info) == FR_OK;
}

/* A card with the settings file on it and an edited copy beside it. */
static void stage(void)
{
    (void)f_unlink(BAK);
    (void)f_unlink(TMP);
    (void)f_unlink(REAL);
    check(write_file(REAL, "p1_a = b\nold\n") == FR_OK, "a settings file with the old mapping");
    check(write_file(TMP, "p1_a = y\nnew\n") == FR_OK, "the edited copy beside it");
}

int main(int argc, char **argv)
{
    if (argc != 2 || (g_fd = open(argv[1], O_RDWR)) < 0) {
        fprintf(stderr, "tb_fat_rename: cannot open %s\n", argc > 1 ? argv[1] : "(no image given)");
        return 2;
    }
    static FATFS fs;
    FRESULT mounted = f_mount(&fs, "/sd", 1);
    if (mounted != FR_OK) {
        fprintf(stderr, "tb_fat_rename: f_mount returned %d\n", (int)mounted);
        return 3;
    }

    char text[64];

    /* 1. The single rename the app first used, onto the name in use. */
    stage();
    {
        FRESULT r = f_rename(TMP, REAL);
        char what[160];
        snprintf(what, sizeof what, "renaming the temporary over the settings file is refused (FR_EXIST=%d, got %d)",
                 (int)FR_EXIST, (int)r);
        check(r == FR_EXIST, what);
    }
    check(read_file(REAL, text, sizeof text) != NULL && strstr(text, "old") != NULL,
          "and the settings file still holds the old mapping");
    check(is_file(TMP), "and the edited copy is still there, nothing half-done");

    /* 2. The sequence the app uses now: through a backup name, never asking
     *    FatFs to overwrite. */
    stage();
    {
        FRESULT aside = f_rename(REAL, BAK);
        check(aside == FR_OK, "the settings file is set aside under the backup name");
        check(read_file(BAK, text, sizeof text) != NULL && strstr(text, "old") != NULL,
              "and it is whole under that name while the new one lands");
        check(f_rename(TMP, REAL) == FR_OK, "the edited copy takes the settings file's name");
        check(read_file(REAL, text, sizeof text) != NULL && strstr(text, "new") != NULL,
              "and the settings file holds the new mapping");
        check(f_unlink(BAK) == FR_OK, "the backup is dropped");
        check(!is_file(BAK) && !is_file(TMP), "leaving neither the backup nor the temporary behind");
    }

    /* 3. What the next save finds, so a second save behaves like the first. */
    stage();
    (void)f_rename(REAL, BAK);
    (void)f_rename(TMP, REAL);
    (void)f_unlink(BAK);
    check(f_unlink(BAK) == FR_NO_FILE,
          "clearing a backup that is not there reports FR_NO_FILE, which the app ignores");

    f_unmount("/sd");
    close(g_fd);

    printf("%d checks, %d failed\n", s_checks, s_failed);
    return s_failed != 0;
}
