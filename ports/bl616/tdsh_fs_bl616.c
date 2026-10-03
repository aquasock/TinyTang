// TinyTang — the filesystem side of the BL616 port.
//
// The SDK's newlib+FatFS port (components/libc/newlib/port_file_fatfs.c,
// enabled by CONFIG_NEWLIB_FATFS) already routes fopen/fread/fwrite/fgets/
// fputs/stat/mkdir/remove/rename/unlink/rmdir to FatFS for "/sd/..." paths.
// It does not implement directory listing, because newlib's opendir/readdir
// need a _getdents syscall the SDK does not provide, so this file supplies
// opendir/readdir/closedir over FatFS directly and mounts the card.

#include "tdsh_bl616.h"

#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#include "board.h"

/* FatFS and POSIX both call their directory object DIR.  Rename FatFS's for
 * this translation unit so the two can coexist; FF_DIR is the same struct
 * ff.c uses, so the API stays compatible. */
#define DIR FF_DIR
#include "ff.h"
#undef DIR
#include "fatfs_diskio_register.h"

#include <dirent.h>   /* ours, from ports/bl616 */

/* Opaque to the shell; reached only through opendir/readdir/closedir. */
struct __tdsh_dir {
    FF_DIR  ffs_dir;
    struct dirent entry;
    FILINFO info;
    bool    open;
};

static FATFS s_fs;
static bool  s_mounted = false;

int tdsh_bl616_fs_mount(void)
{
    board_sdh_gpio_init();
    fatfs_sdh_driver_register();

    FRESULT ret = f_mount(&s_fs, "/sd", 1);
    if (ret != FR_OK) {
        return -(int)ret;
    }
    s_mounted = true;
    return 0;
}

bool tdsh_bl616_fs_ready(void)
{
    return s_mounted;
}

DIR *opendir(const char *path)
{
    if (!path) {
        errno = EINVAL;
        return NULL;
    }
    struct __tdsh_dir *d = (struct __tdsh_dir *)calloc(1, sizeof(*d));
    if (!d) {
        errno = ENOMEM;
        return NULL;
    }
    if (f_opendir(&d->ffs_dir, path) != FR_OK) {
        free(d);
        errno = ENOENT;
        return NULL;
    }
    d->open = true;
    return (DIR *)d;
}

struct dirent *readdir(DIR *dirp)
{
    struct __tdsh_dir *d = (struct __tdsh_dir *)dirp;
    if (!d || !d->open) {
        errno = EBADF;
        return NULL;
    }
    if (f_readdir(&d->ffs_dir, &d->info) != FR_OK || d->info.fname[0] == '\0') {
        return NULL;   /* end of directory */
    }
    memset(&d->entry, 0, sizeof(d->entry));
    d->entry.d_type = (d->info.fattrib & AM_DIR) ? DT_DIR : DT_REG;
    strncpy(d->entry.d_name, d->info.fname, sizeof(d->entry.d_name) - 1);
    d->entry.d_name[sizeof(d->entry.d_name) - 1] = '\0';
    return &d->entry;
}

int closedir(DIR *dirp)
{
    struct __tdsh_dir *d = (struct __tdsh_dir *)dirp;
    if (!d) {
        errno = EBADF;
        return -1;
    }
    if (d->open) {
        f_closedir(&d->ffs_dir);
        d->open = false;
    }
    free(d);
    return 0;
}
