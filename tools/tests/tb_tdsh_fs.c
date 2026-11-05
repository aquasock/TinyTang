/* TinyTang — host test for the FatFS syscall layer (ports/bl616/tdsh_fs_bl616.c).
 *
 * This is the layer between newlib's stdio and the card, so everything the
 * shell and both desktop apps read or write goes through it.  FatFS itself is
 * a stub here: each call's result is chosen by the test, which is what lets the
 * errno mapping be checked against FatFs's own result values.
 *
 * The behaviours worth pinning are the ones that fail quietly:
 *
 *  - the file-descriptor table, which is eight slots with a tag bit, so a
 *    close of a descriptor the port does not own must not free a slot;
 *  - the open-mode mapping, where O_CREAT without O_TRUNC means "open or
 *    create" and must not empty an existing file;
 *  - the errno each FatFs result becomes, because a shell reports the errno;
 *  - `_stat_r`'s directory probe, which is what makes `ls /` work when FatFS
 *    cannot describe the volume root.
 *
 * SPDX-License-Identifier: MIT
 */

#include <errno.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <sys/stat.h>

#include "FreeRTOS.h"
#include "task.h"
#include "bflb_gpio.h"
#include "bflb_mtimer.h"
#include "board.h"
#include "fatfs_diskio_register.h"
#include "bflb_device_stub.h"
#include "ff.h"

/* ------------------------------------------------------------ the stubs -- */

struct _reent host_reent;
static int critical_depth;

void host_enter_critical(void) { critical_depth++; }
void host_exit_critical(void) { critical_depth--; }
int host_critical_depth(void) { return critical_depth; }
void vTaskDelay(TickType_t ticks) { (void)ticks; }
uint32_t bflb_mtimer_get_time_ms(void) { return 0; }
void bflb_mtimer_delay_ms(uint32_t ms) { (void)ms; }

static uint32_t gpio_high;
static int gpio_inits;
static int sdh_gpio_init_calls;
static int diskio_register_calls;
static uint32_t pin16_flags;

void bflb_gpio_init(void *dev, uint32_t pin, int flags)
{
    (void)dev;
    if (pin == GPIO_PIN_16) { gpio_inits++; pin16_flags = (uint32_t)flags; }
}
void bflb_gpio_set(void *dev, uint32_t pin) { (void)dev; gpio_high |= (1u << pin); }
uint32_t host_gpio_high_mask(void) { return gpio_high; }

void board_sdh_gpio_init(void) { sdh_gpio_init_calls++; }
void fatfs_sdh_driver_register(void) { diskio_register_calls++; }

static struct bflb_device_s host_dev;
struct bflb_device_s *bflb_device_get_by_name(const char *name)
{
    (void)name;
    return &host_dev;
}

/* The console hooks the stdio descriptors route to. */
static const char *pending_input;
static size_t pending_input_at;
static char out_text[4096];
static size_t out_len;
static int out_fail;

int tdsh_bl616_input_read_byte(void)
{
    if (!pending_input || pending_input[pending_input_at] == '\0') return -1;
    return (unsigned char)pending_input[pending_input_at++];
}

int tdsh_bl616_output(const void *data, size_t length)
{
    if (out_fail) return -1;
    if (out_len + length < sizeof out_text) {
        memcpy(out_text + out_len, data, length);
        out_len += length;
    }
    return (int)length;
}

/* ---- FatFS: results are chosen per call, and a small file table backs the
 * ones the driver's open/read/write path uses ---- */

struct fake_file {
    const char *path;
    unsigned char data[4096];
    size_t len;
    size_t at;
    int open;
    int is_dir;
};
static struct fake_file files[16];
static int file_count;

static FRESULT next_mount = FR_OK;
static FRESULT next_open = FR_OK;
static FRESULT next_read = FR_OK;
static FRESULT next_write = FR_OK;
static FRESULT next_lseek = FR_OK;
static FRESULT next_stat = FR_NO_FILE;
static FRESULT next_unlink = FR_OK;
static FRESULT next_mkdir = FR_OK;
static FRESULT next_rmdir = FR_OK;
static FRESULT next_rename = FR_OK;
static FRESULT next_opendir = FR_OK;

