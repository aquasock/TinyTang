// TinyTang — the filesystem side of the BL616 port.
//
// The SDK's own newlib port cannot be used: enabling CONFIG_NEWLIB stops the
// USB device from enumerating on this board (measured, see proj.conf).  So
// this file provides what TinyDesk Shell's porting document asks a target to
// provide -- the standard C file calls the core uses -- implemented directly
// over FatFS, with the console descriptors served by the USB CDC.
//
// Structure follows newlib: the engine's fopen/fread/fwrite/fgets/fputs/stat
// and the stdin/stdout descriptors all end up in the reentrant syscalls
// (_open_r/_read_r/_write_r/...), so implementing those gives the shell a
// working stdio without newlib's port.  File descriptors are tagged so the
// console (0/1/2) and FatFS files can share one numbering.

#include "tdsh_bl616.h"

#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <sys/fcntl.h>
#include <sys/stat.h>
#include <sys/reent.h>

#include "board.h"
#include "FreeRTOS.h"
#include "task.h"
#include "bflb_gpio.h"
#include "bflb_mtimer.h"

/* FatFS and POSIX both call their directory object DIR.  Rename FatFS's for
 * this translation unit so the two can coexist; FF_DIR is the same struct
 * ff.c uses, so the API stays compatible. */
#define DIR FF_DIR
#include "ff.h"
#undef DIR
#include "fatfs_diskio_register.h"

#include <dirent.h>   /* ours, from ports/bl616 */

/* ------------------------------------------------------------------ mount */

static FATFS s_fs;
static bool  s_mounted = false;
static int   s_last_result = -999;

/* The card is gated behind a GPIO on this board.
 *
 * The SDK's board_sdh_gpio_init() only routes the six SDH data/clock pins, and
 * on this board that is not enough: the card stays unpowered and f_mount
 * returns FR_NOT_READY (3).  The working firmware on this board drives GPIO 16
 * high to enable SDMMC before initialising the controller, and that is what
 * this does. */
static void sdh_power_enable(void)
{
    struct bflb_device_s *gpio = bflb_device_get_by_name("gpio");
    bflb_gpio_init(gpio, GPIO_PIN_16, GPIO_OUTPUT | GPIO_FLOAT | GPIO_SMT_EN | GPIO_DRV_3);
    bflb_gpio_set(gpio, GPIO_PIN_16);
    bflb_mtimer_delay_ms(30);   /* let the card come up before probing it */
}

int tdsh_bl616_fs_mount(void)
{
    sdh_power_enable();
    board_sdh_gpio_init();
    fatfs_sdh_driver_register();

    FRESULT ret = f_mount(&s_fs, "/sd", 1);
    s_last_result = (int)ret;
    if (ret != FR_OK) {
        return -(int)ret;
    }
    s_mounted = true;
    return 0;
}

int tdsh_bl616_fs_last_result(void)
{
    return s_last_result;
}

bool tdsh_bl616_fs_ready(void)
{
    return s_mounted;
}

/* ------------------------------------------------------------- file table */

#define TDSH_MAX_FILES 8
#define FD_TAG         0x4000   /* marks a FatFS file descriptor */

static FIL  s_fil[TDSH_MAX_FILES];
static bool s_fil_used[TDSH_MAX_FILES];

static int fil_alloc(void)
{
    for (int i = 0; i < TDSH_MAX_FILES; i++) {
        if (!s_fil_used[i]) {
            s_fil_used[i] = true;
            return i;
        }
    }
    return -1;
}

static void fil_free(int idx)
{
    if (idx >= 0 && idx < TDSH_MAX_FILES) {
        s_fil_used[idx] = false;
    }
}

static BYTE fatfs_mode(int flags)
{
    BYTE mode;
    switch (flags & O_ACCMODE) {
    case O_RDONLY: mode = FA_READ; break;
    case O_WRONLY: mode = FA_WRITE; break;
    default:       mode = FA_READ | FA_WRITE; break;
    }
    if (flags & O_CREAT) {
        mode |= (flags & O_TRUNC) ? FA_CREATE_ALWAYS : FA_OPEN_ALWAYS;
    }
    if (flags & O_TRUNC) mode |= FA_CREATE_ALWAYS;
    if (flags & O_APPEND) mode |= FA_OPEN_APPEND;
    return mode;
}

