// SPDX-License-Identifier: MIT
// Pack plumbing shared by TinyWiki and TinyGutenberg: see td_pack.h.
#include "td_pack.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* This SDK's FatFS declares AM_RDO, AM_HID, AM_SYS, AM_DIR and AM_ARC but not
 * the volume label's bit; a card's own label entry is skipped with the rest. */
#define TD_AM_VOLUME_LABEL 0x08

/* One line from an open file; -1 at end of file, 0 for an empty line.  FatFS
 * here has no f_gets (FF_USE_STRFUNC is off), and the distinction between an
 * empty line and the end is what keeps callers' loops from spinning. */
static int read_line(FIL *file, char *out, int cap)
{
    int n = 0;
    bool any = false;
    for (;;)
    {
        UINT got = 0;
        char c = 0;
        if (f_read(file, &c, 1, &got) != FR_OK || got == 0)
        {
            break;
        }
        any = true;
        if (c == '\n' || n >= cap - 1)
        {
            break;
        }
        out[n++] = c;
    }
    out[n] = '\0';
    return any ? n : -1;
}

void td_pack_defaults(td_pack_meta_t *meta)
{
    memset(meta, 0, sizeof *meta);
    meta->title_bytes = WIKI_TITLE_BYTES_DEFAULT;
    meta->record_bytes = WIKI_RECORD_BYTES(WIKI_TITLE_BYTES_DEFAULT);
    snprintf(meta->title, sizeof meta->title, "Wikipedia");
}

bool td_pack_meta_read(const char *dir, td_pack_meta_t *meta)
{
    td_pack_defaults(meta);
    char path[96];
    snprintf(path, sizeof path, "%s/wiki.meta", dir);
    FIL file;
    if (f_open(&file, path, FA_READ) != FR_OK)
    {
        return false;
    }
    char line[200];
    while (read_line(&file, line, sizeof line) >= 0)
    {
        char *equals = strchr(line, '=');
        if (equals == NULL)
        {
            continue;
        }
        *equals = '\0';
        char *key = line;
        char *value = equals + 1;
        while (*key == ' ' || *key == '\t')
        {
            ++key;
        }
        char *end = key + strlen(key);
        while (end > key && (end[-1] == ' ' || end[-1] == '\t'))
        {
            *--end = '\0';
        }
        while (*value == ' ' || *value == '\t')
        {
            ++value;
        }
        if (strcmp(key, "title") == 0)
        {
            snprintf(meta->title, sizeof meta->title, "%s", value);
        }
        else if (strcmp(key, "license") == 0)
        {
            snprintf(meta->license, sizeof meta->license, "%s", value);
        }
        else if (strcmp(key, "main_page") == 0)
        {
            snprintf(meta->main_page, sizeof meta->main_page, "%s", value);
        }
        else if (strcmp(key, "articles") == 0)
        {
            meta->articles = (uint32_t)strtoul(value, NULL, 10);
        }
        else if (strcmp(key, "chunks") == 0)
        {
            meta->chunks = (uint32_t)strtoul(value, NULL, 10);
        }
        else if (strcmp(key, "parts") == 0)
        {
            meta->parts = (uint32_t)strtoul(value, NULL, 10);
        }
        else if (strcmp(key, "authors") == 0)
        {
            meta->authors = (uint32_t)strtoul(value, NULL, 10);
        }
        else if (strcmp(key, "title_bytes") == 0)
        {
            unsigned long v = strtoul(value, NULL, 10);
            if (v >= 8 && v <= WIKI_MAX_TITLE_BYTES)
            {
                meta->title_bytes = (uint16_t)v;
                meta->record_bytes = WIKI_RECORD_BYTES(meta->title_bytes);
            }
        }
        else if (strcmp(key, "record_bytes") == 0)
        {
            unsigned long v = strtoul(value, NULL, 10);
            if (v == (unsigned long)meta->title_bytes + WIKI_RECORD_OVERHEAD)
            {
                meta->record_bytes = (uint16_t)v;
            }
        }
    }
    (void)f_close(&file);
    return true;
}

