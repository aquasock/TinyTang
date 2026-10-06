/* SPDX-License-Identifier: MIT
 * Run the actual app's directory scan with controlled FatFs and allocation
 * failures. GUI and playback code are discarded by the host linker. */
#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

static void *catalog_malloc(size_t bytes);
static void catalog_free(void *ptr);
#define malloc catalog_malloc
#define free   catalog_free
#include "td_phosphor_app.cpp"
#undef malloc
#undef free

static std::vector<FILINFO> first_pass, second_pass;
static bool changed, fail_open, fail_rewind, fail_alloc;
static int fail_read_pass = -1;
static unsigned fail_read_index;
static int opened, closed, list_count;
static size_t requested;
static void *allocation;
static td_widget_t folder_widget, list_widget;

static void *catalog_malloc(size_t bytes)
{
    requested = bytes;
    assert(allocation == nullptr);
    if (fail_alloc)
        return nullptr;
    allocation = std::malloc(bytes);
    assert(allocation != nullptr);
    return allocation;
}

static void catalog_free(void *ptr)
{
    if (ptr == nullptr)
        return;
    assert(ptr == allocation);
    std::free(ptr);
    allocation = nullptr;
}

extern "C" FRESULT f_opendir(DIR *dir, const char *path)
{
    assert(std::strcmp(path, "/sd/music") == 0);
    if (fail_open)
        return FR_NO_PATH;
    *dir = {};
    ++opened;
    return FR_OK;
}

extern "C" FRESULT f_readdir(DIR *dir, FILINFO *info)
{
    if (info == nullptr)
    {
        if (fail_rewind)
            return FR_DISK_ERR;
        dir->index = 0;
        ++dir->pass;
        return FR_OK;
    }
    if (dir->pass == fail_read_pass && dir->index == fail_read_index)
        return FR_DISK_ERR;
    const auto &files = changed && dir->pass != 0 ? second_pass : first_pass;
    *info = dir->index < files.size() ? files[dir->index++] : FILINFO{};
    return FR_OK;
}

extern "C" FRESULT f_closedir(DIR *)
{
    ++closed;
    return FR_OK;
}
extern "C" const char *td_widget_text(const td_widget_t *w)
{
    return w->text;
}
extern "C" void td_widget_set_text(td_widget_t *w, const char *text)
{
    std::snprintf(w->text, sizeof(w->text), "%s", text);
}
extern "C" void td_list_set_count(td_widget_t *, int count)
{
    list_count = count;
}

static FILINFO file(const std::string &name, uint8_t attributes = 0)
{
    FILINFO info = {};
    assert(name.size() < sizeof(info.fname));
    std::strcpy(info.fname, name.c_str());
    info.fattrib = attributes;
    return info;
}

static void reset(void)
{
    on_close(nullptr);
    assert(allocation == nullptr);
    first_pass.clear();
    second_pass.clear();
    changed = fail_open = fail_rewind = fail_alloc = false;
    fail_read_pass = -1;
    opened = closed = list_count = 0;
    folder_widget = {};
    list_widget = {};
    s_folder = &folder_widget;
    s_list = &list_widget;
    // Also exercise replacing an existing catalog. This lets the regression
    // run safely against the old scan, which assumes launch preallocated it.
    s_entries = static_cast<entry *>(catalog_malloc(sizeof(entry) * MAX_TRACKS));
    requested = 0;
}

static void scan(void)
{
    load_folder();
    assert(opened == closed);
    assert(list_count == s_count);
}

int main(void)
{
    reset();
    for (int i = 18; i >= 0; --i)
    {
        char name[32];
        std::snprintf(name, sizeof(name), "Track %02d.MP3", i);
        first_pass.push_back(file(name));
    }
    first_pass.push_back(file("folder.mp3", AM_DIR));
    first_pass.push_back(file("hidden.mp3", AM_HID));
    first_pass.push_back(file("system.mp3", AM_SYS));
    first_pass.push_back(file("notes.txt"));
    first_pass.push_back(file(std::string(92, 'a') + ".mp3"));
    scan();
    assert(s_count == 19 && requested == 19 * sizeof(entry) && s_truncated);
    assert(std::strcmp(s_entries[0].name, "Track 00.MP3") == 0);
    assert(std::strcmp(s_entries[18].name, "Track 18.MP3") == 0);

    reset();
    scan();
    assert(s_count == 0 && allocation == nullptr && requested == 0);
    assert(std::strcmp(folder_widget.text, "/music: 0 tracks") == 0);

    reset();
    first_pass.push_back(file(std::string(91, 'a') + ".mp3"));
    scan();
    assert(s_count == 1 && std::strlen(s_entries[0].name) == 95 && !s_truncated);

    reset();
    for (int i = 0; i < 257; ++i)
        first_pass.push_back(file("song" + std::to_string(i) + ".flac"));
    scan();
    assert(s_count == 256 && requested == 256 * sizeof(entry) && s_truncated);

    reset();
    first_pass.push_back(file("song.mp3"));
    fail_alloc = true;
    scan();
    assert(s_count == 0 && allocation == nullptr);
    assert(std::strstr(folder_widget.text, "Not enough memory") != nullptr);

    reset();
    fail_open = true;
    scan();
    assert(s_count == 0 && allocation == nullptr);

    reset();
    first_pass.push_back(file("song.mp3"));
    fail_rewind = true;
    scan();
    assert(s_count == 0 && allocation == nullptr && requested == 0);

    reset();
    first_pass = {file("one.mp3"), file("two.mp3")};
    fail_read_pass = 1;
    fail_read_index = 1;
    scan();
    assert(s_count == 0 && allocation == nullptr);
    assert(std::strstr(folder_widget.text, "Could not read") != nullptr);

    reset();
    changed = true;
    first_pass = {file("one.mp3")};
    second_pass = {file("one.mp3"), file("two.mp3")};
    scan();
    assert(s_count == 1 && requested == sizeof(entry) && s_truncated);
    on_close(nullptr);
    assert(s_entries == nullptr && allocation == nullptr && s_count == 0);
    std::puts("phosphor catalog: PASS (sizing, filtering, sorting, bounds, failures, cleanup)");
    return 0;
}
