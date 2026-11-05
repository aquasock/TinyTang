/* Host stub: the device handle. SPDX-License-Identifier: MIT */
#ifndef STUB_FS_DEVICE_H
#define STUB_FS_DEVICE_H
struct bflb_device_s { int unused; };
struct bflb_device_s *bflb_device_get_by_name(const char *name);
#endif