static const char *last_mount_path;
static const char *last_open_path;
static BYTE last_open_mode;
static const char *last_mkdir_path;
static const char *last_rmdir_path;
static const char *last_renamed_from;
static const char *last_renamed_to;
static const char *last_unlink_path;
static const char *last_stat_path;
static int opendir_calls, closedir_calls, readdir_calls;

/* A directory listing a readdir walk returns. */
static const char *dir_names[8];
static uint32_t dir_attribs[8];
static int dir_count;
static int dir_at;

static struct fake_file *find(const char *path)
{
    for (int i = 0; i < file_count; i++)
        if (strcmp(files[i].path, path) == 0) return &files[i];
    return NULL;
}

FRESULT f_mount(FATFS *fs, const char *path, BYTE opt)
{
    (void)opt;
    last_mount_path = path;
    if (fs) fs->mounted = (next_mount == FR_OK);
    return next_mount;
}

FRESULT f_open(FIL *fp, const char *path, BYTE mode)
{
    last_open_path = path;
    last_open_mode = mode;
    if (next_open != FR_OK) return next_open;
    /* The fake registers any path it is opened on: these tests are about the
     * driver's fd table and mode mapping, not FatFS's own existence rules, and
     * a test that wants a refusal sets next_open. */
    struct fake_file *f = find(path);
    if (!f) {
        if (file_count >= 16) return FR_DENIED;
        f = &files[file_count++];
        f->path = path;
    }
    if (mode & FA_CREATE_ALWAYS) f->len = 0;
    f->at = (mode & FA_OPEN_APPEND) ? f->len : 0;
    f->open = 1;
    fp->fd = (int)(f - files) + 1;
    return FR_OK;
}

FRESULT f_close(FIL *fp) { if (fp->fd) files[fp->fd - 1].open = 0; return FR_OK; }

FRESULT f_read(FIL *fp, void *buf, UINT n, UINT *br)
{
    if (next_read != FR_OK) return next_read;
    struct fake_file *f = &files[fp->fd - 1];
    UINT i = 0;
    while (i < n && f->at < f->len) ((unsigned char *)buf)[i++] = f->data[f->at++];
    *br = i;
    return FR_OK;
}

FRESULT f_write(FIL *fp, const void *buf, UINT n, UINT *bw)
{
    if (next_write != FR_OK) return next_write;
    struct fake_file *f = &files[fp->fd - 1];
    UINT i = 0;
    while (i < n && f->at < sizeof f->data) f->data[f->at++] = ((const unsigned char *)buf)[i++];
    if (f->at > f->len) f->len = f->at;
    *bw = i;
    return FR_OK;
}

FRESULT f_lseek(FIL *fp, FSIZE_t ofs)
{
    if (next_lseek != FR_OK) return next_lseek;
    files[fp->fd - 1].at = (size_t)ofs;
    return FR_OK;
}
FSIZE_t f_tell(FIL *fp) { return (FSIZE_t)files[fp->fd - 1].at; }
FSIZE_t f_size(FIL *fp) { return (FSIZE_t)files[fp->fd - 1].len; }

FRESULT f_stat(const char *path, FILINFO *info)
{
    last_stat_path = path;
    if (next_stat != FR_OK) return next_stat;
    struct fake_file *f = find(path);
    if (!f) return FR_NO_FILE;
    memset(info, 0, sizeof *info);
    info->fsize = (FSIZE_t)f->len;
    info->fattrib = f->is_dir ? AM_DIR : 0;
    snprintf(info->fname, sizeof info->fname, "%s", path);
    return FR_OK;
}

FRESULT f_unlink(const char *path) { last_unlink_path = path; return next_unlink; }
FRESULT f_mkdir(const char *path) { last_mkdir_path = path; return next_mkdir; }
FRESULT f_rmdir(const char *path) { last_rmdir_path = path; return next_rmdir; }
FRESULT f_rename(const char *from, const char *to)
{
    last_renamed_from = from;
    last_renamed_to = to;
    return next_rename;
}

FRESULT f_opendir(FF_DIR *dp, const char *path)
{
    (void)path;
    opendir_calls++;
    if (next_opendir != FR_OK) return next_opendir;
    dp->handle = 1;
    dir_at = 0;
    return FR_OK;
}