/* --------------------------------------------------------------- syscalls */

int _open_r(struct _reent *reent, const char *path, int flags, int mode)
{
    (void)mode;
    if (!path) {
        reent->_errno = EINVAL;
        return -1;
    }
    int idx = fil_alloc();
    if (idx < 0) {
        reent->_errno = EMFILE;
        return -1;
    }
    FRESULT ret = f_open(&s_fil[idx], path, fatfs_mode(flags));
    if (ret != FR_OK) {
        fil_free(idx);
        reent->_errno = (ret == FR_NO_FILE || ret == FR_NO_PATH) ? ENOENT
                     : (ret == FR_DENIED) ? EACCES
                     : EIO;
        return -1;
    }
    return FD_TAG | idx;
}

int _close_r(struct _reent *reent, int fd)
{
    if ((fd & FD_TAG) == 0) {
        return 0;   /* the console is not ours to close */
    }
    int idx = fd & ~FD_TAG;
    if (idx < 0 || idx >= TDSH_MAX_FILES || !s_fil_used[idx]) {
        reent->_errno = EBADF;
        return -1;
    }
    f_close(&s_fil[idx]);
    fil_free(idx);
    return 0;
}

_ssize_t _read_r(struct _reent *reent, int fd, void *ptr, size_t size)
{
    if (fd == 0) {
        /* stdin: the USB CDC console.  Blocking is the right behaviour for a
         * stdio reader, and this runs in the shell task, never in an ISR. */
        uint8_t *p = (uint8_t *)ptr;
        size_t got = 0;
        while (got < size) {
            int b = tdsh_bl616_console_read_byte();
            if (b < 0) {
                if (got > 0) break;
                vTaskDelay(2);
                continue;
            }
            p[got++] = (uint8_t)b;
            if (p[got - 1] == '\n') break;
        }
        return (_ssize_t)got;
    }
    if ((fd & FD_TAG) == 0) {
        reent->_errno = EBADF;
        return -1;
    }
    int idx = fd & ~FD_TAG;
    if (idx < 0 || idx >= TDSH_MAX_FILES || !s_fil_used[idx]) {
        reent->_errno = EBADF;
        return -1;
    }
    UINT br = 0;
    FRESULT ret = f_read(&s_fil[idx], ptr, (UINT)size, &br);
    if (ret != FR_OK) {
        reent->_errno = EIO;
        return -1;
    }
    return (_ssize_t)br;
}

_ssize_t _write_r(struct _reent *reent, int fd, const void *ptr, size_t size)
{
    if (fd == 1 || fd == 2) {
        /* stdout/stderr: the USB CDC console. */
        int written = tdsh_bl616_console_write(ptr, size);
        if (written < 0) {
            reent->_errno = EIO;
            return -1;
        }
        return (_ssize_t)size;
    }
    if ((fd & FD_TAG) == 0) {
        reent->_errno = EBADF;
        return -1;
    }
    int idx = fd & ~FD_TAG;
    if (idx < 0 || idx >= TDSH_MAX_FILES || !s_fil_used[idx]) {
        reent->_errno = EBADF;
        return -1;
    }
    UINT bw = 0;
    FRESULT ret = f_write(&s_fil[idx], ptr, (UINT)size, &bw);
    if (ret != FR_OK) {
        reent->_errno = EIO;
        return -1;
    }
    return (_ssize_t)bw;
}

