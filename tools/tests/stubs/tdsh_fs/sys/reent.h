/* Host stub: newlib's reentrancy, which glibc does not provide.
 * SPDX-License-Identifier: MIT
 *
 * The module takes a `struct _reent *` in every syscall and sets `_errno` on
 * it, so one instance here is what a test inspects after a failure. */
#ifndef STUB_FS_REENT_H
#define STUB_FS_REENT_H
#include <errno.h>
#include <sys/types.h>
/* newlib spells the signed size_t with a leading underscore; glibc does not. */
typedef ssize_t _ssize_t;
struct _reent { int _errno; };
extern struct _reent host_reent;
#define _REENT (&host_reent)
#endif