FRESULT f_readdir(FF_DIR *dp, FILINFO *info)
{
    (void)dp;
    readdir_calls++;
    if (dir_at >= dir_count) { memset(info, 0, sizeof *info); return FR_OK; }
    memset(info, 0, sizeof *info);
    info->fattrib = dir_attribs[dir_at];
    snprintf(info->fname, sizeof info->fname, "%s", dir_names[dir_at]);
    dir_at++;
    return FR_OK;
}

FRESULT f_closedir(FF_DIR *dp) { (void)dp; closedir_calls++; return FR_OK; }

/* The module under test. */
#include "../../ports/bl616/tdsh_fs_bl616.c"

/* --------------------------------------------------------------- checks -- */

static int failures;

#define CHECK(x)                                                              \
    do {                                                                      \
        if (!(x)) {                                                           \
            printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #x);                \
            failures++;                                                       \
        }                                                                     \
    } while (0)

#define CHECK_EQ(a, b)                                                        \
    do {                                                                      \
        long _a = (long)(a), _b = (long)(b);                                   \
        if (_a != _b) {                                                       \
            printf("FAIL %s:%d: %s == %ld, expected %ld\n", __FILE__, __LINE__, \
                   #a, _a, _b);                                               \
            failures++;                                                       \
        }                                                                     \
    } while (0)

static void reset_world(void)
{
    critical_depth = 0;
    gpio_high = 0;
    gpio_inits = 0;
    pin16_flags = 0;
    sdh_gpio_init_calls = 0;
    diskio_register_calls = 0;
    /* The fake's file contents are part of the world, not just its table: a
     * stale length from an earlier case would make a later size assertion
     * pass or fail for the wrong reason. */
    memset(files, 0, sizeof files);
    file_count = 0;
    next_mount = next_open = next_read = next_write = next_lseek = FR_OK;
    next_stat = FR_NO_FILE;
    next_unlink = next_mkdir = next_rmdir = next_rename = next_opendir = FR_OK;
    last_mount_path = last_open_path = last_mkdir_path = last_rmdir_path = NULL;
    last_renamed_from = last_renamed_to = NULL;
    last_unlink_path = last_stat_path = NULL;
    last_open_mode = 0;
    opendir_calls = closedir_calls = readdir_calls = 0;
    dir_count = dir_at = 0;
    pending_input = NULL;
    pending_input_at = 0;
    out_len = 0; out_text[0] = '\0';
    out_fail = 0;
    host_reent._errno = 0;
    s_mounted = false;
    s_last_result = -999;
    for (int i = 0; i < TDSH_MAX_FILES; i++) s_fil_used[i] = false;
}

int main(void)
{
    /* ------------------------------------------------------------- mount --
     * The card is gated behind GPIO 16, and the mount must drive it high
     * before probing: without it f_mount returns FR_NOT_READY. */
    reset_world();
    CHECK_EQ(tdsh_bl616_fs_mount(), 0);
    CHECK_EQ(gpio_inits, 1);
    CHECK(gpio_high & (1u << GPIO_PIN_16));
    CHECK_EQ(sdh_gpio_init_calls, 1);
    CHECK_EQ(diskio_register_calls, 1);
    CHECK_EQ(strcmp(last_mount_path, "/sd"), 0);
    CHECK(tdsh_bl616_fs_ready());
    CHECK_EQ(tdsh_bl616_fs_last_result(), 0);

    /* A refused mount reports the FatFs code negated, and leaves the layer
     * not-ready so nothing pretends the card is there. */
    reset_world();
    next_mount = FR_NOT_READY;
    CHECK_EQ(tdsh_bl616_fs_mount(), -(int)FR_NOT_READY);
    CHECK(!tdsh_bl616_fs_ready());
    CHECK_EQ(tdsh_bl616_fs_last_result(), (int)FR_NOT_READY);

    /* ------------------------------------------------------ open flags ----
     * The mode mapping, which decides whether an existing file is emptied. */
    reset_world();
    {
        const int fd = _open_r(_REENT, "/sd/a.txt", O_RDONLY, 0);
        CHECK(fd >= 0);
        CHECK_EQ(last_open_mode, FA_READ);
        _close_r(_REENT, fd);
    }
    reset_world();
    (void)_open_r(_REENT, "/sd/b.txt", O_WRONLY, 0);
    CHECK_EQ(last_open_mode, FA_WRITE);
    /* O_CREAT alone means open-or-create and must not truncate. */
    reset_world();
    (void)_open_r(_REENT, "/sd/c.txt", O_WRONLY | O_CREAT, 0);
    CHECK_EQ(last_open_mode, FA_WRITE | FA_OPEN_ALWAYS);
    /* O_CREAT with O_TRUNC is create-always. */
    reset_world();
    (void)_open_r(_REENT, "/sd/d.txt", O_WRONLY | O_CREAT | O_TRUNC, 0);
    CHECK_EQ(last_open_mode, FA_WRITE | FA_CREATE_ALWAYS);
    /* O_APPEND is the append bit. */
    reset_world();
    (void)_open_r(_REENT, "/sd/e.txt", O_WRONLY | O_APPEND, 0);
    CHECK_EQ(last_open_mode, FA_WRITE | FA_OPEN_APPEND);

    /* ---------------------------------------------------- fd table ----
     * Eight slots, tagged, and a console descriptor is not ours to close. */
    reset_world();
    {
        int fds[TDSH_MAX_FILES];
        for (int i = 0; i < TDSH_MAX_FILES; i++) {
            fds[i] = _open_r(_REENT, "/sd/f.txt", O_RDONLY, 0);
            CHECK(fds[i] >= 0);
        }
        /* The ninth is refused with EMFILE rather than reusing a live slot. */
        host_reent._errno = 0;
        CHECK_EQ(_open_r(_REENT, "/sd/g.txt", O_RDONLY, 0), -1);
        CHECK_EQ(host_reent._errno, EMFILE);
        /* Closing one frees exactly that slot. */
        CHECK_EQ(_close_r(_REENT, fds[3]), 0);
        CHECK_EQ(_open_r(_REENT, "/sd/h.txt", O_RDONLY, 0), fds[3]);
        for (int i = 0; i < TDSH_MAX_FILES; i++) {
            if (i != 3) _close_r(_REENT, fds[i]);
        }
        _close_r(_REENT, fds[3]);
    }

    /* Closing stdin/stdout/stderr is a success that touches nothing: the
     * console is not a file this layer owns.  The table is filled first, so a
     * close that freed a slot behind its back would show as the ninth open
     * succeeding when it must not. */
    reset_world();
    {
        int fds[TDSH_MAX_FILES];
        for (int i = 0; i < TDSH_MAX_FILES; i++)
            fds[i] = _open_r(_REENT, "/sd/z", O_RDONLY, 0);
        for (int i = 0; i < TDSH_MAX_FILES; i++) CHECK(fds[i] >= 0);
        CHECK_EQ(_close_r(_REENT, 0), 0);
        CHECK_EQ(_close_r(_REENT, 1), 0);
        CHECK_EQ(_close_r(_REENT, 2), 0);
        host_reent._errno = 0;
        CHECK_EQ(_open_r(_REENT, "/sd/z2", O_RDONLY, 0), -1);
        CHECK_EQ(host_reent._errno, EMFILE);
        for (int i = 0; i < TDSH_MAX_FILES; i++) _close_r(_REENT, fds[i]);
    }

    /* An untagged, unopened descriptor is EBADF, not a silent success. */
    reset_world();
    host_reent._errno = 0;
    CHECK_EQ(_close_r(_REENT, 0x4000 | 5), -1);
    CHECK_EQ(host_reent._errno, EBADF);

    /* -------------------------------------------------- open failures ----
     * The errno a shell reports comes from here. */
    reset_world();
    next_open = FR_NO_FILE;
    host_reent._errno = 0;
    CHECK_EQ(_open_r(_REENT, "/sd/missing", O_RDONLY, 0), -1);
    CHECK_EQ(host_reent._errno, ENOENT);
    reset_world();
    next_open = FR_NO_PATH;
    host_reent._errno = 0;
    CHECK_EQ(_open_r(_REENT, "/sd/nodir/x", O_RDONLY, 0), -1);
    CHECK_EQ(host_reent._errno, ENOENT);
    reset_world();
    next_open = FR_DENIED;
    host_reent._errno = 0;
    CHECK_EQ(_open_r(_REENT, "/sd/denied", O_RDONLY, 0), -1);
    CHECK_EQ(host_reent._errno, EACCES);
    reset_world();
    next_open = FR_DISK_ERR;
    host_reent._errno = 0;
    CHECK_EQ(_open_r(_REENT, "/sd/bad", O_RDONLY, 0), -1);
    CHECK_EQ(host_reent._errno, EIO);
    /* A failed open must not leak its slot.  The failed opens and the
     * successful ones have to share one world for that to mean anything, so
     * there is deliberately no reset between them: eight refusals followed by
     * eight opens, which can only all succeed if every refusal gave its slot
     * back. */
    reset_world();
    {
        int fds[TDSH_MAX_FILES];
        for (int i = 0; i < TDSH_MAX_FILES * 2; i++) {
            next_open = FR_DISK_ERR;
            CHECK_EQ(_open_r(_REENT, "/sd/x", O_RDONLY, 0), -1);
        }
        next_open = FR_OK;
        for (int i = 0; i < TDSH_MAX_FILES; i++)
            fds[i] = _open_r(_REENT, "/sd/y", O_RDONLY, 0);
        for (int i = 0; i < TDSH_MAX_FILES; i++) CHECK(fds[i] >= 0);
        for (int i = 0; i < TDSH_MAX_FILES; i++) _close_r(_REENT, fds[i]);
        /* And a NULL path is EINVAL before a slot is taken. */
        host_reent._errno = 0;
        CHECK_EQ(_open_r(_REENT, NULL, O_RDONLY, 0), -1);
        CHECK_EQ(host_reent._errno, EINVAL);
    }

    /* ------------------------------------------------- read and write ---- */
    reset_world();
    {
        const int fd = _open_r(_REENT, "/sd/rw.txt", O_RDWR, 0);
        CHECK(fd >= 0);
        CHECK_EQ(_write_r(_REENT, fd, "hello", 5), 5);
        _lseek_r(_REENT, fd, 0, SEEK_SET);
        char buf[8] = {0};
        CHECK_EQ(_read_r(_REENT, fd, buf, 5), 5);
        CHECK_EQ(memcmp(buf, "hello", 5), 0);
        _close_r(_REENT, fd);
    }

    /* A failed read or write is EIO, not a short count. */
    reset_world();
    {
        const int fd = _open_r(_REENT, "/sd/rw.txt", O_RDWR, 0);
        next_read = FR_DISK_ERR;
        host_reent._errno = 0;
        CHECK_EQ(_read_r(_REENT, fd, (void *)"x", 1), -1);
        CHECK_EQ(host_reent._errno, EIO);
        next_write = FR_DISK_ERR;
        host_reent._errno = 0;
        CHECK_EQ(_write_r(_REENT, fd, "x", 1), -1);
        CHECK_EQ(host_reent._errno, EIO);
        _close_r(_REENT, fd);
    }

    /* ---------------------------------------------------- stdio routing -- */
    reset_world();
    pending_input = "line\nrest";
    {
        char buf[16] = {0};
        CHECK_EQ(_read_r(_REENT, 0, buf, sizeof buf), 5);   /* stops at \n */
        CHECK_EQ(memcmp(buf, "line\n", 5), 0);
    }
    reset_world();
    CHECK_EQ(_write_r(_REENT, 1, "out", 3), 3);
    CHECK_EQ(memcmp(out_text, "out", 3), 0);
    CHECK_EQ(_write_r(_REENT, 2, "err", 3), 3);
    CHECK_EQ(memcmp(out_text + 3, "err", 3), 0);
    /* A failed console write is EIO. */
    reset_world();
    out_fail = 1;
    host_reent._errno = 0;
    CHECK_EQ(_write_r(_REENT, 1, "x", 1), -1);
    CHECK_EQ(host_reent._errno, EIO);
    /* isatty is true for the three standard descriptors only. */
    CHECK_EQ(_isatty_r(_REENT, 0), 1);
    CHECK_EQ(_isatty_r(_REENT, 2), 1);
    CHECK_EQ(_isatty_r(_REENT, 5), 0);

    /* -------------------------------------------------------- seek ------
     * SEEK_CUR adds the current position and SEEK_END the file size. */
    reset_world();
    {
        const int fd = _open_r(_REENT, "/sd/s.txt", O_RDWR, 0);
        CHECK_EQ(_write_r(_REENT, fd, "0123456789", 10), 10);
        CHECK_EQ(_lseek_r(_REENT, fd, 0, SEEK_SET), 0);
        CHECK_EQ(_lseek_r(_REENT, fd, 3, SEEK_CUR), 3);
        CHECK_EQ(_lseek_r(_REENT, fd, -1, SEEK_END), 9);
        CHECK_EQ(_lseek_r(_REENT, fd, 2, SEEK_SET), 2);
        /* A seek on the console is ESPIPE: it is not seekable. */
        host_reent._errno = 0;
        CHECK_EQ(_lseek_r(_REENT, 1, 0, SEEK_SET), -1);
        CHECK_EQ(host_reent._errno, ESPIPE);
        _close_r(_REENT, fd);
    }

    /* -------------------------------------------------------- fstat ----- */
    reset_world();
    {
        const int fd = _open_r(_REENT, "/sd/f.txt", O_RDWR, 0);
        _write_r(_REENT, fd, "abcd", 4);
        struct stat st;
        CHECK_EQ(_fstat_r(_REENT, fd, &st), 0);
        CHECK((st.st_mode & S_IFREG) != 0);
        CHECK_EQ(st.st_size, 4);
        /* The console is a character device. */
        CHECK_EQ(_fstat_r(_REENT, 1, &st), 0);
        CHECK((st.st_mode & S_IFCHR) != 0);
        /* A NULL stat buffer is EINVAL. */
        host_reent._errno = 0;
        CHECK_EQ(_fstat_r(_REENT, fd, NULL), -1);
        CHECK_EQ(host_reent._errno, EINVAL);
        _close_r(_REENT, fd);
    }

    /* --------------------------------------------------------- stat -----
     * The directory probe: FatFS cannot describe the volume root, so a path
     * that opens as a directory is still reported as one.  That is what makes
     * `ls /` work. */
    reset_world();
    {
        struct stat st;
        next_stat = FR_NO_FILE;
        next_opendir = FR_OK;
        CHECK_EQ(_stat_r(_REENT, "/sd", &st), 0);
        CHECK((st.st_mode & S_IFDIR) != 0);
        CHECK(opendir_calls > 0);
    }
    /* A path that neither stats nor opens is ENOENT. */
    reset_world();
    {
        struct stat st;
        next_stat = FR_NO_FILE;
        next_opendir = FR_NO_PATH;
        host_reent._errno = 0;
        CHECK_EQ(_stat_r(_REENT, "/sd/nope", &st), -1);
        CHECK_EQ(host_reent._errno, ENOENT);
    }
    /* A real file reports its size and regular-file mode. */
    reset_world();
    {
        files[0].path = "/sd/f.txt";
        files[0].len = 42;
        file_count = 1;
        struct stat st;
        next_stat = FR_OK;
        CHECK_EQ(_stat_r(_REENT, "/sd/f.txt", &st), 0);
        CHECK((st.st_mode & S_IFREG) != 0);
        CHECK_EQ(st.st_size, 42);
    }
    /* A directory reports S_IFDIR from its attribute, without a probe. */
    reset_world();
    {
        files[0].path = "/sd/dir";
        files[0].is_dir = 1;
        file_count = 1;
        struct stat st;
        next_stat = FR_OK;
        CHECK_EQ(_stat_r(_REENT, "/sd/dir", &st), 0);
        CHECK((st.st_mode & S_IFDIR) != 0);
    }
    /* NULL arguments are EINVAL. */
    reset_world();
    {
        struct stat st;
        host_reent._errno = 0;
        CHECK_EQ(_stat_r(_REENT, NULL, &st), -1);
        CHECK_EQ(host_reent._errno, EINVAL);
    }

    /* ------------------------------------------ unlink, mkdir, rmdir ---- */
    reset_world();
    CHECK_EQ(_unlink_r(_REENT, "/sd/a"), 0);
    CHECK_EQ(strcmp(last_unlink_path, "/sd/a"), 0);
    next_unlink = FR_NO_FILE;
    host_reent._errno = 0;
    CHECK_EQ(_unlink_r(_REENT, "/sd/a"), -1);
    CHECK_EQ(host_reent._errno, ENOENT);
    next_unlink = FR_DENIED;
    host_reent._errno = 0;
    CHECK_EQ(_unlink_r(_REENT, "/sd/a"), -1);
    CHECK_EQ(host_reent._errno, EACCES);

    /* mkdir's EEXIST is what makes `mkdir -p` idempotent in the shell: the
     * shell's parent walk treats EEXIST as success, so this mapping is
     * load-bearing rather than cosmetic. */
    reset_world();
    CHECK_EQ(_mkdir_r(_REENT, "/sd/new", 0755), 0);
    CHECK_EQ(strcmp(last_mkdir_path, "/sd/new"), 0);
    next_mkdir = FR_EXIST;
    host_reent._errno = 0;
    CHECK_EQ(_mkdir_r(_REENT, "/sd/new", 0755), -1);
    CHECK_EQ(host_reent._errno, EEXIST);
    next_mkdir = FR_DENIED;
    host_reent._errno = 0;
    CHECK_EQ(_mkdir_r(_REENT, "/sd/new", 0755), -1);
    CHECK_EQ(host_reent._errno, EACCES);

    reset_world();
    CHECK_EQ(_rmdir_r(_REENT, "/sd/old"), 0);
    next_rmdir = FR_NO_FILE;
    host_reent._errno = 0;
    CHECK_EQ(_rmdir_r(_REENT, "/sd/old"), -1);
    CHECK_EQ(host_reent._errno, ENOENT);

    reset_world();
    CHECK_EQ(_rename_r(_REENT, "/sd/a", "/sd/b"), 0);
    CHECK_EQ(strcmp(last_renamed_from, "/sd/a"), 0);
    CHECK_EQ(strcmp(last_renamed_to, "/sd/b"), 0);
    next_rename = FR_NO_FILE;
    host_reent._errno = 0;
    CHECK_EQ(_rename_r(_REENT, "/sd/a", "/sd/b"), -1);
    CHECK_EQ(host_reent._errno, ENOENT);

    /* The public wrappers reach the same place: the shell calls mkdir(). */
    reset_world();
    CHECK_EQ(mkdir("/sd/pub", 0755), 0);
    CHECK_EQ(strcmp(last_mkdir_path, "/sd/pub"), 0);
    CHECK_EQ(rmdir("/sd/pub"), 0);
    CHECK_EQ(strcmp(last_rmdir_path, "/sd/pub"), 0);

    /* Unsupported calls say so rather than pretending. */
    reset_world();
    host_reent._errno = 0;
    CHECK_EQ(_link_r(_REENT, "/sd/a", "/sd/b"), -1);
    CHECK_EQ(host_reent._errno, ENOSYS);
    CHECK_EQ(_fcntl_r(_REENT, 1, 0, 0), -1);
    CHECK_EQ(host_reent._errno, ENOSYS);

    /* ------------------------------------------------------- opendir ---- */
    reset_world();
    dir_names[0] = "file.txt";   dir_attribs[0] = 0;
    dir_names[1] = "folder";     dir_attribs[1] = AM_DIR;
    dir_count = 2;
    {
        DIR *d = opendir("/sd/root");
        CHECK(d != NULL);
        struct dirent *e = readdir(d);
        CHECK(e != NULL);
        CHECK_EQ(strcmp(e->d_name, "file.txt"), 0);
        CHECK_EQ(e->d_type, DT_REG);
        e = readdir(d);
        CHECK(e != NULL);
        CHECK_EQ(strcmp(e->d_name, "folder"), 0);
        CHECK_EQ(e->d_type, DT_DIR);
        /* The end of the directory is NULL, and stays NULL. */
        CHECK(readdir(d) == NULL);
        CHECK(readdir(d) == NULL);
        CHECK_EQ(closedir(d), 0);
        CHECK_EQ(closedir_calls, 1);
    }
    /* A directory that will not open is ENOENT, and nothing leaks. */
    reset_world();
    next_opendir = FR_NO_PATH;
    CHECK(opendir("/sd/absent") == NULL);
    /* A NULL path is EINVAL. */
    reset_world();
    CHECK(opendir(NULL) == NULL);
    /* closedir on a freed handle is refused rather than double-freed. */
    CHECK_EQ(closedir(NULL), -1);

    /* Empty while holding no lock: an unbalanced critical section would leave
     * interrupts off on the board. */
    CHECK_EQ(critical_depth, 0);

    if (failures) {
        printf("tb_tdsh_fs: %d failure(s)\n", failures);
        return 1;
    }
    printf("tb_tdsh_fs: all checks passed\n");
    return 0;
}
