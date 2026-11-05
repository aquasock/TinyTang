/* Host stub: the shell types tang_lora.c touches. SPDX-License-Identifier: MIT */
#ifndef STUB_TDSH_H
#define STUB_TDSH_H
#include <stdint.h>
#include <stddef.h>
typedef struct tdsh_session tdsh_session_t;
typedef int (*tdsh_command_fn)(tdsh_session_t *, int, char **);
typedef struct {
    const char *name;
    const char *usage;
    const char *help;
    tdsh_command_fn fn;
    unsigned flags;
} tdsh_command_t;
int tdsh_register_commands(const tdsh_command_t *cmds, size_t count);
#endif
