// TinyGBC — reading /gbc.ini, the settings file at the card root.
//
// The same shape as /nes.ini and read the same way
// (ports/bl616/tang_settings_ini.c).  It holds only [audio] so far, the one
// thing the TinyGBC player takes from the host.  Read once per launch; the
// TinyGBC window on the desktop is what writes it.
//
// SPDX-License-Identifier: MIT

#include "tang_gbc_ini.h"

#include <string.h>

void tang_gbc_defaults(tang_gbc_settings_t *s)
{
    memset(s, 0, sizeof *s);
    tang_audio_defaults(&s->audio);
}

static bool apply(void *ctx, const char *section, const char *key,
                  const char *value)
{
    tang_gbc_settings_t *s = ctx;

    if (strcmp(section, "audio") == 0) {
        return tang_audio_apply(&s->audio, key, value);
    }
    return false;
}

bool tang_gbc_load(tang_gbc_settings_t *s, const char *path)
{
    tang_gbc_defaults(s);
    s->found = tang_settings_walk(path, apply, s, &s->keys);
    return s->found;
}