off_t _lseek_r(struct _reent *reent, int fd, off_t offset, int whence)
{
    if ((fd & FD_TAG) == 0) {
        reent->_errno = ESPIPE;
        return -1;
    }
    int idx = fd & ~FD_TAG;
    if (idx < 0 || idx >= TDSH_MAX_FILES || !s_fil_used[idx]) {
        reent->_errno = EBADF;
        return -1;
    }
    FSIZE_t base = 0;
    if (whence == SEEK_CUR) base = f_tell(&s_fil[idx]);
    else if (whence == SEEK_END) base = f_size(&s_fil[idx]);
    FRESULT ret = f_lseek(&s_fil[idx], base + (FSIZE_t)offset);
    if (ret != FR_OK) {
        reent->_errno = EIO;
        return -1;
    }
    return (off_t)f_tell(&s_fil[idx]);
}

int _fstat_r(struct _reent *reent, int fd, struct stat *st)
{
    if (!st) {
        reent->_errno = EINVAL;
        return -1;
    }
    memset(st, 0, sizeof(*st));
    if ((fd & FD_TAG) == 0) {
        st->st_mode = S_IFCHR;   /* the console */
        return 0;
    }
    int idx = fd & ~FD_TAG;
    if (idx < 0 || idx >= TDSH_MAX_FILES || !s_fil_used[idx]) {
        reent->_errno = EBADF;
        return -1;
    }
    st->st_mode = S_IFREG;
    st->st_size = (off_t)f_size(&s_fil[idx]);
    return 0;
}

int _stat_r(struct _reent *reent, const char *path, struct stat *st)
{
    if (!path || !st) {
        reent->_errno = EINVAL;
        return -1;
    }
    FILINFO info;
    FRESULT ret = f_stat(path, &info);
    if (ret != FR_OK) {
        reent->_errno = (ret == FR_NO_FILE || ret == FR_NO_PATH) ? ENOENT : EIO;
        return -1;
    }
    memset(st, 0, sizeof(*st));
    st->st_mode = (info.fattrib & AM_DIR) ? S_IFDIR : S_IFREG;
    st->st_size = (off_t)info.fsize;
    return 0;
}

int _unlink_r(struct _reent *reent, const char *path)
{
    FRESULT ret = f_unlink(path);
    if (ret != FR_OK) {
        reent->_errno = (ret == FR_NO_FILE) ? ENOENT : EACCES;
        return -1;
    }
    return 0;
}

int _mkdir_r(struct _reent *reent, const char *path, mode_t mode)
{
    (void)mode;
    FRESULT ret = f_mkdir(path);
    if (ret != FR_OK) {
        reent->_errno = (ret == FR_EXIST) ? EEXIST : EACCES;
        return -1;
    }
    return 0;
}

int _rmdir_r(struct _reent *reent, const char *path)
{
    FRESULT ret = f_rmdir(path);
    if (ret != FR_OK) {
        reent->_errno = (ret == FR_NO_FILE) ? ENOENT : EACCES;
        return -1;
    }
    return 0;
}

int _rename_r(struct _reent *reent, const char *oldname, const char *newname)
{
    FRESULT ret = f_rename(oldname, newname);
    if (ret != FR_OK) {
        reent->_errno = (ret == FR_NO_FILE) ? ENOENT : EACCES;
        return -1;
    }
    return 0;
}

int _isatty_r(struct _reent *reent, int fd)
{
    (void)reent;
    return (fd >= 0 && fd <= 2) ? 1 : 0;
}

int _link_r(struct _reent *reent, const char *a, const char *b)
{
    (void)a; (void)b;
    reent->_errno = ENOSYS;
    return -1;
}

int _fcntl_r(struct _reent *reent, int fd, int cmd, int arg)
{
    (void)fd; (void)cmd; (void)arg;
    reent->_errno = ENOSYS;
    return -1;
}

/* ------------------------------------------------------------- directories */

/* Opaque to the shell; reached only through opendir/readdir/closedir. */
struct __tdsh_dir {
    FF_DIR  ffs_dir;
    struct dirent entry;
    FILINFO info;
    bool    open;
};

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

/* The shell calls the public mkdir()/rmdir(), which the SDK newlib port
 * supplied.  Without that port they must come from here. */
int mkdir(const char *path, mode_t mode)
{
    return _mkdir_r(_REENT, path, mode);
}

int rmdir(const char *path)
{
    return _rmdir_r(_REENT, path);
}