void td_pack_dir_of(const char *root, const char *folder, char *out, size_t cap)
{
    if (folder == NULL || folder[0] == '\0')
    {
        snprintf(out, cap, "%s", root);
    }
    else
    {
        snprintf(out, cap, "%s/%s", root, folder);
    }
}

static bool add_pack(td_pack_entry_t *out, int max, int *count, const char *folder,
                     const char *dir)
{
    if (*count >= max)
    {
        return false;
    }
    td_pack_meta_t meta;
    if (!td_pack_meta_read(dir, &meta))
    {
        return false;
    }
    td_pack_entry_t *entry = &out[(*count)++];
    snprintf(entry->folder, sizeof entry->folder, "%s", folder);
    snprintf(entry->title, sizeof entry->title, "%s", meta.title);
    entry->articles = meta.articles;
    return true;
}

int td_pack_scan(const char *root, td_pack_entry_t *out, int max)
{
    int count = 0;
    if (out == NULL || max <= 0)
    {
        return 0;
    }

    char path[96];
    FILINFO info;
    snprintf(path, sizeof path, "%s/wiki.meta", root);
    if (f_stat(path, &info) == FR_OK && (info.fattrib & AM_DIR) == 0)
    {
        (void)add_pack(out, max, &count, "", root);
    }

    DIR handle;
    if (f_opendir(&handle, root) == FR_OK)
    {
        FILINFO entry;
        FRESULT result;
        char dir[96];
        while ((result = f_readdir(&handle, &entry)) == FR_OK && entry.fname[0] != '\0')
        {
            if ((entry.fattrib & (AM_DIR | AM_HID | AM_SYS | TD_AM_VOLUME_LABEL)) != AM_DIR)
            {
                continue;
            }
            if (!wiki_pack_dir_ok(entry.fname))
            {
                continue;
            }
            snprintf(dir, sizeof dir, "%s/%s", root, entry.fname);
            (void)add_pack(out, max, &count, entry.fname, dir);
        }
        (void)f_closedir(&handle);
    }

    // Folder order, so the list does not depend on what FAT hands back.
    for (int i = 1; i < count; ++i)
    {
        td_pack_entry_t moving = out[i];
        int j = i - 1;
        while (j >= 0 && strcasecmp(out[j].folder, moving.folder) > 0)
        {
            out[j + 1] = out[j];
            --j;
        }
        out[j + 1] = moving;
    }
    return count;
}

bool td_pack_record(FIL *index, const td_pack_meta_t *meta, uint32_t number,
                    wiki_record_t *record)
{
    uint8_t raw[WIKI_MAX_TITLE_BYTES + WIKI_RECORD_OVERHEAD];
    if (index == NULL || meta == NULL || number >= meta->articles ||
        meta->record_bytes > sizeof raw)
    {
        return false;
    }
    if (f_lseek(index, (FSIZE_t)number * meta->record_bytes) != FR_OK)
    {
        return false;
    }
    UINT got = 0;
    if (f_read(index, raw, meta->record_bytes, &got) != FR_OK ||
        got != meta->record_bytes)
    {
        return false;
    }
    return wiki_record_parse(raw, meta->title_bytes, record);
}

uint32_t td_pack_lower_bound(FIL *index, const td_pack_meta_t *meta,
                             const char *prefix)
{
    uint32_t low = 0;
    uint32_t high = meta->articles;
    wiki_record_t record;
    while (low < high)
    {
        uint32_t mid = low + (high - low) / 2;
        if (!td_pack_record(index, meta, mid, &record))
        {
            break;
        }
        if (wiki_title_cmp(record.title, prefix) < 0)
        {
            low = mid + 1;
        }
        else
        {
            high = mid;
        }
    }
    return low;
}
