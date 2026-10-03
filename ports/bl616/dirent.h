// TinyTang — POSIX directory interface for the BL616 port.
//
// The RISC-V toolchain's newlib was built without directory support: its
// <dirent.h> is a stub that ends in #error "<dirent.h> not supported".  This
// header shadows it (ports/bl616 precedes the toolchain include path) and the
// matching implementation lives in tdsh_fs_bl616.c over FatFS.
//
// Only the members TinyDesk Shell actually uses are modelled: the shell reads
// d_name, opens with opendir and walks with readdir.
#ifndef TDSH_BL616_DIRENT_H
#define TDSH_BL616_DIRENT_H

#ifdef __cplusplus
extern "C" {
#endif

typedef struct __tdsh_dir DIR;

struct dirent {
    unsigned long  d_ino;
    long           d_off;
    unsigned short d_reclen;
    unsigned char  d_type;
    char           d_name[256];
};

#define DT_UNKNOWN 0
#define DT_DIR     4
#define DT_REG     8

DIR *opendir(const char *name);
struct dirent *readdir(DIR *dirp);
int closedir(DIR *dirp);

#ifdef __cplusplus
}
#endif

#endif /* TDSH_BL616_DIRENT_H */
